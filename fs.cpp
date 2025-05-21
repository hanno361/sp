#include "fs.hpp"
#include <iostream>
#include <fstream> // Dosya işlemleri için
#include <cstring> // strcpy, strcmp vb. için
#include <sys/stat.h> // Dosya varlığını kontrol etmek için (fs_init)
#include <unistd.h> // ftruncate için (fs_init, fs_format)
#include <vector> // read_all_file_info için

// Helper function to check if disk file exists
bool disk_exists() {
    struct stat buffer;
    return (stat(DISK_FILENAME, &buffer) == 0);
}

// Helper function to create and initialize the disk file if it doesn't exist
void ensure_disk_initialized() {
    if (!disk_exists()) {
        std::cout << "Disk file '" << DISK_FILENAME << "' not found. Creating and initializing..." << std::endl;
        std::ofstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::out);
        if (!disk_file) {
            std::cerr << "Error: Could not create disk file '" << DISK_FILENAME << "'." << std::endl;
            // Proje gereksinimlerine göre burada programdan çıkılabilir veya hata yönetimi yapılabilir.
            // Şimdilik sadece bir hata mesajı veriyoruz.
            return;
        }
        // Dosyayı istenen boyuta getirme (truncate)
        if (truncate(DISK_FILENAME, DISK_SIZE_BYTES) != 0) {
            std::cerr << "Error: Could not set disk file size to " << DISK_SIZE_BYTES << " bytes." << std::endl;
            disk_file.close();
            // Hata durumunda dosyayı silmek isteyebiliriz.
            remove(DISK_FILENAME);
            return;
        }
        disk_file.close();
        std::cout << "Disk file '" << DISK_FILENAME << "' created with size " << DISK_SIZE_BYTES << " bytes." << std::endl;
        fs_format(); // Yeni diski formatla
    }
}


void fs_init() {
    std::cout << "Initializing SimpleFS..." << std::endl;
    ensure_disk_initialized();
    // TODO: Gerekirse disk dosyasını açıp, temel metadata kontrolleri yapılabilir.
    // Şimdilik ensure_disk_initialized yeterli.
    std::cout << "SimpleFS initialized." << std::endl;
}

void fs_format() {
    std::cout << "Formatting disk '" << DISK_FILENAME << "' with new metadata structure..." << std::endl;
    
    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk_file) {
        std::cerr << "Error: Could not open disk file '" << DISK_FILENAME << "' for formatting (new structure)." << std::endl;
        if (!disk_exists()) {
             std::cerr << "Error: Disk file '" << DISK_FILENAME << "' does not exist. Cannot format." << std::endl;
             std::cerr << "Run fs_init() first to create and initialize the disk." << std::endl;
             // return; // ensure_disk_initialized zaten fs_init içinde çağrılıyor, burada return edebiliriz.
        } else {
            // Dosya var ama açılamadı, bu farklı bir sorun olabilir.
            std::cerr << "Disk file exists but could not be opened for formatting." << std::endl;
        }
        return; // Her durumda çık
    }

    // 1. Tüm METADATA_AREA_SIZE_BYTES alanını sıfırla
    disk_file.seekp(0, std::ios::beg);
    char zero_buffer[METADATA_AREA_SIZE_BYTES] = {0}; 
    disk_file.write(zero_buffer, METADATA_AREA_SIZE_BYTES);
    if (!disk_file) {
        std::cerr << "Error: Could not write zeros to the entire metadata area." << std::endl;
        disk_file.close();
        return;
    }

    // 2. Süperbloku diske yaz (varsayılan değerlerle)
    Superblock sb; // Kurucu metodunda num_active_files = 0 olur
    disk_file.seekp(0, std::ios::beg); 
    disk_file.write(reinterpret_cast<const char*>(&sb), SUPERBLOCK_ACTUAL_SIZE);
    if (!disk_file) {
        std::cerr << "Error: Could not write initial superblock to metadata." << std::endl;
        disk_file.close();
        return;
    }

    // Bitmap alanı ve FileInfo dizisi alanı zaten yukarıdaki genel sıfırlama ile
    // (is_used = false vs. olacak şekilde) başlatılmış oldu.
    
    disk_file.close();
    std::cout << "Disk formatted successfully (new structure). Superblock, Bitmap, and FileInfo array initialized." << std::endl;
    std::cout << "  Calculated MAX_FILES: " << MAX_FILES_CALCULATED << std::endl;
    std::cout << "  Bitmap size: " << BITMAP_SIZE_BYTES << " bytes (for " << NUM_DATA_BLOCKS << " data blocks)." << std::endl;
    std::cout << "  Superblock size: " << SUPERBLOCK_ACTUAL_SIZE << " bytes." << std::endl;
    std::cout << "  FileInfo entry size: " << FILE_INFO_ENTRY_SIZE << " bytes." << std::endl;
    std::cout << "  Offset for Bitmap in metadata: " << BITMAP_START_OFFSET_IN_METADATA << " bytes." << std::endl;
    std::cout << "  Offset for FileInfo array in metadata: " << FILE_INFO_ARRAY_START_OFFSET_IN_METADATA << " bytes." << std::endl;
    std::cout << "  Usable space for FileInfo array: " << FILE_INFO_ARRAY_USABLE_SIZE << " bytes." << std::endl;


    fs_log(("Disk formatted with new metadata structure (superblock, bitmap, FileInfo array). Max files: " + std::to_string(MAX_FILES_CALCULATED)).c_str());
}

// Helper function to read all FileInfo entries from metadata
std::vector<FileInfo> read_all_file_info(Superblock& sb_out) { // Superblock'u referans olarak al
    std::vector<FileInfo> infos;
    // sb_out.num_active_files = 0; // Fonksiyonun başında sıfırlamak yerine okunan değeri ata

    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in);
    if (!disk_file) {
        std::cerr << "Error: Could not open disk file '" << DISK_FILENAME << "' to read metadata (read_all_file_info)." << std::endl;
        sb_out.num_active_files = -1; // Hata durumunu belirtmek için özel bir değer
        return infos; 
    }

    // 1. Superblock'u oku
    disk_file.seekg(0, std::ios::beg);
    disk_file.read(reinterpret_cast<char*>(&sb_out), SUPERBLOCK_ACTUAL_SIZE);
    if (!disk_file) {
        std::cerr << "Error: Could not read superblock from metadata (read_all_file_info)." << std::endl;
        disk_file.close();
        sb_out.num_active_files = -1; 
        return infos; 
    }

    // total_files_count artık sb_out.num_active_files olacak.

    // 2. FileInfo dizisini oku
    disk_file.seekg(FILE_INFO_ARRAY_START_OFFSET_IN_METADATA, std::ios::beg);
    infos.resize(MAX_FILES_CALCULATED); // Vektörü MAX_FILES_CALCULATED boyutunda hazırla
    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        disk_file.read(reinterpret_cast<char*>(&infos[i]), FILE_INFO_ENTRY_SIZE);
        if (!disk_file && !disk_file.eof()) { 
            std::cerr << "Error: Could not read FileInfo entry " << i << " from metadata (read_all_file_info)." << std::endl;
            infos.resize(i); 
            break; 
        }
        if(disk_file.eof() && i < MAX_FILES_CALCULATED -1) {
            std::cerr << "Warning: Reached EOF prematurely while reading FileInfo entries. Metadata might be incomplete." << std::endl;
            infos.resize(i + 1); 
            break;
        }
    }

    disk_file.close();
    return infos;
}

// Helper function to write a specific FileInfo entry to metadata by index
bool write_file_info_at_index(int index, const FileInfo& fi_to_write, Superblock& sb_to_update) {
    if (index < 0 || index >= MAX_FILES_CALCULATED) {
        std::cerr << "Error: Invalid index " << index << " for writing FileInfo. Max allowed: " << MAX_FILES_CALCULATED -1 << std::endl;
        return false;
    }

    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out); 
    if (!disk_file) {
        std::cerr << "Error: Could not open disk file '" << DISK_FILENAME << "' to write metadata (write_file_info_at_index)." << std::endl;
        return false;
    }

    // 1. FileInfo'yu yaz
    std::streampos pos = FILE_INFO_ARRAY_START_OFFSET_IN_METADATA + (index * FILE_INFO_ENTRY_SIZE);
    disk_file.seekp(pos, std::ios::beg);
    disk_file.write(reinterpret_cast<const char*>(&fi_to_write), FILE_INFO_ENTRY_SIZE);
    if (!disk_file) {
        std::cerr << "Error: Could not write FileInfo at index " << index << " (write_file_info_at_index)." << std::endl;
        disk_file.close();
        return false;
    }

    // 2. Superblock'u güncelle (num_active_files)
    disk_file.seekp(0, std::ios::beg);
    disk_file.write(reinterpret_cast<const char*>(&sb_to_update), SUPERBLOCK_ACTUAL_SIZE);
    if (!disk_file) {
        std::cerr << "Error: Could not update superblock in metadata (write_file_info_at_index)." << std::endl;
        disk_file.close();
        return false; 
    }

    disk_file.close();
    return true;
}


void fs_create(const char* filename) {
    ensure_disk_initialized(); 

    if (filename == nullptr || strlen(filename) == 0) {
        std::cerr << "Error: Filename cannot be empty." << std::endl;
        fs_log("fs_create failed: empty filename.");
        return;
    }

    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error: Filename '" << filename << "' is too long. Max length is " << MAX_FILENAME_LENGTH << "." << std::endl;
        fs_log("fs_create failed: filename too long.");
        return;
    }

    Superblock sb; // Önce superblock'u okumak için
    std::vector<FileInfo> all_files = read_all_file_info(sb); // sb referans ile güncellenecek

    if (sb.num_active_files == -1) { // read_all_file_info'dan hata geldi
        std::cerr << "Error: Could not read metadata to create file." << std::endl;
        fs_log("fs_create failed: metadata read error.");
        return;
    }


    // Dosya adının mevcut olup olmadığını kontrol et
    for (int i = 0; i < all_files.size(); ++i) { 
        if (all_files[i].is_used && strcmp(all_files[i].name, filename) == 0) {
            std::cerr << "Error: File '" << filename << "' already exists." << std::endl;
            fs_log("fs_create failed: file already exists.");
            return;
        }
    }

    if (sb.num_active_files >= MAX_FILES_CALCULATED) {
        std::cerr << "Error: Maximum number of files (" << MAX_FILES_CALCULATED << ") reached. Cannot create new file." << std::endl;
        fs_log("fs_create failed: maximum files reached.");
        return;
    }

    int empty_slot_index = -1;
    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) { 
        // all_files vektörü MAX_FILES_CALCULATED boyutunda olmalı
        // Eğer all_files[i] read_all_file_info içinde düzgün okunduysa ve kullanılmıyorsa
        if (i < all_files.size() && !all_files[i].is_used) {
            empty_slot_index = i;
            break;
        }
    }
    
    if (empty_slot_index == -1) {
        // Bu durum, eğer sb.num_active_files < MAX_FILES_CALCULATED ise, FileInfo'lar arasında
        // bir tutarsızlık olduğunu veya boş slotların 'is_used=true' olarak işaretlendiğini gösterir.
        // Ya da tüm slotlar gerçekten dolu.
        std::cerr << "Error: Could not find an empty slot for new file. Disk might be full or metadata inconsistent." << std::endl;
        fs_log("fs_create failed: no empty slot found.");
        return;
    }

    FileInfo new_file_info;
    strncpy(new_file_info.name, filename, MAX_FILENAME_LENGTH);
    new_file_info.name[MAX_FILENAME_LENGTH] = '\0'; 
    new_file_info.size = 0;                         
    new_file_info.creation_time = time(nullptr);    
    new_file_info.is_used = true;
    new_file_info.start_data_block_index = -1; // start_block -> start_data_block_index
    new_file_info.num_data_blocks_used = 0;   // Yeni eklendi, başlangıçta 0

    sb.num_active_files++; // Aktif dosya sayısını artır
    if (write_file_info_at_index(empty_slot_index, new_file_info, sb)) {
        std::cout << "File '" << filename << "' created successfully." << std::endl;
        fs_log(("File '" + std::string(filename) + "' created.").c_str());
    } else {
        std::cerr << "Error: Failed to write metadata for new file '" << filename << "'." << std::endl;
        fs_log(("fs_create failed: could not write metadata for file '" + std::string(filename) + "' கோட்பਾஸ்").c_str());
        // Hata durumunda num_active_files'ı geri almak gerekebilir, ama şimdilik basit tutalım.
    }
}

