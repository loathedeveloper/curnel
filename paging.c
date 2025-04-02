#include "kernel.h"
#include "paging.h"

// Fiziksel ve sanal bellek yöneticileri
static physical_memory_manager_t pmm;
static virtual_memory_manager_t vmm;

// Kernel sayfa tablosu
extern uint64_t boot_pml4;

// TLB temizleme işlevi
void paging_flush_tlb(void* addr) {
    asm volatile("invlpg (%0)" : : "r" (addr) : "memory");
}

// Tüm TLB'yi temizle
void paging_flush_tlb_all() {
    // CR3 kaydedicisini tekrar yükleyerek tüm TLB'yi temizle
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r" (cr3));
    asm volatile("mov %0, %%cr3" : : "r" (cr3) : "memory");
}

// Fiziksel sayfa tahsis et (bitmap kullanarak)
static void* pmm_alloc_page() {
    // Bitmap'te boş bir bit ara
    for (uint64_t i = 0; i < pmm.bitmap_size * 8; i++) {
        uint64_t byte_index = i / 8;
        uint64_t bit_index = i % 8;
        uint8_t bit_mask = 1 << bit_index;
        
        // Bit 0 ise (sayfa boş), tahsis et
        if (!(pmm.bitmap[byte_index] & bit_mask)) {
            // Sayfayı işaretle (1 = kullanımda)
            pmm.bitmap[byte_index] |= bit_mask;
            
            // Bellek istatistiklerini güncelle
            pmm.free_memory -= PAGE_SIZE;
            pmm.used_memory += PAGE_SIZE;
            
            // Fiziksel adresi döndür
            void* addr = (void*)(i * PAGE_SIZE);
            
            // Sayfa içeriğini temizle
            memset(addr, 0, PAGE_SIZE);
            
            return addr;
        }
    }
    
    terminal_writestring("Hata: Fiziksel bellek doldu!\n");
    return NULL;
}

// Fiziksel sayfayı serbest bırak
static void pmm_free_page(void* addr) {
    // Adresin PAGE_SIZE ile hizalı olduğunu kontrol et
    if ((uint64_t)addr % PAGE_SIZE != 0) {
        terminal_writestring("Hata: Hizalanmamis adres serbest birakilmaya calisiliyor!\n");
        return;
    }
    
    // Bit indeksini hesapla
    uint64_t bit_index = (uint64_t)addr / PAGE_SIZE;
    
    // Bit zaten 0 ise (sayfa zaten boş), hata
    if (!(pmm.bitmap[bit_index / 8] & (1 << (bit_index % 8)))) {
        terminal_writestring("Hata: Zaten serbest olan sayfa serbest birakilmaya calisiliyor!\n");
        return;
    }
    
    // Biti temizle (0 = boş)
    pmm.bitmap[bit_index / 8] &= ~(1 << (bit_index % 8));
    
    // Bellek istatistiklerini güncelle
    pmm.free_memory += PAGE_SIZE;
    pmm.used_memory -= PAGE_SIZE;
}

// Sayfa tablosu girişi oluştur
static uint64_t paging_make_entry(void* phys_addr, uint64_t flags) {
    // Adresin üst 40 bitini (12-51) al ve bayraklarla birleştir
    return ((uint64_t)phys_addr & 0x000FFFFFFFFFF000) | flags;
}

