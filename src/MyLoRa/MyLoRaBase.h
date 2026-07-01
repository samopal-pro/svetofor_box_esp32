/*
 * MyLoRaBase - оптимизированная версия для ESP32/Arduino
 * Только протокол V3
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "unishox2.h"

// ============================================================================
// Настройки оптимизации
// ============================================================================
#ifndef MAX_LEN_PAYLOAD
    #define MAX_LEN_PAYLOAD 128
#endif

#ifndef MYLORA_DEBUG
    #define MYLORA_DEBUG 0
#endif

// ============================================================================
// Маски пакетов V3
// ============================================================================
#define PACKET_V3_MASK                0x80
#define PACKET_V3_COMPRESS_MASK       0x40

// ============================================================================
// Типы пакетов V3
// ============================================================================
#define PACKET_V3_TYPE_START                  0x01
#define PACKET_V3_TYPE_ACK                    0x02
#define PACKET_V3_TYPE_ERROR                  0x03
#define PACKET_V3_TYPE_JSON_TELEMETRY         0x04
#define PACKET_V3_TYPE_JSON_ATTRIBUTE         0x05
#define PACKET_V3_TYPE_JSON_ATTRIBUTE_REQUEST 0x06
#define PACKET_V3_TYPE_JSON_RPC               0x07

// ============================================================================
// Состояния
// ============================================================================
typedef enum {
    NSRX_NONE         = 0,
    NSRX_OK           = 1,
    NSRX_DUBLE        = 2,
    NSRX_BROADCAST    = 100,
    NSRX_ERROR        = -1,
    NSRX_ERROR_SIZE   = -2,
    NSRX_ERROR_ALIEN  = -3,
    NSRX_ERROR_CRC    = -4,
    NSRX_ERROR_TM     = -5,
} NetStateRX_t;

typedef enum {
    NSTX_NONE = 0,
    NSTX_SEND = 10,
    NSTX_OK   = 1,
    NSTX_ERROR = -1,
} NetStateTX_t;

// ============================================================================
// Заголовок пакета V3
// ============================================================================
typedef struct {
    uint8_t  Type;
    uint8_t  TTL;
    uint8_t  AddrTX[6];
    uint8_t  AddrRX[6];
    uint16_t Count;
    uint16_t CRC;
} __attribute__((packed)) MyLoRaHeaderV3_t;

// ============================================================================
// Класс адресов
// ============================================================================
class MyLoRaAddress {
public:
    static void Set(uint8_t* _addr, uint8_t _s0, uint8_t _s1, uint8_t _s2,
                    uint8_t _s3, uint8_t _s4, uint8_t _s5);
    static void Set(uint8_t* _addr, uint64_t _id);
    static void Copy(uint8_t* _addr, uint8_t* _src);
    static bool Cmp(uint8_t* _addr1, uint8_t* _addr2);
    static char* Get(char* s, uint8_t* _addr);
    static bool isBroadcast(uint8_t* _addr);
    static void SetBroadcast(uint8_t* _addr);
};

// ============================================================================
// Основной класс (только V3)
// ============================================================================
class MyLoRaBaseClass {
private:
    uint16_t saveCount;
    uint8_t  saveAddr[6];
    
    // CRC
    uint16_t calcCRC(const uint8_t* _data, uint16_t _len);
    void setCRC(uint8_t* _buff, uint16_t _len);
    bool checkCRC(const uint8_t* _buff, uint16_t _len);

public:
    // ========================================================================
    // JSON документ
    // ========================================================================
    DynamicJsonDocument Json;
    
    // ========================================================================
    // Адреса
    // ========================================================================
    uint8_t Addr[6];
    uint8_t AddrRX[6];
    bool isGate;
    bool isTX_V3;
    bool isRX_V3;
    
    // ========================================================================
    // Данные пакета
    // ========================================================================
    int16_t  Rssi;
    uint16_t Node;
    uint8_t  TTL;
    uint16_t Count;
    
    // ========================================================================
    // Буферы
    // ========================================================================
    uint8_t  BufferTX[MAX_LEN_PAYLOAD];
    uint16_t LengthTX;
    uint8_t  BufferRX[MAX_LEN_PAYLOAD];
    uint16_t LengthRX;
    
    // ========================================================================
    // Заголовки
    // ========================================================================
    MyLoRaHeaderV3_t HeaderTX_V3;
    MyLoRaHeaderV3_t HeaderRX_V3;
    
    // ========================================================================
    // Состояния
    // ========================================================================
    NetStateTX_t StateTX;
    NetStateRX_t StateRX;
    
    // ========================================================================
    // Конструкторы
    // ========================================================================
    MyLoRaBaseClass(uint16_t _node, bool _is_gate = false);
    MyLoRaBaseClass(uint64_t _addr, bool _is_gate = false);
    
    // ========================================================================
    // Основные методы
    // ========================================================================
    void setTTL(uint32_t _time);
    uint32_t decodeTTL(uint8_t _ttl);
    bool RX(uint8_t* _payload, uint16_t _size, int16_t _rssi);
    void SetRequest(bool _isOK);
    void PrintRX();
    void PrintTX();
    
    // ========================================================================
    // Методы работы с JSON
    // ========================================================================
    void SetHeaderTX(uint8_t _type, uint8_t* _addr_rx, bool _is_compress = true);
    bool SetJsonBodyTX();
    bool GetJson();
    
    // ========================================================================
    // Вспомогательные методы
    // ========================================================================
    uint16_t SetCRC_V3(uint8_t* _buff, uint16_t _len);
    bool CheckCRC_V3(uint8_t* _buff, uint16_t _len);
    bool CheckDouble_V3();
    bool RX_V3(uint8_t* _payload, uint16_t _size, int16_t _rssi);
    
    void PrintRX_V3();
    void PrintTX_V3();

private:
    // Внутренние методы CRC
    uint16_t calcCRC_V3_internal(uint8_t* _buff, uint16_t _len);
    void setCRC_V3_internal(uint8_t* _buff, uint16_t _len);
    bool checkCRC_V3_internal(uint8_t* _buff, uint16_t _len);
};