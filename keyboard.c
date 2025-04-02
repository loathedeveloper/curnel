#include "kernel.h"
#include "keyboard.h"
#include "idt.h"

// Klavye durumu
static keyboard_state_t keyboard_state;

// US Klavye düzeni - küçük harfler
const char kbd_us_lowercase[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// US Klavye düzeni - büyük harfler
const char kbd_us_uppercase[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Port I/O işlevleri
uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Tampon işlevleri
static int keyboard_buffer_empty() {
    return keyboard_state.buffer_start == keyboard_state.buffer_end;
}

static int keyboard_buffer_full() {
    return ((keyboard_state.buffer_end + 1) & 0xFF) == keyboard_state.buffer_start;
}

static void keyboard_buffer_put(char c) {
    if (keyboard_buffer_full()) {
        return; // Tampon dolu
    }
    
    keyboard_state.buffer[keyboard_state.buffer_end] = c;
    keyboard_state.buffer_end = (keyboard_state.buffer_end + 1) & 0xFF;
}

static char keyboard_buffer_get() {
    if (keyboard_buffer_empty()) {
        return 0; // Tampon boş
    }
    
    char c = keyboard_state.buffer[keyboard_state.buffer_start];
    keyboard_state.buffer_start = (keyboard_state.buffer_start + 1) & 0xFF;
    return c;
}

// Klavye LED durumunu güncelle
void keyboard_update_leds() {
    uint8_t led_status = 0;
    
    // LED durumlarını ayarla
    if (keyboard_state.scroll_lock) led_status |= 1;
    if (keyboard_state.num_lock) led_status |= 2;
    if (keyboard_state.caps_lock) led_status |= 4;
    
    // Komut gönder
    outb(KEYBOARD_COMMAND_PORT, KEYBOARD_CMD_SET_LEDS);
    outb(KEYBOARD_DATA_PORT, led_status);
}

// Klavye başlatma
void keyboard_init() {
    // Durum bilgilerini temizle
    memset(&keyboard_state, 0, sizeof(keyboard_state_t));
    
    // Klavyeyi etkinleştir
    outb(KEYBOARD_COMMAND_PORT, KEYBOARD_CMD_ENABLE);
    
    // Kesmeyi kaydettir (IRQ1 -> kesme 33)
    register_interrupt_handler(IRQ1, (isr_t)keyboard_handler);
    
    terminal_writestring("Klavye surucusu baslatildi.\n");
}

// Klavye kesme işleyicisi
void keyboard_handler(uint64_t int_no) {
    // Klavyeden veri oku
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // Extended tuş kodu (0xE0)
    if (scancode == 0xE0) {
        keyboard_state.mode = KEYMODE_EXTENDED;
        return;
    }
    
    // Tuş bırakma mı?
    if (scancode & KEY_RELEASE) {
        scancode &= ~KEY_RELEASE; // Tuş kodunu al
        
        // Shift, Ctrl, Alt tuşları için durum güncelle
        switch (scancode) {
            case KEY_LSHIFT:
            case KEY_RSHIFT:
                keyboard_state.shift_pressed = 0;
                break;
                
            case KEY_LCTRL:
                keyboard_state.ctrl_pressed = 0;
                break;
                
            case KEY_LALT:
                keyboard_state.alt_pressed = 0;
                break;
        }
        
        // Extended tuş modunu sıfırla
        keyboard_state.mode = KEYMODE_NORMAL;
        return;
    }
    
    // Tuş basma olayını işle
    switch (scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            keyboard_state.shift_pressed = 1;
            break;
            
        case KEY_LCTRL:
            keyboard_state.ctrl_pressed = 1;
            break;
            
        case KEY_LALT:
            keyboard_state.alt_pressed = 1;
            break;
            
        case KEY_CAPSLOCK:
            keyboard_state.caps_lock = !keyboard_state.caps_lock;
            keyboard_update_leds();
            break;
            
        case KEY_NUMLOCK:
            keyboard_state.num_lock = !keyboard_state.num_lock;
            keyboard_update_leds();
            break;
            
        case KEY_SCROLLLOCK:
            keyboard_state.scroll_lock = !keyboard_state.scroll_lock;
            keyboard_update_leds();
            break;
            
        default:
            // ASCII karaktere dönüştür
            if (scancode < 128) {
                char c;
                
                // Shift veya Caps Lock aktif mi kontrol et
                if (keyboard_state.shift_pressed ^ keyboard_state.caps_lock) {
                    c = kbd_us_uppercase[scancode];
                } else {
                    c = kbd_us_lowercase[scancode];
                }
                
                // Geçerli bir karakter mi?
                if (c) {
                    // Ctrl basılıysa, kontrol karakteri (ASCII 1-26)
                    if (keyboard_state.ctrl_pressed && c >= 'a' && c <= 'z') {
                        c = c - 'a' + 1;
                    }
                    
                    // Tampona ekle
                    keyboard_buffer_put(c);
                }
            }
            break;
    }
    
    // Extended tuş modunu sıfırla
    keyboard_state.mode = KEYMODE_NORMAL;
}

// Klavyeden karakter oku (engelleyici)
char keyboard_getchar() {
    char c;
    
    // Tampon boşsa bekle
    while (keyboard_buffer_empty()) {
        // Kesmeleri etkinleştir ve bekle
        asm volatile("sti; hlt");
    }
    
    c = keyboard_buffer_get();
    return c;
}

// Klavyeden karakter oku (engelleyici olmayan)
int keyboard_getchar_nonblock() {
    if (keyboard_buffer_empty()) {
        return -1; // Tampon boş
    }
    
    return keyboard_buffer_get();
}

// Klavyeden birden çok karakter oku
int keyboard_read(char* buffer, size_t count) {
    size_t i;
    
    for (i = 0; i < count; i++) {
        char c = keyboard_getchar();
        
        // Enter tuşu ile sonlandır
        if (c == '\n') {
            buffer[i] = c;
            i++;
            break;
        }
        
        // Backspace işleme
        if (c == '\b') {
            if (i > 0) {
                i--;
                // Ekrandan da karakteri sil
                terminal_putchar('\b');
                terminal_putchar(' ');
                terminal_putchar('\b');
                i--; // i'yi azalt, for döngüsü tekrar artıracak
            } else {
                i--; // i'yi azalt, for döngüsü tekrar artıracak
            }
            continue;
        }
        
        // Normal karakter
        buffer[i] = c;
        
        // Ekranda göster
        terminal_putchar(c);
    }
    
    return i;
} 