#include "LD2413.h"

// ============================================================================
// Инициализация
// ============================================================================

void LD2413::begin() {
    RADAR_SERIAL.begin(RADAR_BAUDRATE);
}

void LD2413::begin(uint8_t rxPin, uint8_t txPin) {
    RADAR_SERIAL.begin(RADAR_BAUDRATE, SERIAL_8N1, rxPin, txPin);
}

// ============================================================================
// Основные функции
// ============================================================================

void LD2413::init(uint16_t minDist, uint16_t maxDist, uint16_t timeout) {
    // Останавливаем радар
    sendCommand(CMD_STOP);
    waitForCommandResponse();
    
    // Настраиваем параметры
    sendCommandWithArg(CMD_SET_MIN_DISTANCE, minDist);
    waitForCommandResponse();
    
    sendCommandWithArg(CMD_SET_MAX_DISTANCE, maxDist);
    waitForCommandResponse();
    
    sendCommandWithArg(CMD_SET_TIMEOUT, timeout);
    waitForCommandResponse();
    
    // Запускаем радар
    sendCommand(CMD_START);
    waitForCommandResponse();
}

void LD2413::calibrate() {
    sendCommand(CMD_STOP);
    waitForCommandResponse();
    
    sendCommand(CMD_SET_THRESHOLD);
    waitForCommandResponse();
    
    sendCommand(CMD_START);
    waitForCommandResponse();
}

// ============================================================================
// Отправка команд
// ============================================================================

void LD2413::sendCommand(uint8_t cmd) {
    #ifdef ENABLE_DEBUG_SERIAL
        debugPrintCommand(cmd);
    #endif
    
    RADAR_SERIAL.flush();
    
    // Заголовок: FD FC FB FA
    for (uint8_t syncByte : SYNC_BYTES) {
        RADAR_SERIAL.write(syncByte);
    }
    
    // Длина данных (2 байта)
    RADAR_SERIAL.write(0x02);
    RADAR_SERIAL.write(0x00);
    
    // Команда
    RADAR_SERIAL.write(cmd);
    RADAR_SERIAL.write(0x00);
    
    // Футер
    for (uint8_t footerByte : PACKET_FOOTER) {
        RADAR_SERIAL.write(footerByte);
    }
}

void LD2413::sendCommandWithArg(uint8_t cmd, uint16_t arg) {
    uint8_t lowByte = arg & 0xFF;
    uint8_t highByte = (arg >> 8) & 0xFF;
    
    #ifdef ENABLE_DEBUG_SERIAL
        debugPrintCommandWithArg(cmd, arg);
    #endif
    
    RADAR_SERIAL.flush();
    
    // Заголовок: FD FC FB FA
    for (uint8_t syncByte : SYNC_BYTES) {
        RADAR_SERIAL.write(syncByte);
    }
    
    // Длина данных (4 байта)
    RADAR_SERIAL.write(0x04);
    RADAR_SERIAL.write(0x00);
    
    // Команда
    RADAR_SERIAL.write(cmd);
    RADAR_SERIAL.write(0x00);
    
    // Аргумент (2 байта, little-endian)
    RADAR_SERIAL.write(lowByte);
    RADAR_SERIAL.write(highByte);
    
    // Футер
    for (uint8_t footerByte : PACKET_FOOTER) {
        RADAR_SERIAL.write(footerByte);
    }
}

// ============================================================================
// Ожидание ответов
// ============================================================================

bool LD2413::waitForCommandResponse(uint32_t timeoutMs) {
    uint8_t byte;
    uint8_t syncIndex = 0;
    uint8_t dataLen = 0;
    uint8_t bytesRead = 0;
    
    uint32_t startTime = millis();
    
    while (true) {
        // Проверка таймаута
        if (timeoutMs > 0 && (millis() - startTime) > timeoutMs) {
            #ifdef ENABLE_DEBUG_SERIAL
                ENABLE_DEBUG_SERIAL.println("<<< cmd timeout");
            #endif
            return false;
        }
        
        if (!RADAR_SERIAL.available()) {
            continue;
        }
        
        uint8_t c = RADAR_SERIAL.read();
        
        // Поиск синхропоследовательности FD FC FB FA
        if (syncIndex < 4) {
            if (c == SYNC_BYTES[syncIndex]) {
                syncIndex++;
            } else {
                syncIndex = 0; // Сброс при несовпадении
            }
            continue;
        }
        
        // Чтение длины пакета (после синхро-байтов)
        if (dataLen == 0) {
            dataLen = c;
            #ifdef ENABLE_DEBUG_SERIAL
                ENABLE_DEBUG_SERIAL.print("<<< cmd len=");
                ENABLE_DEBUG_SERIAL.println(dataLen);
            #endif
            continue;
        }
        
        // Пропускаем байт 0x00 после длины
        if (bytesRead == 0) {
            bytesRead++;
            continue;
        }
        
        // Чтение данных пакета
        #ifdef ENABLE_DEBUG_SERIAL
            debugPrintByte(c);
        #endif
        
        bytesRead++;
        
        // Проверяем, прочитали ли весь пакет
        // bytesRead: 1 (пропущенный 0x00) + dataLen (данные)
        if (bytesRead >= dataLen + 1) {
            #ifdef ENABLE_DEBUG_SERIAL
                ENABLE_DEBUG_SERIAL.println();
            #endif
            return true;
        }
    }
}

