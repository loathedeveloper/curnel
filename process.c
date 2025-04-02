#include "kernel.h"
#include "process.h"
#include "signals.h"
#include "timer.h"

// Süreç tablosu ve mevcut süreç
static process_t processes[MAX_PROCESSES];
static uint64_t current_process_index = 0;
static uint64_t next_pid = 1;

// Süreç yönetimini başlat
void init_processes() {
    // Süreç tablosunu temizle
    memset(processes, 0, sizeof(processes));
    
    terminal_writestring("Surec yonetimi baslatildi.\n");
}

// Yeni süreç oluştur
uint64_t create_process(const char* name, uint64_t entry_point, uint64_t parent_pid) {
    // Boş süreç girişi ara
    int proc_index = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == 0) {
            proc_index = i;
            break;
        }
    }
    
    if (proc_index == -1) {
        terminal_writestring("Hata: Surec tablosu dolu!\n");
        return 0; // Başarısız
    }
    
    // Süreç yapısını doldur
    process_t* process = &processes[proc_index];
    process->pid = next_pid++;
    process->parent_pid = parent_pid;
    
    // Süreç adını kopyala
    int i;
    for (i = 0; i < 31 && name[i]; i++) {
        process->name[i] = name[i];
    }
    process->name[i] = '\0';
    
    // Süreç durumunu ayarla
    process->state = PROCESS_STATE_READY;
    
    // Süreç kaydedicilerini hazırla
    memset(&process->registers, 0, sizeof(registers_t));
    process->registers.rip = entry_point;
    
    // Süreç yığınını oluştur (4KB)
    process->stack_size = 4096;
    process->stack = kmalloc(process->stack_size);
    process->registers.rsp = (uint64_t)process->stack + process->stack_size;
    
    // Başlangıç zamanını ayarla
    process->start_time = timer_get_ticks();
    process->cpu_time = 0;
    
    // Sinyal bilgilerini başlat
    process->pending_signals = 0;
    process->blocked_signals = 0;
    process->handling_signal = 0;
    process->exit_signal = 0;
    
    // Sinyal işleyicilerini varsayılan değerlere ayarla
    for (int i = 0; i < 32; i++) {
        process->signal_actions[i].handler = SIG_DFL;
        process->signal_actions[i].flags = 0;
        process->signal_actions[i].mask = 0;
    }
    
    // Dosya tanımlayıcılarını sıfırla
    for (int i = 0; i < 16; i++) {
        process->file_descriptors[i] = 0;
    }
    
    // Standart giriş, çıkış ve hata çıkışını ayarla
    process->file_descriptors[0] = 0; // stdin
    process->file_descriptors[1] = 1; // stdout
    process->file_descriptors[2] = 2; // stderr
    
    // Süreç grubu bilgilerini ayarla
    process->process_group = process->pid; // Varsayılan olarak kendi group'unu oluştur
    process->session_id = process->pid;    // Varsayılan olarak kendi session'ını oluştur
    
    // Terminal bilgisini ayarla
    process->tty = 0; // Varsayılan terminal
    
    terminal_writestring("Yeni surec olusturuldu: PID=");
    char pid_str[10];
    int_to_string(process->pid, pid_str);
    terminal_writestring(pid_str);
    terminal_writestring(", Isim=");
    terminal_writestring(process->name);
    terminal_writestring("\n");
    
    return process->pid;
}

// CPU zamanlayıcısı - sıradaki süreci seç ve ona geç
void schedule() {
    // Şu anki süreç
    uint64_t prev_process_index = current_process_index;
    
    // Tüm süreçleri kontrol et ve çalıştırılabilecek olanı bul
    uint64_t checked = 0;
    while (checked < MAX_PROCESSES) {
        current_process_index = (current_process_index + 1) % MAX_PROCESSES;
        checked++;
        
        // Geçerli bir süreç bulundu mu?
        if (processes[current_process_index].pid != 0 && 
            (processes[current_process_index].state == PROCESS_STATE_READY ||
             processes[current_process_index].state == PROCESS_STATE_RUNNING)) {
            break;
        }
    }
    
    // Çalıştırılabilir süreç yoksa, ilk sürece dön
    if (checked == MAX_PROCESSES) {
        current_process_index = prev_process_index;
        return;
    }
    
    // Şu anki süreci RUNNING olarak işaretle
    if (processes[current_process_index].pid != 0) {
        processes[current_process_index].state = PROCESS_STATE_RUNNING;
        
        // Bekleyen sinyalleri kontrol et ve işle
        signal_handle_pending(&processes[current_process_index]);
        
        // Bağlamı değiştir (context switch)
        switch_to_process(processes[current_process_index].pid);
    }
}

