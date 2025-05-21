#include <iostream>
#include "fs.hpp" // Dosya sistemi fonksiyonlarını kullanabilmek için
#include <vector>   // Tahsis edilen blokları takip etmek için
#include <algorithm> // std::find için
#include <set>      // Benzersiz blokları saymak için

// Bitmap testleri için fs.hpp'den bazı sabitlere erişim gerekebilir
// Eğer fs.hpp içinde değillerse, burada tanımlamamız veya fs.hpp'ye eklememiz gerekebilir.
// Şimdilik fs.hpp'de olduklarını varsayıyorum:
// extern const unsigned int NUM_DATA_BLOCKS; // fs.hpp'de zaten var

void test_bitmap_functions() {
    std::cout << "\n--- Bitmap Yönetimi Testleri Başlıyor ---" << std::endl;

    std::vector<int> allocated_blocks_vector; // İsim değişikliği, global scope ile karışmasın diye
    std::set<int> unique_allocated_blocks_set; // İsim değişikliği

    // 1. İlk Blok Tahsisi Testi
    std::cout << "\n[Test 1: İlk Blok Tahsisi (5 blok)]" << std::endl;
    for (int i = 0; i < 5; ++i) {
        int block_idx = find_free_data_block();
        if (block_idx != -1) {
            std::cout << "  Blok " << block_idx << " tahsis edildi." << std::endl;
            allocated_blocks_vector.push_back(block_idx);
            unique_allocated_blocks_set.insert(block_idx);
        } else {
            std::cout << "  Blok tahsis edilemedi (disk dolu olabilir veya hata oluştu)." << std::endl;
            break; 
        }
    }
    std::cout << "  Mevcut tahsisli bloklar (vektör): " << allocated_blocks_vector.size() << std::endl;

    // 2. Blok Serbest Bırakma Testi
    std::cout << "\n[Test 2: Blok Serbest Bırakma (2 blok)]" << std::endl;
    if (allocated_blocks_vector.size() >= 1) { // En az 1 blok varsa ilkini serbest bırak
        int block_to_free1 = allocated_blocks_vector[0];
        
        std::cout << "  Blok " << block_to_free1 << " serbest bırakılıyor..." << std::endl;
        free_data_block(block_to_free1);
        auto it1 = std::find(allocated_blocks_vector.begin(), allocated_blocks_vector.end(), block_to_free1);
        if (it1 != allocated_blocks_vector.end()) allocated_blocks_vector.erase(it1);
        unique_allocated_blocks_set.erase(block_to_free1);

        if (allocated_blocks_vector.size() >= 1) { // Kalanlardan birini daha serbest bırak (eğer varsa)
            int block_to_free2 = allocated_blocks_vector[0]; // Kalanların ilki (indeksi değişmiş olabilir)
             std::cout << "  Blok " << block_to_free2 << " serbest bırakılıyor..." << std::endl;
            free_data_block(block_to_free2);
            auto it2 = std::find(allocated_blocks_vector.begin(), allocated_blocks_vector.end(), block_to_free2);
            if (it2 != allocated_blocks_vector.end()) allocated_blocks_vector.erase(it2);
            unique_allocated_blocks_set.erase(block_to_free2);
        } else {
            std::cout << "  Serbest bırakılacak ikinci blok kalmadı." << std::endl;
        }
    } else {
        std::cout << "  Serbest bırakma testi için yeterli blok tahsis edilemedi." << std::endl;
    }
    std::cout << "  Mevcut tahsisli bloklar (vektör): " << allocated_blocks_vector.size() << std::endl;
    std::cout << "  Mevcut benzersiz tahsisli blok sayısı (set): " << unique_allocated_blocks_set.size() << std::endl;

    // 3. Yeniden Tahsis Testi
    std::cout << "\n[Test 3: Yeniden Tahsis (3 blok - serbest bırakılanlar öncelikli olmalı)]" << std::endl;
    for (int i = 0; i < 3; ++i) {
        int block_idx = find_free_data_block();
        if (block_idx != -1) {
            std::cout << "  Blok " << block_idx << " yeniden tahsis edildi." << std::endl;
            allocated_blocks_vector.push_back(block_idx);
            unique_allocated_blocks_set.insert(block_idx);
        } else {
            std::cout << "  Blok yeniden tahsis edilemedi." << std::endl;
            break;
        }
    }
    std::cout << "  Mevcut tahsisli bloklar (vektör): " << allocated_blocks_vector.size() << std::endl;
    std::cout << "  Mevcut benzersiz tahsisli blok sayısı (set): " << unique_allocated_blocks_set.size() << std::endl;


    // Önceki testlerden kalan tüm blokları temizleyelim ki tüm disk doldurma testi doğru çalışsın
    std::cout << "\n  Tüm disk doldurma testinden önce bilinen bloklar serbest bırakılıyor..." << std::endl;
    // unique_allocated_blocks_set üzerinden gitmek daha doğru, çünkü allocated_blocks_vector artık güncel değil
    for (int block_idx : unique_allocated_blocks_set) { 
        std::cout << "    Blok " << block_idx << " serbest bırakılıyor (temizlik)." << std::endl;
        free_data_block(block_idx);
    }
    allocated_blocks_vector.clear();
    unique_allocated_blocks_set.clear();
    std::cout << "  Temizlik sonrası mevcut tahsisli bloklar (vektör): " << allocated_blocks_vector.size() << std::endl;
    std::cout << "  Temizlik sonrası benzersiz tahsisli bloklar (set): " << unique_allocated_blocks_set.size() << std::endl;
    

    // 4. Tüm Blokları Doldurma Testi
    std::cout << "\n[Test 4: Tüm Blokları Doldurma]" << std::endl;
    std::cout << "  (Toplam " << NUM_DATA_BLOCKS << " veri bloğu var)" << std::endl;
    int successfully_allocated_count = 0;
    // Bu test için allocated_blocks_vector ve unique_allocated_blocks_set'i yeniden kullanalım (yukarıda temizlendi)
    for (unsigned int i = 0; i < NUM_DATA_BLOCKS + 5; ++i) { // NUM_DATA_BLOCKS'tan biraz fazla deneyelim
        int block_idx = find_free_data_block();
        if (block_idx != -1) {
            allocated_blocks_vector.push_back(block_idx); // Takip için ekle
            unique_allocated_blocks_set.insert(block_idx);    // Takip için ekle
            successfully_allocated_count++;
        } else {
            std::cout << "  Blok tahsis edilemedi (i=" << i << "). Muhtemelen disk doldu." << std::endl;
            if (successfully_allocated_count < NUM_DATA_BLOCKS) { // successfully_allocated_count < NUM_DATA_BLOCKS olmalı
                 std::cout << "  UYARI: Disk beklenenden önce doldu! Tahsis edilen: " << successfully_allocated_count << "/" << NUM_DATA_BLOCKS << std::endl;
            }
            break; 
        }
    }
    std::cout << "  Toplam " << successfully_allocated_count << " blok başarıyla tahsis edildi." << std::endl;
    if (successfully_allocated_count != NUM_DATA_BLOCKS) {
        std::cout << "  BEKLENTİ UYUMSUZLUĞU: " << NUM_DATA_BLOCKS << " blok tahsis edilmesi beklenirken " << successfully_allocated_count << " edildi." << std::endl;
    }
    std::cout << "  Benzersiz tahsis edilen blok sayısı (set): " << unique_allocated_blocks_set.size() << std::endl;
    if (unique_allocated_blocks_set.size() != (unsigned int)successfully_allocated_count) {
        std::cout << "  UYARI: Benzersiz set boyutu ile başarılı sayım uyuşmuyor! Set: " << unique_allocated_blocks_set.size() << ", Sayım: " << successfully_allocated_count << std::endl;
    }


    // 5. Tüm Blokları Serbest Bırakma Testi
    std::cout << "\n[Test 5: Tüm Tahsisli Blokları Serbest Bırakma]" << std::endl;
    std::cout << "  " << unique_allocated_blocks_set.size() << " benzersiz blok serbest bırakılacak..." << std::endl;
    for (int block_idx : unique_allocated_blocks_set) { 
        free_data_block(block_idx);
    }
    allocated_blocks_vector.clear(); // Temizle
    unique_allocated_blocks_set.clear(); // Temizle
    std::cout << "  Tüm bloklar serbest bırakıldı (unique_allocated_blocks_set şimdi boş olmalı: " << unique_allocated_blocks_set.size() << ")." << std::endl;

    // 6. Serbest Bırakma Sonrası Tahsis Kontrolü
    std::cout << "\n[Test 6: Tüm bloklar serbest bırakıldıktan sonra 1 blok tahsis etme]" << std::endl;
    int allocated_after_full_clear = find_free_data_block(); // Yeni değişken adı
    if (allocated_after_full_clear != -1) {
        std::cout << "  Blok " << allocated_after_full_clear << " başarıyla tahsis edildi (disk boş olmalıydı)." << std::endl;
        free_data_block(allocated_after_full_clear); // Test için hemen geri bırak
        std::cout << "  Test bloğu " << allocated_after_full_clear << " geri serbest bırakıldı." << std::endl;
    } else {
        std::cout << "  HATA: Tüm bloklar serbest bırakıldıktan sonra yeni blok tahsis edilemedi!" << std::endl;
    }

    // 7. Geçersiz Blok Serbest Bırakma Testleri
    std::cout << "\n[Test 7: Geçersiz Blok Serbest Bırakma Denemeleri]" << std::endl;
    std::cout << "  free_data_block(-1) çağrılıyor..." << std::endl;
    free_data_block(-1);
    std::cout << "  free_data_block(" << NUM_DATA_BLOCKS << ") çağrılıyor (NUM_DATA_BLOCKS olan blok indeksi geçersizdir)..." << std::endl;
    free_data_block(NUM_DATA_BLOCKS); 
    std::cout << "  free_data_block(" << NUM_DATA_BLOCKS + 100 << ") çağrılıyor..." << std::endl;
    free_data_block(NUM_DATA_BLOCKS + 100);

    // 8. Zaten Boş Olan Bloğu Serbest Bırakma Testi
    std::cout << "\n[Test 8: Zaten Boş Olan Bloğu Serbest Bırakma]" << std::endl;
    int block_for_already_free_test = find_free_data_block(); // Yeni değişken adı
    if (block_for_already_free_test != -1) {
        std::cout << "  Geçici test bloğu " << block_for_already_free_test << " tahsis edildi." << std::endl;
        std::cout << "  Blok " << block_for_already_free_test << " ilk kez serbest bırakılıyor..." << std::endl;
        free_data_block(block_for_already_free_test);
        std::cout << "  Blok " << block_for_already_free_test << " ikinci kez (zaten boşken) serbest bırakılıyor..." << std::endl;
        free_data_block(block_for_already_free_test); // Bu çağrıda "already free" uyarısı beklenir.
    } else {
        std::cout << "  Zaten boş blok testi için geçici blok tahsis edilemedi." << std::endl;
    }

    std::cout << "\n--- Bitmap Yönetimi Testleri Tamamlandı ---" << std::endl;
}

