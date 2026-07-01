#include "MyLoRaBase.h"

// ============================================================================
// Константы CRC
// ============================================================================
#define CRC_INIT 0xFFFF
#define CRC_POLY 0x1021

// ============================================================================
// Реализация MyLoRaAddress
// ============================================================================
void MyLoRaAddress::Set(uint8_t* _addr, uint8_t _s0, uint8_t _s1, uint8_t _s2,
                        uint8_t _s3, uint8_t _s4, uint8_t _s5) {
    _addr[0] = _s0; _addr[1] = _s1; _addr[2] = _s2;
    _addr[3] = _s3; _addr[4] = _s4; _addr[5] = _s5;
}

void MyLoRaAddress::Set(uint8_t* _addr, uint64_t _id) {
    for (int i = 5; i >= 0; i--) {
        _addr[i] = (_id >> (8 * (5 - i))) & 0xFF;
    }
}

void MyLoRaAddress::Copy(uint8_t* _addr, uint8_t* _src) {
    _addr[0] = _src[0]; _addr[1] = _src[1]; _addr[2] = _src[2];
    _addr[3] = _src[3]; _addr[4] = _src[4]; _addr[5] = _src[5];
}

bool MyLoRaAddress::Cmp(uint8_t* _addr1, uint8_t* _addr2) {
    return (_addr1[0] == _addr2[0] && _addr1[1] == _addr2[1] &&
            _addr1[2] == _addr2[2] && _addr1[3] == _addr2[3] &&
            _addr1[4] == _addr2[4] && _addr1[5] == _addr2[5]);
}

char* MyLoRaAddress::Get(char* s, uint8_t* _addr) {
    sprintf(s, "%02X%02X%02X%02X%02X%02X",
            _addr[0], _addr[1], _addr[2],
            _addr[3], _addr[4], _addr[5]);
    return s;
}

bool MyLoRaAddress::isBroadcast(uint8_t* _addr) {
    return (_addr[0] == 0xFF && _addr[1] == 0xFF && _addr[2] == 0xFF &&
            _addr[3] == 0xFF && _addr[4] == 0xFF && _addr[5] == 0xFF);
}

void MyLoRaAddress::SetBroadcast(uint8_t* _addr) {
    _addr[0] = 0xFF; _addr[1] = 0xFF; _addr[2] = 0xFF;
    _addr[3] = 0xFF; _addr[4] = 0xFF; _addr[5] = 0xFF;
}

// ============================================================================
// CRC вычисления
// ============================================================================
uint16_t MyLoRaBaseClass::calcCRC(const uint8_t* _data, uint16_t _len) {
    uint16_t crc = CRC_INIT;
    for (uint16_t i = 0; i < _len; i++) {
        crc ^= (uint16_t)_data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ CRC_POLY;
            else crc <<= 1;
        }
    }
    return crc;
}

void MyLoRaBaseClass::setCRC(uint8_t* _buff, uint16_t _len) {
    uint16_t crc = calcCRC(_buff, _len);
    uint16_t offset = sizeof(MyLoRaHeaderV3_t) - sizeof(uint16_t);
    memcpy(_buff + offset, &crc, sizeof(uint16_t));
}

bool MyLoRaBaseClass::checkCRC(const uint8_t* _buff, uint16_t _len) {
    uint16_t offset = sizeof(MyLoRaHeaderV3_t) - sizeof(uint16_t);
    uint16_t crcCalc = calcCRC(_buff, _len);
    uint16_t crcStored;
    memcpy(&crcStored, _buff + offset, sizeof(uint16_t));
    return crcCalc == crcStored;
}

// ============================================================================
// CRC V3 внутренние
// ============================================================================
uint16_t MyLoRaBaseClass::calcCRC_V3_internal(uint8_t* _buff, uint16_t _len) {
    uint16_t crc = CRC_INIT;
    uint16_t crcOffset = sizeof(MyLoRaHeaderV3_t) - sizeof(uint16_t);
    
    for (uint16_t i = 0; i < _len; i++) {
        if (i >= crcOffset && i < crcOffset + sizeof(uint16_t)) continue;
        crc ^= (uint16_t)_buff[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ CRC_POLY;
            else crc <<= 1;
        }
    }
    return crc;
}

