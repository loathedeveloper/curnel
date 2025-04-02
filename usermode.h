#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>
#include "process.h"
#include "paging.h"

// Kullanıcı modu ayrıcalık seviyesi
#define DPL_KERNEL 0
#define DPL_USER   3

// GDT segment tanımlayıcıları
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

// GDT bayrakları
#define GDT_PRESENT     0x80
#define GDT_DPL_RING0   0x00
#define GDT_DPL_RING3   0x60
#define GDT_TYPE_CODE   0x1A  // Yürütülebilir, Okunabilir, Uyumlu değil
#define GDT_TYPE_DATA   0x12  // Yürütülemez, Yazılabilir, Genişlemeyen
#define GDT_TYPE_TSS    0x09  // TSS mevcut
#define GDT_LONG_MODE   0x20  // 64-bit segment

// GDT giriş yapısı
typedef struct {
    uint16_t limit_low;      // Limit (0-15)
    uint16_t base_low;       // Taban (0-15)
    uint8_t  base_middle;    // Taban (16-23)
    uint8_t  access;         // Erişim bayrakları
    uint8_t  granularity;    // Limit (16-19) ve bayraklar
    uint8_t  base_high;      // Taban (24-31)
} __attribute__((packed)) gdt_entry_t;

// GDT işaretçisi yapısı
typedef struct {
    uint16_t limit;          // GDT boyutu
    uint64_t base;           // GDT taban adresi
} __attribute__((packed)) gdt_ptr_t;

// TSS yapısı (64-bit)
typedef struct {
    uint32_t reserved1;      // Ayrılmış
    uint64_t rsp0;           // Ring 0 yığın işaretçisi
    uint64_t rsp1;           // Ring 1 yığın işaretçisi
    uint64_t rsp2;           // Ring 2 yığın işaretçisi
    uint64_t reserved2;      // Ayrılmış
    uint64_t ist1;           // Kesme yığın tablosu 1
    uint64_t ist2;           // Kesme yığın tablosu 2
    uint64_t ist3;           // Kesme yığın tablosu 3
    uint64_t ist4;           // Kesme yığın tablosu 4
    uint64_t ist5;           // Kesme yığın tablosu 5
    uint64_t ist6;           // Kesme yığın tablosu 6
    uint64_t ist7;           // Kesme yığın tablosu 7
    uint64_t reserved3;      // Ayrılmış
    uint16_t reserved4;      // Ayrılmış
    uint16_t iomap_base;     // I/O harita tabanı
} __attribute__((packed)) tss_t;

// GDT ve TSS işlevleri
void gdt_init();
void tss_init(void* kernel_stack);
void tss_set_kernel_stack(void* stack);

// Kullanıcı modu işlevleri
void usermode_enter(void* entry_point, void* user_stack);
void usermode_exit();

// Bellek koruması işlevleri
int usermode_validate_pointer(void* ptr, size_t size, uint32_t access_flags);
int usermode_copy_from_user(void* dst, const void* src, size_t size);
int usermode_copy_to_user(void* dst, const void* src, size_t size);

// Kullanıcı programı yükleme
int usermode_load_program(process_t* process, const char* filename);

#endif // USERMODE_H 