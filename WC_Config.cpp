#include "WC_Config.h"

char strID[16];
uint64_t chipID;
char serNo[33];

JsonDocument config_selector;
JsonDocument config;
JsonDocument config_default;
JsonDocument jsonSave;

String activeConfig = "config1";
String pathConfig = "";
String pathDefault = "";

void setActiveConfig(){
    activeConfig = config_selector["active"].as<String>();
    String s = config_selector[activeConfig]["default"].as<String>();
    pathDefault = "/httpd" + s;
    s = config_selector[activeConfig]["file"].as<String>();
    pathConfig = "/httpd" + s;
}

/**
* Функция инициализации файловой системы
*/
void configInit(){
   LittleFS.begin(true);
   listDir("/",10);
   readJson(CONFIG_SELECTOR,config_selector);
   setActiveConfig();
   configRead();
   readJson(SAVE_JSON,jsonSave);
}

/**
* Копирование файла по умолчанию в конфигураионный файл текущей конфигурации 
*/
void configSetDefault(){
   writeJson(pathConfig.c_str(),config_default);
   readJson(pathConfig.c_str(),config);
}


void configRead(){
 // Считываем конфигурацию по умолчанию
   readJson(pathDefault.c_str(), config_default);
 
// Если текущий файл конфигурации не существует - создаем его из файла по умолчанию
   File f = LittleFS.open(pathConfig, "r");
   if (!f )writeJson(pathConfig.c_str(),config_default);
   close(f);

// Считываем файл с конфигурацией
   readJson(pathConfig.c_str(),config);
}

void configWrite(){
   writeJson(pathConfig.c_str(),config);
}


/**
* Копирование одного файла в другой
*/
bool copyFile(const String& src_file, const String& dst_file, bool overwrite) {
    Serial.printf("!!! copy %s to %s\n",src_file.c_str(),dst_file.c_str());

    if (!overwrite && LittleFS.exists(dst_file)) return false;  

    File src = LittleFS.open(src_file, "r");
    if (!src) return false;

    File dst = LittleFS.open(dst_file, "w");
    if (!dst) { src.close(); return false; }

    src.seek(0);

    uint8_t buf[512];
    size_t len;

    while ((len = src.read(buf, sizeof(buf))) > 0) {
        dst.write(buf, len);
    }

    src.close();
    dst.close();

    return true;
}

/**
* Чтение файла file в Json документ 
*/
bool readJson(const char* file, JsonDocument& doc) {
    File f = LittleFS.open(file, "r");
    if (!f || f.isDirectory()) {
        printJson("READ", file, doc, "File open error");
        return false;
    }

    DeserializationError error = deserializeJson(doc, f);
    f.close();

    if (error) {
        printJson("READ", file, doc, error.c_str());
        return false;
    }

    printJson("READ", file, doc, nullptr);
    return true;
}

/**
* Запись файла file из Json документа 
*/
bool writeJson(const char* file, const JsonDocument& doc) {
    File f = LittleFS.open(file, "w");
    if (!f) {
        printJson("SAVE", file, doc, "File open error");
        return false;
    }

    size_t written = serializeJson(doc, f);
    f.close();

    if (written == 0) {
        printJson("SAVE", file, doc, "Serialize error");
        return false;
    }

    printJson("SAVE", file, doc, nullptr);
    return true;
}

/**
* Вывод JSON документа 
*/
void printJson(const char *label,  const char *file,  const JsonDocument& doc, const char* errorMsg) {
#if defined(DEBUG_SERIAL)

    Serial.printf("!!! %s %s\n", label, file);

    if (errorMsg) {
        Serial.printf("ERROR: %s\n", errorMsg);
        return;
    }

    String s;
//    serializeJsonPretty(doc, s);
    serializeJson(doc, s);

    Serial.println(s);

#endif
}

uint16_t saveCount(){
   uint16_t _count = 0;
   if( jsonSave["BOOT_COUNT"] == "" ){
      jsonSave["BOOT_COUNT"] = _count;   
   }
   else {
      _count = jsonSave["BOOT_COUNT"].as<uint16_t>();
      jsonSave["BOOT_COUNT"] = _count+1;
   }
   writeJson(SAVE_JSON,jsonSave);
   return _count;
}



