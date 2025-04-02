#include "kernel.h"
#include "filesystem.h"

// Global dosya sistemi bilgisi
static fs_info_t fs_info;

// ATA disk okuma fonksiyonu prototipi
extern int ata_read_sectors(uint32_t lba, uint8_t sector_count, void* buffer);
extern int ata_write_sectors(uint32_t lba, uint8_t sector_count, const void* buffer);

// FAT girişini oku
static uint32_t fat32_read_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs_info.fat_start + (fat_offset / fs_info.bytes_per_sector);
    uint32_t entry_offset = fat_offset % fs_info.bytes_per_sector;
    
    // Sektör tamponunu oluştur
    uint8_t sector[512];
    if (ata_read_sectors(fat_sector, 1, sector) != 0) {
        return 0;
    }
    
    // FAT girişini oku (little-endian)
    uint32_t entry = *((uint32_t*)(sector + entry_offset));
    
    // Sadece 28 biti kullan
    return entry & 0x0FFFFFFF;
}

// FAT girişini yaz
static int fat32_write_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs_info.fat_start + (fat_offset / fs_info.bytes_per_sector);
    uint32_t entry_offset = fat_offset % fs_info.bytes_per_sector;
    
    // Sektör tamponunu oku
    uint8_t sector[512];
    if (ata_read_sectors(fat_sector, 1, sector) != 0) {
        return FS_ERROR;
    }
    
    // FAT girişini değiştir (28 bitlik değer + üst 4 bit)
    uint32_t* entry_ptr = (uint32_t*)(sector + entry_offset);
    *entry_ptr = (*entry_ptr & 0xF0000000) | (value & 0x0FFFFFFF);
    
    // Sektörü geri yaz
    if (ata_write_sectors(fat_sector, 1, sector) != 0) {
        return FS_ERROR;
    }
    
    return FS_SUCCESS;
}

// LBA adresinden küme numarasını hesapla
static uint32_t fat32_cluster_to_lba(uint32_t cluster) {
    return fs_info.data_start + (cluster - 2) * fs_info.sectors_per_cluster;
}

// Dosya sistemi başlatma
int fs_init() {
    // Bilgileri sıfırla
    memset(&fs_info, 0, sizeof(fs_info_t));
    return FS_SUCCESS;
}

// Dosya sistemini bağla
int fs_mount(uint32_t device) {
    // Cihaz kimliğini ayarla
    fs_info.device = device;
    
    // MBR'yi oku (ilk sektör)
    uint8_t mbr[512];
    if (ata_read_sectors(0, 1, mbr) != 0) {
        return FS_ERROR;
    }
    
    // İlk bölümün başlangıç sektörünü bul (basit MBR yapısı)
    fs_info.partition_start = *((uint32_t*)(mbr + 0x1BE + 0x08));
    
    // Boot sektörünü oku
    uint8_t boot_sector[512];
    if (ata_read_sectors(fs_info.partition_start, 1, boot_sector) != 0) {
        return FS_ERROR;
    }
    
    // BPB'yi analiz et
    fat32_bpb_t* bpb = (fat32_bpb_t*)boot_sector;
    
    // FAT32 olduğunu doğrula
    if (bpb->boot_signature != FAT32_SIGNATURE) {
        return FS_ERROR;
    }
    
    // Dosya sistemi bilgilerini ayarla
    fs_info.bytes_per_sector = bpb->bytes_per_sector;
    fs_info.sectors_per_cluster = bpb->sectors_per_cluster;
    fs_info.bytes_per_cluster = fs_info.bytes_per_sector * fs_info.sectors_per_cluster;
    fs_info.root_cluster = bpb->root_cluster;
    
    // FAT ve veri bölgelerini hesapla
    fs_info.fat_start = fs_info.partition_start + bpb->reserved_sectors;
    fs_info.data_start = fs_info.fat_start + (bpb->num_fats * bpb->fat_size_32);
    
    // Bağlandı olarak işaretle
    fs_info.mounted = 1;
    
    return FS_SUCCESS;
}

// Dosya sistemini ayır
int fs_unmount() {
    // Bağlantı kesildi olarak işaretle
    fs_info.mounted = 0;
    return FS_SUCCESS;
}