void fs_delete(const char* filename) {
    ensure_disk_initialized();
    fs_log(("fs_delete called for file: " + (filename ? std::string(filename) : "NULL")).c_str());

    if (filename == nullptr || strlen(filename) == 0) {
        std::cerr << "Error (fs_delete): Filename cannot be empty." << std::endl;
        fs_log("fs_delete failed: empty filename.");
        return;
    }

    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error (fs_delete): Filename '" << filename << "' is too long. Max length is " << MAX_FILENAME_LENGTH << "." << std::endl;
        fs_log("fs_delete failed: filename too long.");
        return;
    }

    Superblock sb;
    std::vector<FileInfo> all_files = read_all_file_info(sb);
    if (sb.num_active_files == -1) { 
        std::cerr << "Error (fs_delete): Could not read metadata to delete file '" << filename << "'." << std::endl;
        fs_log("fs_delete failed: metadata read error.");
        return;
    }

    int file_index = -1;
    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        if (all_files[i].is_used && strcmp(all_files[i].name, filename) == 0) {
            file_index = i;
            break;
        }
    }

    if (file_index == -1) {
        std::cerr << "Error (fs_delete): File '" << filename << "' not found." << std::endl;
        fs_log(("fs_delete failed: file not found - " + std::string(filename)).c_str());
        return;
    }

    FileInfo& file_to_delete = all_files[file_index];

    // 1. Veri bloklarını serbest bırak
    if (file_to_delete.start_data_block_index != -1 && file_to_delete.num_data_blocks_used > 0) {
        fs_log(("fs_delete: Freeing " + std::to_string(file_to_delete.num_data_blocks_used) + 
                " data blocks for file '" + std::string(filename) + 
                "' starting from block " + std::to_string(file_to_delete.start_data_block_index)).c_str());
        for (unsigned int i = 0; i < file_to_delete.num_data_blocks_used; ++i) {
            free_data_block(file_to_delete.start_data_block_index + i);
        }
    }

    // 2. FileInfo'yu güncelle (is_used = false ve diğer alanları sıfırla)
    file_to_delete.is_used = false;
    file_to_delete.name[0] = '\0'; // İsmi temizle (isteğe bağlı ama iyi pratik)
    file_to_delete.size = 0;
    file_to_delete.creation_time = 0; // Zamanı sıfırla
    file_to_delete.start_data_block_index = -1;
    file_to_delete.num_data_blocks_used = 0;

    // 3. Superblock'u güncelle (num_active_files azalt)
    if (sb.num_active_files > 0) { // Negatif olmasını engelle
        sb.num_active_files--;
    }

    // 4. Güncellenmiş FileInfo ve Superblock'u diske yaz
    if (write_file_info_at_index(file_index, file_to_delete, sb)) {
        std::cout << "File '" << filename << "' deleted successfully." << std::endl;
        fs_log(("File '" + std::string(filename) + "' deleted successfully. Active files: " + std::to_string(sb.num_active_files)).c_str());
    } else {
        std::cerr << "Error (fs_delete): Failed to write updated metadata to disk for file '" << filename << "'." << std::endl;
        fs_log(("fs_delete failed: metadata write error for file '" + std::string(filename) + "'. Active files might be inconsistent.").c_str());
        // Bu durumda num_active_files geri artırılmalı mı? Ya da bir tutarsızlık durumu loglanmalı.
        // Şimdilik sadece logluyoruz. İleri seviye bir dosya sistemi için daha sağlam hata yönetimi gerekebilir.
    }
}

int fs_write(const char* filename, const char* data, int size) {
    ensure_disk_initialized();

    if (filename == nullptr || strlen(filename) == 0) {
        std::cerr << "Error (fs_write): Filename cannot be empty." << std::endl;
        fs_log("fs_write failed: empty filename.");
        return -1; 
    }

    if (data == nullptr && size > 0) { 
        std::cerr << "Error (fs_write): Data is null but size is positive." << std::endl;
        fs_log("fs_write failed: null data with positive size.");
        return -2; 
    }

    if (size < 0) {
        std::cerr << "Error (fs_write): Size cannot be negative." << std::endl;
        fs_log("fs_write failed: negative size.");
        return -3; 
    }

    Superblock sb;
    std::vector<FileInfo> all_files = read_all_file_info(sb);
    if (sb.num_active_files == -1) { 
        std::cerr << "Error (fs_write): Could not read metadata to write file." << std::endl;
        fs_log("fs_write failed: metadata read error.");
        return -4; 
    }

    int file_index = -1;
    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        if (all_files[i].is_used && strcmp(all_files[i].name, filename) == 0) {
            file_index = i;
            break;
        }
    }

    if (file_index == -1) {
        std::cerr << "Error (fs_write): File '" << filename << "' not found." << std::endl;
        fs_log(("fs_write failed: file not found - " + std::string(filename)).c_str());
        return -5; 
    }

    FileInfo& current_file_info = all_files[file_index];

    // Eğer size 0 ise, dosyayı boşalt (truncate)
    if (size == 0) {
        fs_log(("fs_write called with size 0 for file: " + std::string(filename) + " (truncating). ").c_str());
        if (current_file_info.start_data_block_index != -1 && current_file_info.num_data_blocks_used > 0) {
            fs_log(("fs_write (truncate): Freeing old data blocks for file: " + std::string(filename) + 
                    ", starting from block " + std::to_string(current_file_info.start_data_block_index) + 
                    ", count " + std::to_string(current_file_info.num_data_blocks_used)).c_str());
            for (unsigned int i = 0; i < current_file_info.num_data_blocks_used; ++i) {
                free_data_block(current_file_info.start_data_block_index + i);
            }
        }
        current_file_info.size = 0;
        current_file_info.start_data_block_index = -1;
        current_file_info.num_data_blocks_used = 0;
        
        if (!write_file_info_at_index(file_index, current_file_info, sb)) {
            std::cerr << "Error (fs_write): Failed to update FileInfo on disk for truncate operation on '" << filename << "'." << std::endl;
            fs_log(("fs_write (truncate) failed: error updating FileInfo on disk for " + std::string(filename)).c_str());
            return -9; // Hata kodu: FileInfo yazma hatası
        }
        fs_log(("fs_write (truncate) completed successfully for file: " + std::string(filename)).c_str());
        return 0; // Başarıyla boşaltıldı.
    }

    // Buradan sonrası size > 0 durumu için.

    // 1. Gerekli blok sayısını hesapla.
    unsigned int num_blocks_needed = 0;
    if (size > 0) {
        num_blocks_needed = (size + BLOCK_SIZE_BYTES - 1) / BLOCK_SIZE_BYTES;
    }

    // 2. Mevcut blokları serbest bırak (truncate and write mantığı).
    //    Mevcut FileInfo yapısı (start_data_block_index, num_data_blocks_used)
    //    blokların ardışık olduğunu varsayıyor. Bu varsayıma göre serbest bırakma yapılıyor.
    if (current_file_info.start_data_block_index != -1 && current_file_info.num_data_blocks_used > 0) {
        fs_log(("fs_write: Freeing old data blocks for file: " + std::string(filename) + 
                ", starting from block " + std::to_string(current_file_info.start_data_block_index) + 
                ", count " + std::to_string(current_file_info.num_data_blocks_used)).c_str());
        for (unsigned int i = 0; i < current_file_info.num_data_blocks_used; ++i) {
            // ÖNEMLİ: Bu ardışık blok varsayımına dayanır.
            // Eğer bloklar ardışık değilse, FileInfo içinde blokların listesi tutulmalı ve o liste üzerinden gidilmeli.
            free_data_block(current_file_info.start_data_block_index + i); 
        }
        current_file_info.start_data_block_index = -1;
        current_file_info.num_data_blocks_used = 0;
        current_file_info.size = 0; // Boyutu da sıfırla, çünkü eski içerik gitti.
    }

    // Eğer size=0 ile çağrıldıysa, yeni blok tahsisine gerek yok, sadece FileInfo güncellenmeli.

    // std::vector<int> newly_allocated_blocks; // Artık kullanılmıyor
    current_file_info.start_data_block_index = -1; // Başlangıçta -1 yapalım
    current_file_info.num_data_blocks_used = 0;

    if (num_blocks_needed > 0) {
        fs_log(("fs_write: Attempting to allocate " + std::to_string(num_blocks_needed) + " contiguous blocks for file: " + std::string(filename)).c_str());
        int allocated_start_block = find_and_allocate_contiguous_data_blocks(num_blocks_needed);

        if (allocated_start_block == -1) {
            std::cerr << "Error (fs_write): Disk full or not enough contiguous space. Could not allocate " << num_blocks_needed
                      << " blocks for file '" << filename << "'." << std::endl;
            fs_log(("fs_write failed: disk full or no contiguous space for " + std::string(filename)).c_str());
            // Eski bloklar zaten serbest bırakılmıştı, FileInfo'da boyut 0, start_block -1 olarak kalmalı.
            // Bu durumu write_file_info_at_index ile diske yazmak iyi olabilir, böylece dosya effectively boşaltılmış olur.
            current_file_info.size = 0; // Emin olmak için
            current_file_info.start_data_block_index = -1;
            current_file_info.num_data_blocks_used = 0;
            write_file_info_at_index(file_index, current_file_info, sb); // Dosyayı boş olarak güncelle
            return -6; // Hata kodu: Disk dolu veya ardışık alan yok
        }
        
        current_file_info.start_data_block_index = allocated_start_block;
        current_file_info.num_data_blocks_used = num_blocks_needed;
        fs_log(("fs_write: Successfully allocated " + std::to_string(num_blocks_needed) + " blocks starting from " + std::to_string(allocated_start_block) + ".").c_str());
    }

    // 4. Veriyi bloklara yaz.
    if (current_file_info.num_data_blocks_used > 0 && current_file_info.start_data_block_index != -1) {
        std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out);
        if (!disk_file) {
            std::cerr << "Error (fs_write): Could not open disk file to write data for '" << filename << "'." << std::endl;
            // Tahsis edilen blokları geri serbest bırakmak GEREKİR çünkü veri yazılamadı.
            fs_log(("fs_write failed: could not open disk to write data for " + std::string(filename) + ". Freeing allocated blocks.").c_str());
            for (unsigned int i = 0; i < current_file_info.num_data_blocks_used; ++i) {
                free_data_block(current_file_info.start_data_block_index + i);
            }
            current_file_info.start_data_block_index = -1;
            current_file_info.num_data_blocks_used = 0;
            current_file_info.size = 0;
            // Hatalı durumu FileInfo'ya yansıtmak için diske yaz
            write_file_info_at_index(file_index, current_file_info, sb);
            return -7; // Hata kodu: Disk açılamadı
        }

        const char* data_ptr = data;
        int bytes_remaining_to_write = size;
        unsigned int actual_blocks_used_for_writing = 0;

        for (unsigned int i = 0; i < current_file_info.num_data_blocks_used; ++i) {
            if (bytes_remaining_to_write <= 0) {
                 // Bu durum, num_blocks_needed'in size'dan biraz fazla hesaplandığı (örn. size tam blok katı değilse)
                 // ve tüm verinin zaten yazıldığı anlamına gelir. Kalan tahsisli bloklara bir şey yazmaya gerek yok.
                 // Ancak, num_data_blocks_used hala tüm tahsis edilen blok sayısını yansıtmalı.
                break;
            }
            int block_idx_to_write = current_file_info.start_data_block_index + i;
            
            std::streampos write_pos = METADATA_AREA_SIZE_BYTES + (static_cast<std::streamoff>(block_idx_to_write) * BLOCK_SIZE_BYTES);
            disk_file.seekp(write_pos);

            int bytes_to_write_in_this_block = std::min(bytes_remaining_to_write, static_cast<int>(BLOCK_SIZE_BYTES));
            
            disk_file.write(data_ptr, bytes_to_write_in_this_block);
            if (!disk_file) {
                std::cerr << "Error (fs_write): Failed to write data to block " << block_idx_to_write << " for file '" << filename << "'." << std::endl;
                fs_log(("fs_write failed: error writing data to block " + std::to_string(block_idx_to_write) + " for " + std::string(filename) + ". Freeing blocks.").c_str());
                disk_file.close();
                // Hata! Tahsis edilen tüm blokları geri serbest bırak ve FileInfo'yu sıfırla.
                for (unsigned int k = 0; k < current_file_info.num_data_blocks_used; ++k) {
                    free_data_block(current_file_info.start_data_block_index + k);
                }
                current_file_info.start_data_block_index = -1;
                current_file_info.num_data_blocks_used = 0;
                current_file_info.size = 0;
                write_file_info_at_index(file_index, current_file_info, sb);
                return -8; // Hata kodu: Veri yazma hatası
            }

            data_ptr += bytes_to_write_in_this_block;
            bytes_remaining_to_write -= bytes_to_write_in_this_block;
            actual_blocks_used_for_writing++;
        }
        disk_file.close();

        // Eğer size > 0 iken hiç blok kullanılmadıysa (num_blocks_needed 0 idiyse ve sonra size > 0 olduysa bu mantıksız)
        // veya bir hata olduysa, actual_blocks_used_for_writing beklenen gibi olmayabilir.
        // Şimdilik, current_file_info.num_data_blocks_used = num_blocks_needed (tüm tahsis edilenler) olarak kalacak.
        // Gerçekten veri yazılan blok sayısı actual_blocks_used_for_writing.
        // Dosya boyutu (size) ve num_data_blocks_used (tahsis edilen blok sayısı) tutarlı olmalı.
        // Örneğin, 5 byte yazmak için 1 blok tahsis edilir (512 byte). num_data_blocks_used = 1, size = 5.

        fs_log(("fs_write: Successfully wrote " + std::to_string(size) + " bytes to file " + std::string(filename) + ". Blocks used for writing: " + std::to_string(actual_blocks_used_for_writing)).c_str());
    } else if (size > 0) { // num_blocks_needed > 0 ama blok tahsis edilemedi veya start_index -1 kaldı.
        // Bu durum yukarıda handle edildi (-6 dönüldü), buraya gelinmemeli.
        // Ancak bir güvenlik önlemi olarak loglayalım.
        fs_log(("fs_write: Warning - size is positive (" + std::to_string(size) + ") but no blocks were allocated or available for file " + std::string(filename)).c_str());
        // FileInfo zaten 0/ -1 olarak ayarlanmış olmalı.
    }


    // 5. FileInfo'yu güncelle (size, start_data_block_index, num_data_blocks_used).
    current_file_info.size = size; // Yazılan toplam boyut.
    // current_file_info.start_data_block_index ve num_data_blocks_used zaten yukarıda ayarlandı.
    // Eğer size 0 ise, num_blocks_needed 0 olmalı, bu durumda start_data_block_index=-1, num_data_blocks_used=0 olur.
    if (size == 0) {
        current_file_info.start_data_block_index = -1;
        current_file_info.num_data_blocks_used = 0;
    }
    // Eğer size > 0 ama num_blocks_needed = 0 ise (çok küçük dosyalar için teorik bir durum, BLOCK_SIZE'dan küçükse 1 blok gerekir)
    // bu durum num_blocks_needed hesaplamasında (size + BLOCK_SIZE_BYTES - 1) / BLOCK_SIZE_BYTES ile çözülür.
    // Yani size > 0 ise num_blocks_needed >= 1 olur her zaman.

    // 6. Güncellenmiş FileInfo'yu diske yaz (write_file_info_at_index).
    if (!write_file_info_at_index(file_index, current_file_info, sb)) {
        std::cerr << "Error (fs_write): Failed to update FileInfo on disk for '" << filename << "'." << std::endl;
        fs_log(("fs_write failed: error updating FileInfo on disk for " + std::string(filename) + ". Freeing allocated blocks.").c_str());
        // Veri yazılmış olabilir ama metadata güncellenmedi. Tahsis edilen blokları geri serbest bırak.
        if (current_file_info.start_data_block_index != -1 && current_file_info.num_data_blocks_used > 0) {
            for (unsigned int i = 0; i < current_file_info.num_data_blocks_used; ++i) {
                free_data_block(current_file_info.start_data_block_index + i);
            }
        }
        return -9; // Hata kodu: FileInfo yazma hatası
    }

    fs_log(("fs_write completed successfully for file: " + std::string(filename) + ", new size: " + std::to_string(current_file_info.size) + ", blocks used: " + std::to_string(current_file_info.num_data_blocks_used) + ", start_block: " + std::to_string(current_file_info.start_data_block_index)).c_str());
    return size; // Başarıyla yazılan byte sayısını döndür.
}