/**
* Функция формирование ID и SERNO 
*/
void readID(){
    union uint32_to_uint8 {
       uint32_t data32;
       char  data8[4];
    };
   uint8_t mac[6];
  
   esp_efuse_mac_get_default(mac);
   sprintf(strID, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
   chipID = 0;
   for (int i = 0; i < 6; i++) {
      chipID <<= 8;
      chipID |= mac[i];
  }
  uint32_to_uint8 dd;
  for( int i=0; i<8; i++){
     dd.data32 = esp_efuse_read_reg(EFUSE_BLK3, 7-i);
     for( int j=0; j<4; j++)serNo[i*4+j] = dd.data8[3-j];
  }
  serNo[32] = '\0';
  Serial.printf("!!! ESP ID=%012llX strID=%s serNO=%s\n",chipID,strID,serNo);

}

/**
* Функция показа всех файлов 
*/
void listDir( const char* dirname, uint8_t levels) {
size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    size_t free  = total - used;

    Serial.printf("!!! LittleFS ");
    Serial.printf(" size : %u bytes (%.2f KB)",total, total / 1024.0);
    Serial.printf(" Used: %u bytes (%.2f KB)",used, used / 1024.0);
    Serial.printf(" Free: %u bytes (%.2f KB)\n",free, free / 1024.0);

    File root = LittleFS.open(dirname);
    if (!root) {      
        Serial.printf("??? Failed to open dir: %s\n", dirname);
        return;
    }

    if (!root.isDirectory()) {
        Serial.printf("??? Not a directory: %s\n", dirname);
        return;
    }

    File file = root.openNextFile();

    while (file) {
        if (file.isDirectory()) {
            Serial.printf("!!! [DIR ] %s\n", file.path());
            if (levels > 0) {
                listDir(file.path(), levels - 1);
            }
        } else {
            Serial.printf(
                "!!! [FILE] %s   %u bytes\n",
                file.name(),
                (unsigned int)file.size()
            );
        }
        file = root.openNextFile();
    }
}


#include <ArduinoJson.h>

/**
 * Рекурсивно копирует значения из src в dst.
 * Копируются только ключи, присутствующие в src.
 * Отсутствующие в src ключи dst не изменяются.
 *
 * @param src Исходный JSON-объект.
 * @param dst Целевой JSON-объект.
 * @return true при успешном копировании, false при нехватке памяти.
 */
bool mergeObject(JsonObjectConst src, JsonObject dst){
    for (JsonPairConst kv : src) {
        const char* key = kv.key().c_str();
        JsonVariantConst srcValue = kv.value();

        if (srcValue.is<JsonObjectConst>()) {
            JsonVariant dstValue = dst[key];
            JsonObject dstObj;

            if (dstValue.is<JsonObject>())dstObj = dstValue.as<JsonObject>();
// Создать новый объект или заменить существующее значение            
            else dstObj = dst[key].to<JsonObject>(); 
            
            if (dstObj.isNull())return false;

            if (!mergeObject(srcValue.as<JsonObjectConst>(), dstObj))return false;
        }
        else  {
            if (!dst[key].set(srcValue))return false;
        }
    }

    return true;
}

/**
 * Копирует значения из src_json в dst_json.
 * Выполняет рекурсивное объединение объектов без удаления
 * существующих ключей в dst_json.
 *
 * @param src_json Исходный JSON-документ.
 * @param dst_json Целевой JSON-документ.
 * @return true при успешном копировании, false при нехватке памяти.
 */
bool copyJson(JsonDocument& src_json, JsonDocument& dst_json){
    JsonObjectConst src = src_json.as<JsonObjectConst>();
    JsonObject dst = dst_json.is<JsonObject>()
        ? dst_json.as<JsonObject>()
        : dst_json.to<JsonObject>();

    return mergeObject(src, dst);
}

void saveSet(float _dist, SENSOR_STAT_t _on){
   jsonSave["DISTANCE"]    = _dist;
   jsonSave["STATE_ON"]    = (int)_on;
   writeJson(SAVE_JSON,jsonSave);
   
}
/**
 * Извлечение из элемента JsonDocument цвета в виде uint32_t
*/
uint32_t String2RGB(JsonVariantConst _value){
   const char *_color = _value.as<const char *>();
   const char *p = _color; 
   if( p[0] == '#')p++;
   uint32_t _rgb = strtoul(p, nullptr, 16);
   return _rgb;

}