# Derleyici
CXX = g++

# Derleyici seçenekleri
# -std=c++11: C++11 standardını kullan
# -Wall: Tüm uyarıları göster
# -g: Debug bilgilerini ekle
CXXFLAGS = -std=c++11 -Wall -g

# Bağlayıcı seçenekleri
LDFLAGS =

# Kaynak dosyalar
SRCS = main.cpp fs.cpp

# Build (nesne dosyaları) ve bin (çalıştırılabilir) klasörleri
BUILD_DIR = build
# BIN_DIR = bin # İleride çalıştırılabilir dosya için de ayrı klasör istenirse

# Nesne dosyalar (kaynak dosyalardan .o uzantılı olarak türetilir ve BUILD_DIR içine konur)
OBJS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Çalıştırılabilir dosyanın adı
TARGET_NAME = simplefs
TARGET = $(TARGET_NAME) # Ana dizinde kalacaksa
# TARGET = $(BIN_DIR)/$(TARGET_NAME) # bin klasörüne konacaksa

# Varsayılan hedef: Çalıştırılabilir dosyayı oluştur
all: $(TARGET)

# Çalıştırılabilir dosyayı oluşturma kuralı
$(TARGET): $(OBJS)
	@mkdir -p $(@D) # Hedef dosyanın dizinini oluştur (eğer TARGET bin/simplefs ise bin'i oluşturur)
					# Eğer TARGET sadece simplefs ise, ana dizinde olduğu için @D . olur, mkdir -p . hata vermez.
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@echo "Executable '$(TARGET)' created."

# .cpp dosyalarından BUILD_DIR içindeki .o dosyalarını oluşturma kuralı
$(BUILD_DIR)/%.o: %.cpp fs.hpp | $(BUILD_DIR) # $(BUILD_DIR)'ı order-only prerequisite yap
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo "Compiled '$<' -> '$@'"

# BUILD_DIR klasörünü oluşturma kuralı (order-only prerequisite)
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@echo "Created directory '$(BUILD_DIR)' for object files."

# "make clean" komutuyla oluşturulan dosyaları silme kuralı
clean:
	@echo "Cleaning up..."
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR) # build klasörünü ve içindekileri sil
	# rm -rf $(BIN_DIR) # Eğer BIN_DIR kullanılıyorsa
	rm -f disk.sim simplefs.log # Diğer proje dosyaları
	@echo "Cleanup complete."

# "make run" komutuyla programı çalıştırma
run: $(TARGET)
	./$(TARGET)

# "make format" komutuyla diski formatlama (TARGET varsa, programın kendisi fs_init üzerinden formatlar)
# Bu daha çok manuel bir test veya başlangıç durumu oluşturmak için.
# Ancak programın kendisi zaten fs_init ile diski ele alıyor.
# Bu hedef yerine doğrudan programı çalıştırıp menüden formatlamak daha mantıklı olabilir.
# Şimdilik sembolik olarak bırakıyorum veya "run" ile birleştirilebilir.
# VEYA formatlama için ayrı bir test programı/komutu yazılabilir.
# Bizim durumumuzda fs_init zaten ilk çalıştırmada formatlıyor (eğer disk.sim yoksa).
# Eğer disk.sim varsa ve yeniden formatlamak istiyorsak, program içinden menü ile yapılmalı.

# Fonksiyonları test etmek için basit bir hedef.
# Bu hedef, programı çalıştırır. Testler main.cpp içinde veya ayrı test dosyalarında olabilir.
test: $(TARGET)
	./$(TARGET) # Veya özel test senaryolarını çalıştıran bir script/komut

.PHONY: all clean run test 