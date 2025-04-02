# 64-bit kernel için Makefile

# Derleyiciler ve bayraklar
ASM=nasm
CC=gcc
LD=ld

# 64-bit için derleyici bayrakları
CFLAGS= -ffreestanding -O2 -Wall -Wextra -fno-exceptions -mcmodel=large \
        -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -nostdlib -fno-pie \
        -m64 -c

# Assembler bayrakları
ASMFLAGS= -f elf64

# Linker bayrakları
LDFLAGS= -n -T linker.ld

# Kaynak dosyaları
ASM_SOURCES=boot.asm isr.asm
C_SOURCES=kernel.c memory.c idt.c process.c syscall.c
OBJ=$(ASM_SOURCES:.asm=.o) $(C_SOURCES:.c=.o)

# Hedefler
.PHONY: all clean run-qemu

all: os.bin

# .o dosyalarını oluştur
%.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

# Kerneli bağla
os.bin: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $@

# Kernel ISO'su oluşturma (QEMU veya gerçek makine için)
os.iso: os.bin
	mkdir -p iso/boot/grub
	cp os.bin iso/boot/
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "64-bit Kernel" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot /boot/os.bin' >> iso/boot/grub/grub.cfg
	echo '  boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o os.iso iso

# Temizleme
clean:
	rm -f *.o *.bin *.iso
	rm -rf iso

# QEMU ile çalıştırma
run-qemu: os.bin
	qemu-system-x86_64 -kernel os.bin

# ISO ile QEMU çalıştırma
run-iso: os.iso
	qemu-system-x86_64 -cdrom os.iso 