void MyLoRaBaseClass::setCRC_V3_internal(uint8_t* _buff, uint16_t _len) {
    uint16_t crc = calcCRC_V3_internal(_buff, _len);
    uint16_t offset = sizeof(MyLoRaHeaderV3_t) - sizeof(uint16_t);
    memcpy(_buff + offset, &crc, sizeof(uint16_t));
}

bool MyLoRaBaseClass::checkCRC_V3_internal(uint8_t* _buff, uint16_t _len) {
    uint16_t crcCalc = calcCRC_V3_internal(_buff, _len);
    uint16_t offset = sizeof(MyLoRaHeaderV3_t) - sizeof(uint16_t);
    uint16_t crcStored;
    memcpy(&crcStored, _buff + offset, sizeof(uint16_t));
    return crcCalc == crcStored;
}

// ============================================================================
// Конструкторы
// ============================================================================
MyLoRaBaseClass::MyLoRaBaseClass(uint16_t _node, bool _is_gate)
    : Json(256)
{
    Node = _node;
    Count = 0;
    saveCount = 0xFFFF;
    MyLoRaAddress::Set(saveAddr, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    TTL = 0;
    isGate = _is_gate;
    isTX_V3 = false;
    isRX_V3 = false;
    StateTX = NSTX_NONE;
    StateRX = NSRX_NONE;
    LengthTX = 0;
    LengthRX = 0;
    Rssi = 0;
    memset(Addr, 0, 6);
    memset(AddrRX, 0, 6);
    memset(BufferTX, 0, MAX_LEN_PAYLOAD);
    memset(BufferRX, 0, MAX_LEN_PAYLOAD);
}

MyLoRaBaseClass::MyLoRaBaseClass(uint64_t _addr, bool _is_gate)
    : Json(256)
{
    MyLoRaAddress::Set(Addr, _addr);
    Count = 0;
    saveCount = 0xFFFF;
    MyLoRaAddress::Set(saveAddr, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    TTL = 0;
    isGate = _is_gate;
    isTX_V3 = false;
    isRX_V3 = false;
    StateTX = NSTX_NONE;
    StateRX = NSRX_NONE;
    LengthTX = 0;
    LengthRX = 0;
    Rssi = 0;
    memset(AddrRX, 0, 6);
    memset(BufferTX, 0, MAX_LEN_PAYLOAD);
    memset(BufferRX, 0, MAX_LEN_PAYLOAD);
    
    Node = _is_gate ? 0 : 0xFFFF;
}

// ============================================================================
// Управление TTL
// ============================================================================
void MyLoRaBaseClass::setTTL(uint32_t _time) {
    uint32_t x = 2;
    TTL = 0;
    if (_time == 0) return;
    TTL = 1;
    if (_time <= 2) return;
    for (TTL = 2; TTL < 255; TTL++) {
        x *= 2;
        if (x > _time) break;
    }
}

uint32_t MyLoRaBaseClass::decodeTTL(uint8_t _ttl) {
    if (_ttl == 0) return 0;
    return (uint32_t)pow(2, _ttl);
}

// ============================================================================
// Методы V3
// ============================================================================
void MyLoRaBaseClass::SetHeaderTX(uint8_t _type, uint8_t* _addr_rx, bool _is_compress) {
    isTX_V3 = true;
    HeaderTX_V3.Type = PACKET_V3_MASK | _type;
    if (_is_compress) HeaderTX_V3.Type |= PACKET_V3_COMPRESS_MASK;
    HeaderTX_V3.TTL = TTL;
    HeaderTX_V3.Count = Count++;
    MyLoRaAddress::Copy(HeaderTX_V3.AddrTX, Addr);
    MyLoRaAddress::Copy(HeaderTX_V3.AddrRX, _addr_rx);
    HeaderTX_V3.CRC = 0;
    
    LengthTX = sizeof(MyLoRaHeaderV3_t);
    memcpy(BufferTX, &HeaderTX_V3, LengthTX);
    Json.clear();
}

bool MyLoRaBaseClass::SetJsonBodyTX() {
    bool isCompressed = (HeaderTX_V3.Type & PACKET_V3_COMPRESS_MASK) != 0;
    
    size_t jsonLen = serializeJson(Json, BufferTX + LengthTX,
                                   MAX_LEN_PAYLOAD - LengthTX);
    
    if (jsonLen == 0) return false;
    
    if (isCompressed) {
        char compressed[MAX_LEN_PAYLOAD];
        int compLen = unishox2_compress((char*)(BufferTX + LengthTX),
                                        jsonLen, compressed);
        if (compLen <= 0) return false;
        
        memcpy(BufferTX + LengthTX, compressed, compLen);
        LengthTX += compLen;
        
#if MYLORA_DEBUG
        Serial.print(F(">>> COMPRESS JSON: "));
        Serial.print((char*)(BufferTX + sizeof(MyLoRaHeaderV3_t)));
        Serial.print(F(" RATIO "));
        Serial.print((float)compLen / jsonLen * 100, 1);
        Serial.print(F("% "));
        Serial.print(compLen);
        Serial.print(F("/"));
        Serial.println(jsonLen);
#endif
    } else {
        LengthTX += jsonLen;
#if MYLORA_DEBUG
        Serial.print(F(">>> JSON: "));
        Serial.println((char*)(BufferTX + sizeof(MyLoRaHeaderV3_t)));
#endif
    }
    
    setCRC_V3_internal(BufferTX, LengthTX);
    return true;
}

bool MyLoRaBaseClass::GetJson() {
    if (!isRX_V3) return false;
    
    uint16_t offset = sizeof(MyLoRaHeaderV3_t);
    bool isCompressed = (HeaderRX_V3.Type & PACKET_V3_COMPRESS_MASK) != 0;
    
    if (isCompressed) {
        char decompressed[MAX_LEN_PAYLOAD];
        int decompLen = unishox2_decompress((char*)BufferRX + offset,
                                            LengthRX - offset,
                                            decompressed);
        if (decompLen <= 0) return false;
        decompressed[decompLen] = '\0';
        
        memcpy(BufferRX + offset, decompressed, decompLen + 1);
        LengthRX = offset + decompLen;
    }
    
    BufferRX[LengthRX] = '\0';
    Json.clear();
    DeserializationError error = deserializeJson(Json, (char*)BufferRX + offset);
    return error == DeserializationError::Ok;
}

uint16_t MyLoRaBaseClass::SetCRC_V3(uint8_t* _buff, uint16_t _len) {
    setCRC_V3_internal(_buff, _len);
    return 0;
}

bool MyLoRaBaseClass::CheckCRC_V3(uint8_t* _buff, uint16_t _len) {
    return checkCRC_V3_internal(_buff, _len);
}

bool MyLoRaBaseClass::CheckDouble_V3() {
    bool isDuplicate = false;
    if (saveCount == HeaderRX_V3.Count && 
        MyLoRaAddress::Cmp(HeaderRX_V3.AddrTX, saveAddr)) {
        isDuplicate = true;
    }
    saveCount = HeaderRX_V3.Count;
    MyLoRaAddress::Copy(saveAddr, HeaderRX_V3.AddrTX);
    return isDuplicate;
}

bool MyLoRaBaseClass::RX_V3(uint8_t* _payload, uint16_t _size, int16_t _rssi) {
    isRX_V3 = true;
    uint16_t offset = sizeof(MyLoRaHeaderV3_t);
    
    Rssi = _rssi;
    LengthRX = _size;
    
    if (_size < offset || _size >= MAX_LEN_PAYLOAD) {
        StateRX = NSRX_ERROR_SIZE;
        return false;
    }
    
    memcpy(&HeaderRX_V3, _payload, offset);
    memcpy(BufferRX, _payload, _size);
    
    if (!checkCRC_V3_internal(BufferRX, _size)) {
        StateRX = NSRX_ERROR_CRC;
        return false;
    }
    
    StateRX = NSRX_OK;
    
    if (MyLoRaAddress::isBroadcast(HeaderRX_V3.AddrRX)) {
        StateRX = NSRX_BROADCAST;
    } else if (!MyLoRaAddress::Cmp(Addr, HeaderRX_V3.AddrRX)) {
        StateRX = NSRX_ERROR_ALIEN;
        return false;
    }
    
    if (CheckDouble_V3()) {
        StateRX = NSRX_DUBLE;
        return true;
    }
    
    return true;
}

bool MyLoRaBaseClass::RX(uint8_t* _payload, uint16_t _size, int16_t _rssi) {
    if (_size == 0) {
        StateRX = NSRX_ERROR_SIZE;
        return false;
    }
    
    if (!(_payload[0] & PACKET_V3_MASK)) {
        StateRX = NSRX_ERROR;
        return false;
    }
    
    return RX_V3(_payload, _size, _rssi);
}

void MyLoRaBaseClass::SetRequest(bool _isOK) {
    if (isRX_V3) {
        if (_isOK) {
            SetHeaderTX(PACKET_V3_TYPE_ACK, HeaderRX_V3.AddrTX, false);
        } else {
            SetHeaderTX(PACKET_V3_TYPE_ERROR, HeaderRX_V3.AddrTX, false);
        }
        setCRC_V3_internal(BufferTX, LengthTX);
    }
}

// ============================================================================
// Отладка
// ============================================================================
void MyLoRaBaseClass::PrintTX() {
    if (isTX_V3) {
        PrintTX_V3();
    }
}

void MyLoRaBaseClass::PrintRX() {
    if (isRX_V3) {
        PrintRX_V3();
    }
}

void MyLoRaBaseClass::PrintTX_V3() {
#if MYLORA_DEBUG
    char s[16];
    Serial.print(F(">>> Type="));
    Serial.print(HeaderTX_V3.Type, BIN);
    Serial.print(F(" TX="));
    Serial.print(MyLoRaAddress::Get(s, HeaderTX_V3.AddrTX));
    Serial.print(F(" RX="));
    Serial.print(MyLoRaAddress::Get(s, HeaderTX_V3.AddrRX));
    Serial.print(F(" TTL="));
    Serial.print(HeaderTX_V3.TTL);
    Serial.print(F(" Count="));
    Serial.print(HeaderTX_V3.Count);
    Serial.print(F(" CRC="));
    Serial.print(HeaderTX_V3.CRC);
    Serial.print(F(" Size="));
    Serial.println(LengthTX);
#endif
}

void MyLoRaBaseClass::PrintRX_V3() {
#if MYLORA_DEBUG
    char s[16];
    Serial.print(F("<<< "));
    switch (StateRX) {
        case NSRX_OK:
        case NSRX_BROADCAST:
            Serial.print(F("Type="));
            Serial.print(HeaderRX_V3.Type, BIN);
            Serial.print(F(" TX="));
            Serial.print(MyLoRaAddress::Get(s, HeaderRX_V3.AddrTX));
            Serial.print(F(" RX="));
            Serial.print(MyLoRaAddress::Get(s, HeaderRX_V3.AddrRX));
            Serial.print(F(" CRC="));
            Serial.print(HeaderRX_V3.CRC);
            Serial.print(F(" TTL="));
            Serial.print(HeaderRX_V3.TTL);
            Serial.print(F(" Size="));
            Serial.print(LengthRX);
            Serial.print(F(" Rssi="));
            Serial.print(Rssi);
            Serial.print(F(" Count="));
            Serial.println(HeaderRX_V3.Count);
            break;
        case NSRX_DUBLE:
            Serial.print(F("Duplicate Addr="));
            Serial.print(MyLoRaAddress::Get(s, HeaderRX_V3.AddrTX));
            Serial.print(F(" Count="));
            Serial.println(HeaderRX_V3.Count);
            break;
        case NSRX_ERROR_CRC:
            Serial.println(F("ERROR CRC"));
            break;
        case NSRX_ERROR_ALIEN:
            Serial.print(F("ERROR Other Addr="));
            Serial.println(MyLoRaAddress::Get(s, HeaderRX_V3.AddrRX));
            break;
        default:
            Serial.print(F("ERROR "));
            Serial.println(StateRX);
            break;
    }
#endif
}