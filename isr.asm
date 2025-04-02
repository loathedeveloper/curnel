; 64-bit kesme servis rutinleri
bits 64

; Dışa aktarılan sembolleri tanımla
global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr22
global isr23
global isr24
global isr25
global isr26
global isr27
global isr28
global isr29
global isr30
global isr31
global idt_flush

; C işleyicisini içe aktar
extern isr_handler

; IDT'yi yükle - C'den çağrılır
idt_flush:
    lidt [rdi]    ; RDI kaydedici 64-bit modunda ilk parametre
    ret

; Ortak ISR işleyicisi
isr_common_stub:
    ; Kaydedicileri kaydet
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; C işleyicisini çağır
    mov rdi, rsp    ; İlk parametre: kayıt yapısı
    call isr_handler
    
    ; Kaydedicileri geri yükle
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Kesmeyi geri döndür ve hata kodunu temizle
    add rsp, 16
    iretq

; Her bir ISR için makro
%macro ISR_NOERRCODE 1
    isr%1:
        push 0       ; Sahte hata kodu
        push %1      ; Kesme numarası
        jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
    isr%1:
        push %1      ; Kesme numarası (hata kodu otomatik eklenmiş)
        jmp isr_common_stub
%endmacro

; İlk 32 ISR (CPU hataları)
ISR_NOERRCODE 0    ; Sıfıra bölme hatası
ISR_NOERRCODE 1    ; Debug
ISR_NOERRCODE 2    ; NMI
ISR_NOERRCODE 3    ; Breakpoint
ISR_NOERRCODE 4    ; Overflow
ISR_NOERRCODE 5    ; Bound Range Exceeded
ISR_NOERRCODE 6    ; Invalid Opcode
ISR_NOERRCODE 7    ; Device Not Available
ISR_ERRCODE   8    ; Double Fault
ISR_NOERRCODE 9    ; Coprocessor Segment Overrun
ISR_ERRCODE   10   ; Invalid TSS
ISR_ERRCODE   11   ; Segment Not Present
ISR_ERRCODE   12   ; Stack-Segment Fault
ISR_ERRCODE   13   ; General Protection Fault
ISR_ERRCODE   14   ; Page Fault
ISR_NOERRCODE 15   ; Reserved
ISR_NOERRCODE 16   ; x87 FPU Error
ISR_ERRCODE   17   ; Alignment Check
ISR_NOERRCODE 18   ; Machine Check
ISR_NOERRCODE 19   ; SIMD Floating-Point Exception
ISR_NOERRCODE 20   ; Virtualization Exception
ISR_NOERRCODE 21   ; Reserved
ISR_NOERRCODE 22   ; Reserved
ISR_NOERRCODE 23   ; Reserved
ISR_NOERRCODE 24   ; Reserved
ISR_NOERRCODE 25   ; Reserved
ISR_NOERRCODE 26   ; Reserved
ISR_NOERRCODE 27   ; Reserved
ISR_NOERRCODE 28   ; Reserved
ISR_NOERRCODE 29   ; Reserved
ISR_NOERRCODE 30   ; Reserved
ISR_NOERRCODE 31   ; Reserved 