// Yeni küme tahsis et
static uint32_t fat32_allocate_cluster() {
    if (!fs_info.mounted) {
        return 0;
    }
    
    // FAT içinde boş (0) bir küme ara
    uint32_t fat_sectors = fs_info.data_start - fs_info.fat_start;
    uint8_t sector[512];
    
    for (uint32_t fat_sector = 0; fat_sector < fat_sectors; fat_sector++) {
        uint32_t current_sector = fs_info.fat_start + fat_sector;
        
        if (ata_read_sectors(current_sector, 1, sector) != 0) {
            return 0;
        }
        
        // Sektördeki her FAT girişini kontrol et
        for (uint32_t i = 0; i < fs_info.bytes_per_sector; i += 4) {
            uint32_t* entry = (uint32_t*)(sector + i);
            uint32_t value = *entry & 0x0FFFFFFF;
            
            if (value == 0) {
                // Küme indeksi hesapla
                uint32_t cluster = (fat_sector * fs_info.bytes_per_sector + i) / 4;
                
                // Küme 2'den başlar
                if (cluster >= 2) {
                    // Zincir sonu olarak işaretle
                    fat32_write_fat_entry(cluster, FAT32_EOC);
                    return cluster;
                }
            }
        }
    }
    
    return 0; // Boş küme bulunamadı
}

// Dosya açma
int fs_open(const char* path, fs_file_t* file) {
    if (!fs_info.mounted) {
        return FS_NOT_MOUNTED;
    }
    
    // Yalnızca kök dizini işle (basitlik için)
    if (strcmp(path, "/") == 0) {
        memset(file, 0, sizeof(fs_file_t));
        
        // Kök dizin bilgilerini doldur
        strcpy(file->dirent.name, "/");
        file->dirent.flags = FS_DIRECTORY;
        file->dirent.cluster = fs_info.root_cluster;
        file->current_cluster = fs_info.root_cluster;
        file->position = 0;
        file->device = fs_info.device;
        
        return FS_SUCCESS;
    }
    
    // Dosya adını yoldan ayıkla
    char filename[128];
    const char* name_start = path;
    
    // Başlangıçtaki /'ları atla
    while (*name_start == '/') {
        name_start++;
    }
    
    // Dosya adını kopyala
    strcpy(filename, name_start);
    
    // Kök dizini oku
    uint32_t cluster = fs_info.root_cluster;
    uint32_t sector_lba = fat32_cluster_to_lba(cluster);
    uint8_t sector[512];
    
    // Kök dizin klasörünü tara
    for (uint32_t i = 0; i < fs_info.sectors_per_cluster; i++) {
        if (ata_read_sectors(sector_lba + i, 1, sector) != 0) {
            return FS_ERROR;
        }
        
        // Dizin girişlerini tara
        for (uint32_t j = 0; j < fs_info.bytes_per_sector; j += 32) {
            // FAT32 dizin girişi
            uint8_t* entry = sector + j;
            
            // Boş giriş
            if (entry[0] == 0) {
                break;
            }
            
            // Silinmiş giriş
            if (entry[0] == 0xE5) {
                continue;
            }
            
            // Uzun dosya adı (LFN) girişi
            if (entry[11] == 0x0F) {
                continue;
            }
            
            // 8.3 dosya adını oku ve karşılaştır
            char short_name[13];
            int name_pos = 0;
            
            // Ana kısmı (8 karakter)
            for (int k = 0; k < 8; k++) {
                if (entry[k] != ' ') {
                    short_name[name_pos++] = entry[k];
                }
            }
            
            // Uzantı (3 karakter)
            if (entry[8] != ' ') {
                short_name[name_pos++] = '.';
                for (int k = 8; k < 11; k++) {
                    if (entry[k] != ' ') {
                        short_name[name_pos++] = entry[k];
                    }
                }
            }
            
            // Sonlandır
            short_name[name_pos] = '\0';
            
            // Dosya adını karşılaştır
            if (strcmp(short_name, filename) == 0) {
                // Dosya bulundu, bilgileri doldur
                memset(file, 0, sizeof(fs_file_t));
                
                // Dosya bilgilerini al
                strcpy(file->dirent.name, short_name);
                file->dirent.size = *((uint32_t*)(entry + 28));
                file->dirent.flags = (entry[11] & 0x10) ? FS_DIRECTORY : FS_FILE;
                
                // Başlangıç kümesini al (yüksek ve düşük bitleri birleştir)
                uint32_t cluster_high = *((uint16_t*)(entry + 20));
                uint32_t cluster_low = *((uint16_t*)(entry + 26));
                file->dirent.cluster = (cluster_high << 16) | cluster_low;
                
                file->current_cluster = file->dirent.cluster;
                file->position = 0;
                file->device = fs_info.device;
                
                return FS_SUCCESS;
            }
        }
    }
    
    return FS_NOT_FOUND;
}

