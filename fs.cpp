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
    std::cout << "fs_rename called for: " << old_name << " to " << new_name << ". Not implemented yet." << std::endl;
    fs_log("fs_rename attempt.");
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
    std::cout << "fs_append called for: " << filename << " with size " << size << ". Not implemented yet." << std::endl;
    fs_log("fs_append attempt.");
}

void fs_truncate(const char* filename, int new_size) {
    ensure_disk_initialized();
    std::cout << "fs_truncate called for: " << filename << " to new size " << new_size << ". Not implemented yet." << std::endl;
    fs_log("fs_truncate attempt.");
}

void fs_copy(const char* src_filename, const char* dest_filename) {
    ensure_disk_initialized();
    std::cout << "fs_copy called from " << src_filename << " to " << dest_filename << ". Not implemented yet." << std::endl;
    fs_log("fs_copy attempt.");
}

void fs_mv(const char* old_path, const char* new_path) {
    ensure_disk_initialized();
    std::cout << "fs_mv called from " << old_path << " to " << new_path << ". Not implemented yet (acts like rename)." << std::endl;
    fs_rename(old_path, new_path); 
    fs_log("fs_mv attempt.");
}

void fs_defragment() {
    ensure_disk_initialized();
    std::cout << "fs_defragment called. Not implemented yet." << std::endl;
    fs_log("fs_defragment attempt.");
}

void fs_check_integrity() {
    ensure_disk_initialized();
    std::cout << "fs_check_integrity called. Not implemented yet." << std::endl;
    fs_log("fs_check_integrity attempt.");
}

void fs_backup(const char* backup_filename) {
    ensure_disk_initialized();
    std::cout << "fs_backup called to " << backup_filename << ". Not implemented yet." << std::endl;
    fs_log("fs_backup attempt.");
}

void fs_restore(const char* backup_filename) {
    std::cout << "fs_restore called from " << backup_filename << ". Not implemented yet." << std::endl;
    fs_log("fs_restore attempt.");
}

bool fs_diff(const char* file1, const char* file2) {
    ensure_disk_initialized();
    std::cout << "fs_diff called for: " << file1 << " and " << file2 << ". Not implemented yet. Returning false." << std::endl;
    fs_log("fs_diff attempt.");
    return false;
}

void fs_log(const char* message) {
    // Bu basit bir loglama fonksiyonu, istenirse daha gelişmiş bir hale getirilebilir.
    // Örneğin bir log dosyasına yazabilir. Şimdilik sadece konsola yazıyor.
    // ensure_disk_initialized(); // Loglama için disk init gerekli değil, ama disk işlemleri loglanıyorsa...
    time_t now = time(0);
    char* dt = ctime(&now);
    // ctime'dan dönen string sonunda newline içerir, onu kaldıralım.
    if (dt[strlen(dt) - 1] == '\n') {
        dt[strlen(dt) - 1] = '\0';
    }
    std::cout << "[LOG " << dt << "] " << message << std::endl;

    // İsteğe bağlı: Bir log dosyasına yazma
    // std::ofstream log_file("simplefs.log", std::ios::app);
    // if (log_file.is_open()) {
    //     log_file << "[" << dt << "] " << message << std::endl;
    //     log_file.close();
    // }
}

// Bitmap Yönetimi Yardımcı Fonksiyonları

// Verilen bit numarasının (0-7) byte içindeki değerini döndürür (örn: bit 0 -> 1, bit 1 -> 2, bit 7 -> 128)
inline unsigned char bit_to_char_mask(int bit_num_in_byte) {
    return 1 << bit_num_in_byte;
}

