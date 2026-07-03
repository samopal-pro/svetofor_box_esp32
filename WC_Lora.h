#pragma once

#include "MyConfig.h"

#ifdef IS_LORA
#include <SPI.h>
#include <RadioLib.h>
#include "src/MyLoRa/MyLoRaBase.h"
#endif

// ============================================================
// ПОДКЛЮЧАЕМ НЕОБХОДИМЫЕ ВНЕШНИЕ ЗАВИСИМОСТИ
// ============================================================

// Доступ к конфигурации
#include "WC_Config.h"

// Доступ к общим типам и функциям
#include "WC_Task.h"

/**
 * Инициализация подсистемы LoRa:
 * - Создает задачу FreeRTOS
 * - Настраивает модуль SX1262
 * - Запускает прием
 */
void loraInit();

/**
 * Возвращает статус готовности LoRa
 */
bool loraIsReady();

/**
 * Принудительная отправка телеметрии (например, по RPC команде)
 */
void loraForceSendTelemetry();

/**
 * Проверка наличия LoRa ACK
 */
bool loraHasAck();

/**
 * Сброс флага ACK
 */
void loraClearAck();

// ============================================================
// ВНУТРЕННИЕ ПРОТОТИПЫ (используются только в WC_Lora.cpp)
// ============================================================
#ifdef IS_LORA
void taskLora(void *pvParameters);
void initLoraModule();
void readLora();
bool sendLoraTelemetry();
bool sendLoraAttributes();
void setLoraReceive(bool flag);
bool waitLoraRead(uint32_t timeout);
void IRAM_ATTR onLoraIrq();
#endif