// Dosya kapat
int fs_close(fs_file_t* file) {
    // Hiçbir şey yapmıyoruz (bellekte tutulmadığı için)
    return FS_SUCCESS;
}

// Dosya oku
int fs_read(fs_file_t* file, void* buffer, size_t size) {
    if (!fs_info.mounted) {
        return FS_NOT_MOUNTED;
    }
    
    // Direk dosyası mı?
    if (file->dirent.flags & FS_DIRECTORY) {
        return FS_ERROR;
    }
    
    // Dosya boyutu kontrolü
    if (file->position >= file->dirent.size) {
        return 0; // EOF
    }
    
    // Okuma boyutunu sınırla
    if (file->position + size > file->dirent.size) {
        size = file->dirent.size - file->position;
    }
    
    // Okuma için kalan bayt sayısı
    size_t bytes_left = size;
    uint8_t* dst = (uint8_t*)buffer;
    
    // Mevcut küme içindeki pozisyon
    uint32_t cluster_pos = file->position % fs_info.bytes_per_cluster;
    
    // Doğru kümeye atla
    uint32_t cluster = file->dirent.cluster;
    uint32_t pos = 0;
    
    while (pos + fs_info.bytes_per_cluster <= file->position) {
        cluster = fat32_read_fat_entry(cluster);
        if (cluster >= FAT32_EOC) {
            return 0; // EOF
        }
        pos += fs_info.bytes_per_cluster;
    }
    
    // Okuma işlemi
    while (bytes_left > 0) {
        // Küme sektörlerini oku
        uint32_t sector_lba = fat32_cluster_to_lba(cluster);
        uint8_t sector_buffer[512];
        
        // Küme içindeki sektör
        uint32_t sector_offset = cluster_pos / fs_info.bytes_per_sector;
        
        // Sektör içindeki offset
        uint32_t offset = cluster_pos % fs_info.bytes_per_sector;
        
        // Sektörü oku
        if (ata_read_sectors(sector_lba + sector_offset, 1, sector_buffer) != 0) {
            return FS_ERROR;
        }
        
        // Bu sektörden okunacak bayt sayısı
        uint32_t sector_bytes = fs_info.bytes_per_sector - offset;
        if (sector_bytes > bytes_left) {
            sector_bytes = bytes_left;
        }
        
        // Verileri kopyala
        memcpy(dst, sector_buffer + offset, sector_bytes);
        
        // İlerleme
        dst += sector_bytes;
        bytes_left -= sector_bytes;
        cluster_pos += sector_bytes;
        file->position += sector_bytes;
        
        // Kümenin sonuna ulaşıldığında sonraki kümeye geç
        if (cluster_pos >= fs_info.bytes_per_cluster) {
            cluster = fat32_read_fat_entry(cluster);
            if (cluster >= FAT32_EOC && bytes_left > 0) {
                break; // Zincir sonu
            }
            cluster_pos = 0;
        }
    }
    
    return size - bytes_left;
}