void test_file_write_operations() {
    std::cout << "\n--- Dosya Yazma İşlemleri Testleri Başlıyor ---" << std::endl;

    // Test 1: Basit Yazma Testi
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 1: Basit Yazma (Yeni Dosya)]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    fs_format(); 
    const char* file1 = "testfile1.txt";
    const char* data1 = "Hello World!";
    int size1 = strlen(data1);
    std::cout << "  ACTION: '" << file1 << "' oluşturuluyor..." << std::endl;
    fs_create(file1);
    if (fs_exists(file1)) {
        std::cout << "  ACTION: '" << file1 << "' dosyasına \"" << data1 << "\" yazılıyor (" << size1 << " byte)..." << std::endl;
        int bytes_written = fs_write(file1, data1, size1);
        std::cout << "    fs_write sonucu: " << bytes_written << " (Beklenen: " << size1 << ")" << std::endl;
        if (bytes_written == size1) std::cout << "    [SUCCESS] Yazılan byte sayısı doğru." << std::endl;
        else std::cout << "    [FAILURE] Yazılan byte sayısı yanlış!" << std::endl;

        int reported_size = fs_size(file1);
        std::cout << "    fs_size(\"" << file1 << "\") sonucu: " << reported_size << " (Beklenen: " << size1 << ")" << std::endl;
        if (reported_size == size1) std::cout << "    [SUCCESS] Dosya boyutu doğru." << std::endl;
        else std::cout << "    [FAILURE] Dosya boyutu yanlış!" << std::endl;
    } else {
        std::cout << "  [FAILURE] " << file1 << " oluşturulamadı!" << std::endl;
    }
    std::cout << "  Test 1 Sonrası Durum:" << std::endl; fs_ls();

    // Test 2: Üzerine Yazma Testi (Overwrite)
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 2: Üzerine Yazma (Varolan Dosya)]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    fs_format(); 
    const char* file2_ow = "overwrite.txt";
    const char* initial_data_ow = "Initial content.";
    int initial_size_ow = strlen(initial_data_ow);
    std::cout << "  ACTION: '" << file2_ow << "' oluşturuluyor ve ilk veri yazılıyor..." << std::endl;
    fs_create(file2_ow);
    fs_write(file2_ow, initial_data_ow, initial_size_ow);
    std::cout << "  İlk Yazma Sonrası Durum:" << std::endl; fs_ls();
    
    const char* data2_ow = "This is a longer string for overwriting.";
    int size2_ow = strlen(data2_ow);
    if (fs_exists(file2_ow)) {
        std::cout << "  ACTION: '" << file2_ow << "' dosyasına (üzerine) \"" << data2_ow << "\" yazılıyor (" << size2_ow << " byte)..." << std::endl;
        int bytes_written2 = fs_write(file2_ow, data2_ow, size2_ow);
        std::cout << "    fs_write sonucu: " << bytes_written2 << " (Beklenen: " << size2_ow << ")" << std::endl;
        if (bytes_written2 == size2_ow) std::cout << "    [SUCCESS] Yazılan byte sayısı doğru (üzerine uzun yazma)." << std::endl;
        else std::cout << "    [FAILURE] Yazılan byte sayısı yanlış (üzerine uzun yazma)!" << std::endl;
        
        int reported_size2 = fs_size(file2_ow);
        std::cout << "    fs_size(\"" << file2_ow << "\") sonucu: " << reported_size2 << " (Beklenen: " << size2_ow << ")" << std::endl;
        if (reported_size2 == size2_ow) std::cout << "    [SUCCESS] Dosya boyutu doğru (üzerine uzun yazma)." << std::endl;
        else std::cout << "    [FAILURE] Dosya boyutu yanlış (üzerine uzun yazma)!" << std::endl;
        std::cout << "  Uzun Üzerine Yazma Sonrası Durum:" << std::endl; fs_ls();

        const char* data3_ow = "Short";
        int size3_ow = strlen(data3_ow);
        std::cout << "  ACTION: '" << file2_ow << "' dosyasına (üzerine) \"" << data3_ow << "\" yazılıyor (" << size3_ow << " byte)..." << std::endl;
        int bytes_written3 = fs_write(file2_ow, data3_ow, size3_ow);
        std::cout << "    fs_write sonucu: " << bytes_written3 << " (Beklenen: " << size3_ow << ")" << std::endl;
        if (bytes_written3 == size3_ow) std::cout << "    [SUCCESS] Yazılan byte sayısı doğru (küçük üzerine yazma)." << std::endl;
        else std::cout << "    [FAILURE] Yazılan byte sayısı yanlış (küçük üzerine yazma)!" << std::endl;

        int reported_size3 = fs_size(file2_ow);
        std::cout << "    fs_size(\"" << file2_ow << "\") sonucu: " << reported_size3 << " (Beklenen: " << size3_ow << ")" << std::endl;
        if (reported_size3 == size3_ow) std::cout << "    [SUCCESS] Dosya boyutu doğru (küçük üzerine yazma)." << std::endl;
        else std::cout << "    [FAILURE] Dosya boyutu yanlış (küçük üzerine yazma)!" << std::endl;
    } else {
        std::cout << "  [SKIPPED] " << file2_ow << " bulunamadığı için üzerine yazma testi atlandı." << std::endl;
    }
    std::cout << "  Test 2 Sonrası Durum:" << std::endl; fs_ls();

    // Test 3: Sıfır Byte Yazma (Truncate)
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 3: Sıfır Byte Yazma (Dosyayı Boşaltma)]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    fs_format();
    const char* file3_truncate = "truncate_me.txt";
    std::cout << "  ACTION: '" << file3_truncate << "' oluşturuluyor ve içine veri yazılıyor..." << std::endl;
    fs_create(file3_truncate);
    fs_write(file3_truncate, "some initial data to be truncated", strlen("some initial data to be truncated"));
    std::cout << "  İlk Yazma Sonrası Durum:" << std::endl; fs_ls();

    std::cout << "  ACTION: '" << file3_truncate << "' dosyasına 0 byte yazılıyor (boşaltma)..." << std::endl;
    int bytes_written_truncate = fs_write(file3_truncate, "", 0);
    std::cout << "    fs_write sonucu: " << bytes_written_truncate << " (Beklenen: 0)" << std::endl;
    if (bytes_written_truncate == 0) std::cout << "    [SUCCESS] Yazılan byte sayısı doğru (0 byte)." << std::endl;
    else std::cout << "    [FAILURE] Yazılan byte sayısı yanlış (0 byte)!" << std::endl;

    int reported_size_truncate = fs_size(file3_truncate);
    std::cout << "    fs_size(\"" << file3_truncate << "\") sonucu: " << reported_size_truncate << " (Beklenen: 0)" << std::endl;
    if (reported_size_truncate == 0) std::cout << "    [SUCCESS] Dosya boyutu doğru (0 byte)." << std::endl;
    else std::cout << "    [FAILURE] Dosya boyutu yanlış (0 byte)!" << std::endl;
    std::cout << "  Test 3 Sonrası Durum:" << std::endl; fs_ls();

    // Test 4: Var Olmayan Dosyaya Yazma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 4: Var Olmayan Dosyaya Yazma]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    fs_format();
    const char* non_existent_file = "idontexist.txt";
    const char* data_ne = "test";
    int size_ne = strlen(data_ne);
    std::cout << "  ACTION: Var olmayan '" << non_existent_file << "' dosyasına yazılmaya çalışılıyor..." << std::endl;
    int bytes_written_ne = fs_write(non_existent_file, data_ne, size_ne);
    std::cout << "    fs_write sonucu: " << bytes_written_ne << " (Beklenen: -5)" << std::endl;
    if (bytes_written_ne == -5) std::cout << "    [SUCCESS] Doğru hata kodu (-5) alındı." << std::endl;
    else std::cout << "    [FAILURE] Yanlış hata kodu (" << bytes_written_ne << ") veya işlem beklenmedik şekilde başarılı oldu!" << std::endl;
    std::cout << "  Test 4 Sonrası Durum:" << std::endl; fs_ls();

    // Test 5: Disk Dolu Senaryosu
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 5: Disk Doldurma Denemesi]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    fs_format(); 
    const char* big_file_prefix = "bigfile_";
    // Her biri BLOCK_SIZE_BYTES veri içeren dosyalar oluşturalım
    char single_block_data[BLOCK_SIZE_BYTES]; 
    memset(single_block_data, 'X', BLOCK_SIZE_BYTES -1);
    single_block_data[BLOCK_SIZE_BYTES -1] = '\0'; // Null terminate for strlen, though fs_write uses size
    int single_block_size = BLOCK_SIZE_BYTES; // Yazılacak boyut

    std::cout << "  Toplam veri bloğu sayısı: " << NUM_DATA_BLOCKS << std::endl;
    std::cout << "  Maksimum FileInfo slotu: " << MAX_FILES_CALCULATED << std::endl;
    std::cout << "  Her dosya için yazılacak boyut: " << single_block_size << " (1 blok kaplayacak)" << std::endl;

    int files_to_try = std::min((int)NUM_DATA_BLOCKS + 5, MAX_FILES_CALCULATED + 5); // Hem blok hem de fileinfo limitini zorla
    bool disk_full_error_received = false;

    for (int i = 0; i < files_to_try; ++i) {
        char filename_buf[30];
        snprintf(filename_buf, 30, "%s%02d.txt", big_file_prefix, i);
        
        std::cout << "  DENEME " << i + 1 << ": '" << filename_buf << "' oluşturuluyor..." << std::endl;
        fs_create(filename_buf);
        
        if (!fs_exists(filename_buf)) {
            std::cout << "    [FAILURE] Dosya " << filename_buf << " oluşturulamadı! (MAX_FILES_CALCULATED limitine ulaşılmış olabilir)" << std::endl;
            std::cout << "    Mevcut dosya sayısı üst sınırı (MAX_FILES_CALCULATED): " << MAX_FILES_CALCULATED << std::endl;
            fs_ls();
            break; 
        }

        std::cout << "    ACTION: '" << filename_buf << "' dosyasına " << single_block_size << " byte yazılıyor..." << std::endl;
        int write_res = fs_write(filename_buf, single_block_data, single_block_size);
        
        if (write_res < 0) {
            std::cout << "    fs_write hatası: " << write_res << " (Dosya: " << filename_buf << ")" << std::endl;
            if (write_res == -6) {
                 std::cout << "    [SUCCESS] Disk dolu/yetersiz ardışık alan hatası (-6) alındı." << std::endl;
                 disk_full_error_received = true;
            } else {
                 std::cout << "    [WARNING] Beklenmeyen hata kodu: " << write_res << std::endl;
            }
            break; 
        } else {
            std::cout << "    Yazma başarılı (" << write_res << " byte)." << std::endl;
        }
    }
    std::cout << "  Disk Doldurma Testi Sonrası Durum:" << std::endl;
    fs_ls();
    if (!disk_full_error_received && files_to_try > NUM_DATA_BLOCKS && files_to_try > MAX_FILES_CALCULATED) {
        std::cout << "  [WARNING] Disk doldurma denemesi tamamlandı ancak beklenen -6 hatası alınmadı. Disk yapısını kontrol et." << std::endl;
    }

    std::cout << "\n--- Dosya Yazma İşlemleri Testleri Tamamlandı ---" << std::endl;
}

