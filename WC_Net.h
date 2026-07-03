#pragma once

#include "MyConfig.h"
#include "src/Slib/SHTTPClient.h"
#include "WC_Config.h"
#include "WC_Task.h"

// ============================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ NET
// ============================================================

// Флаги состояния WiFi
extern bool isAP;            // Активен режим точки доступа
extern bool isSTA;           // Активен режим клиента (подключение к роутеру)
extern bool isSendNet;       // Флаг необходимости сброса таймеров при первом подключении
extern bool isWiFiConnected; // Текущее состояние подключения к WiFi сети

// ============================================================
// ПРОТОТИПЫ ФУНКЦИЙ
// ============================================================

// Задачи FreeRTOS
void taskWiFiManager(void *pvParameters);  // Задача управления WiFi подключением
void taskHttpSender(void *pvParameters);   // Задача отправки HTTP запросов на серверы

// Получение текущего статуса датчика
int getStatus();

// Отправка данных на внешние серверы
bool sendCrmMoscowParam();               // Отправка в CRM Москва
bool sendHttpParam();                    // Отправка на HTTP шлюзы (список серверов)
bool sendHttpParamOne(String &host);     // Отправка на один конкретный HTTP шлюз
bool sendParamTB();                      // Отправка телеметрии в ThingsBoard
bool sendAttributeTB();                  // Отправка атрибутов устройства в ThingsBoard
bool authTB(const char *key, const char *secret);  // Авторизация устройства в ThingsBoard

// Вспомогательные функции
uint16_t KeyGen(char *str);  // Генерация контрольной суммы для запросов