// Belirli bir sürece geç
void switch_to_process(uint64_t pid) {
    // Süreci bul
    int proc_index = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            proc_index = i;
            break;
        }
    }
    
    if (proc_index == -1) {
        return; // Süreç bulunamadı
    }
    
    // Süreci RUNNING olarak işaretle
    processes[proc_index].state = PROCESS_STATE_RUNNING;
    
    // İstatistikleri güncelle
    processes[proc_index].cpu_time += 1;
    
    // TODO: Gerçek bağlam değiştirme
    // CPU kaydedicilerini kaydet, yeni sürecin kaydedicilerini yükle
    // Göstermelik olarak şu anki süreci değiştir
    current_process_index = proc_index;
}

// Süreci blokla
void block_process(uint64_t pid) {
    // Süreci bul
    process_t* process = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            process = &processes[i];
            break;
        }
    }
    
    if (!process) {
        return; // Süreç bulunamadı
    }
    
    // Süreci BLOCKED olarak işaretle
    process->state = PROCESS_STATE_BLOCKED;
}

// Süreci bloklanmış durumdan çıkar
void unblock_process(uint64_t pid) {
    // Süreci bul
    process_t* process = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            process = &processes[i];
            break;
        }
    }
    
    if (!process) {
        return; // Süreç bulunamadı
    }
    
    // Süreç BLOCKED durumundaysa READY durumuna getir
    if (process->state == PROCESS_STATE_BLOCKED) {
        process->state = PROCESS_STATE_READY;
    }
}

// Süreci belirli bir süre uyut
void sleep_process(uint64_t pid, uint64_t ms) {
    // Süreci bul
    process_t* process = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            process = &processes[i];
            break;
        }
    }
    
    if (!process) {
        return; // Süreç bulunamadı
    }
    
    // Uyanma zamanını ayarla ve süreci SLEEPING olarak işaretle
    process->sleep_until = timer_get_ticks() + ms;
    process->state = PROCESS_STATE_SLEEPING;
    
    // Başka bir sürece geç
    schedule();
}

// Süreci sonlandır
void exit_process(uint64_t pid, uint64_t exit_code) {
    // Süreci bul
    process_t* process = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            process = &processes[i];
            break;
        }
    }
    
    if (!process) {
        return; // Süreç bulunamadı
    }
    
    // Çıkış sinyalini ebeveyn sürece gönder
    if (process->exit_signal) {
        process_t* parent = NULL;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (processes[i].pid == process->parent_pid) {
                parent = &processes[i];
                break;
            }
        }
        
        if (parent) {
            signal_send(parent, process->exit_signal);
        }
    }
    
    // SIGCHLD sinyalini ebeveyn sürece gönder
    if (process->parent_pid) {
        process_t* parent = NULL;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (processes[i].pid == process->parent_pid) {
                parent = &processes[i];
                break;
            }
        }
        
        if (parent) {
            signal_send(parent, SIGCHLD);
        }
    }
    
    // Süreç kaynakları temizle
    if (process->stack) {
        kfree(process->stack);
        process->stack = NULL;
    }
    
    // Sinyal yığınını temizle
    if (process->signal_stack) {
        kfree(process->signal_stack);
        process->signal_stack = NULL;
    }
    
    // Eğer alt süreçler varsa, onları da öksüz yap
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid != 0 && processes[i].parent_pid == pid) {
            processes[i].parent_pid = 1; // init sürecine bağla
        }
    }
    
    // Süreci zombie durumuna getir, ebeveyn tarafından toplandıktan sonra tamamen silinecek
    terminal_writestring("Surec sonlandirildi: PID=");
    char pid_str[10];
    int_to_string(process->pid, pid_str);
    terminal_writestring(pid_str);
    terminal_writestring(", Cikis kodu=");
    int_to_string(exit_code, pid_str);
    terminal_writestring(pid_str);
    terminal_writestring("\n");
    
    process->state = PROCESS_STATE_ZOMBIE;
    
    // Başka bir sürece geç
    schedule();
}

