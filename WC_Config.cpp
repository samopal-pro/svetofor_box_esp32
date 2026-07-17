// ===== SDEBUG Configuration =====
#define MODULE_NAME "CONFIG"
#define MODULE_DEBUG_LEVEL DEBUG_INFO
#include "src/Slib/SDEBUG.h"


#include "WC_Config.h"

static const custom_app_desc_t __attribute__((section(".rodata_custom_desc"), used)) custom_app_desc = {
    .fw_magic   = ESP_APP_DESC_MAGIC_WORD,
    .fw_name    = FW_NAME,
    .fw_version = FW_VERSION
};


// ===== Global Variables =====
char strID[MAC_STR_LEN] = {0};
char serNo[SERIAL_NO_LEN] = {0};
uint64_t chipID = 0;

String configUUID = "";

JsonDocument config_selector;
JsonDocument config;
JsonDocument httpd_version;
JsonDocument config_default;
JsonDocument jsonSave;

// Используем массивы вместо указателей для совместимости
char activeConfig[16] = "config1";
char pathConfig[64] = {0};
char pathDefault[64] = {0};

// ===== Implementation =====

/**
 * @brief Установка активной конфигурации из селектора
 */
void setActiveConfig() {
    const char* active = config_selector["active"] | "config1";
    strncpy(activeConfig, active, sizeof(activeConfig) - 1);
    activeConfig[sizeof(activeConfig) - 1] = '\0';
    
    // Формируем путь к файлу конфигурации
    const char* file_path = config_selector[activeConfig]["file"] | "/config/config1.json";
    snprintf(pathConfig, sizeof(pathConfig), "%s%s", HTTPD_PATH_PREFIX, file_path);
    
    // Формируем путь к файлу по умолчанию
    const char* default_path = config_selector[activeConfig]["default"] | "/config/config_default1.json";
    snprintf(pathDefault, sizeof(pathDefault), "%s%s", HTTPD_PATH_PREFIX, default_path);
    
    LOG_INFOLN("Active config: %s, file: %s, default: %s", activeConfig, pathConfig, pathDefault);
}

/**
 * @brief Инициализация конфигурации и файловой системы
 */
void configInit() {
    LOG_INFOLN("Initializing configuration system...");
    
    // Монтируем LittleFS без автоформатирования
    if (!LittleFS.begin(false)) {
        LOG_ERRORLN("LittleFS mount failed, attempting format...");
        if (!LittleFS.begin(true)) {
            LOG_ERRORLN("LittleFS format failed! System halted.");
            return;
        }
        LOG_INFOLN("LittleFS formatted and mounted successfully");
    }
    
    LOG_INFOLN("LittleFS mounted, listing files:");
    listDir("/", 2);
    char uuid[40];
    configUUID             = generateUUID(uuid);
    readJson(WEB_VERSION_FILE, httpd_version);
    // Загружаем селектор конфигурации
    if (!readJson(CONFIG_SELECTOR_PATH, config_selector)) {
        LOG_ERRORLN("Failed to load config selector: %s", CONFIG_SELECTOR_PATH);
        return;
    }
    
    setActiveConfig();
    configRead();
    readJson(SAVE_JSON_PATH, jsonSave);
    
    LOG_INFOLN("Configuration system initialized");
}

/**
 * @brief Сброс конфигурации на значения по умолчанию
 */
void configSetDefault() {
    LOG_INFOLN("Resetting config to default: %s -> %s", pathDefault, pathConfig);
    
    if (!writeJson(pathConfig, config_default)) {
        LOG_ERRORLN("Failed to write default config");
        return;
    }
    
    if (!readJson(pathConfig, config)) {
        LOG_ERRORLN("Failed to read config after reset");
    }
}

/**
 * @brief Чтение конфигурации из файлов
 */
