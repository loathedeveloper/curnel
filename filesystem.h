#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>

// Dosya bayrakları
#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_READONLY    0x04

// FAT32 sabitleri
#define FAT32_SIGNATURE     0x28 // FAT32 imzası
#define FAT32_EOC           0x0FFFFFF8 // Zincir sonu işaretçisi

// Dosya sistemi hatları
#define FS_SUCCESS      0  // Başarılı
#define FS_ERROR       -1  // Genel hata
#define FS_NOT_FOUND   -2  // Dosya bulunamadı
#define FS_NOT_MOUNTED -3  // Dosya sistemi bağlanmadı

// Dosya girişi yapısı
typedef struct {
    char name[128];           // Dosya adı
    uint32_t size;            // Dosya boyutu
    uint8_t flags;            // Dosya bayrakları (FS_FILE, FS_DIRECTORY)
    uint32_t cluster;         // Başlangıç küme numarası
    uint32_t modified_time;   // Değişiklik zamanı
    uint32_t created_time;    // Oluşturma zamanı
} fs_dirent_t;

// Dosya yapısı
typedef struct {
    fs_dirent_t dirent;       // Dosya girişi
    uint32_t position;        // Dosyada mevcut pozisyon
    uint32_t current_cluster; // Mevcut küme
    uint32_t device;          // Cihaz tanımlayıcısı
} fs_file_t;

// FAT32 BIOS Parametre Bloğu
typedef struct {
    uint8_t  jmp_boot[3];         // x86 atlama talimatı
    uint8_t  oem_name[8];         // OEM adı
    uint16_t bytes_per_sector;    // Sektör başına bayt (genellikle 512)
    uint8_t  sectors_per_cluster; // Küme başına sektör
    uint16_t reserved_sectors;    // Ayrılmış sektörler
    uint8_t  num_fats;            // FAT tablosu sayısı (genellikle 2)
    uint16_t root_entries;        // FAT32'de kullanılmaz, 0
    uint16_t total_sectors_16;    // FAT32'de kullanılmaz, 0
    uint8_t  media_type;          // Medya türü
    uint16_t fat_size_16;         // FAT32'de kullanılmaz, 0
    uint16_t sectors_per_track;   // Parça başına sektör
    uint16_t num_heads;           // Kafa sayısı
    uint32_t hidden_sectors;      // Gizli sektörler
    uint32_t total_sectors_32;    // Toplam sektör sayısı
    
    // FAT32 genişletilmiş BPB
    uint32_t fat_size_32;         // FAT başına sektör
    uint16_t ext_flags;           // Genişletilmiş bayraklar
    uint16_t fs_version;          // Dosya sistemi sürümü
    uint32_t root_cluster;        // Kök dizin kümesi
    uint16_t fs_info;             // FSInfo sektörü
    uint16_t backup_boot;         // Yedek boot sektörü
    uint8_t  reserved[12];        // Ayrılmış, sıfır
    uint8_t  drive_number;        // Sürücü numarası
    uint8_t  reserved1;           // Ayrılmış, sıfır
    uint8_t  boot_signature;      // Boot imzası (0x28 veya 0x29)
    uint32_t volume_id;           // Birim kimliği
    uint8_t  volume_label[11];    // Birim etiketi
    uint8_t  fs_type[8];          // "FAT32   "
} __attribute__((packed)) fat32_bpb_t;

// Dosya sistemi bilgileri
typedef struct {
    uint32_t device;              // Cihaz kimliği
    uint32_t partition_start;     // Bölüm başlangıç sektörü
    uint32_t fat_start;           // FAT başlangıç sektörü
    uint32_t data_start;          // Veri başlangıç sektörü
    uint32_t root_cluster;        // Kök dizin kümesi
    uint32_t sectors_per_cluster; // Küme başına sektör
    uint32_t bytes_per_sector;    // Sektör başına bayt
    uint32_t bytes_per_cluster;   // Küme başına bayt
    uint8_t mounted;              // Bağlanma durumu
} fs_info_t;

// Dosya sistemi işlevleri
int fs_init();
int fs_mount(uint32_t device);
int fs_unmount();

// Dosya işlemleri
int fs_open(const char* path, fs_file_t* file);
int fs_close(fs_file_t* file);
int fs_read(fs_file_t* file, void* buffer, size_t size);
int fs_write(fs_file_t* file, const void* buffer, size_t size);
int fs_seek(fs_file_t* file, uint32_t position);
int fs_tell(fs_file_t* file);
int fs_eof(fs_file_t* file);

// Dizin işlemleri
int fs_opendir(const char* path, fs_file_t* dir);
int fs_readdir(fs_file_t* dir, fs_dirent_t* dirent);
int fs_closedir(fs_file_t* dir);
int fs_mkdir(const char* path);
int fs_unlink(const char* path);

// Yardımcı işlevler
int fs_stat(const char* path, fs_dirent_t* stat);

#endif // FILESYSTEM_H 