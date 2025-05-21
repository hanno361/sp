# Proje: Basit Dosya Sistemi Simülatörü (SimpleFS)

## Genel Bakış
Bu proje, C++ dilinde sistem çağrıları kullanılarak basit bir dosya sistemi simülatörü geliştirmeyi amaçlamaktadır. Simülatör, gerçek bir dosya sistemine yazmak yerine `disk.sim` adında tek bir dosyayı sanal disk olarak kullanacaktır.

## Klasör Yapısı
- `main.cpp`: Kullanıcı arayüzü ve ana program mantığı.
- `fs.cpp`: Dosya sistemi fonksiyonlarının implementasyonu.
- `fs.hpp`: Dosya sistemi fonksiyonlarının ve veri yapılarının başlık dosyası.
- `Makefile`: Projenin derlenmesi için gerekli komutları içerir.
- `disk.sim`: Sanal disk dosyası.

## Geliştirme Adımları

### Aşama 1: Temel Kurulum ve Disk Yönetimi
- [x] `PROJECT_PLAN.md` dosyası oluşturuldu.
- [x] Temel dosya yapısı (`main.cpp`, `fs.cpp`, `fs.hpp`, `Makefile`) oluşturuldu.
- [x] Sanal disk dosyası (`disk.sim`) oluşturma ve başlatma (`fs_init` veya benzeri bir fonksiyon). (1MB boyutunda)
    - [x] Test Edildi
- [x] `fs_format()`: Diski sıfırlama ve boş bir dosya sistemi oluşturma.
    - [x] Metadata alanını (ilk 4KB) başlatma (toplam dosya sayısı 0 vb.).
    - [x] Test Edildi

### Aşama 2: Metadata ve Temel Dosya İşlemleri
- [x] Metadata yapısının `fs.hpp` içinde tanımlanması.
    - [x] Her dosya için: isim, boyut, başlangıç adresi, oluşturulma tarihi.
    - [x] Sabit blok boyutu (örn: 512 byte) tanımı.
- [x] `fs_create(char* filename)`: Yeni dosya oluşturma ve metadata güncelleme.
    - [x] Aynı isimde dosya oluşturmayı engelleme.
    - [x] Maksimum dosya sayısı kontrolü.
    - [x] Test Edildi
- [x] `fs_ls()`: Diskteki dosyaların isimlerini ve boyutlarını listeleme.
    - [x] Test Edildi
- [x] `fs_exists(char* filename)`: Dosyanın varlığını kontrol etme.
    - [x] Test Edildi
- [x] `fs_size(char* filename)`: Dosyanın boyutunu döndürme.
    - [x] Test Edildi

### Aşama 3: Dosya Okuma ve Yazma İşlemleri
- [x] **3.1: Metadata Yapısı Güncellemesi ve Temel Testler**
    - [x] `fs.hpp`: `Superblock`, `Bitmap` sabitleri, güncellenmiş `FileInfo` (start_data_block_index, num_data_blocks_used), `MAX_FILES_CALCULATED`.
    - [x] `fs.cpp`: `fs_format` fonksiyonu `Superblock` ve `Bitmap` alanlarını initialize edecek şekilde güncellendi.
    - [x] `fs.cpp`: `read_all_file_info`, `write_file_info_at_index`, `fs_create`, `fs_ls`, `fs_exists`, `fs_size` fonksiyonları yeni metadata yapısını (Superblock okuma/yazma, `MAX_FILES_CALCULATED` kullanımı) destekleyecek şekilde güncellendi.
    - [x] `main.cpp`: Sadece `fs_init()` çağrısı ile `fs_format` ve disk başlatma işlemleri test edildi.
- [ ] **3.2: Bitmap Yönetimi için Yardımcı Fonksiyonlar**
    - [x] `find_free_data_block()`: Boş veri bloğu bulur ve meşgul olarak işaretler.
    - [x] `free_data_block(int block_index)`: Veri bloğunu boş olarak işaretler.
    - [x] Test Edildi
- [x] `fs_write(char* filename, char* data, int size)`: Dosyaya veri yazma.
    - [x] Dosya yoksa hata döndürme.
    - [x] Test Edildi
- [x] `fs_read(char* filename, int offset, int size, char* buffer)`: Dosyadan veri okuma.
    - [x] Geçerli offset ve size kontrolü.
    - [x] Test Edildi
- [x] `fs_cat(char* filename)`: Dosyanın içeriğini ekrana yazdırma.
    - [x] Test Edildi

### Aşama 4: Gelişmiş Dosya İşlemleri
- [ ] `fs_delete(char* filename)`: Dosyayı silme ve alanı boş olarak işaretleme.
    - [ ] Test Edildi
- [x] `fs_rename(char* old_name, char* new_name)`: Dosya ismini değiştirme.
    - [x] Test Edildi
- [x] `fs_append(char* filename, char* data, int size)`: Dosyanın sonuna veri ekleme.
    - [x] Test Edildi
- [x] `fs_truncate(char* filename, int new_size)`: Dosya içeriğini kesme/küçültme.
    - [x] Test Edildi
- [ ] `fs_copy(char* src_filename, char* dest_filename)`: Dosya kopyalama.
    - [x] Test Edildi

### Aşama 5: Disk Yönetimi ve Yardımcı Fonksiyonlar (İleri Seviye)
- [x] `fs_mv(char* old_path, char* new_path)`: Dosya taşıma (Dizin desteği eklendiğinde anlamlı olacak, şimdilik ertelenebilir veya basit bir yeniden adlandırma olarak düşünülebilir).
    - [x] Test Edildi
- [x] `fs_defragment()`: Diskteki boş alanları birleştirme (Opsiyonel/İleri seviye).
    - [x] Test Edildi
- [x] `fs_check_integrity()`: Metadata ve veri blokları tutarlılığını kontrol etme.
    - [x] Test Edildi
- [x] `fs_backup(char* backup_filename)`: Disk yedeği alma.
    - [x] Test Edildi
- [ ] `fs_restore(char* backup_filename)`: Disk yedeğini geri yükleme.
    - [x] Test Edildi
- [x] `fs_diff(char* file1, char* file2)`: İki dosyayı karşılaştırma.
    - [x] Test Edildi
- [ ] `fs_log()`: Yapılan işlemleri loglama.
    - [x] Test Edildi

### Aşama 6: Kullanıcı Arayüzü
- [ ] `main.cpp` içinde konsol tabanlı menü oluşturma.
    1. Dosya oluştur
    2. Dosya sil
    3. Dosyaya veri yaz
    4. Dosyadan veri oku
    5. Dosyaları listele
    6. Format at
    ...
    12. Çıkış
- [ ] Kullanıcı girdilerini alma ve ilgili `fs_` fonksiyonlarını çağırma.
- [ ] Test Edildi

## Test Durumları
- [ ] Aynı ada sahip birden fazla dosya oluşturulması engellenmeli. (Aşama 2'de test edilecek)
- [ ] Maksimum dosya sayısı limiti test edilmeli. (Aşama 2'de test edilecek)
- [ ] Disk doluyken yazma işleminin engellenmesi test edilmeli. (Aşama 3'te test edilecek)
- [ ] Format sonrası dosyaların silinmiş olması sağlanmalı. (Aşama 1'de test edilecek) 