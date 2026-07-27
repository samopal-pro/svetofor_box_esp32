// ============================================
// Файл: WC_HttpSend.h
// ============================================
#pragma once
#include "MyConfig.h"
#include "WC_Task.h"
#include "WC_Config.h"
#include "src/Slib/SHTTPClient.h"
#include <LittleFS.h>
#define DEST_FS_USES_LITTLEFS
#include <ESP32-targz.h>
#include <mbedtls/sha256.h>
#include <vector>

/**
 * Класс отправки HTTP-запросов и обновлений
 */
class MyHttpSend {
public:
    MyHttpSend();
    ~MyHttpSend();

    // Инициализация модуля
    bool begin();
    // Отправка данных в CRM Москва
    bool sendCrmMoscowParam();
    // Отправка данных на HTTP-серверы
    bool sendHttpParam();
    // Отправка данных на один HTTP-сервер
    bool sendHttpParamOne(String &host);
    // Отправка телеметрии в ThingsBoard
    bool sendParamTB();
    // Отправка атрибутов устройства в ThingsBoard
    bool sendAttributeTB();
    // Аутентификация в ThingsBoard
    bool authTB(const char *key, const char *secret);
    // Проверка и инициализация версии конфигурации
    void checkConfigVersion();
    // Проверка обновлений с ThingsBoard
    bool checkUpdateTB(bool _flagSendAttributeTB);
    // Обновление прошивки из ThingsBoard
    bool updateFirmwareFromTB();
    // Обновление HTTPD из ThingsBoard
    bool updateHttpdTB();
    // Обновление конфигурации из ThingsBoard
    bool updateConfigTB();
    // Остановка HTTP-клиента
    void stop();

private:
    // Распаковка TAR архива
    bool extractTar(const String& tarPath, const String& destPath);
    // Парсинг списка исключенных файлов
    void parseExcludeList(const String& excludeStr);
    // Вычисление контрольной суммы TAR-заголовка
    unsigned int calculateTarChecksum(unsigned char* header);

    String m_tarPath;                       // Путь к TAR-архиву
    String m_httpdPath;                     // Путь к директории HTTPD
    String m_versionFile;                   // Путь к файлу версии
    std::vector<String> m_excludeFiles;     // Список исключенных файлов
    SimpleHttpClient m_httpClient;          // HTTP-клиент
};

// Получение статуса датчика
int getStatus();
// Генерация контрольного ключа
uint16_t KeyGen(char *str);
extern JsonDocument jsonData;