#pragma once
#include <Arduino.h>

// Настройки отладки
// #define ENABLE_DEBUG_SERIAL Serial

// Настройки аппаратного UART
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3)
    #define RADAR_SERIAL Serial2
#else
    #define RADAR_SERIAL Serial1
#endif

// Константы
#define RADAR_BAUDRATE       115200UL
#define RADAR_CMD_TIMEOUT    1000UL
#define RADAR_DATA_TIMEOUT   1000UL

// Команды радара
enum RadarCommand : uint8_t {
    CMD_STOP               = 0xFF,
    CMD_START              = 0xFE,
    CMD_SET_MIN_DISTANCE   = 0x74,
    CMD_SET_MAX_DISTANCE   = 0x75,
    CMD_SET_THRESHOLD      = 0x72,
    CMD_SET_TIMEOUT        = 0x71,
    CMD_GET_FIRMWARE       = 0x00
};

// Заголовки пакетов
enum PacketHeader : uint8_t {
    HEADER_COMMAND = 0xFD,
    HEADER_DATA    = 0xF4
};

class LD2413 {
public:
    LD2413() = default;
    
    // Инициализация
    void begin();
    void begin(uint8_t rxPin, uint8_t txPin);
    
    // Основные функции
    void init(uint16_t minDistance, uint16_t maxDistance, uint16_t timeout);
    void calibrate();
    
    // Работа с командами
    void sendCommand(uint8_t cmd);
    void sendCommandWithArg(uint8_t cmd, uint16_t arg);
    
    // Ожидание ответов
    bool waitForCommandResponse(uint32_t timeoutMs = RADAR_CMD_TIMEOUT);
    float waitForData(uint32_t timeoutMs = RADAR_DATA_TIMEOUT);
    
private:
    // Вспомогательные методы
    void sendPacketHeader(uint8_t dataLen);
    void debugPrintByte(uint8_t byte);
    void debugPrintCommand(uint8_t cmd, const char* label = "cmd");
    void debugPrintCommandWithArg(uint8_t cmd, uint16_t arg);
    
    // Чтение данных из последовательного порта с синхронизацией
    bool readByteWithSync(uint8_t& byte, uint32_t timeoutMs);
    
    // Константы пакетов
    static constexpr uint8_t SYNC_BYTES[4] = {0xFD, 0xFC, 0xFB, 0xFA};
    static constexpr uint8_t DATA_SYNC[4]  = {0xF4, 0xF3, 0xF2, 0xF1};
    static constexpr uint8_t PACKET_FOOTER[4] = {0x04, 0x03, 0x02, 0x01};
};