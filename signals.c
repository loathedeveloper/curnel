#include "kernel.h"
#include "signals.h"
#include "process.h"
#include "usermode.h"

// Varsayılan sinyal aksiyonları
static int default_signal_actions[MAX_SIGNALS] = {
    0,              // 0 - Geçersiz sinyal
    SIGTERM,        // SIGHUP (1) - Terminate
    SIGTERM,        // SIGINT (2) - Terminate
    SIGTERM | 0x80, // SIGQUIT (3) - Terminate & Core Dump
    SIGTERM | 0x80, // SIGILL (4) - Terminate & Core Dump
    SIGTERM | 0x80, // SIGTRAP (5) - Terminate & Core Dump
    SIGTERM | 0x80, // SIGABRT (6) - Terminate & Core Dump
    SIGTERM | 0x80, // SIGBUS (7) - Terminate & Core Dump
    SIGTERM | 0x80, // SIGFPE (8) - Terminate & Core Dump
    SIGTERM,        // SIGKILL (9) - Terminate (durdurulamaz)
    SIGTERM,        // SIGUSR1 (10) - Terminate
    SIGTERM | 0x80, // SIGSEGV (11) - Terminate & Core Dump
    SIGTERM,        // SIGUSR2 (12) - Terminate
    SIGTERM,        // SIGPIPE (13) - Terminate
    SIGTERM,        // SIGALRM (14) - Terminate
    SIGTERM,        // SIGTERM (15) - Terminate
    SIGTERM,        // SIGSTKFLT (16) - Terminate
    0,              // SIGCHLD (17) - Ignore
    1,              // SIGCONT (18) - Continue (durdurulmuş süreci devam ettir)
    2,              // SIGSTOP (19) - Stop (durdurulamaz)
    2,              // SIGTSTP (20) - Stop
    2,              // SIGTTIN (21) - Stop
    2,              // SIGTTOU (22) - Stop
    0,              // SIGURG (23) - Ignore
    SIGTERM,        // SIGXCPU (24) - Terminate
    SIGTERM,        // SIGXFSZ (25) - Terminate
    SIGTERM,        // SIGVTALRM (26) - Terminate
    SIGTERM,        // SIGPROF (27) - Terminate
    0,              // SIGWINCH (28) - Ignore
    SIGTERM,        // SIGIO (29) - Terminate
    SIGTERM,        // SIGPWR (30) - Terminate
    SIGTERM | 0x80  // SIGSYS (31) - Terminate & Core Dump
};

// Sinyal işleme durumu
static uint8_t signal_handling_enabled = 0;

// Sinyal sistemini başlat
int signal_init(void) {
    signal_handling_enabled = 1;
    terminal_writestring("Sinyal sistemi baslatildi.\n");
    return 0;
}

// Sinyali bir sürece gönder
int signal_send(process_t *process, int signum) {
    if (!process || signum <= 0 || signum >= MAX_SIGNALS) {
        return -1;
    }
    
    // SIGKILL ve SIGSTOP sinyalleri her zaman işlenir
    if (signum == SIGKILL) {
        // Süreci sonlandır
        exit_process(process->pid, 128 + signum);
        return 0;
    } else if (signum == SIGSTOP) {
        // Süreci duraklat
        stop_process(process->pid);
        return 0;
    } else if (signum == SIGCONT) {
        // Süreci devam ettir (durmuşsa)
        if (process->state == PROCESS_STATE_STOPPED) {
            continue_process(process->pid);
        }
        return 0;
    }
    
    // Bekleyen sinyallere ekle
    return signal_add_pending(process, signum);
}

// Sinyal işleyiciyi ayarla (basit sürüm)
int signal_set_handler(int signum, signal_handler_t handler) {
    if (signum <= 0 || signum >= MAX_SIGNALS) {
        return -1;
    }
    
    process_t *current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // SIGKILL ve SIGSTOP için işleyici ayarlanamaz
    if (signum == SIGKILL || signum == SIGSTOP) {
        return -1;
    }
    
    // Önceki işleyiciyi kaydet
    signal_handler_t old_handler = current->signal_actions[signum].handler;
    
    // Yeni işleyiciyi ayarla
    current->signal_actions[signum].handler = handler;
    
    // Önceki işleyiciyi döndür
    return (int)old_handler;
}

// Sinyal aksiyonunu ayarla (detaylı sürüm)
int signal_set_action(int signum, const signal_action_t *act, signal_action_t *oldact) {
    if (signum <= 0 || signum >= MAX_SIGNALS) {
        return -1;
    }
    
    process_t *current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // SIGKILL ve SIGSTOP için aksiyon ayarlanamaz
    if (signum == SIGKILL || signum == SIGSTOP) {
        return -1;
    }
    
    // Eski aksiyonu kaydet
    if (oldact) {
        *oldact = current->signal_actions[signum];
    }
    
    // Yeni aksiyonu ayarla
    if (act) {
        current->signal_actions[signum] = *act;
    }
    
    return 0;
}

