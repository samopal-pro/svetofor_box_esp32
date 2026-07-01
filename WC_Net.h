#pragma once
#include "MyConfig.h"

#ifdef IS_LORA
#include <SPI.h>
/*
#if defined(RADIOLIB_OPT)
// Исключаем лишние чипы и их клоны
#define RADIOLIB_EXCLUDE_CC1101 1
#define RADIOLIB_EXCLUDE_LLCC68 1
#define RADIOLIB_EXCLUDE_NRF24 1
#define RADIOLIB_EXCLUDE_RF69 1
#define RADIOLIB_EXCLUDE_RFM69 1
#define RADIOLIB_EXCLUDE_SI443X 1
#define RADIOLIB_EXCLUDE_RFM2X 1
#define RADIOLIB_EXCLUDE_SX1231 1
#define RADIOLIB_EXCLUDE_SX1233 1
#define RADIOLIB_EXCLUDE_SX127X 1
#define RADIOLIB_EXCLUDE_SX128X 1

// Исключаем неиспользуемые протоколы
#define RADIOLIB_EXCLUDE_LORAWAN 1      // Поставьте 0, если LoRaWAN все же нужен
#define RADIOLIB_EXCLUDE_AFSK 1
#define RADIOLIB_EXCLUDE_AX25 1
#define RADIOLIB_EXCLUDE_BELL 1         // <--- ИСПРАВЛЕНО ЗДЕСЬ
#define RADIOLIB_EXCLUDE_APRS 1
#define RADIOLIB_EXCLUDE_HELLSCHREIBER 1
#define RADIOLIB_EXCLUDE_MORSE 1
#define RADIOLIB_EXCLUDE_RTTY 1
#define RADIOLIB_EXCLUDE_SSTV 1
#define RADIOLIB_EXCLUDE_PAGER 1
#define RADIOLIB_EXCLUDE_DIRECT_RECEIVE 1
#indif
*/
#include <RadioLib.h>
#include "src/MyLoRa/MyLoRaBase.h"
#endif

//#include <WiFi.h>
//#include <HTTPClient.h>
#include "src/Slib/SHTTPClient.h"

#include "WC_Config.h"
#include "WC_Task.h"


extern char SensorID[];
extern bool isAP, isSTA;
extern bool isLora;

void taskLora(void *pvParameters);
void initLora();
void readLora();
bool sendLora();
bool sendLoraAttr();
void setLoraReceive(bool _flag);
bool waitLoraRead(uint32_t _tm);

void IRAM_ATTR onLoraIrq();
void taskNet(void *pvParameters);
void handleEventWiFi(arduino_event_id_t event, arduino_event_info_t info);
bool sendCrmMoscowParam();
bool sendHttpParam();
void sendHttpParamOne(String &_host);
int getStatus();

bool sendParamTB();
bool sendAttributeTB();
bool authTB(const char *_key, const char *_secret);

uint16_t KeyGen(char *str);