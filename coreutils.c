#include "kernel.h"
#include "coreutils.h"
#include "filesystem.h"
#include "process.h"
#include "syscall.h"
#include "signals.h"

// Komut listesi
command_t commands[] = {
    {
        "ls", 
        cmd_ls, 
        "Dizin içeriğini listele", 
        "ls [dizin]"
    },
    {
        "cd", 
        cmd_cd, 
        "Çalışma dizinini değiştir", 
        "cd [dizin]"
    },
    {
        "pwd", 
        cmd_pwd, 
        "Mevcut çalışma dizinini göster", 
        "pwd"
    },
    {
        "mkdir", 
        cmd_mkdir, 
        "Yeni dizin oluştur", 
        "mkdir <dizin>"
    },
    {
        "rmdir", 
        cmd_rmdir, 
        "Dizini sil", 
        "rmdir <dizin>"
    },
    {
        "rm", 
        cmd_rm, 
        "Dosya sil", 
        "rm <dosya>"
    },
    {
        "cp", 
        cmd_cp, 
        "Dosya kopyala", 
        "cp <kaynak> <hedef>"
    },
    {
        "mv", 
        cmd_mv, 
        "Dosya taşı veya yeniden adlandır", 
        "mv <kaynak> <hedef>"
    },
    {
        "cat", 
        cmd_cat, 
        "Dosya içeriğini görüntüle", 
        "cat <dosya>"
    },
    {
        "echo", 
        cmd_echo, 
        "Metni ekrana yazdır", 
        "echo [metin...]"
    },
    {
        "ps", 
        cmd_ps, 
        "Çalışan süreçleri göster", 
        "ps"
    },
    {
        "kill", 
        cmd_kill, 
        "Sürece sinyal gönder", 
        "kill [-s sinyal] <pid>"
    },
    {
        "sleep", 
        cmd_sleep, 
        "Belirtilen süre kadar bekle", 
        "sleep <saniye>"
    },
    {
        "touch", 
        cmd_touch, 
        "Boş dosya oluştur veya zaman damgasını güncelle", 
        "touch <dosya>"
    },
    {
        "clear", 
        cmd_clear, 
        "Ekranı temizle", 
        "clear"
    },
    {
        "help", 
        cmd_help, 
        "Komut yardımını göster", 
        "help [komut]"
    },
    {
        "exit", 
        cmd_exit, 
        "Kabuktan çık", 
        "exit [durum]"
    }
};

// Komut sayısı
int num_commands = sizeof(commands) / sizeof(commands[0]);

// Komut satırı parse etme
int parse_command_line(const char* command_line, char** argv, int max_args) {
    int argc = 0;
    int i = 0;
    int in_word = 0;
    
    while (command_line[i] != '\0' && argc < max_args - 1) {
        // Boşluk karakterlerini atla
        if (command_line[i] == ' ' || command_line[i] == '\t') {
            if (in_word) {
                // Argümanı sonlandır
                ((char*)command_line)[i] = '\0';
                in_word = 0;
            }
        } else {
            if (!in_word) {
                // Yeni argüman başlat
                argv[argc++] = (char*)&command_line[i];
                in_word = 1;
            }
        }
        i++;
    }
    
    // Argüman listesini NULL ile sonlandır
    argv[argc] = NULL;
    
    return argc;
}

// Komut yürütme
int execute_command(const char* command_line) {
    char* argv[64];
    int argc = parse_command_line(command_line, argv, 64);
    
    if (argc == 0) {
        // Boş komut satırı
        return 0;
    }
    
    // Dahili komutları ara
    for (int i = 0; i < num_commands; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            return commands[i].func(argc, argv);
        }
    }
    
    // Bulunamadı
    terminal_writestring("Komut bulunamadi: ");
    terminal_writestring(argv[0]);
    terminal_writestring("\n");
    
    return -1;
}

