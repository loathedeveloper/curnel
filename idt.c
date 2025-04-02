#include "kernel.h"
#include "idt.h"

// IDT girdileri
static idt_entry_t idt_entries[256];
static idt_ptr_t   idt_ptr;

// ISR prototipleri
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

// Kesme işleyicileri
static isr_t interrupt_handlers[256];

// IDT girdisini ayarla
static void idt_set_gate(uint8_t num, uint64_t base, uint16_t selector, uint8_t flags) {
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_middle = (base >> 16) & 0xFFFF;
    idt_entries[num].base_high = (base >> 32) & 0xFFFFFFFF;
    
    idt_entries[num].selector = selector;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags;
    idt_entries[num].reserved = 0;
}

// IDT'yi başlat
void init_idt() {
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base = (uint64_t)&idt_entries;
    
    // IDT'yi temizle
    memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);
    
    // IDT girdilerini ayarla (ilk 32 özel)
    idt_set_gate(0, (uint64_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint64_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint64_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint64_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint64_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint64_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint64_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint64_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint64_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint64_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint64_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint64_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint64_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint64_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint64_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint64_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint64_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint64_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint64_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint64_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint64_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint64_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint64_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint64_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint64_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint64_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint64_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint64_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint64_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint64_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint64_t)isr31, 0x08, 0x8E);
    
    // IDT'yi yükle
    idt_flush((uint64_t)&idt_ptr);
    
    // Kesme işleyicilerini sıfırla
    memset(&interrupt_handlers, 0, sizeof(isr_t) * 256);
}

// Kesme işleyicisini ayarla
void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

// Kesme işleyicisi
void isr_handler(registers_t* regs) {
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else {
        terminal_writestring("Işlenmeyen kesme: ");
        
        // Kesme numarasını görüntüle
        char s[3];
        int_to_string(regs->int_no, s);
        terminal_writestring(s);
        terminal_writestring("\n");
    }
}

// Yardımcı fonksiyon - tamsayıdan string'e
void int_to_string(int num, char* str) {
    int i = 0;
    int sign = 0;
    
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    if (num < 0) {
        sign = 1;
        num = -num;
    }
    
    while (num > 0) {
        str[i++] = (num % 10) + '0';
        num /= 10;
    }
    
    if (sign) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    
    // Ters çevir
    int j, len = i;
    for (j = 0; j < len / 2; j++) {
        char temp = str[j];
        str[j] = str[len - j - 1];
        str[len - j - 1] = temp;
    }
}

// memset implementasyonu
void* memset(void* dest, int val, size_t len) {
    unsigned char* ptr = dest;
    while (len-- > 0) {
        *ptr++ = val;
    }
    return dest;
} 