// 4 seviyeli sayfa tablosunda sanal adresi fiziksel adrese çevir
// (PML4 -> PDPT -> PD -> PT -> Fiziksel sayfa)
static void* paging_walk(void* virt_addr, int alloc, uint64_t flags) {
    uint64_t addr = (uint64_t)virt_addr;
    uint64_t pml4_index = (addr >> 39) & 0x1FF;
    uint64_t pdpt_index = (addr >> 30) & 0x1FF;
    uint64_t pd_index = (addr >> 21) & 0x1FF;
    uint64_t pt_index = (addr >> 12) & 0x1FF;
    
    page_table_t* pml4 = vmm.pml4;
    
    // PML4 girişini kontrol et
    if (!(pml4->entries[pml4_index] & PAGE_PRESENT)) {
        if (!alloc) {
            return NULL; // Sayfa tablosu yok ve oluşturma isteği yok
        }
        
        // Yeni PDPT tablosu oluştur
        void* pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) {
            return NULL;
        }
        
        // PML4 girişini ayarla
        pml4->entries[pml4_index] = paging_make_entry(pdpt_phys, PAGE_PRESENT | PAGE_WRITABLE | flags);
    }
    
    // PDPT tablosunu al
    page_table_t* pdpt = (page_table_t*)((pml4->entries[pml4_index] & 0x000FFFFFFFFFF000));
    
    // PDPT girişini kontrol et
    if (!(pdpt->entries[pdpt_index] & PAGE_PRESENT)) {
        if (!alloc) {
            return NULL;
        }
        
        // Yeni PD tablosu oluştur
        void* pd_phys = pmm_alloc_page();
        if (!pd_phys) {
            return NULL;
        }
        
        // PDPT girişini ayarla
        pdpt->entries[pdpt_index] = paging_make_entry(pd_phys, PAGE_PRESENT | PAGE_WRITABLE | flags);
    }
    
    // PD tablosunu al
    page_table_t* pd = (page_table_t*)((pdpt->entries[pdpt_index] & 0x000FFFFFFFFFF000));
    
    // PD girişini kontrol et
    if (!(pd->entries[pd_index] & PAGE_PRESENT)) {
        if (!alloc) {
            return NULL;
        }
        
        // Yeni PT tablosu oluştur
        void* pt_phys = pmm_alloc_page();
        if (!pt_phys) {
            return NULL;
        }
        
        // PD girişini ayarla
        pd->entries[pd_index] = paging_make_entry(pt_phys, PAGE_PRESENT | PAGE_WRITABLE | flags);
    }
    
    // PT tablosunu al
    page_table_t* pt = (page_table_t*)((pd->entries[pd_index] & 0x000FFFFFFFFFF000));
    
    // PT girişini kontrol et ve döndür
    return (void*)&pt->entries[pt_index];
}

// Sayfalama başlatma
void paging_init(memory_map_entry_t* mmap, uint32_t mmap_count) {
    // Bellek haritasını tara ve toplam belleği hesapla
    pmm.total_memory = 0;
    pmm.free_memory = 0;
    pmm.used_memory = 0;
    pmm.reserved_memory = 0;
    
    for (uint32_t i = 0; i < mmap_count; i++) {
        // Kullanılabilir bellek mi?
        if (mmap[i].type == 1) {
            pmm.total_memory += mmap[i].size;
            pmm.free_memory += mmap[i].size;
        } else {
            pmm.reserved_memory += mmap[i].size;
        }
    }
    
    // Bitmap boyutunu hesapla (her bit bir sayfa temsil eder)
    pmm.bitmap_size = pmm.total_memory / PAGE_SIZE / 8;
    if (pmm.bitmap_size * 8 * PAGE_SIZE < pmm.total_memory) {
        pmm.bitmap_size++; // Yuvarla
    }
    
    // Bitmap için bellek ayır (kernel sonu ile başlangıç arasında sabit bir yer)
    pmm.bitmap = (uint8_t*)0x100000; // 1MB
    
    // Bitmap'i temizle (tüm sayfalar boş)
    memset(pmm.bitmap, 0, pmm.bitmap_size);
    
    // Kernel ve bitmap alanını işaretle (kullanımda)
    uint64_t kernel_start = 0; // Kernel başlangıç adresi
    uint64_t kernel_end = 0x400000; // Varsayılan 4MB
    uint64_t bitmap_end = (uint64_t)pmm.bitmap + pmm.bitmap_size;
    
    // Bitmap ve kernel alanını tahsis edilmiş olarak işaretle
    for (uint64_t addr = kernel_start; addr < kernel_end; addr += PAGE_SIZE) {
        uint64_t bit_index = addr / PAGE_SIZE;
        pmm.bitmap[bit_index / 8] |= (1 << (bit_index % 8));
    }
    
    for (uint64_t addr = (uint64_t)pmm.bitmap; addr < bitmap_end; addr += PAGE_SIZE) {
        uint64_t bit_index = addr / PAGE_SIZE;
        pmm.bitmap[bit_index / 8] |= (1 << (bit_index % 8));
    }
    
    // Bellek istatistiklerini güncelle
    uint64_t reserved_pages = (kernel_end - kernel_start + bitmap_end - (uint64_t)pmm.bitmap) / PAGE_SIZE;
    pmm.free_memory -= reserved_pages * PAGE_SIZE;
    pmm.used_memory += reserved_pages * PAGE_SIZE;
    
    // Geçici PML4 tablosunu kullan
    vmm.pml4 = (page_table_t*)&boot_pml4;
    
    terminal_writestring("Sayfalama baslatildi. Bellek: ");
    char buf[32];
    int_to_string(pmm.total_memory / 1024 / 1024, buf);
    terminal_writestring(buf);
    terminal_writestring(" MB\n");
}

