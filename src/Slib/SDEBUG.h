#pragma once

/*
 * SDEBUG - Универсальная библиотека отладочных сообщений для ESP32 Arduino IDE
 * Версия: 2.0
 * 
 * ===== НОВЫЕ ВОЗМОЖНОСТИ =====
 * - Полное исключение неиспользуемого кода при компиляции
 * - Макрофункции для условной компиляции
 * - DEBUG_JSON_DOC макрос для вывода JSON документов
 * - Улучшенная производительность
 * - Поддержка ArduinoJson
 *
 * ===== ИНСТРУКЦИЯ ПО ИСПОЛЬЗОВАНИЮ =====
 * 
 * 1. ПОДКЛЮЧЕНИЕ:
 *    #include "SDEBUG.h"
 * 
 * 2. НАСТРОЙКА УРОВНЕЙ ОТЛАДКИ:
 *    Глобальный уровень (в главном файле до #include):
 *      #define SDEBUG_LEVEL DEBUG_DEBUG    // 0-OFF, 1-ERROR, 2-INFO, 3-DEBUG
 *    
 *    Уровень модуля (в файле модуля до #include):
 *      #define MODULE_DEBUG_LEVEL DEBUG_INFO
 *      Или использовать глобальный:
 *      #define MODULE_DEBUG_LEVEL DEBUG_DEFAULT  (или не определять)
 * 
 * 3. ОПРЕДЕЛЕНИЕ ИМЕНИ МОДУЛЯ (перед #include):
 *    #define MODULE_NAME "WiFi"
 * 
 * 4. ИНИЦИАЛИЗАЦИЯ (в setup):
 *    SDEBUG_INIT(115200);
 * 
 * 5. МАКРОСЫ ВЫВОДА:
 *    Без перевода строки (можно собирать строку по частям):
 *      LOG_ERROR("текст");
 *      LOG_INFO("текст");
 *      LOG_DEBUG("текст");
 *    
 *    С переводом строки:
 *      LOG_ERRORLN("текст");
 *      LOG_INFOLN("текст");
 *      LOG_DEBUGLN("текст");
 *    
 *    Форматированный вывод:
 *      LOG_INFOLN("Температура: %.1f°C", temp);
 *      LOG_DEBUGLN("IP: %s, Port: %d", ip, port);
 *    
 *    Hex дамп:
 *      LOG_HEXDUMP(data, length);
 *    
 *    С указанием модуля напрямую:
 *      LOG_ERROR_M("WiFi", "Ошибка подключения");
 * 
 * 6. УРОВНИ ОТЛАДКИ:
 *    DEBUG_OFF    (0) - Ничего не выводится
 *    DEBUG_ERROR  (1) - Только LOG_ERROR/LOG_ERRORLN
 *    DEBUG_INFO   (2) - LOG_ERROR + LOG_INFO
 *    DEBUG_DEBUG  (3) - Все сообщения + LOG_HEXDUMP
 *    DEBUG_DEFAULT(255) - Использовать глобальный уровень
 * 
 * 7. ПРИМЕР НАСТРОЙКИ МОДУЛЯ:
 *    #define MODULE_NAME "SENSOR"
 *    #define MODULE_DEBUG_LEVEL DEBUG_ERROR  // Только ошибки
 *    #include "SDEBUG.h"
 * 
 * 8. ПРИМЕР ВЫВОДА:
 *    [ERR] WiFi: Ошибка подключения
 *    [MSG] MAIN: Система запущена
 *    [DBG] SENSOR: Чтение датчика...
 * 
 * 9. ОСОБЕННОСТИ:
 *    - Неиспользуемые уровни ПОЛНОСТЬЮ исключаются при компиляции
 *    - Каждый модуль может иметь свой уровень отладки
 *    - По умолчанию модули используют глобальный уровень
 *    - Есть макросы с LN (перевод строки) и без (сборка строки)
 *    - Цветовые коды убраны (не поддерживаются в консоли Arduino IDE)
 * 
 * 10. УСЛОВНАЯ КОМПИЛЯЦИЯ:
 *     SDEBUG_IF_DEBUG(
 *       // Код только для DEBUG уровня
 *     );
 * 
 * 11. СОВЕТЫ:
 *     - Для продакшена установите SDEBUG_LEVEL DEBUG_OFF
 *     - Для отладки конкретного модуля установите ему DEBUG_DEBUG
 *     - Используйте LOG_xxx без LN для прогресс-баров и длинных строк
 */

#include <Arduino.h>
#include <ArduinoJson.h>

// ===== УРОВНИ ОТЛАДКИ =====
#define DEBUG_OFF    0  // Ничего не выводить
#define DEBUG_ERROR  1  // Только ошибки
#define DEBUG_INFO   2  // Ошибки и сообщения
#define DEBUG_DEBUG  3  // Ошибки, сообщения и отладка