void fs_read(const char* filename, int offset, int size, char* buffer) {
    ensure_disk_initialized();
    fs_log(("fs_read called for file: " + (filename ? std::string(filename) : "NULL") + 
            ", offset: " + std::to_string(offset) + 
            ", size: " + std::to_string(size)).c_str());

    if (filename == nullptr || strlen(filename) == 0) {
        std::cerr << "Error (fs_read): Filename cannot be empty." << std::endl;
        fs_log("fs_read failed: empty filename.");
        // Hata durumunda buffer'ı temizlemek iyi bir pratik olabilir, ancak
        // çağıran tarafın buffer'ı nasıl yönettiğine bağlı.
        // Şimdilik sadece return ediyoruz.
        return;
    }

    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error (fs_read): Filename '" << filename << "' is too long. Max length is " << MAX_FILENAME_LENGTH << "." << std::endl;
        fs_log("fs_read failed: filename too long.");
        return;
    }

    if (offset < 0) {
        std::cerr << "Error (fs_read): Offset cannot be negative (" << offset << ")." << std::endl;
        fs_log("fs_read failed: negative offset.");
        return;
    }

    if (size < 0) {
        std::cerr << "Error (fs_read): Size cannot be negative (" << size << ")." << std::endl;
        fs_log("fs_read failed: negative size.");
        return;
    }

    if (size == 0) {
        fs_log("fs_read: Requested size is 0. Nothing to read.");
        if (buffer != nullptr) buffer[0] = '\0'; // İsteğe bağlı: buffer'ı boş string yap
        return; // Okunacak bir şey yok
    }

    if (buffer == nullptr) { // size > 0 ise buffer null olmamalı
        std::cerr << "Error (fs_read): Buffer is null, cannot read data." << std::endl;
        fs_log("fs_read failed: null buffer with positive size.");
        return;
    }

    Superblock sb;
    std::vector<FileInfo> all_files = read_all_file_info(sb);
    if (sb.num_active_files == -1) { 
        std::cerr << "Error (fs_read): Could not read metadata to read file '" << filename << "'." << std::endl;
        fs_log("fs_read failed: metadata read error.");
        buffer[0] = '\0'; // Hata durumunda buffer'ı temizle
        return;
    }

    int file_index = -1;
    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        if (all_files[i].is_used && strcmp(all_files[i].name, filename) == 0) {
            file_index = i;
            break;
        }
    }

    if (file_index == -1) {
        std::cerr << "Error (fs_read): File '" << filename << "' not found." << std::endl;
        fs_log(("fs_read failed: file not found - " + std::string(filename)).c_str());
        buffer[0] = '\0';
        return;
    }

    const FileInfo& current_file_info = all_files[file_index];

    if (offset >= current_file_info.size && current_file_info.size == 0) { // Dosya boşsa ve offset 0 ise sorun yok, 0 byte okunur
         fs_log(("fs_read: File '" + std::string(filename) + "' is empty and offset is 0. Reading 0 bytes.").c_str());
         buffer[0] = '\0';
         return;
    }

    if (offset >= current_file_info.size ) { // Dosya boş değilken offset dosya boyutunun dışında
        std::cerr << "Error (fs_read): Offset (" << offset << ") is beyond file size (" << current_file_info.size << ") for file '" << filename << "'." << std::endl;
        fs_log("fs_read failed: offset out of bounds.");
        buffer[0] = '\0';
        return;
    }

    if (current_file_info.start_data_block_index == -1 || current_file_info.num_data_blocks_used == 0) {
        // Bu durum dosya boyutu > 0 ise tutarsızlık anlamına gelir, ama FileInfo doğruysa (size=0)
        // ve offset=0 ise buraya offset kontrolünden önce takılmaması lazım.
        // Eğer offset=0, size>0 ve dosya boşsa (start_data_block_index = -1), bu da bir sorun.
        std::cerr << "Error (fs_read): File '" << filename << "' has no data blocks allocated but size is " << current_file_info.size << "." << std::endl;
        fs_log("fs_read failed: file has no data blocks but reports size > 0 or attempting to read from empty file.");
        buffer[0] = '\0';
        return;
    }

    // Okunacak gerçek byte sayısını hesapla
    int bytes_to_actually_read = size;
    if (offset + size > current_file_info.size) {
        bytes_to_actually_read = current_file_info.size - offset;
    }

    if (bytes_to_actually_read <= 0) {
        fs_log(("fs_read: Calculated bytes to read is " + std::to_string(bytes_to_actually_read) + ". Nothing to read for file '" + std::string(filename) + "'.").c_str());
        buffer[0] = '\0';
        return;
    }

    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in);
    if (!disk_file) {
        std::cerr << "Error (fs_read): Could not open disk file '" << DISK_FILENAME << "' to read data for '" << filename << "'." << std::endl;
        fs_log("fs_read failed: could not open disk file for reading data.");
        buffer[0] = '\0';
        return;
    }

    int bytes_read_so_far = 0;
    char* current_buffer_pos = buffer;
    unsigned int current_file_offset = static_cast<unsigned int>(offset);

    // Dosyanın veri blokları ardışık olduğu için okuma daha basit.
    // İlk okunacak blok ve o blok içindeki offset:
    unsigned int first_block_offset_in_file = current_file_offset / BLOCK_SIZE_BYTES;
    unsigned int first_byte_offset_in_first_block = current_file_offset % BLOCK_SIZE_BYTES;

    // Kaç tane bloğa yayılıyor okuma işlemi?
    // Bu hesaplama, blokların ardışık olduğu varsayımına dayanır.
    // unsigned int last_byte_offset_in_file = current_file_offset + bytes_to_actually_read - 1;
    // unsigned int last_block_offset_in_file = last_byte_offset_in_file / BLOCK_SIZE_BYTES;
    // unsigned int num_blocks_to_span = last_block_offset_in_file - first_block_offset_in_file + 1;

    // Daha basit bir döngü kuralım:
    unsigned int data_block_cursor_in_file = first_block_offset_in_file;
    unsigned int internal_block_offset = first_byte_offset_in_first_block;

    while (bytes_read_so_far < bytes_to_actually_read && data_block_cursor_in_file < current_file_info.num_data_blocks_used) {
        unsigned int actual_disk_block_index = current_file_info.start_data_block_index + data_block_cursor_in_file;
        
        std::streampos disk_read_pos = METADATA_AREA_SIZE_BYTES + 
                                       (static_cast<std::streamoff>(actual_disk_block_index) * BLOCK_SIZE_BYTES) + 
                                       internal_block_offset;
        
        disk_file.seekg(disk_read_pos);
        if (!disk_file) {
            std::cerr << "Error (fs_read): Failed to seek to position " << disk_read_pos 
                      << " in block " << actual_disk_block_index << " for file '" << filename << "'." << std::endl;
            fs_log("fs_read failed: seekg error during data read.");
            buffer[0] = '\0';
            disk_file.close();
            return;
        }

        unsigned int bytes_to_read_from_this_disk_block = std::min(
            static_cast<unsigned int>(BLOCK_SIZE_BYTES) - internal_block_offset, 
            static_cast<unsigned int>(bytes_to_actually_read - bytes_read_so_far)
        );

        disk_file.read(current_buffer_pos, bytes_to_read_from_this_disk_block);
        std::streamsize gcount = disk_file.gcount();

        if (!disk_file && gcount != static_cast<std::streamsize>(bytes_to_read_from_this_disk_block)) {
            // EOF normal olabilir eğer dosyanın sonuna ulaşıldıysa ve beklenen kadar okunamadıysa,
            // ama bytes_to_actually_read zaten bunu hesaba katmalı.
            // Yine de, gcount kontrolü önemli.
            std::cerr << "Error (fs_read): Failed to read " << bytes_to_read_from_this_disk_block 
                      << " bytes from block " << actual_disk_block_index << " for file '" << filename 
                      << "'. Read " << gcount << " bytes." << std::endl;
            fs_log("fs_read failed: read error or unexpected EOF during data read.");
            buffer[bytes_read_so_far] = '\0'; // O ana kadar okunanı null terminate et
            disk_file.close();
            return;
        }
        
        current_buffer_pos += gcount;
        bytes_read_so_far += gcount;
        
        data_block_cursor_in_file++; // Bir sonraki dosya bloğuna geç
        internal_block_offset = 0; // Sonraki bloklar için offset her zaman 0'dan başlar
    }

    disk_file.close();
    buffer[bytes_read_so_far] = '\0'; // Okunan veriyi null-terminate et.

    fs_log(("fs_read: Successfully read " + std::to_string(bytes_read_so_far) + 
            " bytes from file '" + std::string(filename) + 
            "' (requested: " + std::to_string(size) + ", offset: " + std::to_string(offset) + ").").c_str());

}

