#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// PIT port numaraları
#define PIT_DATA_PORT0    0x40
#define PIT_DATA_PORT1    0x41
#define PIT_DATA_PORT2    0x42
#define PIT_COMMAND_PORT  0x43

// PIT komutları
#define PIT_CHANNEL0      0x00    // Kanal 0 (sistem saati)
#define PIT_CHANNEL1      0x40    // Kanal 1 (DRAM yenileme)
#define PIT_CHANNEL2      0x80    // Kanal 2 (hoparlör)
#define PIT_READBACK      0xC0    // Readback komutu

#define PIT_LATCH         0x00    // Sayacı kilitle
#define PIT_ACCESS_LOW    0x10    // Düşük byte
#define PIT_ACCESS_HIGH   0x20    // Yüksek byte
#define PIT_ACCESS_BOTH   0x30    // Düşük/yüksek byte

#define PIT_MODE0         0x00    // Kesme sayacı modu
#define PIT_MODE1         0x02    // Hardware tek darbe
#define PIT_MODE2         0x04    // Darbe üretici
#define PIT_MODE3         0x06    // Kare dalga üretici
#define PIT_MODE4         0x08    // Yazılım tetiklemeli darbe
#define PIT_MODE5         0x0A    // Donanım tetiklemeli darbe

#define PIT_BINARY        0x00    // İkili sayım
#define PIT_BCD           0x01    // BCD sayım

// PIT frekansları
#define PIT_BASE_FREQ     1193182   // PIT temel frekansı
#define PIT_DEFAULT_FREQ  100       // Varsayılan kesme frekansı (Hz)

// Zamanlayıcı işlevleri
void timer_init(uint32_t frequency);
void timer_handler(uint64_t int_no);
uint64_t timer_get_ticks();
void timer_sleep(uint32_t ms);

// Zamanlayıcı geri çağırma
typedef void (*timer_callback_t)(uint64_t tick);
void timer_register_callback(timer_callback_t callback);

#endif // TIMER_H 