// ===== СПЕЦИАЛЬНЫЙ УРОВЕНЬ - ИСПОЛЬЗОВАТЬ ГЛОБАЛЬНЫЙ =====
#define DEBUG_DEFAULT 255  // Использовать глобальный уровень SDEBUG_LEVEL

// ===== ГЛОБАЛЬНЫЙ УРОВЕНЬ ОТЛАДКИ =====
#ifndef SDEBUG_LEVEL
  #define SDEBUG_LEVEL DEBUG_DEBUG
#endif

// ===== ИМЯ МОДУЛЯ ПО УМОЛЧАНИЮ =====
#ifndef MODULE_NAME
  #define MODULE_NAME "MAIN"
#endif

// ===== УРОВЕНЬ ОТЛАДКИ МОДУЛЯ =====
#ifndef MODULE_DEBUG_LEVEL
  #define MODULE_DEBUG_LEVEL DEBUG_DEFAULT
#endif

// ===== ВЫЧИСЛЕНИЕ ЭФФЕКТИВНОГО УРОВНЯ ОТЛАДКИ =====
#if MODULE_DEBUG_LEVEL == DEBUG_DEFAULT
  #define _EFFECTIVE_LEVEL SDEBUG_LEVEL
#else
  #define _EFFECTIVE_LEVEL MODULE_DEBUG_LEVEL
#endif

// ===== ТЕКСТОВЫЕ МЕТКИ =====
#define SDEBUG_STR_ERR   "ERR"
#define SDEBUG_STR_MSG   "MSG"
#define SDEBUG_STR_DBG   "DBG"

// ===== МАКРОС ФОРМАТИРОВАНИЯ ПРЕФИКСА =====
#define SDEBUG_PRINT_PREFIX(level) \
  Serial.printf("[%s] %s: ", level, MODULE_NAME)

// ===== МАКРОСЫ ВЫВОДА ОШИБОК =====
#if _EFFECTIVE_LEVEL >= DEBUG_ERROR
  #define _LOG_ERROR_ENABLED 1
  #define LOG_ERROR(fmt, ...) \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_ERR); \
      Serial.printf(fmt, ##__VA_ARGS__); \
    } while(0)
    
  #define LOG_ERRORLN(fmt, ...) \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_ERR); \
      Serial.printf(fmt "\r\n", ##__VA_ARGS__); \
    } while(0)
#else
  #define _LOG_ERROR_ENABLED 0
  #define LOG_ERROR(fmt, ...) ((void)0)
  #define LOG_ERRORLN(fmt, ...) ((void)0)
#endif

// ===== МАКРОСЫ ВЫВОДА СООБЩЕНИЙ =====
#if _EFFECTIVE_LEVEL >= DEBUG_INFO
  #define _LOG_INFO_ENABLED 1
  #define LOG_INFO(fmt, ...) \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_MSG); \
      Serial.printf(fmt, ##__VA_ARGS__); \
    } while(0)
    
  #define LOG_INFOLN(fmt, ...) \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_MSG); \
      Serial.printf(fmt "\r\n", ##__VA_ARGS__); \
    } while(0)
#else
  #define _LOG_INFO_ENABLED 0
  #define LOG_INFO(fmt, ...) ((void)0)
  #define LOG_INFOLN(fmt, ...) ((void)0)
#endif

// ===== МАКРОСЫ ОТЛАДОЧНОГО ВЫВОДА =====
#if _EFFECTIVE_LEVEL >= DEBUG_DEBUG
  #define _LOG_DEBUG_ENABLED 1
  #define LOG_DEBUG(fmt, ...) \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_DBG); \
      Serial.printf(fmt, ##__VA_ARGS__); \
    } while(0)
    
  #define LOG_DEBUGLN(fmt, ...) \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_DBG); \
      Serial.printf(fmt "\r\n", ##__VA_ARGS__); \
    } while(0)
    
  #define LOG_HEXDUMP(data, len) \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_DBG); \
      Serial.printf("Hex dump (%d bytes):\r\n", (int)(len)); \
      for (size_t _i = 0; _i < (len); _i++) { \
        if (_i % 16 == 0) Serial.print("  "); \
        Serial.printf("%02X ", ((const uint8_t*)(data))[_i]); \
        if ((_i + 1) % 16 == 0 || _i == (len) - 1) Serial.println(); \
      } \
    } while(0)
#else
  #define _LOG_DEBUG_ENABLED 0
  #define LOG_DEBUG(fmt, ...) ((void)0)
  #define LOG_DEBUGLN(fmt, ...) ((void)0)
  #define LOG_HEXDUMP(data, len) ((void)0)
#endif

