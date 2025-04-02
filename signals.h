#ifndef SIGNALS_H
#define SIGNALS_H

#include <stdint.h>

// İleri bildirimler (forward declarations)
struct process_t;
typedef struct process_t process_t;

// Sinyal numaraları (POSIX uyumlu)
#define SIGHUP     1   // Hangup
#define SIGINT     2   // Interrupt (Ctrl+C)
#define SIGQUIT    3   // Quit (Ctrl+\)
#define SIGILL     4   // Illegal instruction
#define SIGTRAP    5   // Trace trap
#define SIGABRT    6   // Abort
#define SIGBUS     7   // Bus error
#define SIGFPE     8   // Floating-point exception
#define SIGKILL    9   // Kill (durdurulamaz)
#define SIGUSR1    10  // User-defined signal 1
#define SIGSEGV    11  // Segmentation fault
#define SIGUSR2    12  // User-defined signal 2
#define SIGPIPE    13  // Broken pipe
#define SIGALRM    14  // Alarm clock
#define SIGTERM    15  // Termination
#define SIGSTKFLT  16  // Stack fault
#define SIGCHLD    17  // Child status changed
#define SIGCONT    18  // Continue (durmuş süreci devam ettir)
#define SIGSTOP    19  // Stop (durdurulamaz)
#define SIGTSTP    20  // Keyboard stop (Ctrl+Z)
#define SIGTTIN    21  // Background read from tty
#define SIGTTOU    22  // Background write to tty
#define SIGURG     23  // Urgent socket condition
#define SIGXCPU    24  // CPU time limit exceeded
#define SIGXFSZ    25  // File size limit exceeded
#define SIGVTALRM  26  // Virtual timer expired
#define SIGPROF    27  // Profiling timer expired
#define SIGWINCH   28  // Window size change
#define SIGIO      29  // I/O now possible
#define SIGPWR     30  // Power failure restart
#define SIGSYS     31  // Bad system call

// Maksimum sinyal sayısı
#define MAX_SIGNALS 32

// Sinyal eylemleri
#define SIG_DFL     ((void*)0)   // Varsayılan eylem
#define SIG_IGN     ((void*)1)   // Sinyali görmezden gel
#define SIG_ERR     ((void*)-1)  // Hata 

// Süreç sinyal işleyici fonksiyon türü
typedef void (*signal_handler_t)(int);

// Sinyal eylem yapısı
typedef struct {
    signal_handler_t handler;  // Sinyal işleyici fonksiyon
    uint32_t flags;            // Sinyal işleme bayrakları
    uint64_t mask;             // Bloklanacak sinyaller
} signal_action_t;

// Sinyal bekleyen sinyaller ve bloklanmış sinyaller için bit maskesi
typedef uint64_t sigset_t;

// Sinyal işlevleri
int signal_init(void);
int signal_send(process_t *process, int signum);
int signal_set_handler(int signum, signal_handler_t handler);
int signal_set_action(int signum, const signal_action_t *act, signal_action_t *oldact);
int signal_handle_pending(process_t *process);
int signal_add_pending(process_t *process, int signum);
int signal_block(sigset_t mask);
int signal_unblock(sigset_t mask);
int signal_set_mask(sigset_t mask, sigset_t *oldmask);
int signal_proc_mask(int how, const sigset_t *set, sigset_t *oldset);

// Sistem çağrıları
uint64_t sys_kill(uint64_t pid, uint64_t signum);
uint64_t sys_signal(uint64_t signum, uint64_t handler);
uint64_t sys_sigaction(uint64_t signum, uint64_t act, uint64_t oldact);
uint64_t sys_sigprocmask(uint64_t how, uint64_t set, uint64_t oldset);
uint64_t sys_sigpending(uint64_t set);
uint64_t sys_sigsuspend(uint64_t mask);

#endif // SIGNALS_H 