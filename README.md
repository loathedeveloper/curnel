# 64-bit Basit Linux-Benzeri Kernel

Bu proje, modern x86_64 mimarisi için tasarlanmış basit bir Linux-benzeri kernel uygulamasıdır. Eğitim amaçlıdır ve temel işletim sistemi kavramlarını göstermek için oluşturulmuştur.

## Özellikler

- 64-bit Long Mode desteği
- Kesme işleme (IDT)
- Terminal çıktısı
- Temel bellek yönetimi
- Çok görevli çalışma (multitasking)
- Basit süreç yönetimi
- Sistem çağrıları (syscalls)
- Programlanabilir Interval Timer (PIT) desteği
- PS/2 Klavye sürücüsü
- 64-bit sayfa tablosu yönetimi
- Kullanıcı/çekirdek modu ayrımı (ring 0/3)
- Global Descriptor Table (GDT) ve Task State Segment (TSS)
- Basit FAT32 dosya sistemi desteği
- Pipe (boru) mekanizması
- Süreçler arası iletişim (IPC) temelleri
- Sinyal işleme mekanizması (POSIX uyumlu)
- Süreç grupları ve oturum yönetimi
- Temel komut satırı arayüzü (shell)
- Linux benzeri temel komutlar (coreutils)

## Dosya Yapısı

- `boot.asm`: Bootloader ve 64-bit moda geçiş kodu
- `kernel.c`: Ana kernel kodu
- `kernel.h`: Kernel başlık dosyası
- `idt.c` ve `idt.h`: Kesme Tanımlama Tablosu (IDT) yönetimi
- `memory.c`: Temel bellek yönetimi
- `syscall.c` ve `syscall.h`: Sistem çağrısı işleme
- `process.c` ve `process.h`: Süreç yönetimi
- `isr.asm`: Kesme servis rutinleri
- `timer.c` ve `timer.h`: PIT zamanlayıcı sürücüsü
- `keyboard.c` ve `keyboard.h`: PS/2 klavye sürücüsü
- `paging.c` ve `paging.h`: 64-bit sayfa tablosu yönetimi
- `usermode.c` ve `usermode.h`: Kullanıcı ve çekirdek modu ayrımı
- `filesystem.c` ve `filesystem.h`: FAT32 dosya sistemi sürücüsü
- `pipe.c` ve `pipe.h`: Pipe (boru) implementasyonu
- `signals.c` ve `signals.h`: Sinyal mekanizması implementasyonu
- `shell.c` ve `shell.h`: Komut satırı arayüzü
- `coreutils.c` ve `coreutils.h`: Temel sistem komutları
- `Makefile`: Derleme kuralları
- `linker.ld`: Bağlayıcı betiği

## Derleme ve Çalıştırma

1. Gereksinimleri yükleyin:
   - GCC çapraz derleyici (x86_64-elf hedefi için)
   - NASM assembler
   - QEMU
   - Make

2. Projeyi derleyin:
   ```
   make
   ```

3. QEMU ile çalıştırın:
   ```
   make run
   ```

## Yapılandırma

`Makefile` içindeki yapılandırma ayarlarını değiştirerek derleme sürecini özelleştirebilirsiniz.

## Komut Satırı (Shell)

Sistem başlatıldığında otomatik olarak bir komut satırı arayüzü (shell) başlatılır. Bu arayüz aşağıdaki özellikleri destekler:

- Komut çalıştırma
- Komut geçmişi (yukarı/aşağı ok tuşları ile gezinme)
- Tab ile otomatik tamamlama (basit)
- İmleç hareketi (sol/sağ ok tuşları)
- CTRL+C ile komutu iptal etme
- CTRL+D ile kabuktan çıkma (tampon boşsa)

## Desteklenen Komutlar (Coreutils)

Aşağıdaki temel komutlar desteklenmektedir:

- `ls`: Dizin içeriğini listele
- `cd`: Çalışma dizinini değiştir
- `pwd`: Mevcut çalışma dizinini göster
- `mkdir`: Yeni dizin oluştur
- `rmdir`: Dizini sil
- `rm`: Dosya sil
- `cp`: Dosya kopyala
- `mv`: Dosya taşı veya yeniden adlandır
- `cat`: Dosya içeriğini görüntüle
- `echo`: Metni ekrana yazdır
- `ps`: Çalışan süreçleri göster
- `kill`: Sürece sinyal gönder
- `sleep`: Belirtilen süre kadar bekle
- `touch`: Boş dosya oluştur veya zaman damgasını güncelle
- `clear`: Ekranı temizle
- `help`: Komut yardımını göster
- `exit`: Kabuktan çık

## Sistem Çağrıları

Aşağıdaki sistem çağrıları desteklenmektedir:

