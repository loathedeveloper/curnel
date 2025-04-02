#include "kernel.h"
#include "shell.h"
#include "keyboard.h"
#include "coreutils.h"
#include "filesystem.h"

// Kabuk başlatma
void shell_init(shell_t* shell) {
    // Yapıyı sıfırla
    memset(shell, 0, sizeof(shell_t));
    
    // Çalışma dizinini ayarla
    fs_getcwd(shell->cwd, sizeof(shell->cwd));
    
    // Durumu ayarla
    shell->state = SHELL_STATE_READY;
    
    // Ekranı temizle
    terminal_initialize();
    
    // Hoş geldin mesajı
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("SimpleOS Shell v0.1\n");
    terminal_writestring("Yardim icin 'help' yazin\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    // Prompt göster
    shell_print_prompt(shell);
}

// Kabuk döngüsü
void shell_run(shell_t* shell) {
    shell->state = SHELL_STATE_RUNNING;
    
    while (shell->state == SHELL_STATE_RUNNING) {
        // Klavyeden bir karakter al (bloke olmayan)
        uint8_t scancode = keyboard_read_scancode();
        
        if (scancode != 0) {
            keyboard_event_t event;
            
            // Scancode'u keyboard event'e dönüştür
            if (keyboard_handle_scancode(scancode, &event)) {
                // Sadece bastırma olaylarını işle (bırakma değil)
                if (event.pressed) {
                    if (event.key == KEY_ENTER) {
                        // Enter tuşu - komutu çalıştır
                        terminal_writestring("\n");
                        shell_execute_command(shell);
                        shell_clear_buffer(shell);
                        
                        // Yeni bir prompt göster (eğer shell'den çıkılmadıysa)
                        if (shell->state == SHELL_STATE_RUNNING) {
                            shell_print_prompt(shell);
                        }
                    } else if (event.key == KEY_BACKSPACE) {
                        // Backspace tuşu
                        shell_handle_backspace(shell);
                    } else if (event.key == KEY_ARROW_UP) {
                        // Yukarı ok tuşu - geçmiş komutlarda geriye git
                        shell_handle_up_arrow(shell);
                    } else if (event.key == KEY_ARROW_DOWN) {
                        // Aşağı ok tuşu - geçmiş komutlarda ileriye git
                        shell_handle_down_arrow(shell);
                    } else if (event.key == KEY_ARROW_LEFT) {
                        // Sol ok tuşu - imleçi sola taşı
                        shell_handle_left_arrow(shell);
                    } else if (event.key == KEY_ARROW_RIGHT) {
                        // Sağ ok tuşu - imleçi sağa taşı
                        shell_handle_right_arrow(shell);
                    } else if (event.key == KEY_TAB) {
                        // Tab tuşu - gelecekte dosya/komut tamamlama için
                        // Şimdilik bir boşluk ekle
                        shell_process_input(shell, ' ');
                    } else if (event.key == KEY_ESCAPE) {
                        // Escape tuşu - mevcut girişi temizle
                        shell_clear_buffer(shell);
                        terminal_writestring("\r");
                        shell_print_prompt(shell);
                    } else if (event.key == KEY_CTRL_C) {
                        // CTRL+C - mevcut komutu iptal et
                        terminal_writestring("^C\n");
                        shell_clear_buffer(shell);
                        shell_print_prompt(shell);
                    } else if (event.key == KEY_CTRL_D) {
                        // CTRL+D - shell'den çık (eğer tampon boşsa)
                        if (shell->buffer_pos == 0) {
                            terminal_writestring("exit\n");
                            shell->state = SHELL_STATE_EXIT;
                        }
                    } else if (event.ascii != 0) {
                        // Yazdırılabilir karakterler
                        shell_process_input(shell, event.ascii);
                    }
                }
            }
        }
        
        // Küçük bir gecikme (CPU kullanımını azaltmak için)
        for (volatile int i = 0; i < 10000; i++);
    }
}

// Klavye girişini işle
void shell_process_input(shell_t* shell, char c) {
    // Tamponu taşırmadan karakteri ekle
    if (shell->buffer_pos < SHELL_BUFFER_SIZE - 1) {
        // Diğer karakterleri kaydır
        for (int i = shell->buffer_pos; i < strlen(shell->buffer); i++) {
            terminal_putchar(shell->buffer[i]);
        }
        
        // Karakteri ekle
        shell->buffer[shell->buffer_pos++] = c;
        
        // NULL ile sonlandır
        shell->buffer[shell->buffer_pos] = '\0';
        
        // Karakter göster
        terminal_putchar(c);
        
        // İmleci düzelt
        for (int i = shell->buffer_pos; i < strlen(shell->buffer); i++) {
            terminal_writestring("\b");
        }
    }
}

// Komutu çalıştır
void shell_execute_command(shell_t* shell) {
    // Tamponu NULL ile sonlandır
    shell->buffer[shell->buffer_pos] = '\0';
    
    // Boş komut kontrolü
    if (shell->buffer_pos == 0) {
        return;
    }
    
    // Geçmişe ekle
    shell_add_to_history(shell, shell->buffer);
    
    // Komutu çalıştır
    int result = execute_command(shell->buffer);
    
    // Sonucu kontrol et
    if (result == -1) {
        terminal_writestring("Komut hatasi: ");
        terminal_writestring(shell->buffer);
        terminal_writestring("\n");
    }
    
    // Exit komutu için özel kontrol
    if (strcmp(shell->buffer, "exit") == 0) {
        shell->state = SHELL_STATE_EXIT;
    }
}

// Tamponu temizle
void shell_clear_buffer(shell_t* shell) {
    memset(shell->buffer, 0, SHELL_BUFFER_SIZE);
    shell->buffer_pos = 0;
    shell->history_index = -1;
}

// Komutu geçmişe ekle
void shell_add_to_history(shell_t* shell, const char* command) {
    // Boş komutları ekleme
    if (strlen(command) == 0) {
        return;
    }
    
    // Aynı komutu peş peşe ekleme
    if (shell->history_count > 0 && strcmp(shell->history[shell->history_count - 1], command) == 0) {
        return;
    }
    
    // Geçmiş dolu ise, en eskiyi sil
    if (shell->history_count == SHELL_HISTORY_SIZE) {
        free(shell->history[0]);
        
        // Diğer öğeleri kaydır
        for (int i = 0; i < shell->history_count - 1; i++) {
            shell->history[i] = shell->history[i + 1];
        }
        
        shell->history_count--;
    }
    
    // Yeni komutu ekle
    shell->history[shell->history_count] = strdup(command);
    shell->history_count++;
}

// Geçmişte yukarı git
void shell_handle_up_arrow(shell_t* shell) {
    if (shell->history_count == 0) {
        return;
    }
    
    // Geçmiş indeksini güncelle
    if (shell->history_index == -1) {
        shell->history_index = shell->history_count - 1;
    } else if (shell->history_index > 0) {
        shell->history_index--;
    } else {
        return;
    }
    
    // Mevcut satırı temizle
    terminal_writestring("\r");
    for (int i = 0; i < shell->buffer_pos + strlen(shell->cwd) + 4; i++) {
        terminal_writestring(" ");
    }
    
    // Prompt'u yeniden göster
    terminal_writestring("\r");
    shell_print_prompt(shell);
    
    // Geçmiş komutu yükle
    strcpy(shell->buffer, shell->history[shell->history_index]);
    shell->buffer_pos = strlen(shell->buffer);
    
    // Komutu göster
    terminal_writestring(shell->buffer);
}

// Geçmişte aşağı git
void shell_handle_down_arrow(shell_t* shell) {
    if (shell->history_count == 0 || shell->history_index == -1) {
        return;
    }
    
    // Geçmiş indeksini güncelle
    if (shell->history_index < shell->history_count - 1) {
        shell->history_index++;
    } else {
        shell->history_index = -1;
        
        // Mevcut satırı temizle
        terminal_writestring("\r");
        for (int i = 0; i < shell->buffer_pos + strlen(shell->cwd) + 4; i++) {
            terminal_writestring(" ");
        }
        
        // Prompt'u yeniden göster
        terminal_writestring("\r");
        shell_print_prompt(shell);
        
        // Tamponu temizle
        shell_clear_buffer(shell);
        return;
    }
    
    // Mevcut satırı temizle
    terminal_writestring("\r");
    for (int i = 0; i < shell->buffer_pos + strlen(shell->cwd) + 4; i++) {
        terminal_writestring(" ");
    }
    
    // Prompt'u yeniden göster
    terminal_writestring("\r");
    shell_print_prompt(shell);
    
    // Geçmiş komutu yükle
    strcpy(shell->buffer, shell->history[shell->history_index]);
    shell->buffer_pos = strlen(shell->buffer);
    
    // Komutu göster
    terminal_writestring(shell->buffer);
}

// İmleci sola taşı
void shell_handle_left_arrow(shell_t* shell) {
    if (shell->buffer_pos > 0) {
        shell->buffer_pos--;
        terminal_writestring("\b");
    }
}

// İmleci sağa taşı
void shell_handle_right_arrow(shell_t* shell) {
    if (shell->buffer_pos < strlen(shell->buffer)) {
        terminal_putchar(shell->buffer[shell->buffer_pos]);
        shell->buffer_pos++;
    }
}

// Backspace tuşunu işle
void shell_handle_backspace(shell_t* shell) {
    if (shell->buffer_pos > 0) {
        // İmleci bir sola taşı
        terminal_writestring("\b");
        
        // Karakterleri sola kaydır
        for (int i = shell->buffer_pos - 1; i < strlen(shell->buffer) - 1; i++) {
            shell->buffer[i] = shell->buffer[i + 1];
            terminal_putchar(shell->buffer[i]);
        }
        
        // Son karakteri boşlukla değiştir
        terminal_writestring(" ");
        
        // İmleci yeniden pozisyonla
        for (int i = shell->buffer_pos; i < strlen(shell->buffer); i++) {
            terminal_writestring("\b");
        }
        terminal_writestring("\b");
        
        // Tamponu güncelle
        shell->buffer[strlen(shell->buffer) - 1] = '\0';
        shell->buffer_pos--;
    }
}

// Prompt yazdır
void shell_print_prompt(shell_t* shell) {
    // Çalışma dizinini güncelle
    fs_getcwd(shell->cwd, sizeof(shell->cwd));
    
    // Prompt rengini değiştir
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    
    // Dizini yazdır
    terminal_writestring(shell->cwd);
    
    // Prompt karakterini yazdır
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring(" $ ");
    
    // Normal rengi geri yükle
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

// String kopyalama
char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* new_str = (char*)kmalloc(len);
    
    if (new_str) {
        memcpy(new_str, s, len);
    }
    
    return new_str;
}

// String kopyalama
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}

// Kabuk temizleme
void shell_cleanup(shell_t* shell) {
    // Geçmiş komutları temizle
    for (int i = 0; i < shell->history_count; i++) {
        if (shell->history[i]) {
            kfree(shell->history[i]);
            shell->history[i] = NULL;
        }
    }
    
    // Tamponu temizle
    shell_clear_buffer(shell);
} 