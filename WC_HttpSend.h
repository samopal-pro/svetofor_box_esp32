#pragma once
#include "MyConfig.h"
#include "WC_Task.h"
#include "WC_Config.h"
#include "src/Slib/SHTTPClient.h"
#include <LittleFS.h>
#define DEST_FS_USES_LITTLEFS 
#include <ESP32-targz.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <vector>

struct FileInfo {
   String name;
   size_t size;
   bool isDir;
};

/**
* Класс для потокового Base64 кодирования
*/
class Base64Stream : public Stream {
public:
    Base64Stream(Stream* input, size_t inputSize);
    ~Base64Stream();
    int available() override;
    int read() override;
    int peek() override;
    size_t write(uint8_t) override { return 0; }
    size_t encodedSize() const { return m_encodedSize; }

private:
    void encodeNextBlock();
    
    Stream* m_input;
    size_t m_inputRemaining;
    size_t m_encodedSize;
    uint8_t m_inputBuffer[3];
    char m_outputBuffer[5];
    int m_outputPos;
    int m_outputLen;
    bool m_finished;
    static const char m_base64Table[];
};

/**
* Основной класс для отправки HTTP данных
*/
class MyHttpSend {
public:
    MyHttpSend();
    ~MyHttpSend();
    
    bool begin();                                                    // Инициализация модуля
    bool sendCrmMoscowParam();                                       // Отправка в CRM Москва
    bool sendHttpParam();                                            // Отправка на HTTP шлюзы (список)
    bool sendHttpParamOne(String &host);                             // Отправка на один HTTP шлюз
    bool sendParamTB();                                              // Отправка телеметрии в ThingsBoard
    bool sendAttributeTB();                                          // Отправка атрибутов устройства
    bool authTB(const char *key, const char *secret);                // Авторизация в ThingsBoard
    void checkConfigVersion();                                       // Проверка версии конфигурации
    bool sendConfigToTB();                                           // Отправка конфигурации в ThingsBoard
    bool checkAndUpdateConfig();                                     // Проверка и обновление конфигурации с сервера
    bool sendWebFilesToTB();                                         // Отправка веб-файлов в ThingsBoard
    bool createTarArchive();                                         // Создание TAR архива
    String calculateSHA256(const String& filePath);                  // Расчет SHA-256 файла
    String encodeBase64(const String& input);                        // Base64 кодирование
    String readOrCreateVersionFile();                                // Чтение/создание файла версии
    void cleanupTempFiles();                                         // Очистка временных файлов
    bool checkFirmwareVersionTB();                                   // Проверка обновления версии прошивки
    bool checkHttpdVersionTB();                                      // Проверка версии веб-интерфейса на сервере
    bool updateFirmwareFromTB(const String& fwTitle, const String& fwVersion, const String& fwChecksum); // Обновление прошивки
    bool updateHttpdTB();                                            // Обновление веб-интерфейса с сервера
    void stop();                                                     // Остановка HTTP клиента
    
private:
    void parseExcludeList(const String& excludeStr);
    void addFilesToTar(const String& dirPath, File& tarFile, const String& basePath);
    void writeTarHeader(File& tarFile, const String& fileName, size_t fileSize);
    unsigned int calculateTarChecksum(unsigned char* header);
    bool shouldExcludeFile(const String& filePath);
    bool sendStreamToTB(const String& tbHost, int tbPort, const String& tbToken,
                        const String& infoJson, size_t archiveSize);
    bool fetchConfigFromTB();                                        // Загрузка конфигурации с ThingsBoard
    bool extractTar(const String& tarPath, const String& destPath);  // Распаковка TAR архива
    
    String m_tarPath;
    String m_httpdPath;
    String m_versionFile;
    std::vector<String> m_excludeFiles;
//    static const size_t TAR_BLOCK_SIZE = 512;
    
    SimpleHttpClient m_httpClient;  // Переиспользуемый HTTP клиент
};

int getStatus();                                                     // Получение статуса датчика
uint16_t KeyGen(char *str);                                          // Генерация ключа (CRC)
extern JsonDocument jsonData;                                        // Глобальный JSON документ