void configRead() {
    LOG_INFOLN("Reading configuration...");
    
    // Читаем конфигурацию по умолчанию
    if (!readJson(pathDefault, config_default)) {
        LOG_ERRORLN("Failed to read default config: %s", pathDefault);
        return;
    }
    
    // Проверяем существование текущего файла конфигурации
    if (!LittleFS.exists(pathConfig)) {
        LOG_INFOLN("Config file not found, creating from default: %s", pathConfig);
        if (!writeJson(pathConfig, config_default)) {
            LOG_ERRORLN("Failed to create config file");
            return;
        }
    }
    
    // Читаем текущую конфигурацию
    if (!readJson(pathConfig, config)) {
        LOG_ERRORLN("Failed to read config, using default");
        config.clear();
        copyJson(config_default, config);
    }

// Проверяем, существует ли параметр ESP_NAME в конфигурации
if (config["config2"]["ESP_NAME"].isNull() || 
    config["config2"]["ESP_NAME"].as<String>().length() == 0) {    
  
    // Устанавливаем значение в конфигурации
    config["config2"]["ESP_NAME"] = deviceName();
    
    // Сохраняем изменения
    LOG_INFOLN("ESP_NAME was empty");
    configWrite();
    
}}

/**
 * @brief Запись конфигурации в файл
 */
void configWrite() {
    LOG_INFOLN("Writing configuration to: %s", pathConfig);
    writeJson(pathConfig, config);
}

/**
 * @brief Копирование файла
 * @param src_path Исходный файл
 * @param dst_path Целевой файл
 * @param overwrite Перезаписывать если существует
 * @return true при успехе
 */
bool copyFile(const char* src_path, const char* dst_path, bool overwrite) {
    LOG_INFOLN("Copying file: %s -> %s (overwrite: %d)", src_path, dst_path, overwrite);
    
    if (!overwrite && LittleFS.exists(dst_path)) {
        LOG_INFOLN("File exists and overwrite is disabled");
        return false;
    }
    
    File src = LittleFS.open(src_path, "r");
    if (!src) {
        LOG_ERRORLN("Failed to open source file: %s", src_path);
        return false;
    }
    
    File dst = LittleFS.open(dst_path, "w");
    if (!dst) {
        LOG_ERRORLN("Failed to open destination file: %s", dst_path);
        src.close();
        return false;
    }
    
    uint8_t buffer[FILE_BUFFER_SIZE];
    size_t bytes_read;
    size_t total_copied = 0;
    
    while ((bytes_read = src.read(buffer, sizeof(buffer))) > 0) {
        size_t bytes_written = dst.write(buffer, bytes_read);
        if (bytes_written != bytes_read) {
            LOG_ERRORLN("Write error at offset %u", total_copied);
            src.close();
            dst.close();
            return false;
        }
        total_copied += bytes_written;
    }
    
    src.close();
    dst.close();
    
    LOG_INFOLN("File copied successfully, %u bytes", total_copied);
    return true;
}

/**
 * @brief Чтение JSON из файла
 * @param file_path Путь к файлу
 * @param doc JSON документ для заполнения
 * @return true при успехе
 */
bool readJson(const char* file_path, JsonDocument& doc) {
    File file = LittleFS.open(file_path, "r");
    if (!file || file.isDirectory()) {
        LOG_ERRORLN("Failed to open file for reading: %s", file_path);
        return false;
    }
    
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        LOG_ERRORLN("JSON parse error in %s: %s", file_path, error.c_str());
        return false;
    }
    
    LOG_DEBUGLN("Successfully read JSON from: %s", file_path);
    DEBUG_JSON_DOC(file_path, doc);
    
    return true;
}

/**
 * @brief Запись JSON в файл
 * @param file_path Путь к файлу
 * @param doc JSON документ для записи
 * @return true при успехе
 */
bool writeJson(const char* file_path, const JsonDocument& doc) {
    File file = LittleFS.open(file_path, "w");
    if (!file) {
        LOG_ERRORLN("Failed to open file for writing: %s", file_path);
        return false;
    }
    
    size_t written = serializeJson(doc, file);
    file.close();
    
    if (written == 0) {
        LOG_ERRORLN("Failed to serialize JSON to: %s", file_path);
        return false;
    }
    
    LOG_DEBUGLN("Successfully wrote JSON to: %s (%u bytes)", file_path, written);
    DEBUG_JSON_DOC(file_path, doc);
    
    return true;
}

