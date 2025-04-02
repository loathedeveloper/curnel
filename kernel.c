#include "kernel.h"
#include "idt.h"
#include "process.h"
#include "syscall.h"
#include "timer.h"
#include "keyboard.h"
#include "filesystem.h"
#include "paging.h"
#include "usermode.h"
#include "pipe.h"
#include "signals.h"
#include "shell.h"
#include "coreutils.h"

// VGA terminali için sabitler
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// Renkler
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

// Terminal durumu
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

// Renk oluşturma yardımcısı
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

// VGA giriş değeri oluşturma yardımcısı
static inline uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t) c | (uint16_t) color << 8;
}

// String uzunluğu yardımcı fonksiyonu
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

// Terminal başlatma fonksiyonu
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*) VGA_MEMORY;
    
    // Ekranı temizle
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

// Terminale yazı yazma fonksiyonları
void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
    // Yeni satır kontrolü
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = 0;
        }
        return;
    }

    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = 0;
        }
    }
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        terminal_putchar(data[i]);
    }
}

void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

// Tamsayıyı string'e çevirme
void int_to_string(int num, char* str) {
    int i = 0;
    int is_negative = 0;
    
    // Negatif sayı kontrolü
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    // Sayı 0 ise
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    // Sayıyı rakamlarına göre dönüştür
    while (num != 0) {
        int rem = num % 10;
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
        num = num / 10;
    }
    
    // Negatif ise '-' işareti ekle
    if (is_negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    
    // Diziyi tersine çevir
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Test pipe fonksiyonu
void test_pipe() {
    terminal_writestring("Pipe testi baslatiliyor...\n");
    
    // Pipe oluştur
    uint64_t fds[2];
    if (sys_pipe(fds) != 0) {
        terminal_writestring("Pipe olusturulamadi!\n");
        return;
    }
    
    terminal_writestring("Pipe olusturuldu.\n");
    
    // Fork ile çocuk süreç oluştur
    uint64_t child_pid = sys_fork();
    
    if (child_pid == 0) {
        // Çocuk süreç
        char buf[64];
        
        // Yazma ucunu kapat
        sys_close(fds[1]);
        
        // Pipe'tan oku
        int bytes_read = sys_read(fds[0], buf, 63);
        if (bytes_read > 0) {
            buf[bytes_read] = '\0';
            terminal_writestring("Cocuk sureci okudu: ");
            terminal_writestring(buf);
            terminal_writestring("\n");
        }
        
        // Okuma ucunu kapat
        sys_close(fds[0]);
        
        // Çocuk süreci sonlandır
        sys_exit(0);
    } else if (child_pid > 0) {
        // Ebeveyn süreç
        
        // Okuma ucunu kapat
        sys_close(fds[0]);
        
        // Pipe'a yaz
        const char* message = "Merhaba pipe!";
        sys_write(fds[1], message, strlen(message));
        
        // Yazma ucunu kapat
        sys_close(fds[1]);
        
        // Çocuk sürecin tamamlanmasını bekle
        uint64_t status;
        sys_wait(&status);
        
        terminal_writestring("Pipe testi tamamlandi.\n");
    } else {
        terminal_writestring("Fork hatasi!\n");
    }
}

// Test sinyal işleyicisi
void test_signal_handler(int signum) {
    terminal_writestring("Sinyal alindi: ");
    char sig_str[5];
    int_to_string(signum, sig_str);
    terminal_writestring(sig_str);
    terminal_writestring("\n");
}

// Test sinyal fonksiyonu
void test_signals() {
    terminal_writestring("Sinyal testi baslatiliyor...\n");
    
    // Fork ile çocuk süreç oluştur
    uint64_t child_pid = sys_fork();
    
    if (child_pid == 0) {
        // Çocuk süreç
        // SIGTERM için işleyici kaydet
        sys_signal(SIGTERM, (uint64_t)test_signal_handler);
        
        terminal_writestring("Cocuk surec: SIGTERM icin isleyici kaydedildi\n");
        terminal_writestring("Cocuk surec: 3 saniye bekliyor...\n");
        
        // 3 saniye bekle
        sys_sleep(3000);
        
        terminal_writestring("Cocuk surec: Hala calisiyorum!\n");
        
        // 2 saniye daha bekle
        sys_sleep(2000);
        
        // Çocuk süreci sonlandır
        sys_exit(0);
    } else if (child_pid > 0) {
        // Ebeveyn süreç
        terminal_writestring("Ebeveyn surec: Cocuk surece 2 saniye sonra SIGTERM gonderecek\n");
        
        // 2 saniye bekle
        sys_sleep(2000);
        
        // Çocuk sürece SIGTERM gönder
        terminal_writestring("Ebeveyn surec: Cocuk surece SIGTERM gonderiliyor...\n");
        sys_kill(child_pid, SIGTERM);
        
        // Çocuk sürecin tamamlanmasını bekle
        uint64_t status;
        sys_wait(&status);
        
        terminal_writestring("Sinyal testi tamamlandi.\n");
    } else {
        terminal_writestring("Fork hatasi!\n");
    }
}

// Test süreç fonksiyonu
void test_process() {
    terminal_writestring("Test sureci calistirildi!\n");
    
    // Pipe testini çalıştır
    test_pipe();
    
    // Sinyal testini çalıştır
    test_signals();
    
    // Sistem çağrısını doğrudan çağır (normalde kullanıcı modunda syscall komutlarıyla yapılır)
    sys_write(1, "Bu bir sistem cagrisi ile yazildi!\n", 35);
    
    // İşimiz bittiğinde süreci çıkar
    sys_exit(0);
}

// Test süreç grubu oluşturma ve sinyal gönderme
void test_process_groups() {
    terminal_writestring("Surec grubu testi baslatiliyor...\n");
    
    // Ana sürecin PID'sini al
    uint64_t parent_pid = sys_getpid();
    
    // Birinci çocuk süreci oluştur
    uint64_t child1_pid = sys_fork();
    
    if (child1_pid == 0) {
        // Birinci çocuk süreç
        
        // Yeni süreç grubu oluştur
        sys_setpgid(0, 0);
        uint64_t pgid = sys_getpgid(0);
        
        terminal_writestring("Cocuk 1: Yeni surec grubu olusturuldu: ");
        char pgid_str[5];
        int_to_string(pgid, pgid_str);
        terminal_writestring(pgid_str);
        terminal_writestring("\n");
        
        // SIGTERM için işleyici kaydet
        sys_signal(SIGTERM, (uint64_t)test_signal_handler);
        
        // İkinci çocuk süreci oluştur
        uint64_t child2_pid = sys_fork();
        
        if (child2_pid == 0) {
            // İkinci çocuk (birinci çocuğun alt süreci)
            // SIGTERM için işleyici kaydet
            sys_signal(SIGTERM, (uint64_t)test_signal_handler);
            
            terminal_writestring("Cocuk 2: SIGTERM icin isleyici kaydedildi\n");
            
            // 5 saniye bekle
            sys_sleep(5000);
            
            terminal_writestring("Cocuk 2: Cikis yapiliyor\n");
            sys_exit(0);
        } else {
            // 10 saniye bekle
            sys_sleep(10000);
            
            terminal_writestring("Cocuk 1: Cikis yapiliyor\n");
            sys_exit(0);
        }
    } else {
        // Ana süreç
        // 2 saniye bekle
        sys_sleep(2000);
        
        // Çocuk 1'in süreç grubunu al
        uint64_t child1_pgid = sys_getpgid(child1_pid);
        
        terminal_writestring("Ana surec: Tum surec grubuna sinyal gonderiyor: ");
        char pgid_str[5];
        int_to_string(-child1_pgid, pgid_str); // Negatif PID = tüm süreç grubuna sinyal
        terminal_writestring(pgid_str);
        terminal_writestring("\n");
        
        // Çocuk 1'in süreç grubuna sinyal gönder (negatif PID ile)
        sys_kill(-child1_pgid, SIGTERM);
        
        // Alt süreçlerin sonlanmasını bekle
        uint64_t status;
        sys_wait(&status);
        
        terminal_writestring("Surec grubu testi tamamlandi.\n");
    }
}

// Shell süreci
void shell_process() {
    // Shell yapısını başlat
    shell_t shell;
    shell_init(&shell);
    
    // Shell'i çalıştır (ana döngü)
    shell_run(&shell);
    
    // Shell temizlenme
    shell_cleanup(&shell);
    
    // Süreç çıkışı
    sys_exit(shell.exit_code);
}

// Kernel ana işlevi
void kernel_main(void) {
    // Terminali başlat
    terminal_initialize();

    // Kernel başlangıç mesajları
    terminal_writestring("64-bit Basit Linux-Benzeri Kernel\n");
    terminal_writestring("-------------------------------\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("Kernel basariyla 64-bit Long Mode'a gecti!\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\nKernel modullerini baslatiyorum...\n");
    
    // Bellek yönetimini başlat
    memory_init();
    terminal_writestring("Bellek yonetimi baslatildi.\n");
    
    // IDT'yi başlat
    init_idt();
    terminal_writestring("Kesme tanimlama tablosu (IDT) baslatildi.\n");
    
    // Sayfalama sistemini başlat
    // TODO: memory_map_entry_t yapısını doğru şekilde doldur
    memory_map_entry_t mem_map[1] = {{0x100000, 128 * 1024 * 1024, 1}}; // 128 MB örnek bellek
    paging_init(mem_map, 1);
    
    // GDT ve TSS'yi başlat
    gdt_init();
    void* kernel_stack = kmalloc_page();
    tss_init(kernel_stack);
    
    // Zamanlayıcıyı başlat
    timer_init(100); // 100 Hz (10 ms)
    
    // Klavyeyi başlat
    keyboard_init();
    
    // Süreç yönetimini başlat
    init_processes();
    
    // Dosya sistemini başlat
    fs_init();
    
    // Pipe sistemini ve sistem çağrılarını başlat
    init_syscalls();
    
    // Shell sürecini oluştur
    uint64_t shell_pid = create_process("shell", (uint64_t)shell_process, 0);
    if (shell_pid != 0) {
        terminal_writestring("Shell sureci olusturuldu. PID: ");
        char pid_str[5];
        int_to_string(shell_pid, pid_str);
        terminal_writestring(pid_str);
        terminal_writestring("\n");
    }
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("\nTum moduller basariyla yuklendi!\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    // Kernel ana döngüsü
    while (1) {
        // Süreç zamanlayıcısını çağır
        schedule();
        
        // CPU'yu halt et (enerji tasarrufu)
        asm volatile("hlt");
    }
} 