// Sayfa tahsis et
void* paging_alloc_page() {
    return pmm_alloc_page();
}

// Sayfayı serbest bırak
void paging_free_page(void* addr) {
    pmm_free_page(addr);
}

// Sanal adrese fiziksel sayfa eşle
void* paging_map_page(void* phys_addr, void* virt_addr, uint64_t flags) {
    // Sanal adresi sayfa adresine hizala
    virt_addr = (void*)((uint64_t)virt_addr & ~0xFFF);
    
    // Sayfa tabloları oluştur ve son seviye PT girişini al
    uint64_t* pt_entry = (uint64_t*)paging_walk(virt_addr, 1, flags);
    if (!pt_entry) {
        return NULL;
    }
    
    // Son seviye giriş zaten mevcut mu?
    if (*pt_entry & PAGE_PRESENT) {
        terminal_writestring("Hata: Sayfa zaten eslemesi var!\n");
        return NULL;
    }
    
    // Sayfa tablosu girişini ayarla
    *pt_entry = paging_make_entry(phys_addr, PAGE_PRESENT | flags);
    
    // TLB'yi temizle
    paging_flush_tlb(virt_addr);
    
    return virt_addr;
}

// Sanal adresi eşlemesini kaldır
void paging_unmap_page(void* virt_addr) {
    // Sayfa tabloları oluştur ve son seviye PT girişini al (oluşturma)
    uint64_t* pt_entry = (uint64_t*)paging_walk(virt_addr, 0, 0);
    if (!pt_entry) {
        return; // Eşleme yok
    }
    
    // Sayfa tablosu girişini temizle
    *pt_entry = 0;
    
    // TLB'yi temizle
    paging_flush_tlb(virt_addr);
}

// Sanal adresin karşılık geldiği fiziksel adresi bul
void* paging_get_physical_address(void* virt_addr) {
    // Sayfa tabloları oluştur ve son seviye PT girişini al (oluşturma)
    uint64_t* pt_entry = (uint64_t*)paging_walk(virt_addr, 0, 0);
    if (!pt_entry) {
        return NULL; // Eşleme yok
    }
    
    // Fiziksel adres = PT girişinden sayfa adresi + sanal adresin offset
    return (void*)((*pt_entry & 0x000FFFFFFFFFF000) | ((uint64_t)virt_addr & 0xFFF));
}

// Kernel için tek sayfa tahsis et
void* kmalloc_page() {
    // Fiziksel sayfa tahsis et
    void* phys_addr = pmm_alloc_page();
    if (!phys_addr) {
        return NULL;
    }
    
    // Kernel adres alanında sanal adres seç
    static uint64_t kernel_heap = KERNEL_BASE;
    
    // Sanal adresi eşle
    void* virt_addr = paging_map_page(phys_addr, (void*)kernel_heap, PAGE_WRITABLE);
    if (!virt_addr) {
        pmm_free_page(phys_addr);
        return NULL;
    }
    
    // Sonraki tahsis için adres alanını güncelle
    kernel_heap += PAGE_SIZE;
    
    return virt_addr;
}