/**
* Чтение имени иверсии прошивки и SHA сигнатуры
*/
void printFW(){
       
    // Читаем данные из структуры (они уже в памяти)
    LOG_INFOLN("FW NAME: %s", custom_app_desc.fw_name);
    LOG_INFOLN("FW VERSION: %s", custom_app_desc.fw_version);
    
    // Вывод информации о размере
    LOG_INFOLN("FW DATA SIZE: %d bytes", sizeof(custom_app_desc));
    LOG_INFOLN("FW DATA ADDR: 0x%X", (uint32_t)&custom_app_desc);

    // Получаем указатель на текущий запущенный OTA-раздел
    const esp_partition_t* running = esp_ota_get_running_partition();

    // Получаем информацию о прошивке в этом разделе
    esp_app_desc_t app_info;
    if (esp_ota_get_partition_description(running, &app_info) == ESP_OK) {
        LOG_INFOLN("FW ORIG MAGIC: %lX", app_info.magic_word);
        LOG_INFOLN("FW ORIG NAME: %s", app_info.project_name);
        LOG_INFOLN("FW ORIG VERS: %s", app_info.version);
        LOG_INFO("FW ORIG SHA:  %s", app_info.app_elf_sha256);
        for( int i=0; i<32; i++ )Serial.printf(" %02X",app_info.app_elf_sha256[i]);
        Serial.println();
//        LOG_INFOLN("FW ORIG SIZE: %d bytes", app_info.bin_size); // Размер прошивки
    }
    // Вывод информации о размере
    LOG_INFOLN("FW ORIG SIZE: %d bytes", sizeof(app_info));
    LOG_INFOLN("FW ORIG ADDR: 0x%X", (uint32_t)&app_info);

    // Получаем SHA-256 хеш прошивки
    char sha_256[65];
    if (esp_ota_get_app_elf_sha256(sha_256, sizeof(sha_256)) == ESP_OK) {
        LOG_INFOLN("FW ELF SHA: %s", sha_256);
    }

}

/**
 * @brief Чтение ID устройства и серийного номера
 */
void readID() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    
    // Формируем строку MAC-адреса
    snprintf(strID, sizeof(strID), "%02X%02X%02X%02X%02X%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Формируем 64-битный ID
    chipID = 0;
    for (int i = 0; i < 6; i++) {
        chipID = (chipID << 8) | mac[i];
    }
    
    // Читаем серийный номер из eFuse
    for (int i = 0; i < 8; i++) {
        uint32_t reg_value = esp_efuse_read_reg(EFUSE_BLK3, 7 - i);
        // Извлекаем байты в правильном порядке (big-endian)
        serNo[i * 4 + 0] = (reg_value >> 24) & 0xFF;
        serNo[i * 4 + 1] = (reg_value >> 16) & 0xFF;
        serNo[i * 4 + 2] = (reg_value >> 8) & 0xFF;
        serNo[i * 4 + 3] = reg_value & 0xFF;
    }
    serNo[32] = '\0';
    
    LOG_INFOLN("Device ID: %s, Chip ID: %012llX", strID, chipID);
    LOG_DEBUGLN("Serial Number: %s", serNo);
}

/**
 * @brief Рекурсивный вывод списка файлов
 * @param dirname Начальная директория
 * @param levels Глубина рекурсии
 */
