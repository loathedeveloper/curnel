#include "kernel.h"
#include "syscall.h"
#include "idt.h"
#include "process.h"
#include "pipe.h"
#include "filesystem.h"
#include "timer.h"
#include "usermode.h"
#include "signals.h"

// Sistem çağrı tablosu
static void* syscall_table[64] = {
    NULL,
    (void*)sys_exit,
    (void*)sys_fork,
    (void*)sys_read,
    (void*)sys_write,
    (void*)sys_open,
    (void*)sys_close,
    (void*)sys_sleep,
    (void*)sys_getpid,
    (void*)sys_exec,
    (void*)sys_getppid,
    (void*)sys_pipe,
    (void*)sys_dup,
    (void*)sys_dup2,
    (void*)sys_mkdir,
    (void*)sys_rmdir,
    (void*)sys_wait,
    (void*)sys_kill,
    (void*)sys_signal,
    (void*)sys_sigaction,
    (void*)sys_sigprocmask,
    (void*)sys_sigpending,
    (void*)sys_sigsuspend,
    (void*)sys_setpgid,
    (void*)sys_getpgid,
    (void*)sys_setsid,
    (void*)sys_getsid
};

// Sistem çağrı işleyici fonksiyonları tablosu
static uint64_t (*syscall_handlers[256])(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) = {0};

// Sistem çağrı işleyici
void syscall_handler(registers_t* regs) {
    // Çağrı numarası kontrol
    if (regs->rax >= 64 || syscall_table[regs->rax] == NULL) {
        terminal_writestring("Hata: Gecersiz sistem cagrisi numarasi: ");
        char num_str[5];
        int_to_string(regs->rax, num_str);
        terminal_writestring(num_str);
        terminal_writestring("\n");
        return;
    }
    
    // İlgili sistem çağrısını çağır
    void* func = syscall_table[regs->rax];
    
    // Sistem çağrı parametrelerini kaydedicilerden al
    // x86_64 Linux ABI: rdi, rsi, rdx, r10, r8, r9
    uint64_t result = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))func)(
        regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9
    );
    
    // Sonucu rax'a kaydet
    regs->rax = result;
    
    // Bekleyen sinyalleri kontrol et
    process_t* current = get_current_process();
    if (current) {
        signal_handle_pending(current);
    }
}

// Sistem çağrılarını başlat
void init_syscalls() {
    // Özel kesme 0x80'i syscall_handler fonksiyonuna bağla
    register_interrupt_handler(0x80, (isr_t)syscall_handler);
    
    // Pipe sistemini başlat
    pipe_init();
    
    // Sinyal sistemini başlat
    signal_init();
    
    terminal_writestring("Sistem cagrilari baslatildi.\n");
}

// Sistemden çık
uint64_t sys_exit(uint64_t status) {
    process_t* current = get_current_process();
    if (current) {
        // Çıkış durumunu günce ve süreci sonlandır
        exit_process(current->pid, status);
        
        // Yeni bir sürece geç
        schedule();
    }
    
    return 0;
}

// Süreç oluştur (fork)
uint64_t sys_fork() {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // Yeni süreç oluştur
    uint64_t child_pid = create_process(current->name, 0, current->pid);
    if (child_pid == 0) {
        return -1;
    }
    
    process_t* child = get_process(child_pid);
    if (!child) {
        return -1;
    }
    
    // Çocuk sürecin kayıtlarını kopyala (ancak ebeveyn için rax değerini child_pid yaparken, 
    // çocuk için 0 yap - fork'un geri dönüş değeri mantığı)
    memcpy(&child->registers, &current->registers, sizeof(registers_t));
    child->registers.rax = 0; // Çocuk için dönüş değeri 0
    
    // Sinyal bilgilerini kopyala
    child->pending_signals = current->pending_signals;
    child->blocked_signals = current->blocked_signals;
    
    for (int i = 0; i < 32; i++) {
        child->signal_actions[i] = current->signal_actions[i];
    }
    
    // İşleme döndür
    return child_pid;
}