// Dizin listele - ls komutu
int cmd_ls(int argc, char** argv) {
    const char* path = ".";
    
    // Dizin argümanı kontrolü
    if (argc > 1) {
        path = argv[1];
    }
    
    // Dizin açma
    fs_dir_t* dir = fs_opendir(path);
    if (!dir) {
        terminal_writestring("Hata: Dizin acilamadi: ");
        terminal_writestring(path);
        terminal_writestring("\n");
        return -1;
    }
    
    // Dizin içeriğini okuma
    fs_dirent_t entry;
    int count = 0;
    
    terminal_writestring("Dizin listesi: ");
    terminal_writestring(path);
    terminal_writestring("\n");
    terminal_writestring("----------------------------------\n");
    
    while (fs_readdir(dir, &entry) == 0) {
        if (entry.name[0] == '.' && entry.name[1] == '\0') {
            continue; // '.' girdisini atla
        }
        
        if (entry.name[0] == '.' && entry.name[1] == '.' && entry.name[2] == '\0') {
            continue; // '..' girdisini atla
        }
        
        // Dosya/dizin tipine göre renk ayarla
        if (entry.type == FS_TYPE_DIR) {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
            terminal_writestring(entry.name);
            terminal_writestring("/");
        } else {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            terminal_writestring(entry.name);
        }
        
        // Biçimlendirme için boşluk ekle
        for (int i = strlen(entry.name); i < 20; i++) {
            terminal_writestring(" ");
        }
        
        count++;
        
        // Her 4 öğe için satır sonu ekle
        if (count % 4 == 0) {
            terminal_writestring("\n");
        }
    }
    
    // Son satır sonu ekle (eğer gerekiyorsa)
    if (count % 4 != 0) {
        terminal_writestring("\n");
    }
    
    // Normal rengi geri yükle
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    // Dizini kapat
    fs_closedir(dir);
    
    return 0;
}

// Çalışma dizinini değiştir - cd komutu
int cmd_cd(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Hata: Dizin belirtilmedi\n");
        return -1;
    }
    
    if (fs_chdir(argv[1]) != 0) {
        terminal_writestring("Hata: Dizin değiştirilemedi: ");
        terminal_writestring(argv[1]);
        terminal_writestring("\n");
        return -1;
    }
    
    return 0;
}

// Çalışma dizinini göster - pwd komutu
int cmd_pwd(int argc, char** argv) {
    char path[256];
    
    if (fs_getcwd(path, sizeof(path)) != path) {
        terminal_writestring("Hata: Çalışma dizini alınamadı\n");
        return -1;
    }
    
    terminal_writestring(path);
    terminal_writestring("\n");
    
    return 0;
}

// Yeni dizin oluştur - mkdir komutu
int cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Hata: Dizin adı belirtilmedi\n");
        return -1;
    }
    
    if (fs_mkdir(argv[1]) != 0) {
        terminal_writestring("Hata: Dizin oluşturulamadı: ");
        terminal_writestring(argv[1]);
        terminal_writestring("\n");
        return -1;
    }
    
    return 0;
}

// Dizin sil - rmdir komutu
int cmd_rmdir(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Hata: Dizin adı belirtilmedi\n");
        return -1;
    }
    
    if (fs_rmdir(argv[1]) != 0) {
        terminal_writestring("Hata: Dizin silinemedi: ");
        terminal_writestring(argv[1]);
        terminal_writestring("\n");
        return -1;
    }
    
    return 0;
}

// Dosya sil - rm komutu
int cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Hata: Dosya adı belirtilmedi\n");
        return -1;
    }
    
    if (fs_unlink(argv[1]) != 0) {
        terminal_writestring("Hata: Dosya silinemedi: ");
        terminal_writestring(argv[1]);
        terminal_writestring("\n");
        return -1;
    }
    
    return 0;
}

// Dosya kopyala - cp komutu
int cmd_cp(int argc, char** argv) {
    if (argc < 3) {
        terminal_writestring("Hata: Eksik argüman\n");
        terminal_writestring("Kullanım: cp <kaynak> <hedef>\n");
        return -1;
    }
    
    // Kaynak dosyayı aç
    fs_file_t* src = fs_open(argv[1], "r");
    if (!src) {
        terminal_writestring("Hata: Kaynak dosya açılamadı: ");
        terminal_writestring(argv[1]);
        terminal_writestring("\n");
        return -1;
    }
    
    // Hedef dosyayı aç
    fs_file_t* dst = fs_open(argv[2], "w");
    if (!dst) {
        fs_close(src);
        terminal_writestring("Hata: Hedef dosya açılamadı: ");
        terminal_writestring(argv[2]);
        terminal_writestring("\n");
        return -1;
    }
    
    // Dosyayı kopyala
    char buffer[512];
    size_t nread;
    
    while ((nread = fs_read(src, buffer, sizeof(buffer))) > 0) {
        fs_write(dst, buffer, nread);
    }
    
    // Dosyaları kapat
    fs_close(src);
    fs_close(dst);
    
    return 0;
}