void fs_cat(const char* filename) {
    ensure_disk_initialized();
    fs_log(("fs_cat called for file: " + (filename ? std::string(filename) : "NULL")).c_str());

    if (filename == nullptr || strlen(filename) == 0) {
        std::cerr << "Error (fs_cat): Filename cannot be empty." << std::endl;
        fs_log("fs_cat failed: empty filename.");
        return;
    }

    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error (fs_cat): Filename '" << filename << "' is too long. Max length is " << MAX_FILENAME_LENGTH << "." << std::endl;
        fs_log("fs_cat failed: filename too long.");
        return;
    }

    if (!fs_exists(filename)) {
        std::cerr << "Error (fs_cat): File '" << filename << "' not found." << std::endl;
        fs_log(("fs_cat failed: file not found - " + std::string(filename)).c_str());
        return;
    }

    int file_size = fs_size(filename);
    if (file_size < 0) {
        // fs_size zaten hata mesajını loglamış olmalı (örn: metadata okuma hatası veya dosya bulunamadı - ama exists kontrolü yaptık)
        std::cerr << "Error (fs_cat): Could not determine size for file '" << filename << "' (fs_size returned " << file_size << ")." << std::endl;
        fs_log(("fs_cat failed: could not get file size or file error for - " + std::string(filename)).c_str());
        return;
    }

    if (file_size == 0) {
        fs_log(("fs_cat: File '" + std::string(filename) + "' is empty. Nothing to display.").c_str());
        // İsteğe bağlı: std::cout << "<File '" << filename << "' is empty>" << std::endl;
        return;
    }

    char* buffer = new (std::nothrow) char[file_size + 1]; // +1 null terminator için
    if (buffer == nullptr) {
        std::cerr << "Error (fs_cat): Failed to allocate memory to read file '" << filename << "'." << std::endl;
        fs_log("fs_cat failed: memory allocation error.");
        return;
    }

    fs_read(filename, 0, file_size, buffer);
    // fs_read kendi loglamasını yapar ve buffer'ı null-terminate eder.
    // fs_read içindeki bir hata durumunda buffer boş ("\0") olabilir.

    // fs_read bir hata nedeniyle buffer'ı boşaltmışsa veya gerçekten boş okunmuşsa,
    // bunu kontrol edebiliriz, ancak fs_read zaten cerr'e yazdı.
    // En kötü ihtimalle boş bir şey yazdırırız, bu da kabul edilebilir.
    std::cout << buffer; // Buffer zaten null-terminated olduğu için doğrudan yazdırılabilir.
    // std::cout << std::endl; // Genellikle cat komutları sonda newline eklemez, dosya içeriğine bağlıdır.
    // Eğer dosya sonunda newline yoksa, bir sonraki konsol satırı yapışık başlayabilir.
    // Kullanım senaryosuna göre buraya bir newline eklenebilir.

    delete[] buffer;
    fs_log(("fs_cat: Successfully displayed content of file '" + std::string(filename) + "'.").c_str());
}

void fs_ls() {
    ensure_disk_initialized();
    std::cout << "\n--- Listing Files ---" << std::endl;

    Superblock sb;
    std::vector<FileInfo> all_files = read_all_file_info(sb);

    if (sb.num_active_files == -1) { // read_all_file_info'dan hata
        std::cerr << "Error: Could not read metadata for ls, disk might not be properly initialized." << std::endl;
        fs_log("fs_ls failed: could not read metadata.");
        return;
    }
    
    if (sb.num_active_files == 0) {
        std::cout << "No files found on the disk." << std::endl;
    } else {
        std::cout << "Files on disk (Active: " << sb.num_active_files << " / Max Slots: " << MAX_FILES_CALCULATED << "):" << std::endl;
        std::cout << "-------------------------------------------------------------------------------" << std::endl;
        std::cout << "Name			Size (B)	StartBlk	NumBlks	Creation Time" << std::endl; 
        std::cout << "-------------------------------------------------------------------------------" << std::endl;
        
        int listed_count = 0;
        for (const auto& fi : all_files) {
            if (fi.is_used) {
                char time_buffer[30];
                time_t creation_t = fi.creation_time; 
                strncpy(time_buffer, ctime(&creation_t), 29);
                time_buffer[29] = '\0'; 
                if (time_buffer[strlen(time_buffer) - 1] == '\n') {
                    time_buffer[strlen(time_buffer) - 1] = '\0';
                }
                
                std::cout << fi.name 
                          << "		" << fi.size 
                          << "		" << fi.start_data_block_index
                          << "		" << fi.num_data_blocks_used
                          << "		" << time_buffer << std::endl;
                listed_count++;
            }
        }
        if (listed_count == 0 && sb.num_active_files > 0) {
            std::cout << "No active files found in FileInfo array, but superblock indicates " << sb.num_active_files << " active files (potential inconsistency)." << std::endl;
        }
         if (listed_count != sb.num_active_files) {
            std::cout << "Warning: Listed " << listed_count << " files, but superblock reports " << sb.num_active_files << " active files. (Potential inconsistency)." << std::endl;
        }
        std::cout << "-------------------------------------------------------------------------------" << std::endl;
    }
    fs_log("fs_ls executed.");
}

void fs_rename(const char* old_name, const char* new_name) {
    ensure_disk_initialized();
    fs_log(("fs_rename called for file: '" + (old_name ? std::string(old_name) : "NULL") +
            "' to new name: '" + (new_name ? std::string(new_name) : "NULL") + "'.").c_str());

    if (old_name == nullptr || strlen(old_name) == 0) {
        std::cerr << "Error (fs_rename): Old filename cannot be empty." << std::endl;
        fs_log("fs_rename failed: old_name is empty.");
        return;
    }
    if (new_name == nullptr || strlen(new_name) == 0) {
        std::cerr << "Error (fs_rename): New filename cannot be empty." << std::endl;
        fs_log("fs_rename failed: new_name is empty.");
        return;
    }

    if (strlen(old_name) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error (fs_rename): Old filename '" << old_name << "' is too long. Max length is " << MAX_FILENAME_LENGTH << "." << std::endl;
        fs_log("fs_rename failed: old_name too long.");
        return;
    }
    if (strlen(new_name) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error (fs_rename): New filename '" << new_name << "' is too long. Max length is " << MAX_FILENAME_LENGTH << "." << std::endl;
        fs_log("fs_rename failed: new_name too long.");
        return;
    }

    if (strcmp(old_name, new_name) == 0) {
        std::cout << "Info (fs_rename): Old name and new name are the same ('" << old_name << "'). No action taken." << std::endl;
        fs_log("fs_rename: old and new names are identical. No change.");
        return;
    }

    Superblock sb;
    std::vector<FileInfo> all_files = read_all_file_info(sb);
    if (sb.num_active_files == -1) { 
        std::cerr << "Error (fs_rename): Could not read metadata." << std::endl;
        fs_log("fs_rename failed: metadata read error.");
        return;
    }

    int old_file_index = -1;
    bool new_name_exists = false;

    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        if (all_files[i].is_used) {
            if (strcmp(all_files[i].name, old_name) == 0) {
                old_file_index = i;
            }
            if (strcmp(all_files[i].name, new_name) == 0) {
                new_name_exists = true;
            }
        }
    }

    if (old_file_index == -1) {
        std::cerr << "Error (fs_rename): Source file '" << old_name << "' not found." << std::endl;
        fs_log(("fs_rename failed: source file '" + std::string(old_name) + "' not found.").c_str());
        return;
    }

    if (new_name_exists) {
        std::cerr << "Error (fs_rename): Target filename '" << new_name << "' already exists." << std::endl;
        fs_log(("fs_rename failed: target filename '" + std::string(new_name) + "' already exists.").c_str());
        return;
    }

    // FileInfo'da ismi güncelle
    strncpy(all_files[old_file_index].name, new_name, MAX_FILENAME_LENGTH);
    all_files[old_file_index].name[MAX_FILENAME_LENGTH] = '\0'; // Null-terminate

    // Güncellenmiş FileInfo'yu ve (değişmemiş) Superblock'u diske yaz
    if (write_file_info_at_index(old_file_index, all_files[old_file_index], sb)) {
        std::cout << "File '" << old_name << "' renamed to '" << new_name << "' successfully." << std::endl;
        fs_log(("File '" + std::string(old_name) + "' renamed to '" + std::string(new_name) + "' successfully.").c_str());
    } else {
        std::cerr << "Error (fs_rename): Failed to write updated metadata to disk for renaming '" << old_name << "' to '" << new_name << "'." << std::endl;
        fs_log(("fs_rename failed: metadata write error while renaming '" + std::string(old_name) + "' to '" + std::string(new_name) + "'.")  .c_str());
    }
}

bool fs_exists(const char* filename) {
    ensure_disk_initialized();

    if (filename == nullptr || strlen(filename) == 0) {
        fs_log("fs_exists check for empty filename -> false.");
        return false;
    }

    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        fs_log(("fs_exists check for too long filename ('" + std::string(filename) + "') -> false.").c_str());
        return false;
    }

    Superblock sb; 
    std::vector<FileInfo> all_files = read_all_file_info(sb);

    if (sb.num_active_files == -1){ 
        fs_log("fs_exists failed: could not read metadata.");
        return false; 
    }

    for (const auto& fi : all_files) {
        if (fi.is_used && strcmp(fi.name, filename) == 0) {
            fs_log(("fs_exists check for '" + std::string(filename) + "' -> true.").c_str());
            return true;
        }
    }

    fs_log(("fs_exists check for '" + std::string(filename) + "' -> false.").c_str());
    return false;
}

int fs_size(const char* filename) {
    ensure_disk_initialized();

    if (filename == nullptr || strlen(filename) == 0) {
        fs_log("fs_size check for empty filename -> returning -1.");
        return -1; 
    }

    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        fs_log(("fs_size check for too long filename ('" + std::string(filename) + "') -> returning -1.").c_str());
        return -1; 
    }

    Superblock sb;
    std::vector<FileInfo> all_files = read_all_file_info(sb);

    if (sb.num_active_files == -1){
        fs_log("fs_size failed: could not read metadata. Returning -1.");
        return -1; 
    }

    for (const auto& fi : all_files) {
        if (fi.is_used && strcmp(fi.name, filename) == 0) {
            fs_log(("fs_size for '" + std::string(filename) + "' -> " + std::to_string(fi.size) + ".").c_str());
            return fi.size;
        }
    }

    fs_log(("fs_size: File '" + std::string(filename) + "' not found. Returning -1.").c_str());
    return -1; // Dosya bulunamadı
}

void fs_append(const char* filename, const char* data, int size) {
    ensure_disk_initialized();
    fs_log(("fs_append called for file: " + (filename ? std::string(filename) : "NULL") + 
            ", data_size: " + std::to_string(size)).c_str());

    if (filename == nullptr || strlen(filename) == 0) {
        std::cerr << "Error (fs_append): Filename cannot be empty." << std::endl;
        fs_log("fs_append failed: empty filename.");
        return;
    }

    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error (fs_append): Filename \'" << filename << "\' is too long. Max length is " << MAX_FILENAME_LENGTH << "." << std::endl;
        fs_log("fs_append failed: filename too long.");
        return;
    }

    if (data == nullptr && size > 0) {
        std::cerr << "Error (fs_append): Data is null but size is positive." << std::endl;
        fs_log("fs_append failed: null data with positive size.");
        return;
    }

    if (size < 0) {
        std::cerr << "Error (fs_append): Size to append cannot be negative." << std::endl;
        fs_log("fs_append failed: negative size.");
        return;
    }

    if (size == 0) {
        fs_log("fs_append: Size to append is 0. No changes made.");
        return; // Appending 0 bytes doesn't change the file.
    }

    if (!fs_exists(filename)) {
        std::cerr << "Error (fs_append): File \'" << filename << "\' not found. Cannot append." << std::endl;
        fs_log(("fs_append failed: file not found - " + std::string(filename)).c_str());
        return;
    }

    int old_size = fs_size(filename);
    if (old_size < 0) {
        // fs_size would have logged an error
        std::cerr << "Error (fs_append): Could not determine current size of file \'" << filename << "\'." << std::endl;
        fs_log(("fs_append failed: could not get old size of file - " + std::string(filename)).c_str());
        return;
    }

    int new_size = old_size + size;

    // Allocate buffer for combined data
    char* combined_data = new (std::nothrow) char[new_size];
    if (combined_data == nullptr) {
        std::cerr << "Error (fs_append): Failed to allocate memory for combined data." << std::endl;
        fs_log("fs_append failed: memory allocation error for combined_data.");
        return;
    }

    // Read old content
    if (old_size > 0) {
        fs_read(filename, 0, old_size, combined_data);
        // Check if fs_read was successful by checking if the buffer was filled as expected.
        // A more robust check might involve fs_read returning a status or bytes read.
        // For now, we assume fs_read populates combined_data correctly if old_size > 0.
        // If fs_read had an internal error, it would print to cerr and log.
    }

    // Append new data
    memcpy(combined_data + old_size, data, size);

    // Write the combined data back using fs_write
    fs_log(("fs_append: Attempting to write " + std::to_string(new_size) + " bytes (old: " + std::to_string(old_size) + ", new_to_add: " + std::to_string(size) + ") to file \'" + std::string(filename) + "\'.").c_str());
    int bytes_written = fs_write(filename, combined_data, new_size);

    delete[] combined_data;

    if (bytes_written == new_size) {
        std::cout << "Data appended successfully to file \'" << filename << "\'. New size: " << new_size << " bytes." << std::endl;
        fs_log(("fs_append completed successfully for file: " + std::string(filename) + ". New total size: " + std::to_string(new_size)).c_str());
    } else {
        // fs_write would have logged specific errors
        std::cerr << "Error (fs_append): Failed to write appended data to file \'" << filename << "\'. fs_write returned " << bytes_written << "." << std::endl;
        fs_log(("fs_append failed: fs_write error for file - " + std::string(filename) + ". Expected to write " + std::to_string(new_size) + " but wrote " + std::to_string(bytes_written)).c_str());
        // Potentially, the file might be in an inconsistent state or truncated if fs_write failed partially.
    }
}

