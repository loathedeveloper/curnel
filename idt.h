#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// Kesme tanımlamaları
#define IRQ0  32
#define IRQ1  33
#define IRQ2  34
#define IRQ3  35
#define IRQ4  36
#define IRQ5  37
#define IRQ6  38
#define IRQ7  39
#define IRQ8  40
#define IRQ9  41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

// 64-bit IDT girdisi yapısı
typedef struct {
    uint16_t base_low;       // İşleyici adresinin alt 16 bit'i
    uint16_t selector;       // Kernel segment seçicisi
    uint8_t  always0;        // Her zaman 0
    uint8_t  flags;          // Bayraklar
    uint16_t base_middle;    // İşleyici adresinin orta 16 bit'i
    uint32_t base_high;      // İşleyici adresinin üst 32 bit'i
    uint32_t reserved;       // Ayrılmış, sıfır olmalı
} __attribute__((packed)) idt_entry_t;

// IDT işaretçi yapısı
typedef struct {
    uint16_t limit;          // IDT'nin boyutu (boyut-1)
    uint64_t base;           // IDT'nin başlangıç adresi
} __attribute__((packed)) idt_ptr_t;

// Kesme kayıt yapısı
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;  // Kaydedilen üst 64-bit kaydediciler
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;     // Kaydedilen genel kaydediciler
    uint64_t int_no, err_code;                      // Kesme numarası ve hata kodu
    uint64_t rip, cs, rflags, rsp, ss;              // CPU tarafından otomatik olarak kaydedilen değerler
} registers_t;

// Kesme işleyici fonksiyon işaretçisi
typedef void (*isr_t)(registers_t*);

// IDT işlevleri
void init_idt();
void idt_flush(uint64_t);
void register_interrupt_handler(uint8_t n, isr_t handler);
void isr_handler(registers_t* regs);

// Yardımcı işlevler
void int_to_string(int num, char* str);
void* memset(void* dest, int val, size_t len);

#endif // IDT_H 