// Dosya yaz
int fs_write(fs_file_t* file, const void* buffer, size_t size) {
    if (!fs_info.mounted) {
        return FS_NOT_MOUNTED;
    }
    
    // Dizin mi?
    if (file->dirent.flags & FS_DIRECTORY) {
        return FS_ERROR;
    }
    
    // Yazma için kalan bayt sayısı
    size_t bytes_left = size;
    const uint8_t* src = (const uint8_t*)buffer;
    
    // Mevcut küme içindeki pozisyon
    uint32_t cluster_pos = file->position % fs_info.bytes_per_cluster;
    
    // Doğru kümeye atla
    uint32_t cluster = file->dirent.cluster;
    uint32_t pos = 0;
    uint32_t prev_cluster = 0;
    
    // İlk küme yoksa oluştur
    if (cluster == 0) {
        cluster = fat32_allocate_cluster();
        if (cluster == 0) {
            return FS_ERROR;
        }
        file->dirent.cluster = cluster;
        file->current_cluster = cluster;
    }
    
    while (pos + fs_info.bytes_per_cluster <= file->position) {
        prev_cluster = cluster;
        cluster = fat32_read_fat_entry(cluster);
        
        // Yeni küme gerekiyor mu?
        if (cluster >= FAT32_EOC) {
            cluster = fat32_allocate_cluster();
            if (cluster == 0) {
                return FS_ERROR;
            }
            fat32_write_fat_entry(prev_cluster, cluster);
        }
        
        pos += fs_info.bytes_per_cluster;
    }
    
    // Yazma işlemi
    while (bytes_left > 0) {
        // Küme sektörlerini oku
        uint32_t sector_lba = fat32_cluster_to_lba(cluster);
        uint8_t sector_buffer[512];
        
        // Küme içindeki sektör
        uint32_t sector_offset = cluster_pos / fs_info.bytes_per_sector;
        
        // Sektör içindeki offset
        uint32_t offset = cluster_pos % fs_info.bytes_per_sector;
        
        // Sektörü oku
        if (ata_read_sectors(sector_lba + sector_offset, 1, sector_buffer) != 0) {
            return FS_ERROR;
        }
        
        // Bu sektöre yazılacak bayt sayısı
        uint32_t sector_bytes = fs_info.bytes_per_sector - offset;
        if (sector_bytes > bytes_left) {
            sector_bytes = bytes_left;
        }
        
        // Verileri kopyala
        memcpy(sector_buffer + offset, src, sector_bytes);
        
        // Sektörü geri yaz
        if (ata_write_sectors(sector_lba + sector_offset, 1, sector_buffer) != 0) {
            return FS_ERROR;
        }
        
        // İlerleme
        src += sector_bytes;
        bytes_left -= sector_bytes;
        cluster_pos += sector_bytes;
        file->position += sector_bytes;
        
        // Kümenin sonuna ulaşıldığında sonraki kümeye geç
        if (cluster_pos >= fs_info.bytes_per_cluster) {
            prev_cluster = cluster;
            cluster = fat32_read_fat_entry(cluster);
            
            // Yeni küme gerekiyor mu?
            if (cluster >= FAT32_EOC && bytes_left > 0) {
                uint32_t new_cluster = fat32_allocate_cluster();
                if (new_cluster == 0) {
                    break;
                }
                fat32_write_fat_entry(prev_cluster, new_cluster);
                cluster = new_cluster;
            }
            
            cluster_pos = 0;
        }
    }
    
    // Dosya boyutunu güncelle
    if (file->position > file->dirent.size) {
        file->dirent.size = file->position;
        // Not: Dizin girişini güncellemeyi unutma (gerçek uygulamada gerekli)
    }
    
    return size - bytes_left;
}

// Dosya konumlandır
int fs_seek(fs_file_t* file, uint32_t position) {
    if (!fs_info.mounted) {
        return FS_NOT_MOUNTED;
    }
    
    // Geçerli konumu kontrol et
    if (position > file->dirent.size) {
        position = file->dirent.size;
    }
    
    file->position = position;
    
    // Kümeyi güncellemek gerekebilir (basitleştirme için yapılmadı)
    return FS_SUCCESS;
}

// Dosya konumu
int fs_tell(fs_file_t* file) {
    return file->position;
}

// Dosya sonu kontrolü
int fs_eof(fs_file_t* file) {
    return (file->position >= file->dirent.size);
}

// Dizin aç
int fs_opendir(const char* path, fs_file_t* dir) {
    int result = fs_open(path, dir);
    
    // Açılan şey bir dizin mi kontrol et
    if (result == FS_SUCCESS && !(dir->dirent.flags & FS_DIRECTORY)) {
        return FS_ERROR;
    }
    
    return result;
}

