#pragma once
#include <arduino.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#include <soc/efuse_reg.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <esp32-hal-touch.h>

#include <LittleFS.h>
#include <ArduinoJson.h>

#include "MyConfig.h"

#define CONFIG_SELECTOR "/httpd/config/config.json"
#define SAVE_JSON       "/httpd/config/save.json"

typedef enum  {
  SENSOR_SR04T       = 1, // Все платы работающие по протоколу Trig/Echo
  SENSOR_SR04TM2     = 2, // Последние платы SR04M2 у которых увелмчено время импуься с 10 до 500мс и установлено ограничение на дистанцию 5000
  SENSOR_SR04_75     = 3, // Одинареый сенсор на 7.5м
  SENSOR_TFLUNA_I2C  = 10, // LiDAR TF Mini Plus по I2C
//  SENSOR_TFMINI_I2C  = 11, // LiDAR TF Luna по I2C
  SENSOR_LD2413_UART = 20  // Радар LD-2413 подключенный ро UART
//  SENSOR_A21_I2C     = 23  //A21 I2C сенсор подключен к I2C разъему
} SensorType_t;

enum T_INSTALL_TYPE {
  INSTALL_TYPE_NORMAL  = 1, //Срабатываение датчика в обычном режиме (уменьшение дистанции)
  INSTALL_TYPE_OUTSIDE = 2, //Срабатывание датчика если вне интервала
  INSTALL_TYPE_INSIDE  = 3  //Срабатывание датчика если внутри интервала
};

enum T_NAN_VALUE_FLAG {
  NAN_VALUE_IGNORE  = 1,  
  NAN_VALUE_FREE    = 2,
  NAN_VALUE_BUSY    = 3
};


typedef enum T_STA_MODE {
  STA_OFF           = 0, //Не стартовать AP при загрузке
  STA_ON            = 1, //Всегда стартовать AP при загрухке
  STA_AUTO          = 2  //Стартовать AP при первой загрузке
};

enum ES_STAT {
  STAT_OFF,
  STAT_BT_ON,
  STAT_BT_OFF,
  STAT_WAIT_ON,
  STAT_WAIT_OFF
};

enum CALIBRATION_MODE_t {
   CM_NONE           = 0,
   CM_WAIT           = 1,
   CM_WAIT_MP3       = 2,
   CM_WAIT_WAIT      = 3,
   CM_WAIT_END_MP3   = 4,
   CM_WAIT_REBOOT    = 5,
   CM_ON             = 100
};

enum CMD_MP3_t {
   CMP3_NONE   =  0,
   CMP3_PLAY   =  1,
   CMP3_STOP   =  2,
   CMP3_WAIT   =  3,
   CMP3_VOLUME =  4
};

enum SENSOR_STAT_t {
  SS_NONE      = 0,
  SS_FREE      = 2,
  SS_BUSY      = 3,
  SS_NAN       = -1,
  SS_NAN_FREE  = -2,
  SS_NAN_BUSY  = -3,
  SS_RESTORE   = 100
};

extern char strID[];
extern char serNo[];
extern uint64_t chipID;

extern JsonDocument config_selector;
extern JsonDocument config;
extern JsonDocument config_default;
extern JsonDocument jsonSave;
extern String activeConfig, pathConfig, pathDefault;


void configInit();
void setActiveConfig();
void configSetDefault();
void configRead();
void configWrite();

bool copyFile(const String& src_file, const String& dst_file, bool overwrite = true);
bool writeJson(const char* file, const JsonDocument& doc);
bool readJson(const char* file, JsonDocument& doc);
void printJson(const char *label,  const char *file,  const JsonDocument& doc, const char* errorMsg);

void readID();
void listDir( const char* dirname, uint8_t levels);

bool mergeObject(JsonObjectConst src, JsonObject dst);
bool copyJson(JsonDocument& src_json, JsonDocument& dst_json);

void saveSet(float _dist, SENSOR_STAT_t _on);
uint16_t saveCount();
uint32_t String2RGB(JsonVariantConst _value);