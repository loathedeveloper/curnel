; 64-bit kernel için bootloader
bits 32
section .multiboot
    dd 0x1BADB002            ; Multiboot sihirli sayısı
    dd 0x0                   ; Bayraklar
    dd - (0x1BADB002 + 0x0)  ; Kontrol toplamı

section .text
global start
extern kernel_main

start:
    cli                     ; Kesmeleri devre dışı bırak
    mov esp, stack_top      ; Stack pointer'ı ayarla
    call check_multiboot    ; Multiboot ile başlatıldığını kontrol et
    call check_cpuid        ; CPUID desteğini kontrol et
    call check_long_mode    ; Long mode desteğini kontrol et
    call setup_page_tables  ; Sayfa tablolarını kur
    call enable_paging      ; Paging'i etkinleştir
    
    ; 64-bit moda geçiş
    lgdt [gdt64.pointer]    ; GDT'yi yükle
    jmp gdt64.code:long_mode_start
    
    ; Buraya asla ulaşılmamalı
    hlt

check_multiboot:
    cmp eax, 0x36d76289
    jne .no_multiboot
    ret
.no_multiboot:
    mov al, "0"
    jmp error

check_cpuid:
    ; CPUID komutunu destekleyip desteklemediğini kontrol et
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov al, "1"
    jmp error

check_long_mode:
    ; CPUID ile uzun mod desteğini kontrol et
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, "2"
    jmp error

setup_page_tables:
    ; Page table hazırlama
    mov eax, p3_table
    or eax, 0b11 ; mevcut ve yazılabilir
    mov [p4_table], eax
    
    mov eax, p2_table
    or eax, 0b11 ; mevcut ve yazılabilir
    mov [p3_table], eax
    
    ; P2 tablosunda tüm girdileri ayarla
    mov ecx, 0
.map_p2_table:
    mov eax, 0x200000  ; 2MiB
    mul ecx
    or eax, 0b10000011 ; mevcut, yazılabilir, büyük sayfa
    mov [p2_table + ecx * 8], eax
    
    inc ecx
    cmp ecx, 512
    jne .map_p2_table
    
    ret

enable_paging:
    ; CR3 kaydını page table ile doldur
    mov eax, p4_table
    mov cr3, eax
    
    ; PAE'yi etkinleştir
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    ; Long mode'u etkinleştir
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    
    ; Paging'i etkinleştir
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    ret

error:
    ; Bir ASCII karakter görüntüle, karakter AL'de
    mov dword [0xb8000], 0x4f524f45
    mov dword [0xb8004], 0x4f3a4f52
    mov dword [0xb8008], 0x4f204f20
    mov byte [0xb800a], al
    hlt

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096
stack_bottom:
    resb 4096 * 4
stack_top:

section .rodata
gdt64:
    dq 0 ; Null tanımlayıcı
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; code segment
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .text
bits 64
long_mode_start:
    ; Segment kayıtlarını temizle
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; kernel_main fonksiyonunu çağır
    call kernel_main
    
    ; kernel_main'den dönülürse sonsuz döngüye gir
    hlt
    jmp $ 