// ===== ВСПОМОГАТЕЛЬНЫЕ МАКРОСЫ =====
#define SDEBUG_INIT(baud) \
  do { \
    Serial.begin(baud); \
    delay(100); \
  } while(0)

// Макросы с прямым указанием модуля
#if _EFFECTIVE_LEVEL >= DEBUG_ERROR
  #define LOG_ERROR_M(module, fmt, ...) \
    do { \
      Serial.printf("[ERR] %s: " fmt "\r\n", module, ##__VA_ARGS__); \
    } while(0)
#else
  #define LOG_ERROR_M(module, fmt, ...) ((void)0)
#endif

#if _EFFECTIVE_LEVEL >= DEBUG_INFO
  #define LOG_INFO_M(module, fmt, ...) \
    do { \
      Serial.printf("[MSG] %s: " fmt "\r\n", module, ##__VA_ARGS__); \
    } while(0)
#else
  #define LOG_INFO_M(module, fmt, ...) ((void)0)
#endif

#if _EFFECTIVE_LEVEL >= DEBUG_DEBUG
  #define LOG_DEBUG_M(module, fmt, ...) \
    do { \
      Serial.printf("[DBG] %s: " fmt "\r\n", module, ##__VA_ARGS__); \
    } while(0)
#else
  #define LOG_DEBUG_M(module, fmt, ...) ((void)0)
#endif

// ===== УСЛОВНАЯ КОМПИЛЯЦИЯ =====
#if _EFFECTIVE_LEVEL >= DEBUG_DEBUG
  #define SDEBUG_IF_DEBUG(code) do { code } while(0)
#else
  #define SDEBUG_IF_DEBUG(code) ((void)0)
#endif

#if _EFFECTIVE_LEVEL >= DEBUG_INFO
  #define SDEBUG_IF_INFO(code) do { code } while(0)
#else
  #define SDEBUG_IF_INFO(code) ((void)0)
#endif

#if _EFFECTIVE_LEVEL >= DEBUG_ERROR
  #define SDEBUG_IF_ERROR(code) do { code } while(0)
#else
  #define SDEBUG_IF_ERROR(code) ((void)0)
#endif

// ===== МАКРОСЫ ДЛЯ JSON =====
#if _EFFECTIVE_LEVEL >= DEBUG_DEBUG
  #define DEBUG_JSON_DOC(label, doc) \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_DBG); \
      Serial.printf("JSON %s:\r\n", label); \
      String _jsonStr; \
      serializeJsonPretty(doc, _jsonStr); \
      Serial.println(_jsonStr); \
    } while(0)
    
  #define DEBUG_JSON_DOC_COMPACT(label, doc) \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_DBG); \
      Serial.printf("JSON %s: ", label); \
      serializeJson(doc, Serial); \
      Serial.println(); \
    } while(0)
#else
  #define DEBUG_JSON_DOC(label, doc) ((void)0)
  #define DEBUG_JSON_DOC_COMPACT(label, doc) ((void)0)
#endif

// ===== МАКРОСЫ ДЛЯ ПАМЯТИ =====
#if _EFFECTIVE_LEVEL >= DEBUG_DEBUG
  #define DEBUG_MEMORY() \
    do { \
      SDEBUG_PRINT_PREFIX(SDEBUG_STR_DBG); \
      Serial.printf("Free heap: %u, Max alloc: %u, Frag: %u%%\r\n", \
        ESP.getFreeHeap(), ESP.getMaxAllocHeap(), \
        ESP.getFreeHeap() ? (ESP.getMaxAllocHeap() * 100) / ESP.getFreeHeap() : 0); \
    } while(0)
#else
  #define DEBUG_MEMORY() ((void)0)
#endif

// ===== МАКРОФУНКЦИИ ДЛЯ УСЛОВНОГО ВЫПОЛНЕНИЯ =====
#define SDEBUG_EXEC_ERROR(code) \
  do { if (_EFFECTIVE_LEVEL >= DEBUG_ERROR) { code; } } while(0)

#define SDEBUG_EXEC_INFO(code) \
  do { if (_EFFECTIVE_LEVEL >= DEBUG_INFO) { code; } } while(0)

#define SDEBUG_EXEC_DEBUG(code) \
  do { if (_EFFECTIVE_LEVEL >= DEBUG_DEBUG) { code; } } while(0)

// ===== ПРОВЕРКА УРОВНЕЙ (для использования в if) =====
#define SDEBUG_IS_ERROR() (_EFFECTIVE_LEVEL >= DEBUG_ERROR)
#define SDEBUG_IS_INFO()  (_EFFECTIVE_LEVEL >= DEBUG_INFO)
#define SDEBUG_IS_DEBUG() (_EFFECTIVE_LEVEL >= DEBUG_DEBUG)