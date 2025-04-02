#include "kernel.h"
#include "timer.h"
#include "idt.h"
#include "process.h"

// Zamanlayıcı değişkenleri
static uint64_t timer_ticks = 0;
static uint32_t timer_frequency = 0;
static timer_callback_t timer_callback = NULL;

// PIT zamanlayıcısını başlat
void timer_init(uint32_t frequency) {
    // Frekansı doğrula (en düşük 1 Hz, en yüksek 1193)
    if (frequency < 1) frequency = 1;
    if (frequency > 1193) frequency = 1193;
    
    timer_frequency = frequency;
    
    // Bölen değerini hesapla
    uint32_t divisor = PIT_BASE_FREQ / frequency;
    
    // PIT komutunu ayarla: kanal 0, erişim modu, kare dalga modu, ikili sayım
    outb(PIT_COMMAND_PORT, PIT_CHANNEL0 | PIT_ACCESS_BOTH | PIT_MODE3 | PIT_BINARY);
    
    // Düşük ve yüksek byte'ları gönder
    outb(PIT_DATA_PORT0, divisor & 0xFF);           // Düşük byte
    outb(PIT_DATA_PORT0, (divisor >> 8) & 0xFF);    // Yüksek byte
    
    // Timer kesme işleyicisini kaydet (IRQ0 -> kesme 32)
    register_interrupt_handler(IRQ0, (isr_t)timer_handler);
    
    // Başlangıç değerlerini sıfırla
    timer_ticks = 0;
    
    terminal_writestring("PIT zamanlayicisi baslatildi: ");
    char freq_str[10];
    int_to_string(frequency, freq_str);
    terminal_writestring(freq_str);
    terminal_writestring(" Hz\n");
}

// Zamanlayıcı kesme işleyicisi
void timer_handler(uint64_t int_no) {
    // Tik sayısını artır
    timer_ticks++;
    
    // Kayıtlı bir geri çağırma varsa çağır
    if (timer_callback != NULL) {
        timer_callback(timer_ticks);
    }
    
    // Uyuyan süreçleri kontrol et
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* process = get_process(i);
        
        if (process != NULL && process->state == PROCESS_STATE_SLEEPING) {
            // Sürenin dolup dolmadığını kontrol et
            if (timer_ticks >= process->sleep_until) {
                // Süre doldu, süreci tekrar hazır yap
                process->state = PROCESS_STATE_READY;
            }
        }
    }
    
    // Her 10 ms'de bir zamanlayıcıyı çağır
    if (timer_ticks % (timer_frequency / 100) == 0) {
        schedule();
    }
}

// Zamanlayıcı geri çağırma işlevini kaydet
void timer_register_callback(timer_callback_t callback) {
    timer_callback = callback;
}

// Mevcut tik sayısını al
uint64_t timer_get_ticks() {
    return timer_ticks;
}

// Belirtilen milisaniye kadar bekle
void timer_sleep(uint32_t ms) {
    // Tik cinsinden bekleme süresini hesapla
    uint64_t ticks = (timer_frequency * ms) / 1000;
    
    // Eğer 0 ise en az 1 tik bekle
    if (ticks == 0) ticks = 1;
    
    // Hedef tik değerini hesapla
    uint64_t target_ticks = timer_ticks + ticks;
    
    // Süre dolana kadar bekle
    while (timer_ticks < target_ticks) {
        // Kesmeleri etkinleştir ve bekle
        asm volatile("sti; hlt");
    }
}

// Geçerli süreci uyut
void process_sleep(uint32_t ms) {
    // Mevcut süreci al
    process_t* current = get_current_process();
    
    if (current != NULL && current->pid != 0) { // pid 0 kernel süreci
        // Uyanma zamanını hesapla
        current->sleep_until = timer_ticks + ((timer_frequency * ms) / 1000);
        
        // Süreci uyku durumuna getir
        current->state = PROCESS_STATE_SLEEPING;
        
        // Yeni süreç seçilmesi için zamanlayıcıyı çağır
        schedule();
    } else {
        // Eğer geçerli bir süreç yoksa veya kernel süreci ise, sadece bekle
        timer_sleep(ms);
    }
} 