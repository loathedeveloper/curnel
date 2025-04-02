#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stddef.h>

// Sayfa hizalama sabiti
#define PAGE_SIZE 4096

// Sayfa tablosu bayrakları
#define PAGE_PRESENT    0x1        // Sayfa mevcut
#define PAGE_WRITABLE   0x2        // Sayfa yazılabilir
#define PAGE_USER       0x4        // Kullanıcı erişebilir
#define PAGE_WRITETHROUGH 0x8      // Yazma geçişi etkin
#define PAGE_CACHE_DISABLE 0x10    // Önbellek devre dışı
#define PAGE_ACCESSED   0x20       // Sayfaya erişildi
#define PAGE_DIRTY      0x40       // Sayfa değiştirildi
#define PAGE_SIZE_BIT   0x80       // Huge page (1 GB)
#define PAGE_GLOBAL     0x100      // Global sayfa

// Sanal adres alanı bölümleri
#define KERNEL_BASE     0xFFFFFFFF80000000  // Kernelin başlangıç sanal adresi
#define USER_BASE       0x0000000000400000  // Kullanıcı alanının başlangıcı
#define USER_STACK_TOP  0x00007FFFFFFFFFFF  // Kullanıcı yığınının tepesi

// Sayfa tablosu yapısı (x86_64, 4 seviye)
typedef struct {
    uint64_t entries[512];
} page_table_t;

// Fiziksel bellek haritası
typedef struct {
    uint64_t start;              // Başlangıç adresi
    uint64_t size;               // Boyut (bayt)
    uint8_t  type;               // Bellek türü
} memory_map_entry_t;

// Fiziksel bellek yöneticisi
typedef struct {
    uint64_t total_memory;       // Toplam bellek (bayt)
    uint64_t free_memory;        // Serbest bellek (bayt)
    uint64_t used_memory;        // Kullanılan bellek (bayt)
    uint64_t reserved_memory;    // Ayrılmış bellek (bayt)
    
    uint64_t bitmap_size;        // Bitmap boyutu (bayt)
    uint8_t* bitmap;             // Bellek tahsis bitmap'i
} physical_memory_manager_t;

// Sanal bellek yöneticisi
typedef struct {
    page_table_t* pml4;          // Üst seviye sayfa tablosu (CR3)
} virtual_memory_manager_t;

// Sayfalama işlevleri
void paging_init(memory_map_entry_t* mmap, uint32_t mmap_count);
void* paging_alloc_page();
void paging_free_page(void* addr);
void* paging_map_page(void* phys_addr, void* virt_addr, uint64_t flags);
void paging_unmap_page(void* virt_addr);
void* paging_get_physical_address(void* virt_addr);

// Kernel bellek tahsisi
void* kmalloc_page();
void* kmalloc_pages(uint64_t count);
void kfree_page(void* addr);
void kfree_pages(void* addr, uint64_t count);

// Kullanıcı bellek alanı işlevleri
void* paging_create_user_address_space();
void paging_switch_address_space(void* pml4);
void* paging_alloc_user_page(void* user_pml4, void* virt_addr, uint64_t flags);
void paging_free_user_page(void* user_pml4, void* virt_addr);

// TLB temizleme
void paging_flush_tlb(void* addr);
void paging_flush_tlb_all();

#endif // PAGING_H 