// Bekleyen sinyalleri işle
int signal_handle_pending(process_t *process) {
    if (!process || !signal_handling_enabled) {
        return 0;
    }
    
    // Eğer süreç zaten bir sinyal işliyorsa, şimdilik bir şey yapma
    if (process->handling_signal) {
        return 0;
    }
    
    // Tüm bekleyen sinyalleri kontrol et
    for (int signum = 1; signum < MAX_SIGNALS; signum++) {
        // Eğer bu sinyal beklemedeyse ve bloklanmamışsa
        if ((process->pending_signals & (1UL << signum)) && 
            !(process->blocked_signals & (1UL << signum))) {
            
            // Bekleyen sinyali temizle
            process->pending_signals &= ~(1UL << signum);
            
            // Sinyal işleyicisini al
            signal_handler_t handler = process->signal_actions[signum].handler;
            
            // Varsayılan işleyici
            if (handler == SIG_DFL) {
                int action = default_signal_actions[signum];
                
                if (action & SIGTERM) {
                    // Süreci sonlandır
                    exit_process(process->pid, 128 + signum);
                    return 1;
                } else if (action == 0) {
                    // Görmezden gel
                    continue;
                } else if (action == 1) {
                    // Devam ettir
                    if (process->state == PROCESS_STATE_STOPPED) {
                        continue_process(process->pid);
                    }
                } else if (action == 2) {
                    // Durdur
                    stop_process(process->pid);
                    return 1;
                }
            }
            // Görmezden gel
            else if (handler == SIG_IGN) {
                continue;
            }
            // Özel işleyici
            else {
                // Kullanıcı tanımlı işleyiciyi çağır
                process->handling_signal = 1;
                
                // TODO: Kullanıcı modunda sinyal işleyiciyi çağıracak mantık
                // Bu karmaşık bir mekanizma - syscall dönüşünde kaydedicilere
                // müdahale etmeyi ve kullanıcı modu yığını üzerinde işlem yapmayı gerektirir
                
                // Şimdilik, doğrudan çağrı ile simüle ediyoruz
                handler(signum);
                
                process->handling_signal = 0;
                return 1;
            }
        }
    }
    
    return 0;
}

// Bekleyen sinyale ekle
int signal_add_pending(process_t *process, int signum) {
    if (!process || signum <= 0 || signum >= MAX_SIGNALS) {
        return -1;
    }
    
    // SIGKILL ve SIGSTOP her zaman işlenir
    if (signum == SIGKILL || signum == SIGSTOP) {
        process->pending_signals |= (1UL << signum);
        return 0;
    }
    
    // Eğer bloklanmamışsa, bekleyen sinyale ekle
    if (!(process->blocked_signals & (1UL << signum))) {
        process->pending_signals |= (1UL << signum);
    }
    
    return 0;
}

// Sinyalleri blokla
int signal_block(sigset_t mask) {
    process_t *current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // SIGKILL ve SIGSTOP bloklanamaz
    mask &= ~((1UL << SIGKILL) | (1UL << SIGSTOP));
    
    // Önceki maske değerini kaydet
    sigset_t old_mask = current->blocked_signals;
    
    // Maskeyi güncelle
    current->blocked_signals |= mask;
    
    return old_mask;
}

// Bloklamayı kaldır
int signal_unblock(sigset_t mask) {
    process_t *current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // Önceki maske değerini kaydet
    sigset_t old_mask = current->blocked_signals;
    
    // Maskeyi güncelle
    current->blocked_signals &= ~mask;
    
    return old_mask;
}

// Sinyal maskesini ayarla
int signal_set_mask(sigset_t mask, sigset_t *oldmask) {
    process_t *current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // SIGKILL ve SIGSTOP bloklanamaz
    mask &= ~((1UL << SIGKILL) | (1UL << SIGSTOP));
    
    // Önceki maske değerini kaydet
    if (oldmask) {
        *oldmask = current->blocked_signals;
    }
    
    // Yeni maskeyi ayarla
    current->blocked_signals = mask;
    
    return 0;
}

// Bloklanmış sinyalleri yönet
int signal_proc_mask(int how, const sigset_t *set, sigset_t *oldset) {
    process_t *current = get_current_process();
    if (!current) {
        return -1;
    }
    
    // Önceki maskeyi kaydet
    if (oldset) {
        *oldset = current->blocked_signals;
    }
    
    // Yeni maske ayarı
    if (set) {
        sigset_t mask = *set;
        mask &= ~((1UL << SIGKILL) | (1UL << SIGSTOP)); // SIGKILL ve SIGSTOP bloklanamaz
        
        switch (how) {
            case 0: // SIG_BLOCK
                current->blocked_signals |= mask;
                break;
            case 1: // SIG_UNBLOCK
                current->blocked_signals &= ~mask;
                break;
            case 2: // SIG_SETMASK
                current->blocked_signals = mask;
                break;
            default:
                return -1;
        }
    }
    
    return 0;
}

// Sistem çağrıları
uint64_t sys_kill(uint64_t pid, uint64_t signum) {
    if (signum >= MAX_SIGNALS) {
        return -1;
    }
    
    process_t *process = get_process(pid);
    if (!process) {
        return -1;
    }
    
    return signal_send(process, signum);
}

uint64_t sys_signal(uint64_t signum, uint64_t handler) {
    if (signum >= MAX_SIGNALS) {
        return (uint64_t)SIG_ERR;
    }
    
    return (uint64_t)signal_set_handler(signum, (signal_handler_t)handler);
}

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

uint64_t sys_sigpending(uint64_t set_ptr) {
    if (!set_ptr) {
        return -1;
    }
    
    // Kullanıcı belleğini doğrula
    if (!usermode_validate_pointer((void*)set_ptr, sizeof(sigset_t), PAGE_WRITABLE)) {
        return -1;
    }
    
    // Geçerli süreci al
    process_t *current = get_current_process();
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

uint64_t sys_sigsuspend(uint64_t mask_ptr) {
    if (!mask_ptr) {
        return -1;
    }
    
    // Kullanıcı belleğini doğrula
    if (!usermode_validate_pointer((void*)mask_ptr, sizeof(sigset_t), 0)) {
        return -1;
    }
    
    // Geçerli süreci al
    process_t *current = get_current_process();
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