// Dizin oku
int fs_readdir(fs_file_t* dir, fs_dirent_t* dirent) {
    if (!fs_info.mounted) {
        return FS_NOT_MOUNTED;
    }
    
    // Dizin mi?
    if (!(dir->dirent.flags & FS_DIRECTORY)) {
        return FS_ERROR;
    }
    
    uint32_t cluster = dir->current_cluster;
    
    // Kümeyi oku
    uint32_t sector_lba = fat32_cluster_to_lba(cluster);
    uint8_t sector[512];
    
    // Dizin içindeki pozisyon
    uint32_t dir_offset = dir->position;
    
    // Sektör ve giriş ofsetlerini hesapla
    uint32_t sector_idx = dir_offset / fs_info.bytes_per_sector;
    uint32_t entry_idx = (dir_offset % fs_info.bytes_per_sector) / 32;
    
    // Yeterli sektör olmadığında sonraki kümeye geç
    while (sector_idx >= fs_info.sectors_per_cluster) {
        cluster = fat32_read_fat_entry(cluster);
        if (cluster >= FAT32_EOC) {
            return FS_ERROR; // Dizin sonu
        }
        sector_lba = fat32_cluster_to_lba(cluster);
        sector_idx -= fs_info.sectors_per_cluster;
        dir->current_cluster = cluster;
    }
    
    // Sektörü oku
    if (ata_read_sectors(sector_lba + sector_idx, 1, sector) != 0) {
        return FS_ERROR;
    }
    
    // Tüm dizin girişlerini tara
    while (1) {
        // Dizin girişi
        uint8_t* entry = sector + (entry_idx * 32);
        
        // İlerleme
        entry_idx++;
        dir->position += 32;
        
        // Sektör sonuna ulaşıldığında sonraki sektöre geç
        if (entry_idx * 32 >= fs_info.bytes_per_sector) {
            sector_idx++;
            entry_idx = 0;
            
            // Küme sonuna ulaşıldığında sonraki kümeye geç
            if (sector_idx >= fs_info.sectors_per_cluster) {
                cluster = fat32_read_fat_entry(cluster);
                if (cluster >= FAT32_EOC) {
                    return FS_ERROR; // Dizin sonu
                }
                sector_lba = fat32_cluster_to_lba(cluster);
                sector_idx = 0;
                dir->current_cluster = cluster;
            }
            
            // Yeni sektörü oku
            if (ata_read_sectors(sector_lba + sector_idx, 1, sector) != 0) {
                return FS_ERROR;
            }
        }
        
        // Dizin sonu
        if (entry[0] == 0) {
            return FS_ERROR;
        }
        
        // Silinmiş giriş
        if (entry[0] == 0xE5) {
            continue;
        }
        
        // Uzun dosya adı (LFN) girişi
        if (entry[11] == 0x0F) {
            continue;
        }
        
        // Geçerli giriş bulundu
        // 8.3 dosya adını oku
        char short_name[13];
        int name_pos = 0;
        
        // Ana kısmı (8 karakter)
        for (int i = 0; i < 8; i++) {
            if (entry[i] != ' ') {
                short_name[name_pos++] = entry[i];
            }
        }
        
        // Uzantı (3 karakter)
        if (entry[8] != ' ') {
            short_name[name_pos++] = '.';
            for (int i = 8; i < 11; i++) {
                if (entry[i] != ' ') {
                    short_name[name_pos++] = entry[i];
                }
            }
        }
        
        // Sonlandır
        short_name[name_pos] = '\0';
        
        // Dosya bilgilerini doldur
        strcpy(dirent->name, short_name);
        dirent->size = *((uint32_t*)(entry + 28));
        dirent->flags = (entry[11] & 0x10) ? FS_DIRECTORY : FS_FILE;
        
        // Başlangıç kümesini al
        uint32_t cluster_high = *((uint16_t*)(entry + 20));
        uint32_t cluster_low = *((uint16_t*)(entry + 26));
        dirent->cluster = (cluster_high << 16) | cluster_low;
        
        return FS_SUCCESS;
    }
}

// Dizin kapat
int fs_closedir(fs_file_t* dir) {
    return fs_close(dir);
}

// String karşılaştırma
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// String kopyalama
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
} 