void test_file_read_operations() {
    std::cout << "\n--- Dosya Okuma İşlemleri Testleri Başlıyor ---" << std::endl;

    // Test Hazırlığı: Okuma için bir dosya oluşturalım ve içine veri yazalım.
    fs_format(); 
    const char* filename = "readme.txt";
    const char* content = "This is a test file for fs_read.";
    int content_size = strlen(content);

    std::cout << "  Hazırlık: '" << filename << "' oluşturuluyor ve veri yazılıyor..." << std::endl;
    fs_create(filename);
    int written = fs_write(filename, content, content_size);
    if (written != content_size) {
        std::cout << "  [HAZIRLIK BAŞARISIZ] '" << filename << "' dosyasına yazılamadı. Okuma testleri atlanıyor." << std::endl;
        return;
    }
    std::cout << "  Hazırlık tamamlandı. Dosya içeriği: \"" << content << "\" (" << content_size << " byte)" << std::endl;
    fs_ls();

    char buffer[1024]; // Okuma için yeterince büyük bir buffer

    // Test 1: Var olmayan dosyadan okuma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 1: Var Olmayan Dosyadan Okuma Denemesi]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    const char* non_existing_file = "idontexist_read.txt";
    std::cout << "  ACTION: '" << non_existing_file << "' dosyasından okunmaya çalışılıyor (offset 0, size 10)..." << std::endl;
    buffer[0] = 'A'; // Buffer'ı önceden bilinen bir değere ayarlayalım
    fs_read(non_existing_file, 0, 10, buffer);
    // Beklenti: Hata mesajı alınmalı ve buffer değişmemeli veya boşaltılmalı.
    // fs_read şu an void olduğu için dönüş değeri yok, buffer'ı kontrol edebiliriz.
    if (buffer[0] == '\0' || buffer[0] == 'A') { // fs_read hata durumunda buffer'ı boşaltıyorsa veya dokunmuyorsa
        std::cout << "    [SUCCESS] Var olmayan dosyadan okuma denemesi beklendiği gibi (hata mesajı / buffer durumu)." << std::endl;
    } else {
        std::cout << "    [FAILURE] Var olmayan dosyadan okuma denemesi sonrası buffer beklenmedik: '" << buffer << "'" << std::endl;
    }

    // Test 2: Geçersiz filename (nullptr)
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 2: Geçersiz Dosya Adı (nullptr) ile Okuma Denemesi]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: nullptr dosya adıyla okunmaya çalışılıyor..." << std::endl;
    buffer[0] = 'A';
    fs_read(nullptr, 0, 10, buffer);
    if (buffer[0] == '\0' || buffer[0] == 'A') {
        std::cout << "    [SUCCESS] nullptr dosya adıyla okuma denemesi beklendiği gibi." << std::endl;
    } else {
        std::cout << "    [FAILURE] nullptr dosya adıyla okuma denemesi sonrası buffer beklenmedik." << std::endl;
    }

    // Test 3: Geçersiz offset (negatif)
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 3: Negatif Offset ile Okuma Denemesi]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: '" << filename << "' dosyasından negatif offset (-5) ile okunmaya çalışılıyor..." << std::endl;
    buffer[0] = 'A';
    fs_read(filename, -5, 10, buffer);
    if (buffer[0] == '\0' || buffer[0] == 'A') {
        std::cout << "    [SUCCESS] Negatif offset ile okuma denemesi beklendiği gibi." << std::endl;
    } else {
        std::cout << "    [FAILURE] Negatif offset ile okuma denemesi sonrası buffer beklenmedik." << std::endl;
    }

    // Test 4: Geçersiz size (negatif)
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 4: Negatif Size ile Okuma Denemesi]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: '" << filename << "' dosyasından negatif size (-10) ile okunmaya çalışılıyor..." << std::endl;
    buffer[0] = 'A';
    fs_read(filename, 0, -10, buffer);
    if (buffer[0] == '\0' || buffer[0] == 'A') {
        std::cout << "    [SUCCESS] Negatif size ile okuma denemesi beklendiği gibi." << std::endl;
    } else {
        std::cout << "    [FAILURE] Negatif size ile okuma denemesi sonrası buffer beklenmedik." << std::endl;
    }

    // Test 5: Null buffer ile okuma (size > 0 iken)
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 5: Null Buffer ile Okuma Denemesi (size > 0)]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: '" << filename << "' dosyasından null buffer ile okunmaya çalışılıyor..." << std::endl;
    // Bu test doğrudan bir çökme yaratabilir eğer fs_read içinde null check yoksa.
    // fs_read içinde null buffer kontrolü eklediğimiz için hata mesajı bekliyoruz.
    fs_read(filename, 0, 5, nullptr); 
    // Bu senaryo için çıktıya bakarak manuel kontrol gerekebilir, çünkü program çökebilir veya sadece cerr'e yazar.
    std::cout << "    [INFO] Null buffer testi çağrıldı. fs_read'in hata vermesi bekleniyor." << std::endl;


    // Test 6: Offset dosya boyutunun dışında
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 6: Offset Dosya Boyutunun Dışında Okuma Denemesi]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    int out_of_bounds_offset = content_size + 5;
    std::cout << "  ACTION: '" << filename << "' dosyasından geçersiz offset (" << out_of_bounds_offset << ") ile okunmaya çalışılıyor..." << std::endl;
    buffer[0] = 'A';
    fs_read(filename, out_of_bounds_offset, 5, buffer);
    if (buffer[0] == '\0' || buffer[0] == 'A') {
        std::cout << "    [SUCCESS] Sınır dışı offset ile okuma denemesi beklendiği gibi." << std::endl;
    } else {
        std::cout << "    [FAILURE] Sınır dışı offset ile okuma denemesi sonrası buffer beklenmedik." << std::endl;
    }

    // Test 7: Size 0 ile okuma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 7: Size 0 ile Okuma Denemesi]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: '" << filename << "' dosyasından size 0 ile okunmaya çalışılıyor..." << std::endl;
    strcpy(buffer, "original_data"); // Buffer'a veri koyalım
    fs_read(filename, 0, 0, buffer);
    if (strcmp(buffer, "") == 0 || strcmp(buffer, "original_data") == 0) { // fs_read size=0'da buffer'ı boşaltabilir veya dokunmayabilir
        std::cout << "    [SUCCESS] Size 0 ile okuma beklendiği gibi. Buffer: '" << buffer << "'" << std::endl;
    } else {
        std::cout << "    [FAILURE] Size 0 ile okuma sonrası buffer beklenmedik: '" << buffer << "'" << std::endl;
    }

    // Test 8: Dosyanın tamamını okuma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 8: Dosyanın Tamamını Okuma]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: '" << filename << "' dosyasının tamamı (offset 0, size " << content_size << ") okunuyor..." << std::endl;
    memset(buffer, 0, sizeof(buffer)); // Buffer'ı temizle
    fs_read(filename, 0, content_size, buffer);
    std::cout << "    Okunan: '" << buffer << "'" << std::endl;
    std::cout << "    Beklenen: '" << content << "'" << std::endl;
    if (strcmp(buffer, content) == 0) {
        std::cout << "    [SUCCESS] Dosyanın tamamı doğru okundu." << std::endl;
    } else {
        std::cout << "    [FAILURE] Dosyanın tamamı yanlış okundu!" << std::endl;
    }

    // Test 9: Dosyanın başından kısmi okuma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 9: Dosyanın Başından Kısmi Okuma]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    int partial_size_start = 5;
    std::string expected_partial_start(content, partial_size_start);
    std::cout << "  ACTION: '" << filename << "' dosyasının başından " << partial_size_start << " byte (offset 0) okunuyor..." << std::endl;
    memset(buffer, 0, sizeof(buffer));
    fs_read(filename, 0, partial_size_start, buffer);
    std::cout << "    Okunan: '" << buffer << "'" << std::endl;
    std::cout << "    Beklenen: '" << expected_partial_start << "'" << std::endl;
    if (strcmp(buffer, expected_partial_start.c_str()) == 0) {
        std::cout << "    [SUCCESS] Dosyanın başından kısmi okuma doğru." << std::endl;
    } else {
        std::cout << "    [FAILURE] Dosyanın başından kısmi okuma yanlış!" << std::endl;
    }

    // Test 10: Dosyanın ortasından kısmi okuma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 10: Dosyanın Ortasından Kısmi Okuma]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    int partial_offset_middle = 8;
    int partial_size_middle = 10;
    if (content_size >= partial_offset_middle + partial_size_middle) {
        std::string expected_partial_middle(content + partial_offset_middle, partial_size_middle);
        std::cout << "  ACTION: '" << filename << "' dosyasının ortasından " << partial_size_middle << " byte (offset " << partial_offset_middle << ") okunuyor..." << std::endl;
        memset(buffer, 0, sizeof(buffer));
        fs_read(filename, partial_offset_middle, partial_size_middle, buffer);
        std::cout << "    Okunan: '" << buffer << "'" << std::endl;
        std::cout << "    Beklenen: '" << expected_partial_middle << "'" << std::endl;
        if (strcmp(buffer, expected_partial_middle.c_str()) == 0) {
            std::cout << "    [SUCCESS] Dosyanın ortasından kısmi okuma doğru." << std::endl;
        } else {
            std::cout << "    [FAILURE] Dosyanın ortasından kısmi okuma yanlış!" << std::endl;
        }
    } else {
        std::cout << "  [SKIPPED] Dosya, ortadan okuma testi için yeterince büyük değil." << std::endl;
    }

    // Test 11: Dosyanın sonundan kısmi okuma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 11: Dosyanın Sonundan Kısmi Okuma]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    int partial_size_end = 6;
    if (content_size >= partial_size_end) {
        int partial_offset_end = content_size - partial_size_end;
        std::string expected_partial_end(content + partial_offset_end, partial_size_end);
        std::cout << "  ACTION: '" << filename << "' dosyasının sonundan " << partial_size_end << " byte (offset " << partial_offset_end << ") okunuyor..." << std::endl;
        memset(buffer, 0, sizeof(buffer));
        fs_read(filename, partial_offset_end, partial_size_end, buffer);
        std::cout << "    Okunan: '" << buffer << "'" << std::endl;
        std::cout << "    Beklenen: '" << expected_partial_end << "'" << std::endl;
        if (strcmp(buffer, expected_partial_end.c_str()) == 0) {
            std::cout << "    [SUCCESS] Dosyanın sonundan kısmi okuma doğru." << std::endl;
        } else {
            std::cout << "    [FAILURE] Dosyanın sonundan kısmi okuma yanlış!" << std::endl;
        }
    } else {
        std::cout << "  [SKIPPED] Dosya, sondan okuma testi için yeterince büyük değil." << std::endl;
    }

    // Test 12: İstenen size dosya sonunu aşıyorsa
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 12: İstenen Size Dosya Sonunu Aşıyorsa]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    int offset_overflow = content_size / 2;
    int size_overflow = content_size; // Bu, offset ile birlikte kesinlikle dosya sonunu aşar
    std::string expected_overflow(content + offset_overflow, content_size - offset_overflow);
    std::cout << "  ACTION: '" << filename << "' dosyasından offset " << offset_overflow << " ile " << size_overflow << " byte okunmaya çalışılıyor (sona kadar okunmalı)..." << std::endl;
    memset(buffer, 0, sizeof(buffer));
    fs_read(filename, offset_overflow, size_overflow, buffer);
    std::cout << "    Okunan: '" << buffer << "'" << std::endl;
    std::cout << "    Beklenen: '" << expected_overflow << "'" << std::endl;
    if (strcmp(buffer, expected_overflow.c_str()) == 0) {
        std::cout << "    [SUCCESS] Dosya sonunu aşan okuma doğru (sadece sona kadar okundu)." << std::endl;
    } else {
        std::cout << "    [FAILURE] Dosya sonunu aşan okuma yanlış!" << std::endl;
    }
    
    // Test 13: Çoklu bloklara yayılan okuma testi (eğer dosya yeterince büyükse)
    // Bu test için daha büyük bir dosya oluşturalım.
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 13: Çoklu Bloklara Yayılan Okuma]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    const char* multiblock_file = "multiblock.txt";
    std::string multiblock_content;
    for(int i=0; i < BLOCK_SIZE_BYTES + (BLOCK_SIZE_BYTES/2); ++i) { // 1.5 blok boyutu
        multiblock_content += std::to_string(i % 10);
    }
    fs_create(multiblock_file);
    int written_mb = fs_write(multiblock_file, multiblock_content.c_str(), multiblock_content.length());
    if (written_mb != static_cast<int>(multiblock_content.length())) {
        std::cout << "  [HAZIRLIK BAŞARISIZ] '" << multiblock_file << "' dosyasına yazılamadı. Çoklu blok okuma testi atlanıyor." << std::endl;
    } else {
        std::cout << "  Hazırlık: '" << multiblock_file << "' oluşturuldu (" << multiblock_content.length() << " byte)." << std::endl;
        // a. Tamamını oku
        std::cout << "    ACTION (a): '" << multiblock_file << "' dosyasının tamamı okunuyor..." << std::endl;
        memset(buffer, 0, sizeof(buffer));
        fs_read(multiblock_file, 0, multiblock_content.length(), buffer);
        if (strcmp(buffer, multiblock_content.c_str()) == 0) {
            std::cout << "      [SUCCESS] Çoklu blok dosyasının tamamı doğru okundu." << std::endl;
        } else {
            std::cout << "      [FAILURE] Çoklu blok dosyasının tamamı yanlış okundu!" << std::endl;
            // std::cout << "        Okunan:   '" << buffer << "'" << std::endl;
            // std::cout << "        Beklenen: '" << multiblock_content.c_str() << "'" << std::endl;
        }

        // b. Blok sınırını aşan kısmi okuma (ilk bloktan başlayıp ikinci bloğa taşan)
        int offset_span = BLOCK_SIZE_BYTES - 10;
        int size_span = 20; // 10 byte ilk bloktan, 10 byte ikinci bloktan
        if (multiblock_content.length() >= static_cast<unsigned int>(offset_span + size_span)) {
             std::string expected_span = multiblock_content.substr(offset_span, size_span);
             std::cout << "    ACTION (b): '" << multiblock_file << "' offset " << offset_span << " dan " << size_span << " byte (blok aşan) okunuyor..." << std::endl;
            memset(buffer, 0, sizeof(buffer));
            fs_read(multiblock_file, offset_span, size_span, buffer);
            std::cout << "      Okunan: '" << buffer << "'" << std::endl;
            std::cout << "      Beklenen: '" << expected_span << "'" << std::endl;
            if (strcmp(buffer, expected_span.c_str()) == 0) {
                std::cout << "        [SUCCESS] Blokları aşan kısmi okuma doğru." << std::endl;
            } else {
                std::cout << "        [FAILURE] Blokları aşan kısmi okuma yanlış!" << std::endl;
            }
        } else {
            std::cout << "    [SKIPPED] Çoklu blok dosyası, blok aşan kısmi okuma için yeterli değil." << std::endl;
        }
    }

    std::cout << "\n--- Dosya Okuma İşlemleri Testleri Tamamlandı ---" << std::endl;
}

