#include "kernel.h"
#include "usermode.h"
#include "paging.h"
#include "process.h"

// GDT ve TSS yapıları
static gdt_entry_t gdt[6];  // Null, Kernel Code, Kernel Data, User Code, User Data, TSS
static gdt_ptr_t gdt_ptr;
static tss_t tss;

// GDT girişi ayarlama işlevi
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    // Taban adresi
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    // Limitler
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    
    // Granülasyon bayrakları
    gdt[num].granularity |= granularity & 0xF0;
    
    // Erişim bayrakları
    gdt[num].access = access;
}

// TSS girişini ayarla (64-bit)
static void gdt_set_tss(int num, uint64_t base, uint32_t limit, uint8_t access) {
    // Normal GDT girişini ayarla
    gdt_set_gate(num, (uint32_t)base, limit, access, 0);
    
    // 64-bit TSS için ek işlemler burada gerekebilir
    // Bu basit örnek için yeterli
}

// GDT'yi yükle
extern void gdt_flush(uint64_t gdt_ptr_addr);
extern void tss_flush(uint16_t tss_seg);

// GDT başlatma
void gdt_init() {
    // GDT işaretçisi
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 6) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    
    // Null segment
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // Kernel Code segment (0x08)
    gdt_set_gate(1, 0, 0xFFFFF, GDT_PRESENT | GDT_DPL_RING0 | GDT_TYPE_CODE | GDT_LONG_MODE, 0xA0);
    
    // Kernel Data segment (0x10)
    gdt_set_gate(2, 0, 0xFFFFF, GDT_PRESENT | GDT_DPL_RING0 | GDT_TYPE_DATA, 0xC0);
    
    // User Code segment (0x18)
    gdt_set_gate(3, 0, 0xFFFFF, GDT_PRESENT | GDT_DPL_RING3 | GDT_TYPE_CODE | GDT_LONG_MODE, 0xA0);
    
    // User Data segment (0x20)
    gdt_set_gate(4, 0, 0xFFFFF, GDT_PRESENT | GDT_DPL_RING3 | GDT_TYPE_DATA, 0xC0);
    
    // TSS (0x28)
    gdt_set_tss(5, (uint64_t)&tss, sizeof(tss), GDT_PRESENT | GDT_TYPE_TSS);
    
    // GDT'yi yükle
    gdt_flush((uint64_t)&gdt_ptr);
    
    terminal_writestring("GDT baslatildi.\n");
}

// TSS başlatma
void tss_init(void* kernel_stack) {
    // TSS bellek alanını temizle
    memset(&tss, 0, sizeof(tss));
    
    // Ring 0 yığınını ayarla
    tss.rsp0 = (uint64_t)kernel_stack;
    
    // TSS'yi yükle
    tss_flush(GDT_TSS);
    
    terminal_writestring("TSS baslatildi.\n");
}

// TSS kernel yığınını güncelle
void tss_set_kernel_stack(void* stack) {
    tss.rsp0 = (uint64_t)stack;
}

// Kullanıcı moduna girişi hazırlayan assembly fonksiyonu
extern void usermode_jump(uint64_t user_cs, uint64_t user_ds, uint64_t user_rip, uint64_t user_rsp);

// Kullanıcı moduna gir
void usermode_enter(void* entry_point, void* user_stack) {
    // Kullanıcı moduna geçmeden önce her şeyin hazır olduğundan emin ol
    
    // Geçerli işlemi al
    process_t* current = get_current_process();
    if (!current) {
        terminal_writestring("Hata: Kullanici moduna gecerken gecerli islem yok!\n");
        return;
    }
    
    // Kernel yığını TSS'de ayarla
    tss_set_kernel_stack(current->stack + current->stack_size);
    
    // Kullanıcı adres alanına geçiş
    if (current->page_directory != 0) {
        paging_switch_address_space((void*)current->page_directory);
    }
    
    // Kullanıcı moduna geçiş
    terminal_writestring("Kullanici moduna geciliyor...\n");
    usermode_jump(GDT_USER_CODE | 3, GDT_USER_DATA | 3, (uint64_t)entry_point, (uint64_t)user_stack);
    
    // Bu noktaya asla ulaşılmamalı
}

// Kullanıcı modundan çık
void usermode_exit() {
    // İşlem sonlandığında, scheduler tarafından kernel moduna dönülür
}