// Dosya taşı/yeniden adlandır - mv komutu
int cmd_mv(int argc, char** argv) {
    if (argc < 3) {
        terminal_writestring("Hata: Eksik argüman\n");
        terminal_writestring("Kullanım: mv <kaynak> <hedef>\n");
        return -1;
    }
    
    if (fs_rename(argv[1], argv[2]) != 0) {
        terminal_writestring("Hata: Dosya taşınamadı: ");
        terminal_writestring(argv[1]);
        terminal_writestring(" -> ");
        terminal_writestring(argv[2]);
        terminal_writestring("\n");
        return -1;
    }
    
    return 0;
}

// Dosya içeriğini göster - cat komutu
int cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Hata: Dosya adı belirtilmedi\n");
        return -1;
    }
    
    // Dosyayı aç
    fs_file_t* file = fs_open(argv[1], "r");
    if (!file) {
        terminal_writestring("Hata: Dosya açılamadı: ");
        terminal_writestring(argv[1]);
        terminal_writestring("\n");
        return -1;
    }
    
    // Dosyayı oku ve göster
    char buffer[512];
    size_t nread;
    
    while ((nread = fs_read(file, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[nread] = '\0';
        terminal_writestring(buffer);
    }
    
    terminal_writestring("\n");
    
    // Dosyayı kapat
    fs_close(file);
    
    return 0;
}

// Metin yazdır - echo komutu
int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        terminal_writestring(argv[i]);
        if (i < argc - 1) {
            terminal_writestring(" ");
        }
    }
    
    terminal_writestring("\n");
    
    return 0;
}

// Süreçleri listele - ps komutu
int cmd_ps(int argc, char** argv) {
    terminal_writestring("PID\tDURUM\tCPU\tAD\n");
    terminal_writestring("-----------------------------------\n");
    
    // Tüm süreçleri gez (basitleştirilmiş)
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = get_process(i);
        if (proc && proc->pid != 0) {
            // PID
            char pid_str[10];
            int_to_string(proc->pid, pid_str);
            terminal_writestring(pid_str);
            terminal_writestring("\t");
            
            // Durum
            const char* state_str = "UNKNOWN";
            switch (proc->state) {
                case PROCESS_STATE_READY:
                    state_str = "READY";
                    break;
                case PROCESS_STATE_RUNNING:
                    state_str = "RUNNING";
                    break;
                case PROCESS_STATE_BLOCKED:
                    state_str = "BLOCKED";
                    break;
                case PROCESS_STATE_SLEEPING:
                    state_str = "SLEEPING";
                    break;
                case PROCESS_STATE_ZOMBIE:
                    state_str = "ZOMBIE";
                    break;
                case PROCESS_STATE_STOPPED:
                    state_str = "STOPPED";
                    break;
            }
            
            terminal_writestring(state_str);
            terminal_writestring("\t");
            
            // CPU süresi
            char cpu_str[10];
            int_to_string(proc->cpu_time, cpu_str);
            terminal_writestring(cpu_str);
            terminal_writestring("\t");
            
            // Süreç adı
            terminal_writestring(proc->name);
            terminal_writestring("\n");
        }
    }
    
    return 0;
}

// Sinyal gönder - kill komutu
int cmd_kill(int argc, char** argv) {
    int signum = SIGTERM; // Varsayılan sinyal
    int pid_arg_index = 1;
    
    // -s seçeneğini kontrol et
    if (argc >= 3 && strcmp(argv[1], "-s") == 0) {
        // Sinyal adını sayıya çevir
        if (strcmp(argv[2], "SIGTERM") == 0 || strcmp(argv[2], "15") == 0) {
            signum = SIGTERM;
        } else if (strcmp(argv[2], "SIGKILL") == 0 || strcmp(argv[2], "9") == 0) {
            signum = SIGKILL;
        } else if (strcmp(argv[2], "SIGINT") == 0 || strcmp(argv[2], "2") == 0) {
            signum = SIGINT;
        } else if (strcmp(argv[2], "SIGHUP") == 0 || strcmp(argv[2], "1") == 0) {
            signum = SIGHUP;
        } else if (strcmp(argv[2], "SIGCONT") == 0 || strcmp(argv[2], "18") == 0) {
            signum = SIGCONT;
        } else if (strcmp(argv[2], "SIGSTOP") == 0 || strcmp(argv[2], "19") == 0) {
            signum = SIGSTOP;
        } else {
            // Sayı olarak çevirmeyi dene
            signum = atoi(argv[2]);
            if (signum <= 0 || signum >= MAX_SIGNALS) {
                terminal_writestring("Hata: Geçersiz sinyal numarası\n");
                return -1;
            }
        }
        
        pid_arg_index = 3;
    }
    
    if (pid_arg_index >= argc) {
        terminal_writestring("Hata: PID belirtilmedi\n");
        return -1;
    }
    
    // PID'yi çevir
    int pid = atoi(argv[pid_arg_index]);
    
    // Sinyali gönder
    if (sys_kill(pid, signum) != 0) {
        terminal_writestring("Hata: Sinyal gönderilemedi\n");
        return -1;
    }
    
    return 0;
}

