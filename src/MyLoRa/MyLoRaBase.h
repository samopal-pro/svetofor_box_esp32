#ifndef MyLoRaBase_h
#define MyLoRaBase_h
#include <Arduino.h>
#include <ArduinoJson.h>    ////https://github.com/bblanchon/ArduinoJson
#include "unishox2.h"       ////https://github.com/siara-cc/Unishox_Arduino_lib


// Служебные пакеты
#define PACKET_TYPE_START                      1  // Стартовый пакет к серверу
#define PACKET_TYPE_ACK                       30  // Подтверждение получения
#define PACKET_TYPE_CRC_ERROR                 31  // Ошибка контрольной суммы

#define PACKET_TYPE_JSON_TELEMETRY            60  // Пакет с телеметрией в JSON формате {"PAR1":VAL1,"PAR2"=VAL2} с CRC
#define PACKET_TYPE_JSON_ATTRIBUTE            61  // Пакет с атрибуиами в JSON формате {"ATTR1":"VAL1","ATTR2"="VAL2"} с CRC
#define PACKET_TYPE_JSON_ATTRIBUTE_REQUEST    62  // Пакет с запросом аттрибутов в формате JSON ARRAY {["ATTR1","ATTR2"]} с CRC
#define PACKET_TYPE_JSON_RPC                  63  // Пакет с RPC командой в JSON формате с CRC

#define PACKET_V3_MASK                        B10000000 // Маска для пакетов MyLoRa V3
#define PACKET_V3_COMPRESS_MASK               B01000000 // Маска для пакетов сжатых по алгоритму Unishox

#define PACKET_V3_TYPE_START                  B00000001 // Маска для стартового пакета/пинг-пакета без сообщения
#define PACKET_V3_TYPE_ACK                    B00000010 // Маска для пакета подтверждения сообщения
#define PACKET_V3_TYPE_ERROR                  B00000011 // Маска для пакета сообщающего об ошибке исходного пакета
#define PACKET_V3_TYPE_JSON_TELEMETRY         B00000100 // Маска для пакета с телеметрией в JSON формате {"PAR1":VAL1,"PAR2"=VAL2}
#define PACKET_V3_TYPE_JSON_ATTRIBUTE         B00000101 // Маска для пакета с атрибуиами в JSON формате {"ATTR1":"VAL1","ATTR2"="VAL2"}
#define PACKET_V3_TYPE_JSON_ATTRIBUTE_REQUEST B00000110 // Маска для пакета с запросом аттрибутов в формате JSON ARRAY {["ATTR1","ATTR2"]}
#define PACKET_V3_TYPE_JSON_RPC               B00000111 // Маска для пакета с RPC командой в JSON формате


#define MAX_LEN_PAYLOAD 256
#define SEND_ATT_COUNT  3
#define TIMEOUT_RX  1000
#define TIMEOUT_TX  500
#define DELAY_TX    500

#define MYLORA_DEBUG     1
//#define RGB_DEBUG

typedef enum {
   NSRX_NONE         = 0,  //Сообщение не принято
   NSRX_OK           = 1,  //Сообщение принято и прошло валидацию
   NSRX_DUBLE        = 2,  //Сообщение принято как дубликат
   NSRX_BROADCAST    = 100,  //Сообщение принято как бродкаст
   NSRX_ERROR        = -1, // Общая ошибка
   NSRX_ERROR_SIZE   = -2, // Ошибка длины пакета
   NSRX_ERROR_ALIEN  = -3, // Пакет ппредназначен другой ноде
   NSRX_ERROR_CRC    = -4, // Ошибка контрольной суммы
   NSRX_ERROR_TM     = -5, // Таймаут приема сообщения
} NetStateRX_t;
     
typedef enum {
   NSTX_NONE          = 0,  // Сообщение неотправлено
   NSTX_SEND          = 10, // Сообщение отправлено и ожидает подтверждения
   NSTX_OK            = 1,  // Подтверждение получено
   NSTX_ERROR         = -1, // Получено подтверждение с ошибкой
} NetStateTX_t;


