#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include <stddef.h>

// Pipe tampon boyutu
#define PIPE_BUFFER_SIZE 4096

// Pipe bayrakları
#define PIPE_FLAG_READABLE  0x01
#define PIPE_FLAG_WRITABLE  0x02
#define PIPE_FLAG_NONBLOCK  0x04

// Pipe hata kodları
#define PIPE_SUCCESS        0
#define PIPE_ERROR_FULL    -1
#define PIPE_ERROR_EMPTY   -2
#define PIPE_ERROR_CLOSED  -3

// Pipe yapısı
typedef struct {
    uint64_t id;                  // Benzersiz pipe kimliği
    uint8_t buffer[PIPE_BUFFER_SIZE]; // Veri tamponu
    size_t read_pos;              // Okuma konumu
    size_t write_pos;             // Yazma konumu
    size_t data_size;             // Tamponda bulunan veri miktarı
    uint8_t flags;                // Bayraklar
    uint64_t reader_pid;          // Okuyucu süreç kimliği
    uint64_t writer_pid;          // Yazıcı süreç kimliği
    uint8_t reader_open;          // Okuma ucu açık mı?
    uint8_t writer_open;          // Yazma ucu açık mı?
} pipe_t;

// Pipe yönetimi için fonksiyon prototipleri
int pipe_create(uint64_t* read_fd, uint64_t* write_fd);
int pipe_close(uint64_t fd);
int pipe_read(uint64_t fd, void* buffer, size_t count);
int pipe_write(uint64_t fd, const void* buffer, size_t count);
int pipe_set_flags(uint64_t fd, uint8_t flags);

// Dahili işlevler
pipe_t* pipe_get_by_fd(uint64_t fd);
int pipe_init();

#endif // PIPE_H 