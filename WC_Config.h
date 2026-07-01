#pragma once
#include <Arduino.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#include <soc/efuse_reg.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <esp32-hal-touch.h>

#include <LittleFS.h>
#include <ArduinoJson.h>

#include "MyConfig.h"

// ===== SDEBUG Configuration =====
#define MODULE_NAME "CONFIG"
#define MODULE_DEBUG_LEVEL DEBUG_INFO
#include "src/Slib/SDEBUG.h"

// ===== Path Constants =====
#define CONFIG_SELECTOR_PATH    "/httpd/config/config.json"
#define SAVE_JSON_PATH          "/httpd/config/save.json"
#define HTTPD_PATH_PREFIX       "/httpd"

// ===== Buffer Sizes =====
#define MAC_STR_LEN             13
#define SERIAL_NO_LEN           33
#define FILE_BUFFER_SIZE        512

// ===== Sensor Types =====
typedef enum : uint8_t {
    SENSOR_SR04T        = 1,    // Trig/Echo протокол
    SENSOR_SR04TM2      = 2,    // Увеличенный импульс 500мс
    SENSOR_SR04_75      = 3,    // Одинарный 7.5м
    SENSOR_TFLUNA_I2C   = 10,   // LiDAR TF Luna I2C
    SENSOR_LD2413_UART  = 20    // Радар LD-2413 UART
} SensorType_t;

// ===== Installation Types =====
typedef enum : uint8_t {
    INSTALL_TYPE_NORMAL     = 1,    // Уменьшение дистанции
    INSTALL_TYPE_OUTSIDE    = 2,    // Вне интервала
    INSTALL_TYPE_INSIDE     = 3     // Внутри интервала
} T_INSTALL_TYPE;

// ===== NaN Value Flags =====
typedef enum : uint8_t {
    NAN_VALUE_IGNORE    = 1,
    NAN_VALUE_FREE      = 2,
    NAN_VALUE_BUSY      = 3
} T_NAN_VALUE_FLAG;

// ===== WiFi Station Modes =====
typedef enum : uint8_t {
    STA_OFF     = 0,    // Не стартовать AP
    STA_ON      = 1,    // Всегда стартовать AP
    STA_AUTO    = 2     // Авто при первой загрузке
} T_STA_MODE;

// ===== Status LED States =====
typedef enum : uint8_t {
    STAT_OFF,
    STAT_BT_ON,
    STAT_BT_OFF,
    STAT_WAIT_ON,
    STAT_WAIT_OFF
} ES_STAT;

// ===== Calibration Modes =====
typedef enum : uint8_t {
    CM_NONE         = 0,
    CM_WAIT         = 1,
    CM_WAIT_MP3     = 2,
    CM_WAIT_WAIT    = 3,
    CM_WAIT_END_MP3 = 4,
    CM_WAIT_REBOOT  = 5,
    CM_ON           = 100
} CALIBRATION_MODE_t;

// ===== MP3 Commands =====
typedef enum : uint8_t {
    CMP3_NONE   = 0,
    CMP3_PLAY   = 1,
    CMP3_STOP   = 2,
    CMP3_WAIT   = 3,
    CMP3_VOLUME = 4
} CMD_MP3_t;

// ===== Sensor States =====
typedef enum : int8_t {
    SS_NONE         = 0,
    SS_FREE         = 2,
    SS_BUSY         = 3,
    SS_NAN          = -1,
    SS_NAN_FREE     = -2,
    SS_NAN_BUSY     = -3,
    SS_RESTORE      = 100
} SENSOR_STAT_t;

// ===== Global Variables (extern) =====
extern char strID[MAC_STR_LEN];
extern char serNo[SERIAL_NO_LEN];
extern uint64_t chipID;

extern JsonDocument config_selector;
extern JsonDocument config;
extern JsonDocument config_default;
extern JsonDocument jsonSave;

// Используем массивы для совместимости со старым кодом
extern char activeConfig[16];
extern char pathConfig[64];
extern char pathDefault[64];

// ===== Core Functions =====
void configInit();
void configSetDefault();
void configRead();
void configWrite();
void setActiveConfig();  // Делаем публичной для использования из WC_HTTP.cpp

// ===== File Operations =====
bool copyFile(const char* src_path, const char* dst_path, bool overwrite = true);
bool writeJson(const char* file_path, const JsonDocument& doc);
bool readJson(const char* file_path, JsonDocument& doc);

// ===== System Functions =====
void readID();
void listDir(const char* dirname, uint8_t levels);

// ===== JSON Merge Functions =====
bool mergeObject(JsonObjectConst src, JsonObject dst);
bool copyJson(JsonDocument& src_json, JsonDocument& dst_json);

// ===== Save/Load Functions =====
void saveSet(float distance, SENSOR_STAT_t state);
uint16_t saveCount();

// ===== Utility Functions =====
uint32_t String2RGB(JsonVariantConst value);