// Bekle - sleep komutu
int cmd_sleep(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Hata: Süre belirtilmedi\n");
        return -1;
    }
    
    int seconds = atoi(argv[1]);
    if (seconds <= 0) {
        terminal_writestring("Hata: Geçersiz süre\n");
        return -1;
    }
    
    // Mili-saniye cinsinden bekle
    sys_sleep(seconds * 1000);
    
    return 0;
}

// Dosya oluştur - touch komutu
int cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Hata: Dosya adı belirtilmedi\n");
        return -1;
    }
    
    // Dosya varsa aç ve kapat, yoksa oluştur
    fs_file_t* file = fs_open(argv[1], "a");
    if (!file) {
        terminal_writestring("Hata: Dosya oluşturulamadı: ");
        terminal_writestring(argv[1]);
        terminal_writestring("\n");
        return -1;
    }
    
    fs_close(file);
    
    return 0;
}

// Ekranı temizle - clear komutu
int cmd_clear(int argc, char** argv) {
    terminal_initialize();
    return 0;
}

// Yardım göster - help komutu
int cmd_help(int argc, char** argv) {
    if (argc > 1) {
        // Belirli bir komut için yardım göster
        for (int i = 0; i < num_commands; i++) {
            if (strcmp(argv[1], commands[i].name) == 0) {
                print_command_help(&commands[i]);
                return 0;
            }
        }
        
        terminal_writestring("Hata: Bilinmeyen komut: ");
        terminal_writestring(argv[1]);
        terminal_writestring("\n");
        return -1;
    }
    
    // Tüm komutları listele
    terminal_writestring("Kullanılabilir komutlar:\n");
    terminal_writestring("---------------------\n");
    
    for (int i = 0; i < num_commands; i++) {
        terminal_writestring(commands[i].name);
        
        // Biçimlendirme için boşluk ekle
        for (int j = strlen(commands[i].name); j < 10; j++) {
            terminal_writestring(" ");
        }
        
        terminal_writestring(" - ");
        terminal_writestring(commands[i].description);
        terminal_writestring("\n");
    }
    
    terminal_writestring("\nDaha fazla bilgi için: help <komut>\n");
    
    return 0;
}

// Kabuktan çık - exit komutu
int cmd_exit(int argc, char** argv) {
    int status = 0;
    
    if (argc > 1) {
        status = atoi(argv[1]);
    }
    
    // Şu anki süreci sonlandır
    uint64_t pid = sys_getpid();
    exit_process(pid, status);
    
    return 0; // Buraya asla ulaşılmaz
}

// Yardım bilgisini yazdır
void print_command_help(const command_t* cmd) {
    terminal_writestring("Komut: ");
    terminal_writestring(cmd->name);
    terminal_writestring("\n\n");
    
    terminal_writestring("Açıklama: ");
    terminal_writestring(cmd->description);
    terminal_writestring("\n\n");
    
    terminal_writestring("Kullanım: ");
    terminal_writestring(cmd->usage);
    terminal_writestring("\n");
}

// String to integer dönüşümü (atoi)
int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    int i = 0;
    
    // Boşlukları atla
    while (str[i] == ' ' || str[i] == '\t') {
        i++;
    }
    
    // İşaret kontrolü
    if (str[i] == '-') {
        sign = -1;
        i++;
    } else if (str[i] == '+') {
        i++;
    }
    
    // Sayıyı çevir
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }
    
    return sign * result;
}

// İki string karşılaştırma (strcmp)
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
} 