void test_file_cat_operations() {
    std::cout << "\n--- Dosya Cat İşlemleri Testleri Başlıyor ---" << std::endl;

    // Hazırlık: Test için dosyalar oluşturalım
    fs_format();
    const char* file_content_cat = "cat_content.txt";
    const char* content_cat = "This is content for fs_cat test.\nIt has multiple lines.";
    int content_cat_size = strlen(content_cat);

    const char* file_empty_cat = "empty_cat.txt";

    std::cout << "  Hazırlık: Test dosyaları oluşturuluyor..." << std::endl;
    fs_create(file_content_cat);
    fs_write(file_content_cat, content_cat, content_cat_size);
    fs_create(file_empty_cat); // fs_write çağrılmadığı için boş kalacak

    std::cout << "  Hazırlık sonrası dosya listesi:" << std::endl;
    fs_ls();

    // Test 1: İçeriği olan dosyayı cat ile yazdırma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 1: İçeriği Olan Dosyayı Yazdırma ('" << file_content_cat << "')]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_cat(\"" << file_content_cat << "\") çağrılıyor. Beklenen çıktı:" << std::endl;
    std::cout << "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv" << std::endl;
    std::cout << content_cat; // Beklenen çıktıyı önce yazdırıyoruz
    std::cout << "ʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌ" << std::endl;
    std::cout << "  fs_cat çıktısı:" << std::endl;
    std::cout << "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv" << std::endl;
    fs_cat(file_content_cat);
    std::cout << "ʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌ" << std::endl;
    // Başarı/başarısızlık durumu manuel olarak terminal çıktısından kontrol edilecek.

    // Test 2: Boş dosyayı cat ile yazdırma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 2: Boş Dosyayı Yazdırma ('" << file_empty_cat << "')]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_cat(\"" << file_empty_cat << "\") çağrılıyor. Beklenen çıktı: (hiçbir şey veya dosya boş mesajı)" << std::endl;
    std::cout << "  fs_cat çıktısı:" << std::endl;
    std::cout << "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv" << std::endl;
    fs_cat(file_empty_cat);
    std::cout << "ʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌʌ" << std::endl;
    // Başarı/başarısızlık durumu manuel olarak terminal çıktısından kontrol edilecek (fs_log'da mesaj olmalı).

    // Test 3: Var olmayan dosyayı cat ile yazdırma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 3: Var Olmayan Dosyayı Yazdırma ('idontexist_cat.txt')]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    const char* non_existent_cat = "idontexist_cat.txt";
    std::cout << "  ACTION: fs_cat(\"" << non_existent_cat << "\") çağrılıyor. Beklenen: Hata mesajı." << std::endl;
    fs_cat(non_existent_cat);
    // Başarı/başarısızlık durumu manuel olarak terminal çıktısından (stderr ve fs_log) kontrol edilecek.

    // Test 4: Nullptr dosya adı ile cat çağırma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 4: Nullptr Dosya Adı ile Yazdırma]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_cat(nullptr) çağrılıyor. Beklenen: Hata mesajı." << std::endl;
    fs_cat(nullptr);
    // Başarı/başarısızlık durumu manuel olarak terminal çıktısından (stderr ve fs_log) kontrol edilecek.
    
    // Test 5: Boş dosya adı ("") ile cat çağırma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 5: Boş Dosya Adı (\"\") ile Yazdırma]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_cat(\"\") çağrılıyor. Beklenen: Hata mesajı." << std::endl;
    fs_cat("");
    // Başarı/başarısızlık durumu manuel olarak terminal çıktısından (stderr ve fs_log) kontrol edilecek.

    std::cout << "\n--- Dosya Cat İşlemleri Testleri Tamamlandı ---" << std::endl;
}