// Kernel için birden çok sayfa tahsis et
void* kmalloc_pages(uint64_t count) {
    // İlk sayfayı tahsis et
    void* first_page = kmalloc_page();
    if (!first_page) {
        return NULL;
    }
    
    // Ardışık sayfaları tahsis et
    for (uint64_t i = 1; i < count; i++) {
        void* phys_addr = pmm_alloc_page();
        if (!phys_addr) {
            // Hata durumunda önceki sayfaları serbest bırak
            kfree_pages(first_page, i);
            return NULL;
        }
        
        // Sonraki sanal adresi hesapla
        void* virt_addr = (void*)((uint64_t)first_page + i * PAGE_SIZE);
        
        // Sanal adresi eşle
        if (!paging_map_page(phys_addr, virt_addr, PAGE_WRITABLE)) {
            pmm_free_page(phys_addr);
            kfree_pages(first_page, i);
            return NULL;
        }
    }
    
    return first_page;
}

// Kernel sayfasını serbest bırak
void kfree_page(void* addr) {
    // Fiziksel adresi bul
    void* phys_addr = paging_get_physical_address(addr);
    if (!phys_addr) {
        return;
    }
    
    // Sanal adresi eşlemesini kaldır
    paging_unmap_page(addr);
    
    // Fiziksel sayfayı serbest bırak
    pmm_free_page(phys_addr);
}

// Kernel sayfalarını serbest bırak
void kfree_pages(void* addr, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        kfree_page((void*)((uint64_t)addr + i * PAGE_SIZE));
    }
}

// Kullanıcı adres alanı oluştur
void* paging_create_user_address_space() {
    // Yeni PML4 tablosu oluştur
    page_table_t* new_pml4 = (page_table_t*)pmm_alloc_page();
    if (!new_pml4) {
        return NULL;
    }
    
    // PML4 tablosunu temizle
    memset(new_pml4, 0, PAGE_SIZE);
    
    // Kernel adres alanı girdilerini kopyala (yüksek sanal adresler)
    for (int i = 256; i < 512; i++) {
        new_pml4->entries[i] = vmm.pml4->entries[i];
    }
    
    return new_pml4;
}

// Adres alanını değiştir
void paging_switch_address_space(void* pml4) {
    // CR3 kaydedicisini güncelle
    asm volatile("mov %0, %%cr3" : : "r" (pml4) : "memory");
}

// Kullanıcı sayfası tahsis et
void* paging_alloc_user_page(void* user_pml4, void* virt_addr, uint64_t flags) {
    // Sanal adresin kullanıcı alanında olup olmadığını kontrol et
    if ((uint64_t)virt_addr >= KERNEL_BASE) {
        terminal_writestring("Hata: Kullanici sayfasi kernel adres alaninda!\n");
        return NULL;
    }
    
    // Fiziksel sayfa tahsis et
    void* phys_addr = pmm_alloc_page();
    if (!phys_addr) {
        return NULL;
    }
    
    // Geçici olarak orijinal PML4'ü kaydet
    page_table_t* original_pml4 = vmm.pml4;
    
    // Kullanıcı PML4'ünü aktif hale getir
    vmm.pml4 = (page_table_t*)user_pml4;
    
    // Sayfayı eşle
    void* mapped_addr = paging_map_page(phys_addr, virt_addr, flags | PAGE_USER);
    
    // Orijinal PML4'e geri dön
    vmm.pml4 = original_pml4;
    
    if (!mapped_addr) {
        pmm_free_page(phys_addr);
        return NULL;
    }
    
    return mapped_addr;
}

// Kullanıcı sayfasını serbest bırak
void paging_free_user_page(void* user_pml4, void* virt_addr) {
    // Geçici olarak orijinal PML4'ü kaydet
    page_table_t* original_pml4 = vmm.pml4;
    
    // Kullanıcı PML4'ünü aktif hale getir
    vmm.pml4 = (page_table_t*)user_pml4;
    
    // Fiziksel adresi bul
    void* phys_addr = paging_get_physical_address(virt_addr);
    
    // Sayfayı eşlemesini kaldır
    paging_unmap_page(virt_addr);
    
    // Fiziksel sayfayı serbest bırak
    if (phys_addr) {
        pmm_free_page(phys_addr);
    }
    
    // Orijinal PML4'e geri dön
    vmm.pml4 = original_pml4;
} 