void fs_truncate(const char* filename, int new_size) {
    ensure_disk_initialized();
    fs_log(("fs_truncate called for file: " + (filename ? std::string(filename) : "NULL") + 
            ", new_size: " + std::to_string(new_size)).c_str());

    if (filename == nullptr || strlen(filename) == 0) {
        std::cerr << "Error (fs_truncate): Filename cannot be empty." << std::endl;
        fs_log("fs_truncate failed: empty filename.");
        return;
    }

    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error (fs_truncate): Filename \'" << filename << "\' is too long. Max length is " << MAX_FILENAME_LENGTH << "." << std::endl;
        fs_log("fs_truncate failed: filename too long.");
        return;
    }

    if (new_size < 0) {
        std::cerr << "Error (fs_truncate): New size cannot be negative." << std::endl;
        fs_log("fs_truncate failed: negative new_size.");
        return;
    }

    if (!fs_exists(filename)) {
        std::cerr << "Error (fs_truncate): File \'" << filename << "\' not found. Cannot truncate." << std::endl;
        fs_log(("fs_truncate failed: file not found - " + std::string(filename)).c_str());
        return;
    }

    int current_size = fs_size(filename);
    if (current_size < 0) {
        std::cerr << "Error (fs_truncate): Could not determine current size of file \'" << filename << "\'." << std::endl;
        fs_log(("fs_truncate failed: could not get current_size of file - " + std::string(filename)).c_str());
        return;
    }

    if (new_size == current_size) {
        std::cout << "Info (fs_truncate): New size is the same as current size. No changes made to file \'" << filename << "\'." << std::endl;
        fs_log(("fs_truncate: new_size is same as current. No change for " + std::string(filename)).c_str());
        return;
    }

    if (new_size == 0) {
        fs_log(("fs_truncate: New size is 0. Emptying file \'" + std::string(filename) + "\'.").c_str());
        int written = fs_write(filename, "", 0);
        if (written == 0) {
            std::cout << "File \'" << filename << "\' truncated to 0 bytes successfully." << std::endl;
            fs_log(("fs_truncate: Successfully emptied file " + std::string(filename)).c_str());
        } else {
            std::cerr << "Error (fs_truncate): Failed to empty file \'" << filename << "\'. fs_write returned " << written << std::endl;
            fs_log(("fs_truncate: Error emptying file " + std::string(filename) + ". fs_write returned: " + std::to_string(written)).c_str());
        }
        return;
    }

    // new_size > 0 durumu
    if (new_size < current_size) { // Küçültme
        fs_log(("fs_truncate: Truncating file \'" + std::string(filename) + "\' from " + std::to_string(current_size) + " to " + std::to_string(new_size) + " bytes.").c_str());
        char* buffer = new (std::nothrow) char[new_size];
        if (!buffer) {
            std::cerr << "Error (fs_truncate): Memory allocation failed for shrink buffer." << std::endl;
            fs_log("fs_truncate failed: memory allocation for shrink buffer.");
            return;
        }
        fs_read(filename, 0, new_size, buffer); // İlk new_size kadarını oku
        // fs_read hata durumunda buffer'ı boşaltabilir veya loglayabilir.
        // Burada fs_read'in başarılı olduğunu varsayıyoruz, çünkü kritik bir hata yoksa devam eder.
        
        int written = fs_write(filename, buffer, new_size);
        delete[] buffer;

        if (written == new_size) {
            std::cout << "File \'" << filename << "\' truncated to " << new_size << " bytes successfully." << std::endl;
            fs_log(("fs_truncate: Successfully truncated " + std::string(filename) + " to " + std::to_string(new_size)).c_str());
        } else {
            std::cerr << "Error (fs_truncate): Failed to write truncated data to file \'" << filename << "\'. fs_write returned " << written << std::endl;
            fs_log(("fs_truncate: Error truncating (shrink) " + std::string(filename) + ". fs_write returned: " + std::to_string(written)).c_str());
        }
    } else { // new_size > current_size (Büyütme)
        fs_log(("fs_truncate: Expanding file \'" + std::string(filename) + "\' from " + std::to_string(current_size) + " to " + std::to_string(new_size) + " bytes.").c_str());
        char* buffer = new (std::nothrow) char[new_size];
        if (!buffer) {
            std::cerr << "Error (fs_truncate): Memory allocation failed for expand buffer." << std::endl;
            fs_log("fs_truncate failed: memory allocation for expand buffer.");
            return;
        }

        if (current_size > 0) {
            fs_read(filename, 0, current_size, buffer); // Mevcut içeriği buffer'ın başına oku
        }
        // Kalan kısmı sıfırla (new_size - current_size kadar)
        memset(buffer + current_size, 0, new_size - current_size);

        int written = fs_write(filename, buffer, new_size);
        delete[] buffer;

        if (written == new_size) {
            std::cout << "File \'" << filename << "\' expanded to " << new_size << " bytes successfully." << std::endl;
            fs_log(("fs_truncate: Successfully expanded " + std::string(filename) + " to " + std::to_string(new_size)).c_str());
        } else {
            std::cerr << "Error (fs_truncate): Failed to write expanded data to file \'" << filename << "\'. fs_write returned " << written << std::endl;
            fs_log(("fs_truncate: Error expanding " + std::string(filename) + ". fs_write returned: " + std::to_string(written)).c_str());
        }
    }
}

void fs_copy(const char* src_filename, const char* dest_filename) {
    ensure_disk_initialized();
    fs_log(("fs_copy called from: \'" + (src_filename ? std::string(src_filename) : "NULL") +
            "\' to: \'" + (dest_filename ? std::string(dest_filename) : "NULL") + "\'.").c_str());

    if (src_filename == nullptr || strlen(src_filename) == 0) {
        std::cerr << "Error (fs_copy): Source filename cannot be empty." << std::endl;
        fs_log("fs_copy failed: empty source filename.");
        return;
    }
    if (dest_filename == nullptr || strlen(dest_filename) == 0) {
        std::cerr << "Error (fs_copy): Destination filename cannot be empty." << std::endl;
        fs_log("fs_copy failed: empty destination filename.");
        return;
    }

    if (strlen(src_filename) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error (fs_copy): Source filename \'" << src_filename << "\' is too long." << std::endl;
        fs_log("fs_copy failed: source filename too long.");
        return;
    }
    if (strlen(dest_filename) > MAX_FILENAME_LENGTH) {
        std::cerr << "Error (fs_copy): Destination filename \'" << dest_filename << "\' is too long." << std::endl;
        fs_log("fs_copy failed: destination filename too long.");
        return;
    }

    if (strcmp(src_filename, dest_filename) == 0) {
        std::cerr << "Error (fs_copy): Source and destination filenames cannot be the same." << std::endl;
        fs_log("fs_copy failed: source and destination are the same.");
        return;
    }

    if (!fs_exists(src_filename)) {
        std::cerr << "Error (fs_copy): Source file \'" << src_filename << "\' not found." << std::endl;
        fs_log(("fs_copy failed: source file not found - " + std::string(src_filename)).c_str());
        return;
    }

    if (fs_exists(dest_filename)) {
        std::cerr << "Error (fs_copy): Destination file \'" << dest_filename << "\' already exists." << std::endl;
        fs_log(("fs_copy failed: destination file already exists - " + std::string(dest_filename)).c_str());
        return;
    }

    int src_size = fs_size(src_filename);
    if (src_size < 0) {
        std::cerr << "Error (fs_copy): Could not determine size of source file \'" << src_filename << "\'." << std::endl;
        fs_log(("fs_copy failed: could not get size of source file - " + std::string(src_filename)).c_str());
        return;
    }

    // Kaynak dosya boşsa, sadece hedef dosyayı oluştur ve çık.
    if (src_size == 0) {
        fs_create(dest_filename); // fs_create hata kontrolünü yapar
        // fs_create başarılıysa log atar, değilse hata mesajı verir.
        // Ekstra kontrol: fs_exists(dest_filename) ile dosyanın oluştuğunu teyit et.
        if (fs_exists(dest_filename)) {
            std::cout << "File \'" << src_filename << "\' (empty) copied to \'" << dest_filename << "\' successfully." << std::endl;
            fs_log(("fs_copy: Copied empty file \'" + std::string(src_filename) + "\' to \'" + std::string(dest_filename) + "\'.").c_str());
        } else {
            std::cerr << "Error (fs_copy): Failed to create empty destination file \'" << dest_filename << "\' after attempting to copy empty source." << std::endl;
            fs_log(("fs_copy failed: error creating empty destination file \'" + std::string(dest_filename) + "\'.").c_str());
        }
        return;
    }

    char* buffer = new (std::nothrow) char[src_size];
    if (buffer == nullptr) {
        std::cerr << "Error (fs_copy): Failed to allocate memory to read source file \'" << src_filename << "\'." << std::endl;
        fs_log("fs_copy failed: memory allocation error for reading source.");
        return;
    }

    fs_read(src_filename, 0, src_size, buffer);
    // fs_read kendi loglamasını yapar. Başarısız olursa buffer boş olabilir veya hata mesajı basılmıştır.
    // Burada fs_read'in içeriği doğru okuduğunu varsayalım.

    fs_create(dest_filename);
    if (!fs_exists(dest_filename)) {
        std::cerr << "Error (fs_copy): Failed to create destination file \'" << dest_filename << "\'." << std::endl;
        fs_log(("fs_copy failed: could not create destination file - " + std::string(dest_filename)).c_str());
        delete[] buffer;
        return;
    }

    int bytes_written = fs_write(dest_filename, buffer, src_size);
    delete[] buffer;

    if (bytes_written == src_size) {
        std::cout << "File \'" << src_filename << "\' copied to \'" << dest_filename << "\' successfully (" << src_size << " bytes)." << std::endl;
        fs_log(("fs_copy: Successfully copied \'" + std::string(src_filename) + "\' to \'" + std::string(dest_filename) + "\' (" + std::to_string(src_size) + " bytes).").c_str());
    } else {
        std::cerr << "Error (fs_copy): Failed to write data to destination file \'" << dest_filename << "\'. Expected " << src_size << " bytes, wrote " << bytes_written << "." << std::endl;
        fs_log(("fs_copy failed: error writing to destination file \'" + std::string(dest_filename) + "\'. Wrote " + std::to_string(bytes_written) + "/" + std::to_string(src_size) + " bytes.").c_str());
        // Kopyalama başarısız olduğu için oluşturulan hedef dosyayı silmek iyi bir pratik olabilir.
        fs_delete(dest_filename); // Hata mesajlarını fs_delete kendi yönetir.
    }
}

void fs_mv(const char* old_path, const char* new_path) {
    fs_rename(old_path, new_path); // Dizin yapımız olmadığı için şimdilik aynı işlev
}

// Bitmap Yönetimi Yardımcı Fonksiyonları

// Verilen byte içindeki belirli bir bit'in değerini kontrol etmek için maske döndürür.
// Örn: bit_num_in_byte = 0 -> 00000001 (1)
//      bit_num_in_byte = 7 -> 10000000 (128)
inline unsigned char bit_to_char_mask(int bit_num_in_byte) {
    return 1 << bit_num_in_byte;
}

