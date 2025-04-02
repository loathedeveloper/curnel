#include "kernel.h"
#include "pipe.h"
#include "process.h"

// Pipe dizisi
#define MAX_PIPES 64
static pipe_t pipes[MAX_PIPES];
static uint64_t next_pipe_id = 1;

// Pipe yönetimini başlat
int pipe_init() {
    // Tüm pipe yapılarını sıfırla
    memset(pipes, 0, sizeof(pipes));
    terminal_writestring("Pipe yonetimi baslatildi.\n");
    return PIPE_SUCCESS;
}

// Yeni bir pipe oluştur
int pipe_create(uint64_t* read_fd, uint64_t* write_fd) {
    // Boş bir pipe bul
    int pipe_index = -1;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].id == 0) {
            pipe_index = i;
            break;
        }
    }
    
    // Pipe sınırına ulaşıldı mı?
    if (pipe_index == -1) {
        return PIPE_ERROR_FULL;
    }
    
    // Pipe yapısını ayarla
    pipe_t* pipe = &pipes[pipe_index];
    pipe->id = next_pipe_id++;
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->data_size = 0;
    pipe->flags = 0;
    pipe->reader_open = 1;
    pipe->writer_open = 1;
    
    // Geçerli süreci al
    process_t* current = get_current_process();
    if (current) {
        pipe->reader_pid = current->pid;
        pipe->writer_pid = current->pid;
    }
    
    // Dosya tanımlayıcılarını oluştur
    // En düşük 2 bit pipe indeksini, 3. bit okuma/yazma modunu belirler
    *read_fd = (pipe->id << 3) | PIPE_FLAG_READABLE;
    *write_fd = (pipe->id << 3) | PIPE_FLAG_WRITABLE;
    
    return PIPE_SUCCESS;
}

// Pipe'ı FD'den bul
pipe_t* pipe_get_by_fd(uint64_t fd) {
    uint64_t pipe_id = fd >> 3;
    
    // Tüm pipe'ları dolaş
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].id == pipe_id) {
            return &pipes[i];
        }
    }
    
    return NULL;
}

// Pipe'ı kapat
int pipe_close(uint64_t fd) {
    pipe_t* pipe = pipe_get_by_fd(fd);
    if (!pipe) {
        return PIPE_ERROR_CLOSED;
    }
    
    // Hangi ucun kapatıldığını kontrol et
    if (fd & PIPE_FLAG_READABLE) {
        pipe->reader_open = 0;
    }
    
    if (fd & PIPE_FLAG_WRITABLE) {
        pipe->writer_open = 0;
    }
    
    // Her iki uç da kapalıysa pipe'ı tamamen kaldır
    if (!pipe->reader_open && !pipe->writer_open) {
        pipe->id = 0; // Pipe'ı serbest bırak
    }
    
    return PIPE_SUCCESS;
}

// Pipe'tan oku
int pipe_read(uint64_t fd, void* buffer, size_t count) {
    pipe_t* pipe = pipe_get_by_fd(fd);
    if (!pipe) {
        return PIPE_ERROR_CLOSED;
    }
    
    // Okuma ucu açık mı ve okuma izni var mı?
    if (!pipe->reader_open || !(fd & PIPE_FLAG_READABLE)) {
        return PIPE_ERROR_CLOSED;
    }
    
    // Veri var mı?
    if (pipe->data_size == 0) {
        // Engelleme modunu kontrol et
        if (pipe->flags & PIPE_FLAG_NONBLOCK) {
            return PIPE_ERROR_EMPTY;
        }
        
        // Yazma ucu kapalıysa boş dön
        if (!pipe->writer_open) {
            return 0; // EOF
        }
        
        // Veri olana kadar oku süreci blokla
        process_t* current = get_current_process();
        while (pipe->data_size == 0 && pipe->writer_open) {
            block_process(current->pid);
            schedule(); // Başka bir sürece geç
        }
        
        // Tekrar kontrol et
        if (pipe->data_size == 0) {
            return 0; // EOF
        }
    }
    
    // Okunacak veri miktarını belirle
    size_t bytes_to_read = (count < pipe->data_size) ? count : pipe->data_size;
    
    // Veriyi kopyala
    uint8_t* buf = (uint8_t*)buffer;
    for (size_t i = 0; i < bytes_to_read; i++) {
        buf[i] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUFFER_SIZE;
    }
    
    // Veri boyutunu güncelle
    pipe->data_size -= bytes_to_read;
    
    // Yazar sürecini uyandır (yazma yeri açıldı)
    if (pipe->writer_pid != 0) {
        unblock_process(pipe->writer_pid);
    }
    
    return bytes_to_read;
}

// Pipe'a yaz
int pipe_write(uint64_t fd, const void* buffer, size_t count) {
    pipe_t* pipe = pipe_get_by_fd(fd);
    if (!pipe) {
        return PIPE_ERROR_CLOSED;
    }
    
    // Yazma ucu açık mı ve yazma izni var mı?
    if (!pipe->writer_open || !(fd & PIPE_FLAG_WRITABLE)) {
        return PIPE_ERROR_CLOSED;
    }
    
    // Okuma ucu kapalıysa hata döndür
    if (!pipe->reader_open) {
        return PIPE_ERROR_CLOSED; // SIGPIPE sinyali olmalı normalde
    }
    
    // Yazılacak veri işlenene kadar döngü
    size_t bytes_written = 0;
    const uint8_t* buf = (const uint8_t*)buffer;
    
    while (bytes_written < count) {
        // Pipe doluysa bekle veya hata döndür
        if (pipe->data_size == PIPE_BUFFER_SIZE) {
            // Engelleme modunu kontrol et
            if (pipe->flags & PIPE_FLAG_NONBLOCK) {
                return (bytes_written > 0) ? bytes_written : PIPE_ERROR_FULL;
            }
            
            // Boş yer olana kadar yazar süreci blokla
            process_t* current = get_current_process();
            block_process(current->pid);
            schedule(); // Başka bir sürece geç
            
            // Tekrar kontrol et
            if (!pipe->reader_open) {
                return PIPE_ERROR_CLOSED;
            }
        }
        
        // Yazılabilir byte sayısını hesapla
        size_t space_available = PIPE_BUFFER_SIZE - pipe->data_size;
        size_t bytes_to_write = (count - bytes_written < space_available) ? 
                                (count - bytes_written) : space_available;
        
        // Veriyi kopyala
        for (size_t i = 0; i < bytes_to_write; i++) {
            pipe->buffer[pipe->write_pos] = buf[bytes_written + i];
            pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUFFER_SIZE;
        }
        
        // Güncelle
        pipe->data_size += bytes_to_write;
        bytes_written += bytes_to_write;
        
        // Okuyucu süreci uyandır (veri var)
        if (pipe->reader_pid != 0) {
            unblock_process(pipe->reader_pid);
        }
    }
    
    return bytes_written;
}

// Pipe bayraklarını ayarla
int pipe_set_flags(uint64_t fd, uint8_t flags) {
    pipe_t* pipe = pipe_get_by_fd(fd);
    if (!pipe) {
        return PIPE_ERROR_CLOSED;
    }
    
    // Sadece NONBLOCK bayrağını ayarlamayı destekle
    pipe->flags = (pipe->flags & ~PIPE_FLAG_NONBLOCK) | (flags & PIPE_FLAG_NONBLOCK);
    
    return PIPE_SUCCESS;
} 