// Süreci durdur (SIGSTOP benzeri)
void stop_process(uint64_t pid) {
    // Süreci bul
    process_t* process = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            process = &processes[i];
            break;
        }
    }
    
    if (!process) {
        return; // Süreç bulunamadı
    }
    
    // Süreci STOPPED olarak işaretle
    process->state = PROCESS_STATE_STOPPED;
    
    terminal_writestring("Surec durduruldu: PID=");
    char pid_str[10];
    int_to_string(process->pid, pid_str);
    terminal_writestring(pid_str);
    terminal_writestring("\n");
    
    // Başka bir sürece geç
    schedule();
}

// Durmuş süreci devam ettir (SIGCONT benzeri)
void continue_process(uint64_t pid) {
    // Süreci bul
    process_t* process = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            process = &processes[i];
            break;
        }
    }
    
    if (!process) {
        return; // Süreç bulunamadı
    }
    
    // Süreç STOPPED durumundaysa READY durumuna getir
    if (process->state == PROCESS_STATE_STOPPED) {
        process->state = PROCESS_STATE_READY;
        
        terminal_writestring("Surec devam ettirildi: PID=");
        char pid_str[10];
        int_to_string(process->pid, pid_str);
        terminal_writestring(pid_str);
        terminal_writestring("\n");
    }
}

// Şu anki süreci al
process_t* get_current_process() {
    if (processes[current_process_index].pid != 0) {
        return &processes[current_process_index];
    }
    return NULL;
}

// Belirli PID'ye sahip süreci al
process_t* get_process(uint64_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return NULL;
}

// Süreç grubunu ayarla
int set_process_group(uint64_t pid, uint64_t pgid) {
    process_t* process = get_process(pid);
    if (!process) {
        return -1;
    }
    
    // pgid 0 ise, sürecin kendi pid'sini kullan
    if (pgid == 0) {
        pgid = pid;
    }
    
    // Süreç grubunu ayarla
    process->process_group = pgid;
    return 0;
}

// Yeni bir oturum oluştur
int create_session(void) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // Eğer süreç grubu lideri ise, yeni oturum oluşturulamaz
    if (current->pid == current->process_group) {
        return -1;
    }
    
    // Yeni oturum ve süreç grubu oluştur
    current->session_id = current->pid;
    current->process_group = current->pid;
    
    return current->session_id;
}

// Sürecin öksüz bir süreç grubuna ait olup olmadığını kontrol et
int is_orphaned_process_group(uint64_t pgid) {
    // Süreç grubundaki tüm süreçlerin ebeveynleri aynı oturumda değilse,
    // bu süreç grubu öksüzdür
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid != 0 && processes[i].process_group == pgid) {
            process_t* parent = get_process(processes[i].parent_pid);
            if (parent && parent->session_id == processes[i].session_id) {
                return 0; // En az bir ebeveyn aynı oturumda
            }
        }
    }
    
    return 1; // Tüm süreçlerin ebeveynleri farklı oturumlarda
}

// Sürecin belirtilen süreç grubuna ait olup olmadığını kontrol et
int is_process_group_member(uint64_t pid, uint64_t pgid) {
    process_t* process = get_process(pid);
    if (!process) {
        return 0;
    }
    
    return (process->process_group == pgid);
}

// Bir süreç grubundaki tüm süreçlere sinyal gönder
int send_signal_to_process_group(uint64_t pgid, int signum) {
    int sent = 0;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid != 0 && processes[i].process_group == pgid) {
            if (signal_send(&processes[i], signum) == 0) {
                sent++;
            }
        }
    }
    
    return sent; // Kaç sürece sinyal gönderildiğini döndür
}

// Bir süreç grubundaki süreçlerin tamamlanması için bekle
int wait_for_process_group(uint64_t pgid, uint64_t* status) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // Süreç grubunda aktif süreç var mı kontrol et
    int active_processes = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid != 0 && 
            processes[i].process_group == pgid && 
            processes[i].parent_pid == current->pid &&
            processes[i].state != PROCESS_STATE_ZOMBIE) {
            active_processes++;
        }
    }
    
    if (active_processes == 0) {
        // Aktif süreç yok, wait'i tamamla
        return 0;
    }
    
    // Aktif süreçler var, blokla ve bekle
    current->state = PROCESS_STATE_BLOCKED;
    schedule();
    
    return 0;
} 