#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include <stddef.h>
#include "coreutils.h"

#define SHELL_BUFFER_SIZE 256
#define SHELL_HISTORY_SIZE 10

// Kabuk durumları
typedef enum {
    SHELL_STATE_READY,
    SHELL_STATE_RUNNING,
    SHELL_STATE_EXIT
} shell_state_t;

// Kabuk yapısı
typedef struct {
    char buffer[SHELL_BUFFER_SIZE];     // Giriş tamponu
    size_t buffer_pos;                  // Tampon pozisyonu
    char* history[SHELL_HISTORY_SIZE];  // Komut geçmişi
    int history_count;                  // Geçmiş komut sayısı
    int history_index;                  // Geçmiş indeksi
    char cwd[256];                      // Çalışma dizini
    shell_state_t state;                // Kabuk durumu
    uint32_t exit_code;                 // Çıkış kodu
} shell_t;

// Kabuk işlevleri
void shell_init(shell_t* shell);
void shell_run(shell_t* shell);
void shell_process_input(shell_t* shell, char c);
void shell_execute_command(shell_t* shell);
void shell_clear_buffer(shell_t* shell);
void shell_add_to_history(shell_t* shell, const char* command);
void shell_handle_up_arrow(shell_t* shell);
void shell_handle_down_arrow(shell_t* shell);
void shell_handle_left_arrow(shell_t* shell);
void shell_handle_right_arrow(shell_t* shell);
void shell_handle_backspace(shell_t* shell);
void shell_print_prompt(shell_t* shell);
void shell_cleanup(shell_t* shell);

#endif // SHELL_H 