float LD2413::waitForData(uint32_t timeoutMs) {
    uint8_t byte;
    uint8_t syncIndex = 0;
    uint8_t dataLen = 0;
    uint8_t bytesRead = 0;
    uint8_t floatBytes[4] = {0};
    uint8_t floatIndex = 0;
    
    uint32_t startTime = millis();
    float result = NAN;
    
    while (true) {
        // Проверка таймаута
        if (timeoutMs > 0 && (millis() - startTime) > timeoutMs) {
            return NAN;
        }
        
        if (!RADAR_SERIAL.available()) {
            // Если нет данных, возвращаем последнее полученное значение
            return result;
        }
        
        uint8_t c = RADAR_SERIAL.read();
        
        // Поиск синхропоследовательности F4 F3 F2 F1
        if (syncIndex < 4) {
            if (c == DATA_SYNC[syncIndex]) {
                syncIndex++;
            } else {
                syncIndex = 0;
            }
            continue;
        }
        
        // Чтение длины пакета
        if (dataLen == 0) {
            dataLen = c;
            #ifdef ENABLE_DEBUG_SERIAL
                ENABLE_DEBUG_SERIAL.print("<<< data len=");
                ENABLE_DEBUG_SERIAL.print(dataLen);
            #endif
            continue;
        }
        
        // Пропускаем байт 0x00
        if (bytesRead == 0) {
            bytesRead++;
            continue;
        }
        
        // Чтение данных пакета (4 байта float)
        if (floatIndex < 4) {
            floatBytes[floatIndex++] = c;
            #ifdef ENABLE_DEBUG_SERIAL
                debugPrintByte(c);
            #endif
        }
        
        bytesRead++;
        
        // Если прочитали 4 байта float
        if (floatIndex == 4) {
            // Преобразование байтов в float (little-endian)
            union {
                float f;
                uint8_t b[4];
            } floatUnion;
            
            for (int i = 0; i < 4; i++) {
                floatUnion.b[i] = floatBytes[i];
            }
            
            result = floatUnion.f;
            
            #ifdef ENABLE_DEBUG_SERIAL
                ENABLE_DEBUG_SERIAL.print(" val=");
                ENABLE_DEBUG_SERIAL.println(result, 0);
            #endif
            
            // Сброс для следующего пакета
            floatIndex = 0;
            bytesRead = 0;
            dataLen = 0;
            syncIndex = 0;
        }
    }
}

// ============================================================================
// Вспомогательные методы
// ============================================================================

void LD2413::debugPrintByte(uint8_t byte) {
    #ifdef ENABLE_DEBUG_SERIAL
        if (byte < 0x10) {
            ENABLE_DEBUG_SERIAL.print(" 0x0");
        } else {
            ENABLE_DEBUG_SERIAL.print(" 0x");
        }
        ENABLE_DEBUG_SERIAL.print(byte, HEX);
    #endif
}

void LD2413::debugPrintCommand(uint8_t cmd, const char* label) {
    #ifdef ENABLE_DEBUG_SERIAL
        ENABLE_DEBUG_SERIAL.print(">>> ");
        ENABLE_DEBUG_SERIAL.print(label);
        debugPrintByte(cmd);
        ENABLE_DEBUG_SERIAL.println();
    #endif
}

void LD2413::debugPrintCommandWithArg(uint8_t cmd, uint16_t arg) {
    #ifdef ENABLE_DEBUG_SERIAL
        ENABLE_DEBUG_SERIAL.print(">>> cmd");
        debugPrintByte(cmd);
        ENABLE_DEBUG_SERIAL.print(" arg");
        debugPrintByte(arg & 0xFF);
        debugPrintByte((arg >> 8) & 0xFF);
        ENABLE_DEBUG_SERIAL.println();
    #endif
}