void test_file_delete_operations() {
    std::cout << "\n--- Dosya Silme İşlemleri Testleri Başlıyor ---" << std::endl;

    // Test Hazırlığı: Silmek için birkaç dosya oluşturalım
    fs_format();
    const char* file_del_1 = "todelete_1.txt";
    const char* content_del_1 = "Some content for delete test 1.";
    const char* file_del_2 = "todelete_2.txt";
    const char* content_del_2 = "Another file to be deleted.";
    const char* file_del_3 = "persistent.txt"; // Bu silinmeyecek
    const char* content_del_3 = "This file should remain.";

    std::cout << "  Hazırlık: Test dosyaları oluşturuluyor..." << std::endl;
    fs_create(file_del_1);
    fs_write(file_del_1, content_del_1, strlen(content_del_1));
    fs_create(file_del_2);
    fs_write(file_del_2, content_del_2, strlen(content_del_2));
    fs_create(file_del_3);
    fs_write(file_del_3, content_del_3, strlen(content_del_3));

    std::cout << "  Hazırlık sonrası dosya listesi:" << std::endl;
    fs_ls();

    // Test 1: Var olan bir dosyayı silme
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 1: Var Olan Dosyayı Silme ('" << file_del_1 << "')]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_delete(\"" << file_del_1 << "\") çağrılıyor..." << std::endl;
    fs_delete(file_del_1);
    std::cout << "  Silme sonrası '" << file_del_1 << "' var mı? (fs_exists): " << (fs_exists(file_del_1) ? "EVET (HATA!)" : "HAYIR (Doğru)") << std::endl;
    std::cout << "  Test 1 sonrası dosya listesi:" << std::endl;
    fs_ls();

    // Test 2: Başka bir var olan dosyayı silme
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 2: Başka Bir Var Olan Dosyayı Silme ('" << file_del_2 << "')]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_delete(\"" << file_del_2 << "\") çağrılıyor..." << std::endl;
    fs_delete(file_del_2);
    std::cout << "  Silme sonrası '" << file_del_2 << "' var mı? (fs_exists): " << (fs_exists(file_del_2) ? "EVET (HATA!)" : "HAYIR (Doğru)") << std::endl;
    std::cout << "  Test 2 sonrası dosya listesi:" << std::endl;
    fs_ls();

    // Test 3: Var olmayan bir dosyayı silmeye çalışma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 3: Var Olmayan Dosyayı Silme ('idontexist_del.txt')]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    const char* non_existent_del = "idontexist_del.txt";
    std::cout << "  ACTION: fs_delete(\"" << non_existent_del << "\") çağrılıyor. Beklenen: Hata mesajı." << std::endl;
    fs_delete(non_existent_del);
    std::cout << "  Test 3 sonrası dosya listesi (değişmemeli):" << std::endl;
    fs_ls();

    // Test 4: Nullptr dosya adı ile silme çağırma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 4: Nullptr Dosya Adı ile Silme]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_delete(nullptr) çağrılıyor. Beklenen: Hata mesajı." << std::endl;
    fs_delete(nullptr);
    std::cout << "  Test 4 sonrası dosya listesi (değişmemeli):" << std::endl;
    fs_ls();

    // Test 5: Boş dosya adı ("") ile silme çağırma
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 5: Boş Dosya Adı (\"\") ile Silme]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_delete(\"\") çağrılıyor. Beklenen: Hata mesajı." << std::endl;
    fs_delete("");
    std::cout << "  Test 5 sonrası dosya listesi (değişmemeli):" << std::endl;
    fs_ls();

    // Test 6: Kalan son dosyayı da silme
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 6: Kalan Son Dosyayı Silme ('" << file_del_3 << "')]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_delete(\"" << file_del_3 << "\") çağrılıyor..." << std::endl;
    fs_delete(file_del_3);
    std::cout << "  Silme sonrası '" << file_del_3 << "' var mı? (fs_exists): " << (fs_exists(file_del_3) ? "EVET (HATA!)" : "HAYIR (Doğru)") << std::endl;
    std::cout << "  Test 6 sonrası dosya listesi (boş olmalı):" << std::endl;
    fs_ls();

    // Test 7: Disk boşken bir dosya silmeye çalışma (var olmayan dosya gibi davranmalı)
    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[Test 7: Disk Boşken Dosya Silme Denemesi ('anyfile.txt')]" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "  ACTION: fs_delete(\"anyfile.txt\") çağrılıyor. Beklenen: Hata mesajı." << std::endl;
    fs_delete("anyfile.txt");
    std::cout << "  Test 7 sonrası dosya listesi (hala boş olmalı):" << std::endl;
    fs_ls();

    std::cout << "\n--- Dosya Silme İşlemleri Testleri Tamamlandı ---" << std::endl;
}

int main() {
    // Dosya sistemini başlat (bu, disk yoksa fs_format'ı çağıracak)
    fs_init(); 

    std::cout << "\nSimpleFS initialized." << std::endl;
    // std::cout << "Please check the output of fs_format (if called) to see metadata parameters." << std::endl;
    
    // test_bitmap_functions();
    // test_file_write_operations();
    // test_file_read_operations(); 
    // test_file_cat_operations(); // Bir önceki test fonksiyonu çağrısı yorum satırı yapıldı.
    test_file_delete_operations(); // Yeni test fonksiyonu çağrısı

    // fs_create, fs_ls gibi diğer fonksiyonlar yeni metadata yapısıyla uyumlu hale getirildikten sonra
    // onların testleri buraya eklenebilir. Şimdilik sadece bitmap testleri.

    std::cout << "\nProgramdan çıkılıyor." << std::endl;
    return 0;
} 