void free_data_block(int block_index) {
    if (block_index < 0 || block_index >= NUM_DATA_BLOCKS) {
        std::cerr << "Error (free_data_block): Invalid data block index " << block_index << ". Valid range is 0-" << NUM_DATA_BLOCKS - 1 << std::endl;
        fs_log(("free_data_block failed: invalid block index " + std::to_string(block_index)).c_str());
        return;
    }

    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk_file) {
        std::cerr << "Error: Could not open disk file '" << DISK_FILENAME << "' to free data block." << std::endl;
        fs_log("free_data_block failed: could not open disk file.");
        return;
    }

    std::streampos bitmap_byte_offset = BITMAP_START_OFFSET_IN_METADATA + (block_index / 8);
    int bit_in_byte = block_index % 8;
    unsigned char byte_val;

    disk_file.seekg(bitmap_byte_offset);
    disk_file.read(reinterpret_cast<char*>(&byte_val), 1);

    if (!disk_file) {
        std::cerr << "Error: Could not read bitmap byte for block " << block_index << std::endl;
        fs_log(("free_data_block failed: could not read bitmap for block " + std::to_string(block_index)).c_str());
        disk_file.close();
        return;
    }

    if (!(byte_val & bit_to_char_mask(bit_in_byte))) {
        std::cout << "Warning (free_data_block): Data block " << block_index << " is already free." << std::endl;
        fs_log(("free_data_block warning: block " + std::to_string(block_index) + " already free.").c_str());
        disk_file.close();
        return; // Zaten boşsa bir şey yapma
    }

    byte_val &= ~bit_to_char_mask(bit_in_byte); // Biti sıfırla (boşalt)

    disk_file.seekp(bitmap_byte_offset);
    disk_file.write(reinterpret_cast<const char*>(&byte_val), 1);

    if (!disk_file) {
        std::cerr << "Error: Could not write updated bitmap byte for block " << block_index << std::endl;
        fs_log(("free_data_block failed: could not write bitmap for block " + std::to_string(block_index)).c_str());
    } else {
        // fs_log(("Data block " + std::to_string(block_index) + " freed successfully.").c_str());
    }
    disk_file.close();
}


// Belirtilen sayıda ardışık boş veri bloğu bulur, onları bitmap'te meşgul olarak işaretler
// ve ilk bulunan bloğun indeksini döndürür. Bulamazsa -1 döndürür.
int find_and_allocate_contiguous_data_blocks(int num_blocks_to_find) {
    if (num_blocks_to_find <= 0) {
        std::cerr << "Error (find_and_allocate_contiguous_data_blocks): Number of blocks to find must be positive. Requested: " << num_blocks_to_find << std::endl;
        fs_log(("find_and_allocate_contiguous_data_blocks failed: non-positive num_blocks_to_find: " + std::to_string(num_blocks_to_find)).c_str());
        return -1;
    }
    if (num_blocks_to_find > NUM_DATA_BLOCKS) {
         std::cerr << "Error (find_and_allocate_contiguous_data_blocks): Requested " << num_blocks_to_find << " blocks, but only " << NUM_DATA_BLOCKS << " total data blocks exist." << std::endl;
         fs_log(("find_and_allocate_contiguous_data_blocks failed: requested more blocks than exist: " + std::to_string(num_blocks_to_find)).c_str());
        return -1;
    }


    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk_file) {
        std::cerr << "Error: Could not open disk file '" << DISK_FILENAME << "' for find_and_allocate_contiguous_data_blocks." << std::endl;
        fs_log("find_and_allocate_contiguous_data_blocks failed: could not open disk file.");
        return -1;
    }

    char bitmap_buffer[BITMAP_SIZE_BYTES];
    disk_file.seekg(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
    disk_file.read(bitmap_buffer, BITMAP_SIZE_BYTES);
    if (!disk_file) {
        std::cerr << "Error: Could not read bitmap from disk (find_and_allocate_contiguous_data_blocks)." << std::endl;
        fs_log("find_and_allocate_contiguous_data_blocks failed: could not read bitmap.");
        disk_file.close();
        return -1;
    }

    for (int start_block_idx = 0; start_block_idx <= NUM_DATA_BLOCKS - num_blocks_to_find; ++start_block_idx) {
        bool all_free = true;
        for (int i = 0; i < num_blocks_to_find; ++i) {
            int current_block_idx = start_block_idx + i;
            int byte_index_in_bitmap = current_block_idx / 8;
            int bit_index_in_byte = current_block_idx % 8;

            if (bitmap_buffer[byte_index_in_bitmap] & bit_to_char_mask(bit_index_in_byte)) {
                all_free = false; // Bu blok dolu, sonraki başlangıçtan devam et
                start_block_idx = current_block_idx; // Optimizasyon: Direkt dolu bloğun sonrasından başla
                break;
            }
        }

        if (all_free) {
            // İstenen sayıda ardışık boş blok bulundu. Şimdi bunları işaretleyelim.
            for (int i = 0; i < num_blocks_to_find; ++i) {
                int block_to_allocate = start_block_idx + i;
                int byte_idx = block_to_allocate / 8;
                int bit_idx = block_to_allocate % 8;
                bitmap_buffer[byte_idx] |= bit_to_char_mask(bit_idx); // Biti 1 yap (meşgul)
            }
            // Güncellenmiş bitmap'i diske yaz
            disk_file.seekp(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
            disk_file.write(bitmap_buffer, BITMAP_SIZE_BYTES);
            if (!disk_file) {
                std::cerr << "Error: Could not write updated bitmap to disk after allocating blocks (find_and_allocate_contiguous_data_blocks)." << std::endl;
                fs_log("find_and_allocate_contiguous_data_blocks failed: could not write updated bitmap.");
                disk_file.close();
                return -1; // Yazma hatası
            }
            disk_file.close();
            // fs_log(("Allocated " + std::to_string(num_blocks_to_find) + " contiguous blocks starting from " + std::to_string(start_block_idx)).c_str());
            return start_block_idx;
        }
    }

    disk_file.close();
    // fs_log(("Could not find " + std::to_string(num_blocks_to_find) + " contiguous free data blocks.").c_str());
    return -1; // Yeterli ardışık boş blok bulunamadı
}

// Bitmap'i tarar, ilk boş veri bloğunu bulur, onu meşgul olarak işaretler
// ve blok indeksini döndürür. Boş blok yoksa -1 döndürür.
int find_free_data_block() {
    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk_file) {
        std::cerr << "Error: Could not open disk file '" << DISK_FILENAME << "' to find free data block." << std::endl;
        fs_log("find_free_data_block failed: could not open disk file.");
        return -1;
    }

    // Bitmap'i oku
    char bitmap[BITMAP_SIZE_BYTES];
    disk_file.seekg(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
    disk_file.read(bitmap, BITMAP_SIZE_BYTES);

    if (!disk_file) {
        std::cerr << "Error: Could not read bitmap from metadata (find_free_data_block)." << std::endl;
        fs_log("find_free_data_block failed: could not read bitmap.");
        disk_file.close();
        return -1;
    }

    for (int i = 0; i < NUM_DATA_BLOCKS; ++i) {
        int byte_index = i / 8;
        int bit_index_in_byte = i % 8; // Düzeltildi: bit_in_byte -> bit_index_in_byte

        if (!(bitmap[byte_index] & bit_to_char_mask(bit_index_in_byte))) {
            // Boş blok bulundu (bit 0). Onu 1 yap (meşgul) ve diske yaz.
            bitmap[byte_index] |= bit_to_char_mask(bit_index_in_byte);
            
            disk_file.seekp(BITMAP_START_OFFSET_IN_METADATA + byte_index, std::ios::beg);
            disk_file.write(&bitmap[byte_index], 1); // Sadece değişen byte'ı yaz

            if (!disk_file) {
                std::cerr << "Error: Could not write updated bitmap to metadata (find_free_data_block)." << std::endl;
                fs_log("find_free_data_block failed: could not write updated bitmap.");
                disk_file.close();
                return -1; // Yazma hatası
            }
            disk_file.close();
            // fs_log(("Data block " + std::to_string(i) + " allocated.").c_str());
            return i; // Bulunan boş bloğun indeksini döndür
        }
    }

    disk_file.close();
    // fs_log("No free data block found.");
    return -1; // Boş blok bulunamadı
}

// ------------- LOGLAMA YARDIMCI FONKSİYONU -------------
void fs_log(const char* message) {
    std::ofstream log_file(LOG_FILENAME, std::ios_base::app); // Append modunda aç
    if (log_file.is_open()) {
        // Zaman damgası ekleyebiliriz (isteğe bağlı)
        // time_t now = time(0);
        // tm *ltm = localtime(&now);
        // log_file << 1900 + ltm->tm_year << "/" << 1 + ltm->tm_mon << "/" << ltm->tm_mday << " ";
        // log_file << ltm->tm_hour << ":" << ltm->tm_min << ":" << ltm->tm_sec << " - ";
        log_file << message << std::endl;
        log_file.close();
    } else {
        std::cerr << "Warning: Unable to open log file: " << LOG_FILENAME << std::endl;
    }
}

// Superblock'tan aktif dosya sayısını okumak için yardımcı fonksiyon
int fs_count_active_files() {
    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in);
    if (!disk_file) {
        // Hata durumunda -1 veya başka bir belirteç döndürülebilir.
        // fs_ls ve diğerleri zaten metadata okuma hatasını ele alıyor.
        return -1; 
    }
    Superblock sb;
    disk_file.seekg(0, std::ios::beg);
    disk_file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
    disk_file.close();
    if (!disk_file && !disk_file.eof()) { // eof olmayan okuma hatası
        return -2; // Farklı bir hata kodu
    }
    return sb.num_active_files;
}

// Bir dosyanın kullandığı blok sayısını FileInfo'dan okur
int fs_get_num_blocks_used(const char* filename) {
    if (!fs_exists(filename)) {
        return -1; // Dosya yok
    }
    Superblock sb_dummy; // read_all_file_info tarafından kullanılacak, ama num_active_files'ı önemli değil
    std::vector<FileInfo> all_files = read_all_file_info(sb_dummy);
    // read_all_file_info hata durumunda boş vektör veya sb_dummy.num_active_files = -1 yapabilir
    // fs_exists zaten bunu kontrol etmiş olmalı.

    for (const auto& fi : all_files) {
        if (fi.is_used && strcmp(fi.name, filename) == 0) {
            return static_cast<int>(fi.num_data_blocks_used);
        }
    }
    return -2; // Dosya bulundu (fs_exists geçti) ama FileInfo'da bulunamadı (tutarsızlık)
}

// read_all_file_info fonksiyonunun (eğer varsa) static olmadığından emin olun.
// Zaten static değildi, bu satır sadece kontrol amaçlı bir yorum.
// std::vector<FileInfo> read_all_file_info(Superblock& sb_out) { ... } 
// fonksiyonu zaten fs.cpp'de mevcut.

