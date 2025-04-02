#ifndef KERNEL_H
#define KERNEL_H

#include <stddef.h>
#include <stdint.h>

// Bellek yönetimi
void* kmalloc(size_t size);
void kfree(void* ptr);
void memory_init(void);

// Terminal fonksiyonları
void terminal_initialize(void);
void terminal_setcolor(uint8_t color);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);

// Yardımcı fonksiyonlar
size_t strlen(const char* str);
void* memset(void* dest, int val, size_t len);
void memcpy(void* dest, const void* src, size_t n);
void int_to_string(int num, char* str);

// Test fonksiyonları
void test_process();

// Kernel ana işlevi
void kernel_main(void);

#endif // KERNEL_H 