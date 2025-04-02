#include "kernel.h"

// Basit bellek yönetim sistemi
// Bu gerçek bir kernel için oldukça basittir

#define MEMORY_START 0x100000 // 1MB başlangıç
#define MEMORY_SIZE  0x100000 // 1MB bellek alanı
#define BLOCK_SIZE   4096     // 4KB blok boyutu

typedef struct memory_block {
    size_t size;
    uint8_t is_free;
    struct memory_block* next;
} memory_block_t;

static memory_block_t* memory_start = NULL;
static uint8_t memory_initialized = 0;

// Bellek yönetimini başlat
void memory_init(void) {
    if (memory_initialized) return;
    
    // İlk blok
    memory_start = (memory_block_t*)MEMORY_START;
    memory_start->size = MEMORY_SIZE - sizeof(memory_block_t);
    memory_start->is_free = 1;
    memory_start->next = NULL;
    
    memory_initialized = 1;
}

// Bellek bloğu ayır
void* kmalloc(size_t size) {
    if (!memory_initialized) {
        memory_init();
    }
    
    if (size == 0) {
        return NULL;
    }
    
    // 8 byte'a hizala
    if (size % 8 != 0) {
        size = size + (8 - (size % 8));
    }
    
    memory_block_t* current = memory_start;
    
    // Uygun blok bul
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            // Blok yeterince büyük mü?
            if (current->size > size + sizeof(memory_block_t) + BLOCK_SIZE) {
                // Yeni blok oluştur
                memory_block_t* new_block = (memory_block_t*)((uint64_t)current + sizeof(memory_block_t) + size);
                new_block->size = current->size - size - sizeof(memory_block_t);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                // Mevcut bloğu ayarla
                current->size = size;
                current->next = new_block;
            }
            
            current->is_free = 0;
            return (void*)((uint64_t)current + sizeof(memory_block_t));
        }
        
        current = current->next;
    }
    
    // Yeterli bellek yok
    return NULL;
}

// Bellek bloğunu serbest bırak
void kfree(void* ptr) {
    if (ptr == NULL || !memory_initialized) {
        return;
    }
    
    memory_block_t* block = (memory_block_t*)((uint64_t)ptr - sizeof(memory_block_t));
    
    // Blok serbest olarak işaretle
    block->is_free = 1;
    
    // Ardışık boş blokları birleştir
    memory_block_t* current = memory_start;
    
    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
            // Blokları birleştir
            current->size = current->size + sizeof(memory_block_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
} 