void fs_defragment() {
    ensure_disk_initialized();
    fs_log("Defragmentation process started.");

    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk_file) {
        std::cerr << "Error (fs_defragment): Could not open disk file '\" << DISK_FILENAME << \"'." << std::endl;
        fs_log("fs_defragment failed: could not open disk file.");
        return;
    }

    Superblock sb;
    // Read Superblock
    disk_file.seekg(0, std::ios::beg);
    disk_file.read(reinterpret_cast<char*>(&sb), SUPERBLOCK_ACTUAL_SIZE);
    if (!disk_file) {
        std::cerr << "Error (fs_defragment): Could not read superblock." << std::endl;
        fs_log("fs_defragment failed: could not read superblock.");
        disk_file.close();
        return;
    }

    // Read all FileInfo entries
    std::vector<FileInfo> all_files_info(MAX_FILES_CALCULATED);
    disk_file.seekg(FILE_INFO_ARRAY_START_OFFSET_IN_METADATA, std::ios::beg);
    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        disk_file.read(reinterpret_cast<char*>(&all_files_info[i]), FILE_INFO_ENTRY_SIZE);
        if (!disk_file && !disk_file.eof()) {
            std::cerr << "Error (fs_defragment): Could not read FileInfo entry " << i << "." << std::endl;
            fs_log("fs_defragment failed: error reading FileInfo entries.");
            disk_file.close();
            return;
        }
    }

    std::vector<int> active_file_indices;
    for(int i=0; i < MAX_FILES_CALCULATED; ++i) {
        if(all_files_info[i].is_used) {
            active_file_indices.push_back(i);
        }
    }

    if (active_file_indices.empty()) {
        fs_log("fs_defragment: No active files to defragment.");
        disk_file.close();
        return;
    }

    char new_bitmap[BITMAP_SIZE_BYTES];
    memset(new_bitmap, 0, BITMAP_SIZE_BYTES); // All blocks initially free

    unsigned int next_target_data_block = 0; 
    char* file_content_buffer = nullptr; 

    for (int file_idx : active_file_indices) {
        FileInfo& current_fi = all_files_info[file_idx];

        if (current_fi.num_data_blocks_used == 0 || current_fi.size == 0) {
            current_fi.start_data_block_index = -1; 
            current_fi.num_data_blocks_used = 0;
            continue;
        }

        file_content_buffer = new (std::nothrow) char[current_fi.size];
        if (!file_content_buffer) {
            std::cerr << "Error (fs_defragment): Failed to allocate memory for file '" << current_fi.name << "' content." << std::endl;
            fs_log("fs_defragment failed: memory allocation error for reading file content.");
            disk_file.close();
            return;
        }

        fs_log(("Defragmenting file: " + std::string(current_fi.name) +
                ", size: " + std::to_string(current_fi.size) +
                ", old_start_block: " + std::to_string(current_fi.start_data_block_index) +
                ", num_blocks: " + std::to_string(current_fi.num_data_blocks_used)).c_str());

        // Direct Read Logic
        if (current_fi.start_data_block_index != -1 && current_fi.num_data_blocks_used > 0) {
            char* temp_buffer_ptr = file_content_buffer;
            int bytes_remaining_to_read_for_file = current_fi.size;
            
            for (unsigned int k = 0; k < current_fi.num_data_blocks_used; ++k) {
                if (bytes_remaining_to_read_for_file <= 0) break;

                unsigned int actual_disk_block_to_read = current_fi.start_data_block_index + k;
                std::streampos read_pos = METADATA_AREA_SIZE_BYTES +
                                          (static_cast<std::streamoff>(actual_disk_block_to_read) * BLOCK_SIZE_BYTES);
                disk_file.seekg(read_pos);

                int bytes_to_read_in_this_block = std::min(bytes_remaining_to_read_for_file, static_cast<int>(BLOCK_SIZE_BYTES));
                
                disk_file.read(temp_buffer_ptr, bytes_to_read_in_this_block);
                if (!disk_file) {
                    std::cerr << "Error (fs_defragment): Failed to read data for file '" << current_fi.name 
                              << "' from block " << actual_disk_block_to_read << std::endl;
                    fs_log("fs_defragment error: failed reading file data during move.");
                    delete[] file_content_buffer;
                    disk_file.close();
                    return;
                }
                temp_buffer_ptr += bytes_to_read_in_this_block;
                bytes_remaining_to_read_for_file -= bytes_to_read_in_this_block;
            }
        }
        // End Direct Read

        bool needs_move = (current_fi.start_data_block_index != next_target_data_block);

        if (needs_move) {
             fs_log(("Moving data for file " + std::string(current_fi.name) + " from block " + 
                    std::to_string(current_fi.start_data_block_index) + " to " + std::to_string(next_target_data_block)).c_str());
            // Direct Write Logic
            const char* data_to_write_ptr = file_content_buffer;
            int bytes_remaining_to_write_for_file = current_fi.size;

            for (unsigned int k = 0; k < current_fi.num_data_blocks_used; ++k) {
                if (bytes_remaining_to_write_for_file <= 0) break;

                unsigned int actual_disk_block_to_write = next_target_data_block + k;
                 std::streampos write_pos = METADATA_AREA_SIZE_BYTES +
                                           (static_cast<std::streamoff>(actual_disk_block_to_write) * BLOCK_SIZE_BYTES);
                disk_file.seekp(write_pos);

                int bytes_to_write_in_this_block = std::min(bytes_remaining_to_write_for_file, static_cast<int>(BLOCK_SIZE_BYTES));

                disk_file.write(data_to_write_ptr, bytes_to_write_in_this_block);
                if (!disk_file) {
                    std::cerr << "Error (fs_defragment): Failed to write data for file '" << current_fi.name 
                              << "' to new block " << actual_disk_block_to_write << std::endl;
                    fs_log("fs_defragment error: failed writing file data during move.");
                    delete[] file_content_buffer;
                    disk_file.close();
                    return;
                }
                data_to_write_ptr += bytes_to_write_in_this_block;
                bytes_remaining_to_write_for_file -= bytes_to_write_in_this_block;
            }
            // End Direct Write
        } else {
            fs_log(("File " + std::string(current_fi.name) + " is already in its defragmented position (block " + 
                   std::to_string(current_fi.start_data_block_index) + "). No data move needed.").c_str());
        }

        current_fi.start_data_block_index = next_target_data_block;

        for (unsigned int k = 0; k < current_fi.num_data_blocks_used; ++k) {
            unsigned int block_to_mark = next_target_data_block + k;
            unsigned int byte_idx = block_to_mark / 8;
            unsigned int bit_idx = block_to_mark % 8;
            if (byte_idx < BITMAP_SIZE_BYTES) {
                new_bitmap[byte_idx] |= bit_to_char_mask(bit_idx); // Use helper
            } else {
                 std::cerr << "Error (fs_defragment): Bitmap index out of bounds for block " << block_to_mark << std::endl;
                 fs_log("fs_defragment error: bitmap index out of bounds during new bitmap creation.");
                 delete[] file_content_buffer;
                 disk_file.close();
                 return;
            }
        }
        
        next_target_data_block += current_fi.num_data_blocks_used;
        delete[] file_content_buffer;
        file_content_buffer = nullptr; 
    }

    disk_file.seekp(FILE_INFO_ARRAY_START_OFFSET_IN_METADATA, std::ios::beg);
    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        disk_file.write(reinterpret_cast<const char*>(&all_files_info[i]), FILE_INFO_ENTRY_SIZE);
        if (!disk_file) {
            std::cerr << "Error (fs_defragment): Could not write updated FileInfo entry " << i << "." << std::endl;
            fs_log("fs_defragment failed: error writing updated FileInfo entries.");
            disk_file.close();
            return;
        }
    }

    disk_file.seekp(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
    disk_file.write(new_bitmap, BITMAP_SIZE_BYTES);
    if (!disk_file) {
        std::cerr << "Error (fs_defragment): Could not write new bitmap to disk." << std::endl;
        fs_log("fs_defragment failed: error writing new bitmap.");
        disk_file.close();
        return;
    }

    disk_file.seekp(0, std::ios::beg);
    disk_file.write(reinterpret_cast<const char*>(&sb), SUPERBLOCK_ACTUAL_SIZE);
     if (!disk_file) {
        std::cerr << "Error (fs_defragment): Could not rewrite superblock." << std::endl;
        fs_log("fs_defragment failed: could not rewrite superblock.");
    }

    disk_file.close();
    fs_log("Defragmentation process completed successfully.");
}

FileInfo fs_get_file_info_debug(const char* filename) {
    FileInfo not_found_fi; // Varsayılan olarak is_used=false, name boş vb.
    not_found_fi.is_used = false;
    not_found_fi.start_data_block_index = -2; // Hata kodu olarak kullanılabilir
    not_found_fi.num_data_blocks_used = 0;
    not_found_fi.size = -1;

    if (filename == nullptr || strlen(filename) == 0 || strlen(filename) > MAX_FILENAME_LENGTH) {
        fs_log("fs_get_file_info_debug: Invalid filename provided.");
        return not_found_fi;
    }

    ensure_disk_initialized(); // Disk dosyasının var olduğundan emin ol
    Superblock sb_dummy; 
    std::vector<FileInfo> all_files = read_all_file_info(sb_dummy);

    if (sb_dummy.num_active_files == -1){ 
        fs_log("fs_get_file_info_debug: Could not read metadata.");
        return not_found_fi;
    }

    for (const auto& fi : all_files) {
        if (fi.is_used && strcmp(fi.name, filename) == 0) {
            return fi; // Bulunan FileInfo'nun kopyasını döndür
        }
    }

    fs_log(("fs_get_file_info_debug: File '" + std::string(filename) + "' not found.").c_str());
    return not_found_fi; // Dosya bulunamadı
}

void fs_check_integrity() {
    ensure_disk_initialized();
    fs_log("File system integrity check started.");
    bool is_consistent = true;
    int issues_found = 0;

    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in);
    if (!disk_file) {
        fs_log("fs_check_integrity CRITICAL: Could not open disk file.");
        std::cerr << "CRITICAL (fs_check_integrity): Could not open disk file '" << DISK_FILENAME << "'." << std::endl;
        return;
    }

    // 1. Superblock ve FileInfo listesini oku
    Superblock sb;
    disk_file.seekg(0, std::ios::beg);
    disk_file.read(reinterpret_cast<char*>(&sb), SUPERBLOCK_ACTUAL_SIZE);
    if (!disk_file) {
        fs_log("fs_check_integrity ERROR: Could not read superblock.");
        is_consistent = false; issues_found++;
        disk_file.close();
        return;
    }

    std::vector<FileInfo> all_files_info(MAX_FILES_CALCULATED);
    disk_file.seekg(FILE_INFO_ARRAY_START_OFFSET_IN_METADATA, std::ios::beg);
    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        disk_file.read(reinterpret_cast<char*>(&all_files_info[i]), FILE_INFO_ENTRY_SIZE);
        if (!disk_file && !disk_file.eof()) {
            fs_log(("fs_check_integrity ERROR: Could not read FileInfo entry " + std::to_string(i) + ".").c_str());
            is_consistent = false; issues_found++;
            disk_file.close();
            return;
        }
    }

    // 2. Bitmap'i oku
    char bitmap[BITMAP_SIZE_BYTES];
    disk_file.seekg(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
    disk_file.read(bitmap, BITMAP_SIZE_BYTES);
    if (!disk_file) {
        fs_log("fs_check_integrity ERROR: Could not read bitmap.");
        is_consistent = false; issues_found++;
        disk_file.close();
        return;
    }
    disk_file.close(); // Disk okumaları tamamlandı.

    // Kontrol 1: Superblock'taki aktif dosya sayısı ile FileInfo'lardaki sayının tutarlılığı
    int active_files_in_fileinfo = 0;
    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        if (all_files_info[i].is_used) {
            active_files_in_fileinfo++;
        }
    }
    if (sb.num_active_files != active_files_in_fileinfo) {
        fs_log(("fs_check_integrity WARNING: Superblock active files count (" + std::to_string(sb.num_active_files) + 
               ") does not match count from FileInfo entries (" + std::to_string(active_files_in_fileinfo) + ").").c_str());
        is_consistent = false; issues_found++;
    }

    // Kontrol 2: Her aktif FileInfo'nun kendi iç tutarlılığı ve Bitmap ile tutarlılığı
    std::vector<bool> block_usage_tracker(NUM_DATA_BLOCKS, false); // Hangi blokların FileInfo'lar tarafından kullanıldığını izler

    for (int i = 0; i < MAX_FILES_CALCULATED; ++i) {
        if (all_files_info[i].is_used) {
            const FileInfo& fi = all_files_info[i];
            std::string filename_str(fi.name);

            // a. Boyut ve blok kullanımı
            if (fi.size > 0 && (fi.num_data_blocks_used == 0 || fi.start_data_block_index == -1)) {
                fs_log(("fs_check_integrity WARNING: File '" + filename_str + "' has size " + std::to_string(fi.size) +
                       " but no data blocks allocated (num_blocks: " + std::to_string(fi.num_data_blocks_used) +
                       ", start_block: " + std::to_string(fi.start_data_block_index) + ").").c_str());
                is_consistent = false; issues_found++;
            }
            if (fi.size == 0 && (fi.num_data_blocks_used != 0 || fi.start_data_block_index != -1)) {
                 fs_log(("fs_check_integrity WARNING: File '" + filename_str + "' has size 0 but data blocks seem allocated (num_blocks: " +
                        std::to_string(fi.num_data_blocks_used) + ", start_block: " + std::to_string(fi.start_data_block_index) + "). Should be 0 and -1.").c_str());
                is_consistent = false; issues_found++;
            }

            // b. Gerekli blok sayısı ile tahsis edilen blok sayısının tutarlılığı
            if (fi.size > 0) {
                unsigned int expected_blocks = (fi.size + BLOCK_SIZE_BYTES - 1) / BLOCK_SIZE_BYTES;
                if (fi.num_data_blocks_used != expected_blocks) {
                    fs_log(("fs_check_integrity WARNING: File '" + filename_str + "' size " + std::to_string(fi.size) +
                           " requires " + std::to_string(expected_blocks) + " blocks, but FileInfo states " +
                           std::to_string(fi.num_data_blocks_used) + " blocks are used.").c_str());
                    is_consistent = false; issues_found++;
                }
            }

            // c. Blok indekslerinin geçerliliği ve Bitmap ile tutarlılığı
            if (fi.num_data_blocks_used > 0) {
                if (fi.start_data_block_index < 0 || 
                    static_cast<unsigned int>(fi.start_data_block_index) + fi.num_data_blocks_used > NUM_DATA_BLOCKS) {
                    fs_log(("fs_check_integrity WARNING: File '" + filename_str + "' has invalid block range (start: " +
                           std::to_string(fi.start_data_block_index) + ", num: " + std::to_string(fi.num_data_blocks_used) + ").").c_str());
                    is_consistent = false; issues_found++;
                } else {
                    for (unsigned int k = 0; k < fi.num_data_blocks_used; ++k) {
                        unsigned int current_block_idx = fi.start_data_block_index + k;
                        unsigned int byte_idx = current_block_idx / 8;
                        unsigned int bit_idx = current_block_idx % 8;

                        if (!(bitmap[byte_idx] & bit_to_char_mask(bit_idx))) {
                            fs_log(("fs_check_integrity WARNING: File '" + filename_str + "' uses block " +
                                   std::to_string(current_block_idx) + ", but bitmap marks it as free.").c_str());
                            is_consistent = false; issues_found++;
                        }
                        if (block_usage_tracker[current_block_idx]) {
                            fs_log(("fs_check_integrity WARNING: Block " + std::to_string(current_block_idx) +
                                   " is marked as used by multiple FileInfo entries (e.g., '" + filename_str + "').").c_str());
                            is_consistent = false; issues_found++;
                        }
                        block_usage_tracker[current_block_idx] = true;
                    }
                }
            }
        }
    }

    // Kontrol 3: Bitmap'teki "dolu" blokların FileInfo'lar tarafından kullanılıp kullanılmadığı
    for (unsigned int block_idx = 0; block_idx < NUM_DATA_BLOCKS; ++block_idx) {
        unsigned int byte_idx = block_idx / 8;
        unsigned int bit_idx = block_idx % 8;
        bool bitmap_is_set = (bitmap[byte_idx] & bit_to_char_mask(bit_idx));

        if (bitmap_is_set && !block_usage_tracker[block_idx]) {
            fs_log(("fs_check_integrity WARNING: Bitmap marks block " + std::to_string(block_idx) +
                   " as used, but no FileInfo entry claims it (lost block).").c_str());
            is_consistent = false; issues_found++;
        }
        if (!bitmap_is_set && block_usage_tracker[block_idx]) {
            // Bu durum zaten Kontrol 2.c.ii'de yakalanmış olmalı (FileInfo kullanıyor ama bitmap boş diyor)
            // Yine de bir güvenlik olarak eklenebilir veya oradaki log mesajı buraya taşınabilir.
            fs_log(("fs_check_integrity WARNING: Bitmap marks block " + std::to_string(block_idx) +
                   " as free, but a FileInfo entry claims it (bitmap inconsistency - should have been caught earlier).").c_str());
            is_consistent = false; issues_found++;
        }
    }

    if (is_consistent) {
        fs_log("File system integrity check passed. No issues found.");
    } else {
        fs_log(("File system integrity check FAILED. Issues found: " + std::to_string(issues_found)).c_str());
    }
    // Fonksiyon bir bool döndürmüyor, sadece logluyor.
}