void listDir(const char* dirname, uint8_t levels) {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    size_t free_space = total - used;
    
    LOG_INFOLN("LittleFS Stats - Total: %.2f KB, Used: %.2f KB (%.1f%%), Free: %.2f KB",
               total / 1024.0, used / 1024.0, (used * 100.0) / total, free_space / 1024.0);
    
    File root = LittleFS.open(dirname);
    if (!root) {
        LOG_ERRORLN("Failed to open directory: %s", dirname);
        return;
    }
    
    if (!root.isDirectory()) {
        LOG_ERRORLN("Not a directory: %s", dirname);
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            LOG_INFOLN("[DIR ] %s", file.path());
            if (levels > 0) {
                listDir(file.path(), levels - 1);
            }
        } else {
            LOG_INFOLN("[FILE] %-30s %8u bytes", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}

/**
 * @brief Рекурсивное объединение JSON объектов
 * @param src Исходный объект
 * @param dst Целевой объект
 * @return true при успехе
 */
bool mergeObject(JsonObjectConst src, JsonObject dst) {
    const size_t MAX_DEPTH = 10;  // Защита от глубокой рекурсии
    static size_t depth = 0;
    
    if (++depth > MAX_DEPTH) {
        LOG_ERRORLN("Max merge depth exceeded");
        depth--;
        return false;
    }
    
    for (JsonPairConst kv : src) {
        const char* key = kv.key().c_str();
        JsonVariantConst src_value = kv.value();
        
        if (src_value.is<JsonObjectConst>()) {
            JsonObject dst_obj;
            JsonVariant dst_value = dst[key];
            
            if (dst_value.is<JsonObject>()) {
                dst_obj = dst_value.as<JsonObject>();
            } else {
                dst_obj = dst[key].to<JsonObject>();
                if (dst_obj.isNull()) {
                    LOG_ERRORLN("Failed to create object for key: %s", key);
                    depth--;
                    return false;
                }
            }
            
            if (!mergeObject(src_value.as<JsonObjectConst>(), dst_obj)) {
                depth--;
                return false;
            }
        } else {
            if (!dst[key].set(src_value)) {
                LOG_ERRORLN("Failed to set value for key: %s", key);
                depth--;
                return false;
            }
        }
    }
    
    depth--;
    return true;
}

/**
 * @brief Копирование JSON документа с объединением
 * @param src_json Исходный документ
 * @param dst_json Целевой документ
 * @return true при успехе
 */
bool copyJson(JsonDocument& src_json, JsonDocument& dst_json) {
    JsonObjectConst src = src_json.as<JsonObjectConst>();
    JsonObject dst = dst_json.is<JsonObject>() 
                     ? dst_json.as<JsonObject>() 
                     : dst_json.to<JsonObject>();
    
    if (dst.isNull()) {
        LOG_ERRORLN("Failed to create destination object");
        return false;
    }
    
    return mergeObject(src, dst);
}

/**
 * @brief Сохранение состояния датчика
 * @param distance Дистанция
 * @param state Состояние сенсора
 */
void saveSet(float distance, SENSOR_STAT_t state) {
    jsonSave["DISTANCE"] = distance;
    jsonSave["STATE_ON"] = static_cast<int>(state);
    writeJson(SAVE_JSON_PATH, jsonSave);
    LOG_DEBUGLN("Saved state: distance=%.2f, state=%d", distance, state);
}

/**
 * @brief Увеличение счетчика загрузок
 * @return Текущее значение счетчика
 */
uint16_t saveCount() {
    uint16_t count = jsonSave["BOOT_COUNT"] | 0;
    jsonSave["BOOT_COUNT"] = count + 1;
    writeJson(SAVE_JSON_PATH, jsonSave);
    LOG_INFOLN("Boot count: %u", count + 1);
    return count;
}

/**
 * @brief Конвертация строки цвета в RGB
 * @param value JSON значение с цветом
 * @return 24-битное значение RGB
 */
uint32_t String2RGB(JsonVariantConst value) {
    const char* color_str = value.as<const char*>();
    
    if (!color_str || strlen(color_str) == 0) {
        LOG_ERRORLN("Empty color string");
        return 0;
    }
    
    // Пропускаем символ '#' если есть
    if (color_str[0] == '#') {
        color_str++;
    }
    
    // Проверяем длину строки
    size_t len = strlen(color_str);
    if (len != 6) {
        LOG_ERRORLN("Invalid color format: %s (expected 6 hex digits)", color_str);
        return 0;
    }
    
    char* end_ptr;
    uint32_t rgb = strtoul(color_str, &end_ptr, 16);
    
    if (*end_ptr != '\0') {
        LOG_ERRORLN("Invalid hex characters in color: %s", color_str);
        return 0;
    }
    
    LOG_DEBUGLN("Color converted: %s -> 0x%06X", value.as<const char*>(), rgb);
    return rgb;
}

String deviceName() {
    String name;
    name.reserve(64);
    
    if (strlen(serNo) > 0) {
        name += serNo;
    } else {
        name += DEVICE_NAME_YEAR;
    }
    name += DEVICE_NAM_SUFFIX;
    
    LOG_DEBUGLN("Device name: %s", name.c_str());
    return name;
}

/**
 * Генерирует UUID v4 на основе аппаратного ГСЧ ESP32.
 * Буфер должен быть размером не менее 37 байт (36 символов + терминатор).
 */
char *generateUUID(char* buffer) {
    static const char hex[] = "0123456789abcdef";

    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            buffer[i] = '-';
        } else if (i == 14) {
            buffer[i] = '4';
        } else if (i == 19) {
            uint8_t variant = esp_random() & 0x03;
            buffer[i] = (variant == 0x00) ? '8' :
                        (variant == 0x01) ? '9' :
                        (variant == 0x02) ? 'a' : 'b';
        } else {
            buffer[i] = hex[esp_random() & 0x0F];
        }
    }

    buffer[36] = '\0';
    return buffer;
}