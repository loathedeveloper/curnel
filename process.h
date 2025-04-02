#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "idt.h"

// Süreç durumları
#define PROCESS_STATE_READY    0
#define PROCESS_STATE_RUNNING  1
#define PROCESS_STATE_BLOCKED  2
#define PROCESS_STATE_SLEEPING 3
#define PROCESS_STATE_ZOMBIE   4
#define PROCESS_STATE_STOPPED  5

// Maksimum süreç sayısı
#define MAX_PROCESSES 64

// Sinyaller için yapılar
typedef uint64_t sigset_t;

// Sinyal işleyici fonksiyon türü
typedef void (*signal_handler_t)(int);

// Sinyal işleyici yapısı
typedef struct {
    signal_handler_t handler;  // Sinyal işleyici fonksiyon
    uint32_t flags;            // Bayraklar
    uint64_t mask;             // Bloklanacak sinyaller
} signal_action_t;

// Süreç yapısı
typedef struct {
    uint64_t pid;              // Süreç kimliği
    uint64_t parent_pid;       // Ebeveyn süreç kimliği
    char name[32];             // Süreç adı
    uint8_t state;             // Süreç durumu
    
    // Kayıt durumu
    registers_t registers;     // Kaydedilen kaydediciler
    
    // Bellek bilgisi
    uint64_t page_directory;   // Sayfa dizini
    void* stack;               // Yığın işaretçisi
    uint64_t stack_size;       // Yığın boyutu
    
    // İstatistikler
    uint64_t sleep_until;      // Uyanma zamanı
    uint64_t start_time;       // Başlangıç zamanı
    uint64_t cpu_time;         // CPU kullanım süresi
    
    // Sinyal bilgileri
    sigset_t pending_signals;  // Bekleyen sinyaller
    sigset_t blocked_signals;  // Bloklanmış sinyaller
    signal_action_t signal_actions[32]; // Sinyal işleyiciler
    void* signal_stack;        // Sinyal işleyici yığını
    uint8_t handling_signal;   // Sinyal işleniyor mu?
    int exit_signal;           // Çıkışta gönderilecek sinyal
    
    // Dosya tanımlayıcıları
    uint64_t file_descriptors[16]; // Sürecin açık dosya tanımlayıcıları
    
    // Terminal bilgisi
    uint64_t tty;              // Terminal kimliği
    
    // Grup bilgisi
    uint64_t process_group;    // Süreç grup kimliği
    uint64_t session_id;       // Oturum kimliği
} process_t;

// Süreç yönetim fonksiyonları
void init_processes();
uint64_t create_process(const char* name, uint64_t entry_point, uint64_t parent_pid);
void schedule();
void switch_to_process(uint64_t pid);
void block_process(uint64_t pid);
void unblock_process(uint64_t pid);
void sleep_process(uint64_t pid, uint64_t ms);
void exit_process(uint64_t pid, uint64_t exit_code);
void stop_process(uint64_t pid);
void continue_process(uint64_t pid);
process_t* get_current_process();
process_t* get_process(uint64_t pid);
int set_process_group(uint64_t pid, uint64_t pgid);
int create_session(void);
int is_orphaned_process_group(uint64_t pgid);
int is_process_group_member(uint64_t pid, uint64_t pgid);

// İş kontrolü için fonksiyonlar
int send_signal_to_process_group(uint64_t pgid, int signum);
int wait_for_process_group(uint64_t pgid, uint64_t* status);

#endif // PROCESS_H 