// Oku
uint64_t sys_read(uint64_t fd, char* buf, uint64_t count) {
    // Eğer fd bir pipe ise
    pipe_t* pipe = pipe_get_by_fd(fd);
    if (pipe) {
        // Kullanıcı bellek adresi doğrula
        if (!usermode_validate_pointer(buf, count, PAGE_WRITABLE)) {
            return -1;
        }
        
        // Geçici kernel tamponu oluştur
        char* kbuf = kmalloc(count);
        if (!kbuf) {
            return -1;
        }
        
        // Pipe'tan oku
        int bytes_read = pipe_read(fd, kbuf, count);
        
        // Kullanıcı belleğine kopyala
        if (bytes_read > 0) {
            if (!usermode_copy_to_user(buf, kbuf, bytes_read)) {
                kfree(kbuf);
                return -1;
            }
        }
        
        kfree(kbuf);
        return bytes_read;
    }
    
    // Standart giriş
    if (fd == 0) {
        // Klavyeden oku
        int read_count = keyboard_read(buf, count);
        return read_count;
    }
    
    // Dosya
    fs_file_t* file = NULL; // Dosya tanıtıcısını bul
    // TODO: Dosya tablosundan fd numarasıyla dosyayı bul
    
    if (file) {
        return fs_read(file, buf, count);
    }
    
    return -1;
}

// Yaz
uint64_t sys_write(uint64_t fd, const char* buf, uint64_t count) {
    // Eğer fd bir pipe ise
    pipe_t* pipe = pipe_get_by_fd(fd);
    if (pipe) {
        // Kullanıcı bellek adresi doğrula
        if (!usermode_validate_pointer((void*)buf, count, 0)) {
            return -1;
        }
        
        // Geçici kernel tamponu oluştur
        char* kbuf = kmalloc(count);
        if (!kbuf) {
            return -1;
        }
        
        // Kullanıcı belleğinden kopyala
        if (!usermode_copy_from_user(kbuf, buf, count)) {
            kfree(kbuf);
            return -1;
        }
        
        // Pipe'a yaz
        int bytes_written = pipe_write(fd, kbuf, count);
        kfree(kbuf);
        return bytes_written;
    }
    
    // Standart çıkış
    if (fd == 1 || fd == 2) {
        // Kullanıcı bellek adresi doğrula
        if (!usermode_validate_pointer((void*)buf, count, 0)) {
            return -1;
        }
        
        // Geçici kernel tamponu oluştur
        char* kbuf = kmalloc(count + 1);
        if (!kbuf) {
            return -1;
        }
        
        // Kullanıcı belleğinden kopyala
        if (!usermode_copy_from_user(kbuf, buf, count)) {
            kfree(kbuf);
            return -1;
        }
        
        // NULL ile sonlandır
        kbuf[count] = '\0';
        
        // Terminale yaz
        terminal_writestring(kbuf);
        kfree(kbuf);
        return count;
    }
    
    // Dosya
    fs_file_t* file = NULL; // Dosya tanıtıcısını bul
    // TODO: Dosya tablosundan fd numarasıyla dosyayı bul
    
    if (file) {
        return fs_write(file, buf, count);
    }
    
    return -1;
}

// Dosya aç
uint64_t sys_open(const char* pathname, uint64_t flags) {
    // TODO: Dosya açma mantığını uygula
    return -1;
}

// Dosya kapat
uint64_t sys_close(uint64_t fd) {
    // Eğer fd bir pipe ise
    pipe_t* pipe = pipe_get_by_fd(fd);
    if (pipe) {
        return pipe_close(fd);
    }
    
    // Dosya
    fs_file_t* file = NULL; // Dosya tanıtıcısını bul
    // TODO: Dosya tablosundan fd numarasıyla dosyayı bul
    
    if (file) {
        return fs_close(file);
    }
    
    return -1;
}