// Kullanıcı işaretçisini doğrula
int usermode_validate_pointer(void* ptr, size_t size, uint32_t access_flags) {
    // İşaretçi geçerli bir kullanıcı alanında mı?
    if ((uint64_t)ptr < USER_BASE || (uint64_t)ptr >= USER_STACK_TOP) {
        return 0; // Geçersiz adres
    }
    
    // İşaretçinin kapsadığı tüm sayfaları kontrol et
    uint64_t start_addr = (uint64_t)ptr;
    uint64_t end_addr = start_addr + size - 1;
    
    // Sayfa başlangıçlarını hizala
    uint64_t start_page = start_addr & ~0xFFF;
    uint64_t end_page = end_addr & ~0xFFF;
    
    // Her sayfayı kontrol et
    for (uint64_t page = start_page; page <= end_page; page += PAGE_SIZE) {
        // Sayfanın fiziksel karşılığını kontrol et
        void* phys_addr = paging_get_physical_address((void*)page);
        if (!phys_addr) {
            return 0; // Sayfa eşlenmemiş
        }
        
        // Sayfa hakları kontrol et
        uint64_t* pt_entry = (uint64_t*)paging_walk((void*)page, 0, 0);
        if (!pt_entry || !(*pt_entry & PAGE_PRESENT)) {
            return 0; // Sayfa mevcut değil
        }
        
        // Kullanıcı erişimi kontrol et
        if (!(*pt_entry & PAGE_USER)) {
            return 0; // Kullanıcıya kapalı
        }
        
        // Yazma erişimi kontrol et (eğer gerekiyorsa)
        if ((access_flags & PAGE_WRITABLE) && !(*pt_entry & PAGE_WRITABLE)) {
            return 0; // Yazma erişimi yok
        }
    }
    
    return 1; // İşaretçi geçerli
}

// Kullanıcı alanından kernel alanına kopyala
int usermode_copy_from_user(void* dst, const void* src, size_t size) {
    // Kullanıcı işaretçisini doğrula
    if (!usermode_validate_pointer((void*)src, size, 0)) {
        return 0; // Geçersiz kaynak
    }
    
    // Bayt bayt kopyala (kullanıcı belleği sayfaları arası geçişler için)
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    
    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    
    return 1; // Başarılı
}

// Kernel alanından kullanıcı alanına kopyala
int usermode_copy_to_user(void* dst, const void* src, size_t size) {
    // Kullanıcı işaretçisini doğrula
    if (!usermode_validate_pointer(dst, size, PAGE_WRITABLE)) {
        return 0; // Geçersiz hedef
    }
    
    // Bayt bayt kopyala (kullanıcı belleği sayfaları arası geçişler için)
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    
    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    
    return 1; // Başarılı
}

