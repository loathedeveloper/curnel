#ifndef COREUTILS_H
#define COREUTILS_H

#include <stdint.h>
#include <stddef.h>

// Komut prototipi tanımlaması
typedef int (*command_func_t)(int argc, char** argv);

// Komut yapısı
typedef struct {
    const char* name;         // Komut adı
    command_func_t func;      // Komut işlevi
    const char* description;  // Komut açıklaması
    const char* usage;        // Kullanım bilgisi
} command_t;

// Dahili komut listesi
extern command_t commands[];
extern int num_commands;

// Komut yürütme
int execute_command(const char* command_line);

// Temel Linux komutları
int cmd_ls(int argc, char** argv);
int cmd_cd(int argc, char** argv);
int cmd_pwd(int argc, char** argv);
int cmd_mkdir(int argc, char** argv);
int cmd_rmdir(int argc, char** argv);
int cmd_rm(int argc, char** argv);
int cmd_cp(int argc, char** argv);
int cmd_mv(int argc, char** argv);
int cmd_cat(int argc, char** argv);
int cmd_echo(int argc, char** argv);
int cmd_ps(int argc, char** argv);
int cmd_kill(int argc, char** argv);
int cmd_sleep(int argc, char** argv);
int cmd_touch(int argc, char** argv);
int cmd_clear(int argc, char** argv);
int cmd_help(int argc, char** argv);
int cmd_exit(int argc, char** argv);

// Komut satırı ayrıştırma yardımcı işlevleri
int parse_command_line(const char* command_line, char** argv, int max_args);
void print_command_help(const command_t* cmd);

#endif // COREUTILS_H 