// Uyku
uint64_t sys_sleep(uint64_t milliseconds) {
    process_sleep(milliseconds);
    return 0;
}

// Süreç kimliği
uint64_t sys_getpid() {
    process_t* current = get_current_process();
    if (current) {
        return current->pid;
    }
    return 0;
}

// Program çalıştır
uint64_t sys_exec(const char* path, char* const argv[]) {
    // TODO: Dosya sisteminden program yükleme
    return -1;
}

// Ebeveyn süreç kimliği
uint64_t sys_getppid() {
    process_t* current = get_current_process();
    if (current) {
        return current->parent_pid;
    }
    return 0;
}

// Pipe oluştur
uint64_t sys_pipe(uint64_t* fds) {
    // Kullanıcı bellek adresi doğrula
    if (!usermode_validate_pointer(fds, 2 * sizeof(uint64_t), PAGE_WRITABLE)) {
        return -1;
    }
    
    // Yeni pipe oluştur
    uint64_t read_fd, write_fd;
    int result = pipe_create(&read_fd, &write_fd);
    
    if (result == PIPE_SUCCESS) {
        // Dosya tanımlayıcılarını kullanıcı belleğine yaz
        if (!usermode_copy_to_user(&fds[0], &read_fd, sizeof(uint64_t)) ||
            !usermode_copy_to_user(&fds[1], &write_fd, sizeof(uint64_t))) {
            // Kullanıcı alanına kopyalamada hata
            pipe_close(read_fd);
            pipe_close(write_fd);
            return -1;
        }
        return 0;
    }
    
    return -1;
}

// Dosya tanımlayıcısını çoğalt
uint64_t sys_dup(uint64_t oldfd) {
    // TODO: Dosya tanımlayıcısı çoğaltma işlevi
    return -1;
}

// Dosya tanımlayıcısını belirli bir değere çoğalt
uint64_t sys_dup2(uint64_t oldfd, uint64_t newfd) {
    // TODO: Belirli bir değere dosya tanımlayıcısı çoğaltma işlevi
    return -1;
}

// Dizin oluştur
uint64_t sys_mkdir(const char* pathname, uint64_t mode) {
    // Kullanıcı bellek adresi doğrula
    if (!usermode_validate_pointer((void*)pathname, strlen(pathname) + 1, 0)) {
        return -1;
    }
    
    // Geçici kernel tamponu oluştur
    char* kpath = kmalloc(strlen(pathname) + 1);
    if (!kpath) {
        return -1;
    }
    
    // Kullanıcı belleğinden kopyala
    if (!usermode_copy_from_user(kpath, pathname, strlen(pathname) + 1)) {
        kfree(kpath);
        return -1;
    }
    
    // Dizin oluştur
    int result = fs_mkdir(kpath);
    kfree(kpath);
    return result;
}

// Dizin sil
uint64_t sys_rmdir(const char* pathname) {
    // Kullanıcı bellek adresi doğrula
    if (!usermode_validate_pointer((void*)pathname, strlen(pathname) + 1, 0)) {
        return -1;
    }
    
    // Geçici kernel tamponu oluştur
    char* kpath = kmalloc(strlen(pathname) + 1);
    if (!kpath) {
        return -1;
    }
    
    // Kullanıcı belleğinden kopyala
    if (!usermode_copy_from_user(kpath, pathname, strlen(pathname) + 1)) {
        kfree(kpath);
        return -1;
    }
    
    // Dizin sil
    int result = fs_unlink(kpath);
    kfree(kpath);
    return result;
}

