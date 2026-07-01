/*
 * Lightweight Unishox2 - минимальная реализация
 * Универсальная версия без зависимостей от архитектуры
 */

#include "unishox2.h"
#include <ctype.h>
#include <string.h>

// ============================================================================
// Таблица масок для битовых операций
// ============================================================================
static const uint8_t usx_mask[8] = {
    0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF
};

// ============================================================================
// Внутренние функции
// ============================================================================

/**
 * Добавление битов в выходной буфер
 */
static inline int append_bits(char* out, int olen, int bit_pos, uint8_t code, int clen) {
    if (bit_pos / 8 >= olen) return -1;
    
    while (clen > 0) {
        int byte_pos = bit_pos >> 3;
        int bit_offset = bit_pos & 7;
        
        if (byte_pos >= olen) return -1;
        
        int bits_to_write = clen;
        if (bits_to_write + bit_offset > 8) {
            bits_to_write = 8 - bit_offset;
        }
        
        uint8_t value = (code >> (clen - bits_to_write)) & usx_mask[bits_to_write - 1];
        out[byte_pos] |= (value >> bit_offset);
        
        bit_pos += bits_to_write;
        clen -= bits_to_write;
    }
    
    return bit_pos;
}

/**
 * Чтение одного бита
 */
static inline int read_bit(const char* in, int bit_pos) {
    return (in[bit_pos >> 3] & (0x80 >> (bit_pos & 7))) ? 1 : 0;
}

/**
 * Чтение нескольких битов
 */
static inline int read_bits(const char* in, int bit_pos, int count) {
    int result = 0;
    for (int i = 0; i < count; i++) {
        if (read_bit(in, bit_pos + i)) {
            result |= (1 << (count - 1 - i));
        }
    }
    return result;
}

// ============================================================================
// Кодирование символа
// ============================================================================

static int encode_char(char* out, int olen, int bit_pos, char c) {
    // Пробел
    if (c == ' ') {
        bit_pos = append_bits(out, olen, bit_pos, 0, 1);
        if (bit_pos < 0) return -1;
        bit_pos = append_bits(out, olen, bit_pos, 0, 5);
        return bit_pos;
    }
    
    // Строчные буквы (a-z)
    if (c >= 'a' && c <= 'z') {
        int code = c - 'a' + 1;
        bit_pos = append_bits(out, olen, bit_pos, 0, 1);
        if (bit_pos < 0) return -1;
        bit_pos = append_bits(out, olen, bit_pos, code, 5);
        return bit_pos;
    }
    
    // Прописные буквы (A-Z)
    if (c >= 'A' && c <= 'Z') {
        int code = c - 'A' + 1;
        bit_pos = append_bits(out, olen, bit_pos, 1, 1);
        if (bit_pos < 0) return -1;
        bit_pos = append_bits(out, olen, bit_pos, code, 5);
        return bit_pos;
    }
    
    // Цифры (0-9)
    if (c >= '0' && c <= '9') {
        int code = 27 + (c - '0');
        bit_pos = append_bits(out, olen, bit_pos, 0, 1);
        if (bit_pos < 0) return -1;
        bit_pos = append_bits(out, olen, bit_pos, code, 6);
        return bit_pos;
    }
    
    // Специальные символы JSON
    switch (c) {
        case '"':  // кавычка
            bit_pos = append_bits(out, olen, bit_pos, 0b00, 2);
            break;
        case ':':  // двоеточие
            bit_pos = append_bits(out, olen, bit_pos, 0b01, 2);
            break;
        case ',':  // запятая
            bit_pos = append_bits(out, olen, bit_pos, 0b10, 2);
            break;
        case '{':  // открывающая фигурная
            bit_pos = append_bits(out, olen, bit_pos, 0b11, 2);
            break;
        case '}':  // закрывающая фигурная
            bit_pos = append_bits(out, olen, bit_pos, 0b110, 3);
            break;
        case '[':  // открывающая квадратная
            bit_pos = append_bits(out, olen, bit_pos, 0b1110, 4);
            break;
        case ']':  // закрывающая квадратная
            bit_pos = append_bits(out, olen, bit_pos, 0b1111, 4);
            break;
        default:
            // Другие символы - 8 бит
            bit_pos = append_bits(out, olen, bit_pos, 0, 1);
            if (bit_pos < 0) return -1;
            bit_pos = append_bits(out, olen, bit_pos, (uint8_t)c, 7);
    }
    
    return bit_pos;
}

// ============================================================================
// Декодирование символа
// ============================================================================

static int decode_char(const char* in, int* bit_pos, int len, char* out) {
    if (*bit_pos + 6 >= len) return -1;
    
    // Читаем флаг регистра
    int is_upper = read_bit(in, *bit_pos);
    (*bit_pos)++;
    
    // Читаем код (5 бит)
    int code = read_bits(in, *bit_pos, 5);
    (*bit_pos) += 5;
    
    // Декодируем
    if (code == 0) {
        *out = ' ';
        return 1;
    } else if (code >= 1 && code <= 26) {
        *out = (is_upper ? 'A' : 'a') + code - 1;
        return 1;
    } else if (code >= 27 && code <= 36) {
        *out = '0' + (code - 27);
        return 1;
    }
    
    return -1;
}

// ============================================================================
// Публичные API
// ============================================================================

/**
 * Сжатие строки
 */
int unishox2_compress(const char* in, int len, char* out) {
    if (!in || !out || len <= 0) return -1;
    
    // Очищаем выходной буфер
    memset(out, 0, UNISHOX_MAX_BUFFER);
    int bit_pos = 0;
    
    // Кодируем каждый символ
    for (int i = 0; i < len; i++) {
        bit_pos = encode_char(out, UNISHOX_MAX_BUFFER, bit_pos, in[i]);
        if (bit_pos < 0) return -1;
    }
    
    // Выравнивание по байту (добавляем нулевые биты)
    int result = append_bits(out, UNISHOX_MAX_BUFFER, bit_pos, 0, 
                           (8 - (bit_pos % 8)) & 7);
    if (result < 0) return -1;
    
    // Возвращаем размер в байтах
    return (result + 7) / 8;
}

/**
 * Распаковка строки
 */
int unishox2_decompress(const char* in, int len, char* out) {
    if (!in || !out || len <= 0) return -1;
    
    // Очищаем выходной буфер
    memset(out, 0, UNISHOX_MAX_BUFFER);
    int bit_pos = 0;
    int out_pos = 0;
    int max_bits = len * 8;
    
    // Декодируем символы
    while (bit_pos + 6 < max_bits && out_pos < UNISHOX_MAX_BUFFER - 1) {
        char c;
        int result = decode_char(in, &bit_pos, max_bits, &c);
        if (result < 0) break;
        out[out_pos++] = c;
    }
    
    // Завершаем строку нулем
    out[out_pos] = '\0';
    return out_pos;
}