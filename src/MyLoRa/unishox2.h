/*
 * Lightweight Unishox2 for Arduino/ESP32
 * Универсальная версия без зависимостей от архитектуры
 */

#pragma once

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Настройки
// ============================================================================
#define UNISHOX_VERSION "2.0-lite"
#define UNISHOX_MAX_BUFFER 128

// ============================================================================
// API
// ============================================================================

/**
 * Быстрое сжатие для JSON/текста
 * Возвращает размер сжатых данных или -1 при ошибке
 */
int unishox2_compress(const char* in, int len, char* out);

/**
 * Быстрая распаковка
 * Возвращает размер распакованных данных или -1 при ошибке
 */
int unishox2_decompress(const char* in, int len, char* out);

/**
 * Упрощенные имена для совместимости
 */
#define unishox2_compress_simple unishox2_compress
#define unishox2_decompress_simple unishox2_decompress

#ifdef __cplusplus
}
#endif