int fs_backup(const char* backup_filename) {
    ensure_disk_initialized(); // Ana diskimizin var olduğundan emin olalım
    fs_log(("Backup process started. Target backup file: '" + std::string(backup_filename) + "'").c_str());

    if (backup_filename == nullptr || strlen(backup_filename) == 0) {
        fs_log("fs_backup ERROR: Backup filename cannot be null or empty.");
        std::cerr << "Error (fs_backup): Backup filename cannot be null or empty." << std::endl;
        return -1; // Hata kodu: Geçersiz backup dosya adı
    }

    std::ifstream source_disk(DISK_FILENAME, std::ios::binary);
    if (!source_disk) {
        fs_log(("fs_backup CRITICAL: Could not open source disk file '" + std::string(DISK_FILENAME) + "' for reading.").c_str());
        std::cerr << "Error (fs_backup): Could not open source disk file '" << DISK_FILENAME << "' for reading." << std::endl;
        return -2; // Hata kodu: Kaynak disk açılamadı
    }

    std::ofstream backup_file(backup_filename, std::ios::binary | std::ios::trunc);
    if (!backup_file) {
        fs_log(("fs_backup ERROR: Could not create or open backup file '" + std::string(backup_filename) + "' for writing.").c_str());
        std::cerr << "Error (fs_backup): Could not create or open backup file '" << backup_filename << "' for writing." << std::endl;
        source_disk.close();
        return -3; // Hata kodu: Backup dosyası oluşturulamadı/açılamadı
    }

    char buffer[1024]; 
    while (source_disk.read(buffer, sizeof(buffer)) || source_disk.gcount() > 0) {
        backup_file.write(buffer, source_disk.gcount());
        if (!backup_file) {
            fs_log(("fs_backup ERROR: Failed to write to backup file '" + std::string(backup_filename) + "'.").c_str());
            std::cerr << "Error (fs_backup): Failed to write to backup file '" << backup_filename << "'." << std::endl;
            source_disk.close();
            backup_file.close();
            // remove(backup_filename); // İsteğe bağlı, yarım dosyayı sil
            return -4; // Hata kodu: Backup dosyasına yazma hatası
        }
    }

    int return_code = 0; // Başarılı varsayalım
    if (source_disk.eof()) { 
        fs_log(("Backup of '" + std::string(DISK_FILENAME) + "' to '" + std::string(backup_filename) + "' completed successfully.").c_str());
        std::cout << "Disk backup completed successfully to '" << backup_filename << "'." << std::endl;
        // return_code = 0; // Zaten 0
    } else if (source_disk.fail()) { 
        fs_log(("fs_backup ERROR: Failed to read from source disk '" + std::string(DISK_FILENAME) + "' before EOF.").c_str());
        std::cerr << "Error (fs_backup): Failed to read from source disk '" << DISK_FILENAME << "'." << std::endl;
        // remove(backup_filename); // İsteğe bağlı, yarım dosyayı sil
        return_code = -5; // Hata kodu: Kaynak diskten okuma hatası
    }

    source_disk.close();
    backup_file.close();
    return return_code;
}

// Geri yükleme fonksiyonu
void fs_restore(const char* backup_filename) {
    if (backup_filename == nullptr || strlen(backup_filename) == 0) {
        fs_log("fs_restore ERROR: Backup filename cannot be null or empty.");
        std::cerr << "Error (fs_restore): Backup filename cannot be null or empty." << std::endl;
        return; // Hata kodu döndürmek yerine sadece return ediyoruz (fonksiyon void olduğu için)
    }

    ensure_disk_initialized(); // Ana diskimizin var olduğundan emin olmak için fs_init mantığı, ama formatlamaz
    fs_log(("Restore process started from backup file: '" + std::string(backup_filename) + "'").c_str());

    // if (backup_filename == nullptr || strlen(backup_filename) == 0) { // Bu kontrol yukarıya taşındı
    //     fs_log("fs_restore ERROR: Backup filename cannot be null or empty.");
    //     std::cerr << "Error (fs_restore): Backup filename cannot be null or empty." << std::endl;
    //     return; // Veya hata kodu
    // }

    std::ifstream backup_source(backup_filename, std::ios::binary);
    if (!backup_source) {
        fs_log(("fs_restore CRITICAL: Could not open backup file '" + std::string(backup_filename) + "' for reading.").c_str());
        std::cerr << "Error (fs_restore): Could not open backup file '" << backup_filename << "' for reading." << std::endl;
        return;
    }

    // Hedef disk dosyasını (disk.sim) yazmak üzere aç (truncate etmeli)
    std::ofstream target_disk(DISK_FILENAME, std::ios::binary | std::ios::trunc);
    if (!target_disk) {
        fs_log(("fs_restore CRITICAL: Could not open/create target disk file '" + std::string(DISK_FILENAME) + "' for writing.").c_str());
        std::cerr << "Error (fs_restore): Could not open/create target disk file '" << DISK_FILENAME << "' for writing." << std::endl;
        backup_source.close();
        return;
    }

    target_disk << backup_source.rdbuf(); // Tüm içeriği kopyala

    bool success_restore = true;
    if (!target_disk.good()) {
        fs_log(("fs_restore ERROR: Error occurred while writing to target disk file '" + std::string(DISK_FILENAME) + "'.").c_str());
        std::cerr << "Error (fs_restore): Error occurred while writing to target disk file '" << DISK_FILENAME << "'." << std::endl;
        success_restore = false;
    }

    backup_source.close();
    target_disk.close();

    if (success_restore) {
        fs_log(("Restore process completed successfully from '" + std::string(backup_filename) + "' to '" + std::string(DISK_FILENAME) + "'.").c_str());
        std::cout << "Disk restore successful from: " << backup_filename << " to: " << DISK_FILENAME << std::endl;
        // Geri yükleme sonrası FS'nin tutarlı olması için ek kontroller veya fs_check_integrity() çağrılabilir.
        // Örneğin, superblock'taki dosya sayısı ve bitmap tutarlı mı?
        fs_log("Running integrity check after restore...");
        fs_check_integrity(); // Yedekten sonra bir kontrol iyi olabilir.
    } else {
        // Hata durumunda, disk.sim dosyası bozulmuş olabilir. Eski haline getirmek zor.
        // Kullanıcıya bilgi verilmeli.
        fs_log(("fs_restore CRITICAL ERROR: Restore failed. Disk file \\\'" + std::string(DISK_FILENAME) + "\\\' may be corrupted.").c_str());
        std::cerr << "CRITICAL ERROR (fs_restore): Restore failed. Disk file \\\'" << DISK_FILENAME << "\\\' may be corrupted." << std::endl;
    }
}

int fs_diff(const char* filename1, const char* filename2) {
    ensure_disk_initialized();
    fs_log(("fs_diff called for files: \\\'" + (filename1 ? std::string(filename1) : "NULL") + "\\\' and \\\'" + (filename2 ? std::string(filename2) : "NULL") + "\\\'.").c_str());

    if (filename1 == nullptr || strlen(filename1) == 0 || filename2 == nullptr || strlen(filename2) == 0) {
        std::cerr << "Error (fs_diff): Filenames cannot be null or empty." << std::endl;
        fs_log("fs_diff failed: one or both filenames are null or empty.");
        return -1; // Hata kodu: Geçersiz dosya adı
    }

    if (strcmp(filename1, filename2) == 0) {
        fs_log("fs_diff: Filenames are identical. Files are considered the same.");
        return 0; // Aynı dosya adları, aynı kabul edilir.
    }

    if (!fs_exists(filename1)) {
        std::cerr << "Error (fs_diff): File \\\'" << filename1 << "\\\' not found." << std::endl;
        fs_log(("fs_diff failed: file1 not found - " + std::string(filename1)).c_str());
        return -2; // Hata kodu: İlk dosya yok
    }
    if (!fs_exists(filename2)) {
        std::cerr << "Error (fs_diff): File \\\'" << filename2 << "\\\' not found." << std::endl;
        fs_log(("fs_diff failed: file2 not found - " + std::string(filename2)).c_str());
        return -3; // Hata kodu: İkinci dosya yok
    }

    int size1 = fs_size(filename1);
    int size2 = fs_size(filename2);

    if (size1 < 0 || size2 < 0) {
        std::cerr << "Error (fs_diff): Could not determine size of one or both files." << std::endl;
        fs_log("fs_diff failed: could not get size for one or both files.");
        return -4; // Hata kodu: Boyut okuma hatası
    }

    if (size1 != size2) {
        fs_log("fs_diff: Files have different sizes. Considered different.");
        return 1; // Farklı boyutlar, farklı kabul edilir.
    }

    // Boyutlar aynı ve 0 ise, dosyalar aynıdır.
    if (size1 == 0) {
        fs_log("fs_diff: Both files are empty. Considered the same.");
        return 0; 
    }

    // Boyutlar aynı ve 0'dan büyükse, içerikleri karşılaştır.
    char* buffer1 = new (std::nothrow) char[size1];
    char* buffer2 = new (std::nothrow) char[size1]; // size1 == size2

    if (!buffer1 || !buffer2) {
        std::cerr << "Error (fs_diff): Memory allocation failed for buffers." << std::endl;
        fs_log("fs_diff failed: memory allocation error for buffers.");
        if (buffer1) delete[] buffer1;
        if (buffer2) delete[] buffer2;
        return -5; // Hata kodu: Bellek hatası
    }

    fs_read(filename1, 0, size1, buffer1);
    // fs_read kendi hatalarını loglar. Eğer buffer1 boş gelirse, bu bir okuma hatasıdır.
    // Ancak fs_read void olduğu için dönüş değerinden kontrol edemiyoruz.
    // fs_size kontrolü sonrası buraya gelindiği için dosya var ve boyutu biliniyor.

    fs_read(filename2, 0, size2, buffer2); // size1 == size2

    int diff_result = memcmp(buffer1, buffer2, size1);

    delete[] buffer1;
    delete[] buffer2;

    if (diff_result == 0) {
        fs_log("fs_diff: File contents are identical.");
        return 0; // İçerikler aynı
    } else {
        fs_log("fs_diff: File contents are different.");
        return 1; // İçerikler farklı
    }
}