// Alt süreç için bekle
uint64_t sys_wait(uint64_t* status) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // Alt süreçleri kontrol et
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = get_process(i);
        if (proc && proc->parent_pid == current->pid && proc->state == PROCESS_STATE_ZOMBIE) {
            // Durum değerini kullanıcı alanına kopyala
            uint64_t exit_code = 0; // TODO: Çıkış kodunu sakla
            
            if (status) {
                if (!usermode_validate_pointer(status, sizeof(uint64_t), PAGE_WRITABLE)) {
                    return -1;
                }
                
                if (!usermode_copy_to_user(status, &exit_code, sizeof(uint64_t))) {
                    return -1;
                }
            }
            
            // Süreç kaydını temizle
            uint64_t pid = proc->pid;
            // TODO: Süreci tamamen temizle
            
            return pid;
        }
    }
    
    // Beklenecek alt süreç yok, blokla ve yeniden dene
    block_process(current->pid);
    schedule();
    
    return -1;
}

// Sinyal gönder
uint64_t sys_kill(uint64_t pid, uint64_t signum) {
    if (signum >= MAX_SIGNALS) {
        return -1;
    }
    
    // Özel PID değerlerini kontrol et
    if (pid == 0) {
        // Aynı süreç grubundaki tüm süreçlere gönder
        process_t* current = get_current_process();
        if (!current) {
            return -1;
        }
        
        return send_signal_to_process_group(current->process_group, signum);
    } else if (pid < -1) {
        // Belirli bir süreç grubundaki tüm süreçlere gönder
        uint64_t pgid = -pid; // Negatif PID süreç grubu ID'sini temsil eder
        return send_signal_to_process_group(pgid, signum);
    } else if (pid == -1) {
        // Göndermek için izin verilen tüm süreçlere gönder
        // Basitlik için, şimdilik tüm süreçlere gönderelim
        int sent = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            process_t* proc = get_process(i);
            if (proc && proc->pid != 0) {
                if (signal_send(proc, signum) == 0) {
                    sent++;
                }
            }
        }
        return sent;
    } else {
        // Tek bir sürece gönder
        process_t* process = get_process(pid);
        if (!process) {
            return -1;
        }
        
        return signal_send(process, signum);
    }
}

// Sinyal işleyici ayarla
uint64_t sys_signal(uint64_t signum, uint64_t handler) {
    if (signum >= MAX_SIGNALS) {
        return (uint64_t)SIG_ERR;
    }
    
    return (uint64_t)signal_set_handler(signum, (signal_handler_t)handler);
}

// Sinyal aksiyonu ayarla
uint64_t sys_sigaction(uint64_t signum, uint64_t act_ptr, uint64_t oldact_ptr) {
    if (signum >= MAX_SIGNALS) {
        return -1;
    }
    
    signal_action_t act, oldact;
    
    // Kullanıcı belleğinden oku
    if (act_ptr) {
        if (!usermode_validate_pointer((void*)act_ptr, sizeof(signal_action_t), 0)) {
            return -1;
        }
        
        if (!usermode_copy_from_user(&act, (void*)act_ptr, sizeof(signal_action_t))) {
            return -1;
        }
    }
    
    // Sinyal aksiyonunu ayarla
    int result = signal_set_action(signum, act_ptr ? &act : NULL, oldact_ptr ? &oldact : NULL);
    
    // Önceki aksiyonu kullanıcı belleğine yaz
    if (oldact_ptr) {
        if (!usermode_validate_pointer((void*)oldact_ptr, sizeof(signal_action_t), PAGE_WRITABLE)) {
            return -1;
        }
        
        if (!usermode_copy_to_user((void*)oldact_ptr, &oldact, sizeof(signal_action_t))) {
            return -1;
        }
    }
    
    return result;
}

