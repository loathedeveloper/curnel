#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "idt.h"

// Sistem çağrı numaraları
#define SYS_EXIT       1   // Süreç çıkış
#define SYS_FORK       2   // Yeni süreç oluşturma
#define SYS_READ       3   // Okuma
#define SYS_WRITE      4   // Yazma
#define SYS_OPEN       5   // Dosya açma
#define SYS_CLOSE      6   // Dosya kapatma
#define SYS_SLEEP      7   // Uyuma
#define SYS_GETPID     8   // Süreç kimliğini alma
#define SYS_EXEC       9   // Program çalıştırma
#define SYS_GETPPID    10  // Ebeveyn süreç kimliğini alma
#define SYS_PIPE       11  // Pipe oluştur
#define SYS_DUP        12  // Dosya tanımlayıcısını çoğalt
#define SYS_DUP2       13  // Dosya tanımlayıcısını belirli bir değere çoğalt
#define SYS_MKDIR      14  // Dizin oluştur
#define SYS_RMDIR      15  // Dizin sil
#define SYS_WAIT       16  // Alt süreç için bekle
#define SYS_KILL       17  // Sinyal gönder
#define SYS_SIGNAL     18  // Sinyal işleyicisini ayarla
#define SYS_SIGACTION  19  // Sinyal aksiyonunu ayarla
#define SYS_SIGPROCMASK 20 // Sinyal maskesini ayarla
#define SYS_SIGPENDING 21  // Bekleyen sinyalleri al
#define SYS_SIGSUSPEND 22  // Geçici sinyal maskesi ile askıya al
#define SYS_SETPGID    23  // Süreç grubunu ayarla
#define SYS_GETPGID    24  // Süreç grubunu al
#define SYS_SETSID     25  // Yeni oturum oluştur
#define SYS_GETSID     26  // Oturum kimliğini al

// Sistem çağrı işleyici
void syscall_handler(registers_t* regs);

// Sistem çağrı inicializasyonu
void init_syscalls();

// Sistem çağrı işlevleri prototipleri
uint64_t sys_exit(uint64_t status);
uint64_t sys_fork();
uint64_t sys_read(uint64_t fd, char* buf, uint64_t count);
uint64_t sys_write(uint64_t fd, const char* buf, uint64_t count);
uint64_t sys_open(const char* pathname, uint64_t flags);
uint64_t sys_close(uint64_t fd);
uint64_t sys_sleep(uint64_t milliseconds);
uint64_t sys_getpid();
uint64_t sys_exec(const char* path, char* const argv[]);
uint64_t sys_getppid();
uint64_t sys_pipe(uint64_t* fds);
uint64_t sys_dup(uint64_t oldfd);
uint64_t sys_dup2(uint64_t oldfd, uint64_t newfd);
uint64_t sys_mkdir(const char* pathname, uint64_t mode);
uint64_t sys_rmdir(const char* pathname);
uint64_t sys_wait(uint64_t* status);
uint64_t sys_kill(uint64_t pid, uint64_t signum);
uint64_t sys_signal(uint64_t signum, uint64_t handler);
uint64_t sys_sigaction(uint64_t signum, uint64_t act, uint64_t oldact);
uint64_t sys_sigprocmask(uint64_t how, uint64_t set, uint64_t oldset);
uint64_t sys_sigpending(uint64_t set);
uint64_t sys_sigsuspend(uint64_t mask);
uint64_t sys_setpgid(uint64_t pid, uint64_t pgid);
uint64_t sys_getpgid(uint64_t pid);
uint64_t sys_setsid();
uint64_t sys_getsid(uint64_t pid);

#endif // SYSCALL_H 