// Belirtilen sayıda ardışık boş veri bloğu bulur, işaretler ve başlangıç indeksini döndürür.
// Bulamazsa -1 döndürür.
int find_and_allocate_contiguous_data_blocks(int num_blocks_to_find) {
    if (num_blocks_to_find <= 0) {
        fs_log("Error (find_and_allocate_contiguous_data_blocks): Number of blocks to find must be positive.");
        return -1;
    }
    if (static_cast<unsigned int>(num_blocks_to_find) > NUM_DATA_BLOCKS) {
        fs_log("Error (find_and_allocate_contiguous_data_blocks): Requested more blocks than available on disk.");
        return -1;
    }

    ensure_disk_initialized();
    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk_file) {
        std::cerr << "Error (find_and_allocate_contiguous_data_blocks): Could not open disk file '" << DISK_FILENAME << "'." << std::endl;
        fs_log("Error (find_and_allocate_contiguous_data_blocks): Could not open disk file.");
        return -1;
    }

    char bitmap_buffer[BITMAP_SIZE_BYTES];
    disk_file.seekg(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
    disk_file.read(bitmap_buffer, BITMAP_SIZE_BYTES);
    if (!disk_file) {
        std::cerr << "Error (find_and_allocate_contiguous_data_blocks): Could not read bitmap from disk." << std::endl;
        fs_log("Error (find_and_allocate_contiguous_data_blocks): Could not read bitmap.");
        disk_file.close();
        return -1;
    }

    for (unsigned int start_block_idx = 0; start_block_idx <= NUM_DATA_BLOCKS - num_blocks_to_find; ++start_block_idx) {
        bool all_contiguous_blocks_free = true;
        for (int i = 0; i < num_blocks_to_find; ++i) {
            unsigned int current_block_idx = start_block_idx + i;
            unsigned int byte_index = current_block_idx / 8;
            unsigned int bit_index_in_byte = current_block_idx % 8;

            // bitmap_buffer sınırlarını kontrol etmeye gerek yok çünkü start_block_idx <= NUM_DATA_BLOCKS - num_blocks_to_find
            // ve i < num_blocks_to_find, bu yüzden current_block_idx her zaman < NUM_DATA_BLOCKS olacaktır.
            // byte_index de BITMAP_SIZE_BYTES içinde kalacaktır.

            if (bitmap_buffer[byte_index] & bit_to_char_mask(bit_index_in_byte)) { // Eğer blok doluysa (bit 1 ise)
                all_contiguous_blocks_free = false;
                break; // Bu başlangıç indeksi uygun değil, bir sonrakine geç.
            }
        }

        if (all_contiguous_blocks_free) {
            // Ardışık boş bloklar bulundu! Bu blokları meşgul olarak işaretle.
            for (int i = 0; i < num_blocks_to_find; ++i) {
                unsigned int block_to_allocate = start_block_idx + i;
                unsigned int byte_index = block_to_allocate / 8;
                unsigned int bit_index_in_byte = block_to_allocate % 8;
                bitmap_buffer[byte_index] |= bit_to_char_mask(bit_index_in_byte); // Biti 1 yap
            }

            // Güncellenmiş bitmap'i diske geri yaz
            disk_file.seekp(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
            disk_file.write(bitmap_buffer, BITMAP_SIZE_BYTES);
            if (!disk_file) {
                std::cerr << "Error (find_and_allocate_contiguous_data_blocks): Could not write updated bitmap to disk." << std::endl;
                fs_log("Error (find_and_allocate_contiguous_data_blocks): Could not write updated bitmap.");
                // Hata durumunda, ideal olarak yapılan değişiklikler geri alınmalı, ama bu karmaşık olabilir.
                // Şimdilik sadece hata döndürelim.
                disk_file.close();
                return -1; // Yazma hatası
            }

            disk_file.close();
            fs_log(("Contiguous data blocks from " + std::to_string(start_block_idx) + " to " + std::to_string(start_block_idx + num_blocks_to_find - 1) + " allocated.").c_str());
            return static_cast<int>(start_block_idx);
        }
    }

    // Uygun ardışık boş blok grubu bulunamadı
    disk_file.close();
    fs_log(("Warning (find_and_allocate_contiguous_data_blocks): No " + std::to_string(num_blocks_to_find) + " contiguous free data blocks available.").c_str());
    return -1;
}

int find_free_data_block() {
    ensure_disk_initialized(); // Disk yoksa hata verecek veya oluşturacak

    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk_file) {
        std::cerr << "Error (find_free_data_block): Could not open disk file '" << DISK_FILENAME << "'." << std::endl;
        fs_log("Error (find_free_data_block): Could not open disk file.");
        return -1; // Hata kodu
    }

    // 1. Bitmap'i oku
    char bitmap_buffer[BITMAP_SIZE_BYTES];
    disk_file.seekg(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
    disk_file.read(bitmap_buffer, BITMAP_SIZE_BYTES);
    if (!disk_file) {
        std::cerr << "Error (find_free_data_block): Could not read bitmap from disk." << std::endl;
        fs_log("Error (find_free_data_block): Could not read bitmap.");
        disk_file.close();
        return -1;
    }

    // 2. Boş blok ara (0 olan bit)
    for (unsigned int block_idx = 0; block_idx < NUM_DATA_BLOCKS; ++block_idx) {
        unsigned int byte_index = block_idx / 8;
        unsigned int bit_index_in_byte = block_idx % 8;

        if (byte_index >= BITMAP_SIZE_BYTES) { // Güvenlik kontrolü, olmamalı
            std::cerr << "Error (find_free_data_block): block_idx out of bitmap range." << std::endl;
            fs_log("Error (find_free_data_block): block_idx out of bitmap range.");
            disk_file.close();
            return -1; 
        }

        // Biti kontrol et (0 mı?)
        if (!(bitmap_buffer[byte_index] & bit_to_char_mask(bit_index_in_byte))) {
            // Boş blok bulundu! Bu bloğu meşgul olarak işaretle (1 yap)
            bitmap_buffer[byte_index] |= bit_to_char_mask(bit_index_in_byte);

            // 3. Güncellenmiş bitmap'i diske geri yaz
            disk_file.seekp(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
            disk_file.write(bitmap_buffer, BITMAP_SIZE_BYTES);
            if (!disk_file) {
                std::cerr << "Error (find_free_data_block): Could not write updated bitmap to disk." << std::endl;
                fs_log("Error (find_free_data_block): Could not write updated bitmap.");
                // TODO: Bu durumda yapılan değişiklik geri alınmalı mı? Ya da en azından loglanmalı.
                disk_file.close();
                return -1; // Yazma hatası
            }

            disk_file.close();
            fs_log(("Data block " + std::to_string(block_idx) + " allocated.").c_str());
            return static_cast<int>(block_idx);
        }
    }

    // Boş blok bulunamadı
    disk_file.close();
    std::cout << "Warning (find_free_data_block): No free data blocks available. Disk might be full." << std::endl;
    fs_log("Warning (find_free_data_block): No free data blocks available.");
    return -1; 
}

void free_data_block(int block_index) {
    if (block_index < 0 || static_cast<unsigned int>(block_index) >= NUM_DATA_BLOCKS) {
        std::cerr << "Error (free_data_block): Invalid block index " << block_index << ". Max is " << NUM_DATA_BLOCKS -1 << std::endl;
        fs_log(("Error (free_data_block): Invalid block index " + std::to_string(block_index)).c_str());
        return;
    }

    ensure_disk_initialized();

    std::fstream disk_file(DISK_FILENAME, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk_file) {
        std::cerr << "Error (free_data_block): Could not open disk file '" << DISK_FILENAME << "'." << std::endl;
        fs_log("Error (free_data_block): Could not open disk file.");
        return;
    }

    // 1. Bitmap'i oku (sadece ilgili byte'ı okumak da bir optimizasyon olabilirdi ama şimdilik tümünü okuyalım)
    char bitmap_buffer[BITMAP_SIZE_BYTES];
    disk_file.seekg(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
    disk_file.read(bitmap_buffer, BITMAP_SIZE_BYTES);
    if (!disk_file) {
        std::cerr << "Error (free_data_block): Could not read bitmap from disk." << std::endl;
        fs_log("Error (free_data_block): Could not read bitmap.");
        disk_file.close();
        return;
    }

    // 2. İlgili bloğun bitini 0 yap (boş olarak işaretle)
    unsigned int byte_index = static_cast<unsigned int>(block_index) / 8;
    unsigned int bit_index_in_byte = static_cast<unsigned int>(block_index) % 8;

    if (byte_index >= BITMAP_SIZE_BYTES) { // Güvenlik
         std::cerr << "Error (free_data_block): calculated byte_index out of bitmap range." << std::endl;
         fs_log("Error (free_data_block): calculated byte_index out of bitmap range.");
         disk_file.close();
         return;
    }
    
    // Biti kontrol et (1 miydi? Zaten 0 ise bir şey yapmaya gerek yok ama loglayabiliriz)
    if (!(bitmap_buffer[byte_index] & bit_to_char_mask(bit_index_in_byte))) {
        std::cout << "Warning (free_data_block): Block " << block_index << " was already free." << std::endl;
        fs_log(("Warning (free_data_block): Block " + std::to_string(block_index) + " was already free.").c_str());
    }

    bitmap_buffer[byte_index] &= ~(bit_to_char_mask(bit_index_in_byte)); // Biti 0 yap

    // 3. Güncellenmiş bitmap'i diske geri yaz
    disk_file.seekp(BITMAP_START_OFFSET_IN_METADATA, std::ios::beg);
    disk_file.write(bitmap_buffer, BITMAP_SIZE_BYTES);
    if (!disk_file) {
        std::cerr << "Error (free_data_block): Could not write updated bitmap to disk." << std::endl;
        fs_log("Error (free_data_block): Could not write updated bitmap.");
        disk_file.close();
        return;
    }

    disk_file.close();
    fs_log(("Data block " + std::to_string(block_index) + " freed.").c_str());
} 