// Sinyal maskesini ayarla
uint64_t sys_sigprocmask(uint64_t how, uint64_t set_ptr, uint64_t oldset_ptr) {
    sigset_t set, oldset;
    
    // Kullanıcı belleğinden oku
    if (set_ptr) {
        if (!usermode_validate_pointer((void*)set_ptr, sizeof(sigset_t), 0)) {
            return -1;
        }
        
        if (!usermode_copy_from_user(&set, (void*)set_ptr, sizeof(sigset_t))) {
            return -1;
        }
    }
    
    // Sinyal maskesini ayarla
    int result = signal_proc_mask(how, set_ptr ? &set : NULL, oldset_ptr ? &oldset : NULL);
    
    // Önceki maskeyi kullanıcı belleğine yaz
    if (oldset_ptr) {
        if (!usermode_validate_pointer((void*)oldset_ptr, sizeof(sigset_t), PAGE_WRITABLE)) {
            return -1;
        }
        
        if (!usermode_copy_to_user((void*)oldset_ptr, &oldset, sizeof(sigset_t))) {
            return -1;
        }
    }
    
    return result;
}

// Bekleyen sinyalleri al
uint64_t sys_sigpending(uint64_t set_ptr) {
    if (!set_ptr) {
        return -1;
    }
    
    // Kullanıcı belleğini doğrula
    if (!usermode_validate_pointer((void*)set_ptr, sizeof(sigset_t), PAGE_WRITABLE)) {
        return -1;
    }
    
    // Geçerli süreci al
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // Bekleyen sinyalleri kullanıcı belleğine kopyala
    sigset_t pending = current->pending_signals;
    if (!usermode_copy_to_user((void*)set_ptr, &pending, sizeof(sigset_t))) {
        return -1;
    }
    
    return 0;
}

// Geçici sinyal maskesi ile askıya al
uint64_t sys_sigsuspend(uint64_t mask_ptr) {
    if (!mask_ptr) {
        return -1;
    }
    
    // Kullanıcı belleğini doğrula
    if (!usermode_validate_pointer((void*)mask_ptr, sizeof(sigset_t), 0)) {
        return -1;
    }
    
    // Geçerli süreci al
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // Maske değerini kullanıcı belleğinden al
    sigset_t mask;
    if (!usermode_copy_from_user(&mask, (void*)mask_ptr, sizeof(sigset_t))) {
        return -1;
    }
    
    // Önceki maskeyi kaydet
    sigset_t old_mask = current->blocked_signals;
    
    // Yeni maskeyi ayarla
    current->blocked_signals = mask & ~((1UL << SIGKILL) | (1UL << SIGSTOP));
    
    // Sinyal gelene kadar süreci blokla
    block_process(current->pid);
    schedule();
    
    // Eski maskeyi geri yükle
    current->blocked_signals = old_mask;
    
    // Bu fonksiyon her zaman -1 döndürür ve errno EINTR olur
    return -1;
}

// Süreç grubunu ayarla
uint64_t sys_setpgid(uint64_t pid, uint64_t pgid) {
    // pid 0 ise geçerli süreci kullan
    if (pid == 0) {
        process_t* current = get_current_process();
        if (!current) {
            return -1;
        }
        pid = current->pid;
    }
    
    return set_process_group(pid, pgid);
}

// Süreç grubunu al
uint64_t sys_getpgid(uint64_t pid) {
    // pid 0 ise geçerli süreci kullan
    if (pid == 0) {
        process_t* current = get_current_process();
        if (!current) {
            return -1;
        }
        return current->process_group;
    }
    
    // Süreci bul
    process_t* process = get_process(pid);
    if (!process) {
        return -1;
    }
    
    return process->process_group;
}

// Yeni oturum oluştur
uint64_t sys_setsid() {
    return create_session();
}

// Oturum kimliğini al
uint64_t sys_getsid(uint64_t pid) {
    // pid 0 ise geçerli süreci kullan
    if (pid == 0) {
        process_t* current = get_current_process();
        if (!current) {
            return -1;
        }
        return current->session_id;
    }
    
    // Süreci bul
    process_t* process = get_process(pid);
    if (!process) {
        return -1;
    }
    
    return process->session_id;
} 