// Kullanıcı programını bellekte oluştur ve yükle
// Bu basit bir ELF yükleyici olacak 
int usermode_load_program(process_t* process, const char* filename) {
    // Dosya sisteminden programı oku
    fs_file_t file;
    if (fs_open(filename, &file) != FS_SUCCESS) {
        terminal_writestring("Hata: Program dosyasi acilamadi!\n");
        return 0;
    }
    
    // Basit ELF başlık kontrolü (sadece kavramsal örnek)
    uint8_t elf_header[64];
    if (fs_read(&file, elf_header, 64) != 64) {
        terminal_writestring("Hata: ELF baslik okunamadi!\n");
        fs_close(&file);
        return 0;
    }
    
    // ELF dosyası mı kontrol et (sihirli sayı)
    if (elf_header[0] != 0x7F || elf_header[1] != 'E' || elf_header[2] != 'L' || elf_header[3] != 'F') {
        terminal_writestring("Hata: Gecerli bir ELF dosyasi degil!\n");
        fs_close(&file);
        return 0;
    }
    
    // 64-bit ELF mi?
    if (elf_header[4] != 2) { // ELFCLASS64
        terminal_writestring("Hata: 64-bit ELF dosyasi degil!\n");
        fs_close(&file);
        return 0;
    }
    
    // Yürütülebilir mi?
    if (elf_header[16] != 2) { // ET_EXEC
        terminal_writestring("Hata: Yurutulebilir dosya degil!\n");
        fs_close(&file);
        return 0;
    }
    
    // Program başlığı offset ve boyutunu al
    uint64_t ph_offset = *((uint64_t*)(elf_header + 32)); // e_phoff
    uint16_t ph_size = *((uint16_t*)(elf_header + 54));   // e_phentsize
    uint16_t ph_count = *((uint16_t*)(elf_header + 56));  // e_phnum
    
    // Giriş noktasını al
    uint64_t entry_point = *((uint64_t*)(elf_header + 24)); // e_entry
    
    // Kullanıcı adres alanı oluştur
    void* user_pml4 = paging_create_user_address_space();
    if (!user_pml4) {
        terminal_writestring("Hata: Kullanici adres alani olusturulamadi!\n");
        fs_close(&file);
        return 0;
    }
    
    // Adres alanını işleme ata
    process->page_directory = (uint64_t)user_pml4;
    
    // Program başlıklarını oku ve yükle
    for (uint16_t i = 0; i < ph_count; i++) {
        // Dosya konumunu ayarla
        fs_seek(&file, ph_offset + i * ph_size);
        
        // Program başlığını oku
        uint8_t ph_data[56]; // sizeof(Elf64_Phdr)
        if (fs_read(&file, ph_data, 56) != 56) {
            terminal_writestring("Hata: Program basligi okunamadi!\n");
            fs_close(&file);
            return 0;
        }
        
        // Program başlığı türü
        uint32_t ph_type = *((uint32_t*)ph_data); // p_type
        
        // Sadece yüklenebilir segmentleri işle (PT_LOAD)
        if (ph_type != 1) { // PT_LOAD
            continue;
        }
        
        // Segment bilgilerini al
        uint64_t p_vaddr = *((uint64_t*)(ph_data + 16)); // p_vaddr
        uint64_t p_filesz = *((uint64_t*)(ph_data + 32)); // p_filesz
        uint64_t p_memsz = *((uint64_t*)(ph_data + 40));  // p_memsz
        uint64_t p_offset = *((uint64_t*)(ph_data + 8));  // p_offset
        uint32_t p_flags = *((uint32_t*)(ph_data + 4));   // p_flags
        
        // Bellek ayır ve sayfa eşlemeleri oluştur
        uint64_t page_count = (p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
        
        // Sayfa hizalama
        uint64_t vaddr_aligned = p_vaddr & ~0xFFF;
        uint64_t offset_in_page = p_vaddr & 0xFFF;
        
        // Sayfa bayraklarını oluştur
        uint64_t page_flags = PAGE_PRESENT;
        if (p_flags & 0x2) { // PF_W
            page_flags |= PAGE_WRITABLE;
        }
        
        // Sayfaları eşle
        for (uint64_t j = 0; j < page_count; j++) {
            uint64_t vaddr = vaddr_aligned + j * PAGE_SIZE;
            
            // Sayfa tahsis et
            void* page = paging_alloc_user_page(user_pml4, (void*)vaddr, page_flags);
            if (!page) {
                terminal_writestring("Hata: Kullanici sayfasi tahsis edilemedi!\n");
                fs_close(&file);
                return 0;
            }
            
            // İlk sayfada offset var mı?
            uint64_t page_offset = (j == 0) ? offset_in_page : 0;
            
            // Dosyadan verileri oku
            if (j * PAGE_SIZE < p_filesz) {
                uint64_t bytes_to_read = PAGE_SIZE - page_offset;
                if (j * PAGE_SIZE + bytes_to_read > p_filesz) {
                    bytes_to_read = p_filesz - j * PAGE_SIZE;
                }
                
                // Dosya konumunu ayarla
                fs_seek(&file, p_offset + j * PAGE_SIZE);
                
                // Sayfa içeriğini oku
                fs_read(&file, (void*)(vaddr + page_offset), bytes_to_read);
            }
        }
    }
    
    // Kullanıcı yığını oluştur
    uint64_t stack_size = 64 * 1024; // 64 KB
    uint64_t stack_bottom = USER_STACK_TOP - stack_size;
    
    // Yığın sayfalarını eşle
    for (uint64_t addr = stack_bottom; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
        if (!paging_alloc_user_page(user_pml4, (void*)addr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)) {
            terminal_writestring("Hata: Kullanici yigini tahsis edilemedi!\n");
            fs_close(&file);
            return 0;
        }
    }
    
    // İşlem bilgilerini güncelle
    process->registers.rip = entry_point;
    process->registers.rsp = USER_STACK_TOP;
    
    // Dosyayı kapat
    fs_close(&file);
    
    return 1; // Başarılı
}

// Assembly stub for usermode transition
__attribute__((naked)) void usermode_jump(uint64_t user_cs, uint64_t user_ds, uint64_t user_rip, uint64_t user_rsp) {
    asm volatile(
        "mov %rdx, %rcx    \n" // rcx = user_rip (3. parametre, rdx)
        "mov %r8, %r11     \n" // r11 = user_rsp (4. parametre, r8)
        "mov %rsi, %r9     \n" // r9 = user_ds (2. parametre, rsi)
        "mov %r9, %ds      \n" // ds = user_ds
        "mov %r9, %es      \n" // es = user_ds
        "mov %r9, %fs      \n" // fs = user_ds
        "mov %r9, %gs      \n" // gs = user_ds
        "pushq %r9         \n" // ss (user_ds)
        "pushq %r11        \n" // rsp (user_rsp)
        "pushq $0x202      \n" // rflags (IF biti açık)
        "pushq %rdi        \n" // cs (user_cs, 1. parametre)
        "pushq %rcx        \n" // rip (user_rip)
        "iretq             \n" // Kullanıcı moduna geçiş
    );
} 