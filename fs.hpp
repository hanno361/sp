#ifndef FS_HPP
#define FS_HPP

#include <string>
#include <vector>
#include <ctime> // Zaman bilgisi için
#include <sys/types.h> // off_t için

// Disk ve Blok Sabitleri
const unsigned int BLOCK_SIZE_BYTES = 512;           // Örnek: 512 Bytes
const char DISK_FILENAME[] = "disk.sim";
const unsigned int DISK_SIZE_BYTES = 1 * 1024 * 1024; // 1 MB

// Metadata Alanı Sabitleri
const unsigned int METADATA_AREA_SIZE_BYTES = 4 * 1024;    // 4 KB. Bu alan içinde süperblok, bitmap ve FileInfo'lar olacak.

// Veri Alanı Hesaplamaları
// const unsigned int DATA_AREA_START_OFFSET = METADATA_AREA_SIZE_BYTES; // Bu diskteki offset, metadata'dan sonra.
const unsigned int TOTAL_DATA_AREA_SIZE = DISK_SIZE_BYTES - METADATA_AREA_SIZE_BYTES;
const unsigned int NUM_DATA_BLOCKS = TOTAL_DATA_AREA_SIZE / BLOCK_SIZE_BYTES;

// Bitmap Hesaplamaları (Veri blokları için)
const unsigned int BITMAP_SIZE_BYTES = (NUM_DATA_BLOCKS + 7) / 8; // Her bit bir veri bloğunu temsil eder, yukarı yuvarla.

// Süperblok Yapısı (Basit)
struct Superblock {
    // int total_fileinfo_slots; // MAX_FILES_CALCULATED ile aynı olacak, belki gereksiz
    int num_active_files;       // Aktif (silinmemiş) dosya sayısı
    // Gelecekte eklenebilir: unsigned int disk_size_total; unsigned int block_size_actual; 
    // unsigned int num_total_data_blocks; unsigned int actual_bitmap_size_bytes;
    // unsigned int file_info_array_offset_in_metadata; unsigned int max_file_entries;

    Superblock() : num_active_files(0) {}
};
const unsigned int SUPERBLOCK_ACTUAL_SIZE = sizeof(Superblock); 

// FileInfo ve Maksimum Dosya Sayısı Hesaplamaları
const int MAX_FILENAME_LENGTH = 255; 

struct FileInfo {
    char name[MAX_FILENAME_LENGTH + 1]; 
    int size;                           // Dosya boyutu (byte cinsinden)
    time_t creation_time;               
    bool is_used;                       
    
    off_t start_data_block_index;       // Veri alanındaki ilk bloğun indeksi (-1 ise blok yok)
    unsigned int num_data_blocks_used;  // Bu dosyanın kullandığı veri bloğu sayısı

    FileInfo() : size(0), creation_time(0), is_used(false), 
                 start_data_block_index(-1), num_data_blocks_used(0) {
        name[0] = '\0';
    }
};
const unsigned int FILE_INFO_ENTRY_SIZE = sizeof(FileInfo);

// Metadata içindeki elemanların başlangıç ofsetleri (metadata alanı başına göre relative)
const unsigned int BITMAP_START_OFFSET_IN_METADATA = SUPERBLOCK_ACTUAL_SIZE;
const unsigned int FILE_INFO_ARRAY_START_OFFSET_IN_METADATA = SUPERBLOCK_ACTUAL_SIZE + BITMAP_SIZE_BYTES;

// Kalan metadata alanını FileInfo'lar için hesapla
// Önce FILE_INFO_ARRAY_START_OFFSET_IN_METADATA'nın METADATA_AREA_SIZE_BYTES'ı aşmadığından emin olmalıyız.
const unsigned int FILE_INFO_ARRAY_USABLE_SIZE = (METADATA_AREA_SIZE_BYTES > FILE_INFO_ARRAY_START_OFFSET_IN_METADATA) ? (METADATA_AREA_SIZE_BYTES - FILE_INFO_ARRAY_START_OFFSET_IN_METADATA) : 0;
const int MAX_FILES_CALCULATED = (FILE_INFO_ARRAY_USABLE_SIZE > 0 && FILE_INFO_ENTRY_SIZE > 0) ? (FILE_INFO_ARRAY_USABLE_SIZE / FILE_INFO_ENTRY_SIZE) : 0;


// Fonksiyon Bildirimleri
void fs_init(); // Diski başlatır, yoksa oluşturur
void fs_format();
void fs_create(const char* filename);
void fs_delete(const char* filename);
int fs_write(const char* filename, const char* data, int size);
void fs_read(const char* filename, int offset, int size, char* buffer);
void fs_ls();
void fs_rename(const char* old_name, const char* new_name);
bool fs_exists(const char* filename);
int fs_size(const char* filename);
void fs_append(const char* filename, const char* data, int size);
void fs_truncate(const char* filename, int new_size);
void fs_copy(const char* src_filename, const char* dest_filename);
void fs_mv(const char* old_path, const char* new_path); // Şimdilik fs_rename gibi davranabilir
void fs_defragment();
void fs_check_integrity();
void fs_backup(const char* backup_filename);
void fs_restore(const char* backup_filename);
void fs_cat(const char* filename);
bool fs_diff(const char* file1, const char* file2);
void fs_log(const char* message); // Loglama için basit bir fonksiyon

// Bitmap Yönetimi Yardımcı Fonksiyonları
int find_free_data_block();
void free_data_block(int block_index);
int find_and_allocate_contiguous_data_blocks(int num_blocks_to_find);

#endif // FS_HPP 