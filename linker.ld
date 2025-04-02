/* Kernel için linker script */
ENTRY(start)

SECTIONS {
    /* Kernel 1MB adresinde başlayacak */
    . = 1M;

    .text : ALIGN(4K) {
        *(.multiboot) /* Multiboot header */
        *(.text)      /* Kod bölümü */
    }
    
    .rodata : ALIGN(4K) {
        *(.rodata)    /* Salt okunur veri */
    }
    
    .data : ALIGN(4K) {
        *(.data)      /* Başlatılmış veri */
    }
    
    .bss : ALIGN(4K) {
        *(COMMON)     /* Ortak semboller */
        *(.bss)       /* Başlatılmamış veri */
    }
    
    /* Kernel bitiş adresini takip edin */
    _kernel_end = .;
} 