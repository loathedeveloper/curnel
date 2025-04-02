#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// PS/2 Klavye Port numaraları
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_COMMAND_PORT 0x64

// Klavye durum bitleri
#define KEYBOARD_STATUS_OUTPUT_FULL 0x01

// Klavye komutları
#define KEYBOARD_CMD_ENABLE      0xF4
#define KEYBOARD_CMD_DISABLE     0xF5
#define KEYBOARD_CMD_RESET       0xFF
#define KEYBOARD_CMD_SET_RATE    0xF3
#define KEYBOARD_CMD_ECHO        0xEE
#define KEYBOARD_CMD_SET_LEDS    0xED

// SCANCODE_SET1 tuş kodları (yalnızca basit bir alt küme)
#define KEY_ESC         0x01
#define KEY_1           0x02
#define KEY_2           0x03
#define KEY_3           0x04
#define KEY_4           0x05
#define KEY_5           0x06
#define KEY_6           0x07
#define KEY_7           0x08
#define KEY_8           0x09
#define KEY_9           0x0A
#define KEY_0           0x0B
#define KEY_MINUS       0x0C
#define KEY_EQUALS      0x0D
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_Q           0x10
#define KEY_W           0x11
#define KEY_E           0x12
#define KEY_R           0x13
#define KEY_T           0x14
#define KEY_Y           0x15
#define KEY_U           0x16
#define KEY_I           0x17
#define KEY_O           0x18
#define KEY_P           0x19
#define KEY_LEFTBRACKET 0x1A
#define KEY_RIGHTBRACKET 0x1B
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_A           0x1E
#define KEY_S           0x1F
#define KEY_D           0x20
#define KEY_F           0x21
#define KEY_G           0x22
#define KEY_H           0x23
#define KEY_J           0x24
#define KEY_K           0x25
#define KEY_L           0x26
#define KEY_SEMICOLON   0x27
#define KEY_QUOTE       0x28
#define KEY_BACKTICK    0x29
#define KEY_LSHIFT      0x2A
#define KEY_BACKSLASH   0x2B
#define KEY_Z           0x2C
#define KEY_X           0x2D
#define KEY_C           0x2E
#define KEY_V           0x2F
#define KEY_B           0x30
#define KEY_N           0x31
#define KEY_M           0x32
#define KEY_COMMA       0x33
#define KEY_DOT         0x34
#define KEY_SLASH       0x35
#define KEY_RSHIFT      0x36
#define KEY_PRINTSCREEN 0x37
#define KEY_LALT        0x38
#define KEY_SPACE       0x39
#define KEY_CAPSLOCK    0x3A
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44
#define KEY_NUMLOCK     0x45
#define KEY_SCROLLLOCK  0x46
#define KEY_HOME        0x47
#define KEY_UP          0x48
#define KEY_PAGEUP      0x49
#define KEY_KEYPADMINUS 0x4A
#define KEY_LEFT        0x4B
#define KEY_KEYPAD5     0x4C
#define KEY_RIGHT       0x4D
#define KEY_KEYPADPLUS  0x4E
#define KEY_END         0x4F
#define KEY_DOWN        0x50
#define KEY_PAGEDOWN    0x51
#define KEY_INSERT      0x52
#define KEY_DELETE      0x53

// Release bit (Set 1)
#define KEY_RELEASE     0x80

// Klavye modları
#define KEYMODE_NORMAL  0
#define KEYMODE_EXTENDED 1
#define KEYMODE_RELEASE  2

// ASCII tabloları
extern const char kbd_us_lowercase[128];
extern const char kbd_us_uppercase[128];

// Klavye durumu
typedef struct {
    uint8_t shift_pressed;     // Shift tuşu basılı mı?
    uint8_t ctrl_pressed;      // Ctrl tuşu basılı mı?
    uint8_t alt_pressed;       // Alt tuşu basılı mı?
    uint8_t caps_lock;         // Caps Lock aktif mi?
    uint8_t num_lock;          // Num Lock aktif mi?
    uint8_t scroll_lock;       // Scroll Lock aktif mi?
    uint8_t mode;              // Klavye modu (normal, extended, release)
    
    // Klavye tamponu
    char buffer[256];          // Karakter tamponu
    uint8_t buffer_start;      // Tampon başlangıç indeksi
    uint8_t buffer_end;        // Tampon bitiş indeksi
} keyboard_state_t;

// Klavye işlevleri
void keyboard_init();
void keyboard_handler(uint64_t int_no);
char keyboard_getchar();
int keyboard_getchar_nonblock();
int keyboard_read(char* buffer, size_t count);
void keyboard_update_leds();

// Tamamlayıcı işlevler
uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t value);

#endif // KEYBOARD_H 