typedef struct {
   uint8_t  Type;       //Тип пакета
   uint8_t  TTL;        //Время жизни пакета в сек. Задается как степень двойки 1,2,4,16,32,64,128,256,512,1024,2048,4096,8192... Значение 0 - устройство всегда на связи
   uint16_t NodeTX;     //Адрес узла отправителя
   uint16_t NodeRX;     //Адрес ноды получателя (0x0000 - шлюз, 0xFFFF - бродкаст)
   uint16_t Count;      //Счетчик пакета
}MyLoRaHeader_t;


typedef struct {
     uint8_t Type;      //Тип пакета (должна быть установлена маска PACKET_V3_MASK)
     uint8_t TTL;       //Время жизни пакета в сек. Задается как степень двойки 1,2,4,16,32,64,128,256,512,1024,2048,4096,8192... Значение 0 - устройство всегда на связи
     uint8_t AddrTX[6]; //LoRaMAC адрес отправителя
     uint8_t AddrRX[6]; //LoRaMAC адрес получателяю=. 00000000 для шлюза, FFFFFFFF - броадкаст
     uint16_t Count;    //Счетчик пакета (для отсеивания дубликатов)
     uint16_t CRC;      //Контрольная сумма
} MyLoRaHeaderV3_t;

class MyLoRaAddress {
public:
    static void Set(uint8_t *_addr, uint8_t _s0, uint8_t _s1, uint8_t _s2, uint8_t _s3, uint8_t _s4, uint8_t _s5);
    static void Set(uint8_t *_addr, uint64_t _id);
    static void Copy(uint8_t *_addr, uint8_t *_src);
    static bool Cmp(uint8_t *_addr1, uint8_t *_addr2);
    static char* Get(char *s, uint8_t *_addr);
    static bool isBroadcast(uint8_t *_addr);
    static void SetBroadcast(uint8_t *_addr);
};

class MyLoRaBaseClass {
  private:
     uint16_t saveCount, saveNode;
     uint8_t saveAddr[6];
     
  public:
     JsonDocument Json;     
//     uint16_t CountTX;

     uint8_t Addr[6], AddrRX[6];
     bool isGate;
     bool isTX_V3, isRX_V3;

     int16_t  Rssi;
     uint16_t Node;
     uint8_t  TTL;
//     uint16_t NodeRX;
     uint16_t Count;
//     uint8_t  Payload[MAX_LEN_PAYLOAD];
//     uint16_t Length;
     uint8_t  BufferTX[MAX_LEN_PAYLOAD];
     uint16_t LengthTX;
     uint8_t  BufferRX[MAX_LEN_PAYLOAD];
     uint16_t LengthRX;

     MyLoRaHeader_t HeaderTX, HeaderRX;
     MyLoRaHeaderV3_t HeaderTX_V3, HeaderRX_V3;

     NetStateTX_t StateTX;
     NetStateRX_t StateRX; 
         
     MyLoRaBaseClass( uint16_t _node , bool _is_gate = false);
     MyLoRaBaseClass( uint64_t _addr , bool _is_gate = false);

     void setTTL( uint32_t _time );
     uint32_t decodeTTL(uint8_t _ttl);
     void SetRequest( bool _isOK );
      bool RX(uint8_t *_payload, uint16_t _size, int16_t _rssi);
//     bool TX();
     void PrintRX();
     void PrintTX();
    
// Работа с JSON сообщениями (версия 2.0)
     void StartTX( uint8_t _type, uint16_t _nodeRX);
     uint16_t calc_CRC(uint8_t *_payload, uint16_t _len);
     void SetCRC();
     bool CheckCRC();
     bool SetJson();
     bool GetJson();
     bool isTypeSpec( uint8_t _type);
     bool isTypeCRC( uint8_t _type);
// Работа с сообщениями (версия 3.0)
     void SetHeaderTX(uint8_t _type, uint8_t *_addr_rx, bool _is_compress=true);
     bool SetJsonBodyTX();
     uint16_t SetCRC_V3(uint8_t *_buff, uint16_t _len);
     bool CheckCRC_V3(uint8_t *_buff, uint16_t _len);
     bool CheckDouble_V3();
      bool RX_V3(uint8_t *_payload, uint16_t _size, int16_t _rssi);
     

     void PrintRX_V3();
     void PrintTX_V3();
};


#endif