1. `SYS_EXIT (1)`: Süreç sonlandırma
2. `SYS_FORK (2)`: Yeni süreç oluşturma
3. `SYS_READ (3)`: Okuma işlemi
4. `SYS_WRITE (4)`: Yazma işlemi
5. `SYS_OPEN (5)`: Dosya açma
6. `SYS_CLOSE (6)`: Dosya kapatma
7. `SYS_SLEEP (7)`: Belirli bir süre bekleme
8. `SYS_GETPID (8)`: Süreç kimliğini alma
9. `SYS_EXEC (9)`: Program çalıştırma
10. `SYS_GETPPID (10)`: Ebeveyn süreç kimliğini alma
11. `SYS_PIPE (11)`: Pipe oluşturma
12. `SYS_DUP (12)`: Dosya tanımlayıcısını çoğaltma
13. `SYS_DUP2 (13)`: Dosya tanımlayıcısını belirli bir değere çoğaltma
14. `SYS_MKDIR (14)`: Dizin oluşturma
15. `SYS_RMDIR (15)`: Dizin silme
16. `SYS_WAIT (16)`: Alt süreç için bekleme
17. `SYS_KILL (17)`: Sinyal gönderme
18. `SYS_SIGNAL (18)`: Sinyal işleyicisi ayarlama
19. `SYS_SIGACTION (19)`: Sinyal eylemi ayarlama
20. `SYS_SIGPROCMASK (20)`: Sinyal maskesi ayarlama
21. `SYS_SIGPENDING (21)`: Bekleyen sinyalleri alma
22. `SYS_SIGSUSPEND (22)`: Geçici sinyal maskesi ile askıya alma
23. `SYS_SETPGID (23)`: Süreç grubu kimliği ayarlama
24. `SYS_GETPGID (24)`: Süreç grubu kimliğini alma
25. `SYS_SETSID (25)`: Yeni oturum oluşturma
26. `SYS_GETSID (26)`: Oturum kimliğini alma

## Sinyal Sistemi

Kernel, POSIX benzeri bir sinyal işleme mekanizması sunar:

- Standart sinyaller (SIGINT, SIGTERM, SIGKILL, SIGSTOP, vb.)
- Özelleştirilebilir sinyal işleyicileri
- Sinyal maskeleme
- Süreç gruplarına sinyal gönderme
- Sinyal eylemleri (SIG_IGN, SIG_DFL, özel işleyiciler)

Sinyaller, süreçler arası iletişim ve süreç yönetimi için kullanılır:

```c
// Bir sinyali işlemek için fonksiyon tanımlama
void handle_signal(int signum) {
    // Sinyal işleme kodu
}

// Bir sinyal için işleyici ayarlama
signal_handler_t old_handler = signal(SIGTERM, handle_signal);

// Bir sürece sinyal gönderme
kill(process_id, SIGTERM);

// Sinyal maskesini ayarlama
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigprocmask(SIG_BLOCK, &mask, NULL);
```

## İleriki Gelişmeler

- ELF dosya formatı desteği
- Ağ sürücüsü
- Daha gelişmiş dosya sistemi desteği
- Dinamik bellek yönetimi
- Grafik kullanıcı arayüzü temelleri
- Daha fazla komut desteği
- Boru ve yönlendirme (| ve >) desteği
- Daha gelişmiş shell özellikleri

## Lisans

Bu proje açık kaynaklıdır ve eğitim amaçlıdır.

## Katkıda Bulunanlar

- Kernel geliştiriciler

## Gereksinimler

- GCC Cross-Compiler (x86_64-elf-gcc)
- NASM
- LD (GNU Linker)
- QEMU (test etmek için)
- xorriso ve grub-mkrescue (ISO oluşturmak için)

## Derleme

Projeyi derlemek için:

```bash
make
```

ISO oluşturmak için:

```bash
make os.iso
```

## Test Etme

QEMU ile kernel'i doğrudan çalıştırmak için:

```bash
make run-qemu
```

ISO dosyasını QEMU ile çalıştırmak için:

```bash
make run-iso
```

## Proje Yapısı

- **boot.asm**: 64-bit moda geçen boot kodu
- **isr.asm**: Kesme servis rutinleri assembly kodu
- **kernel.c**: Kernel ana kodu
- **kernel.h**: Kernel header dosyası
- **memory.c**: Bellek yönetimi
- **idt.c**: Kesme tanımlama tablosu
- **idt.h**: Kesme header dosyası
- **process.c**: Süreç yönetimi
- **process.h**: Süreç yönetimi header dosyası
- **syscall.c**: Sistem çağrıları
- **syscall.h**: Sistem çağrıları header dosyası
- **signals.c**: Sinyal mekanizması
- **signals.h**: Sinyal tanımları ve prototipleri
- **shell.c**: Kabuk implementasyonu
- **shell.h**: Kabuk tanımları ve prototipleri
- **coreutils.c**: Temel komutlar implementasyonu
- **coreutils.h**: Komut tanımları
- **linker.ld**: Linker script
- **Makefile**: Derleme ayarları

## Sistemi Genişletme

Bu kernel, eğitim amaçlı bir temel yapıdır. Aşağıdaki bileşenler eklenebilir:

1. **Dosya Sistemi**: Daha gelişmiş bir dosya sistemi desteği
2. **Ağ Yığını**: TCP/IP gibi ağ protokolleri desteği
3. **USB Desteği**: USB cihazlarla iletişim
4. **Grafik Arayüzü**: Daha gelişmiş bir grafik desteği
5. **Çoklu İşlemci Desteği**: SMP mimarisi desteği
6. **Aygıt Sürücüleri**: Çeşitli donanım sürücüleri

## Notlar

Bu kernel eğitim amaçlıdır ve sadece temel işlevleri içerir. Gerçek bir işletim sistemi için çok daha fazla özellik ve karmaşık kod gereklidir. 