#include "MyLoRaBase.h"

/********************************************************************************************
 * Виртуальный класс работы с LoRaMAC адресом
*********************************************************************************************/
/**
 * Установить LoRaMac адрес из 6 отдельных байт
 */
void MyLoRaAddress::Set(uint8_t *_addr,  uint8_t _s0, uint8_t _s1, uint8_t _s2, uint8_t _s3, uint8_t _s4, uint8_t _s5){
    _addr[0] = _s0;
    _addr[1] = _s1;
    _addr[2] = _s2;
    _addr[3] = _s3;
    _addr[4] = _s4;
    _addr[5] = _s5;
}

/**
 * Установить LoRaMac адрес из ооочень длинного целого числа
 */
void MyLoRaAddress::Set(uint8_t* _addr,  uint64_t _id){
    for(int i=5;i>=0;i--)_addr[i] = (_id>>(8*(5-i)))&0xFF;
}

/**
 * Установить LoRaMac адрес как Broadcast
 */
void MyLoRaAddress::SetBroadcast(uint8_t* _addr){
    _addr[0] = 0xff;
    _addr[1] = 0xff;
    _addr[2] = 0xff;
    _addr[3] = 0xff;
    _addr[4] = 0xff;
    _addr[5] = 0xff;
}


/**
 * Копировать LoRaMac адрес 
 */
void MyLoRaAddress::Copy( uint8_t *_addr, uint8_t *_src){
    _addr[0] = _src[0];
    _addr[1] = _src[1];
    _addr[2] = _src[2];
    _addr[3] = _src[3];
    _addr[4] = _src[4];
    _addr[5] = _src[5];
   }

/**
 * Сравнение двух LoRaMac адресов
*/
 bool MyLoRaAddress::Cmp( uint8_t *_addr1, uint8_t *_addr2){
    if( _addr1[0] == _addr2[0] &&
        _addr1[1] == _addr2[1] &&
        _addr1[2] == _addr2[2] &&
        _addr1[3] == _addr2[3] &&
        _addr1[4] == _addr2[4] &&
        _addr1[5] == _addr2[5] )return true;
    else return false;    
 }

/**
 * Проверка LoRa адреса на Broadcast
*/
 bool MyLoRaAddress::isBroadcast( uint8_t *_addr1){
    if( _addr1[0] == 0xff &&
        _addr1[1] == 0xff &&
        _addr1[2] == 0xff &&
        _addr1[3] == 0xff &&
        _addr1[4] == 0xff &&
        _addr1[5] == 0xff )return true;
    else return false;    
 }

/**
 * Преобразовать  LoRaMac адрес в строку
*/
char* MyLoRaAddress::Get(char *s, uint8_t *_addr){
   sprintf(s,"%02X%02X%02X%0X%02X%02X",_addr[0],_addr[1],_addr[2],_addr[3],_addr[4],_addr[5]);
   return s;
}


/**
 * @brief Construct a new My Lo Ra Base Class:: My Lo Ra Base Class object
 * 
 * @param _node - собственный адрес ноды
 */
MyLoRaBaseClass::MyLoRaBaseClass(uint16_t _node, bool _is_gate ){
   Node      = _node;
   Count     = 0;
 //  CountTX   = SEND_ATT_COUNT;
   saveCount = 0xffff;
   saveNode  = 0xffff;
   MyLoRaAddress::Set(saveAddr,0xff,0xff,0xff,0xff,0xff,0xff);
   TTL       = 0;
 }

/**
 * @brief Конструктор для протокола V3
 * 
 * @param _node - собственный адрес ноды
 */
MyLoRaBaseClass::MyLoRaBaseClass(uint64_t _addr, bool _is_gate ){
   MyLoRaAddress::Set(Addr,_addr);
   char s[20];
//   Serial.print(F("<<<>>> Addr: "));
//   Serial.println(MyLoRaAddress::Get(s,Addr));
   Count     = 0;
   saveCount = 0xffff;
   saveNode  = 0xffff;
   MyLoRaAddress::Set(saveAddr,0xff,0xff,0xff,0xff,0xff,0xff);
   TTL       = 0;
   if( _is_gate )Node = 0;
   else Node = 0xffff;
   isGate    = _is_gate;
 }


/**
 * Формирование заголовка пакета
 * @param _type - тип передваемого пакета
 * @param _nodeRX - адрес ноды получателя
 * @param _ttl - степень 2 время жизни пакета
 */
void MyLoRaBaseClass::StartTX( uint8_t _type, uint16_t _nodeRX){
   isTX_V3 = false;
//   CountTX         = _countTX;
   HeaderTX.Type   = _type;
   HeaderTX.TTL    = TTL;
   HeaderTX.Count  = Count++;
   HeaderTX.NodeTX = Node;
   HeaderTX.NodeRX = _nodeRX;
//   Length          = 0;
   memcpy(BufferTX, &HeaderTX, sizeof(MyLoRaHeader_t) );
   LengthTX = sizeof(MyLoRaHeader_t);
   Json.clear();
//   Serial.print("HeaderSize=");
//   Serial.println(sizeof(MyLoRaHeader_t));
 /*  
#ifdef MYLORA_DEBUG
   Serial.print("Header: NodeTX=");
   Serial.print(HeaderTX.NodeTX);
   Serial.print(" NodeRX=");
   Serial.println(HeaderTX.NodeRX);
#endif
*/
}

void MyLoRaBaseClass::setTTL( uint32_t _time ){
   uint32_t _x = 2;
   TTL = 0;
   if( _time == 0  )return;
   TTL = 1;
   if( _time <= 2  )return;
   for( TTL=2; TTL<255; TTL++){
       _x *= 2;
/*       
       Serial.print("TTL: ");
       Serial.print(_time);
       Serial.print(" ");
       Serial.print(_x);
       Serial.print(" ");
       Serial.println(TTL);
*/       
       if( _x > _time )break;
   } 
}

uint32_t MyLoRaBaseClass::decodeTTL( uint8_t _ttl ){
   if( _ttl == 0 )return 0;
   return (uint32_t)pow(2,_ttl);
}




/**
 * Вычисление контрольной суммы
 */
uint16_t MyLoRaBaseClass::calc_CRC(uint8_t *_payload, uint16_t _len){
   uint16_t crc       = 0;
   for( int i=0; i<_len; i++ )crc += _payload[i];
   return crc;
}


/**
 * Вычисление контрольной суммы в исходящем буфере
 */
void MyLoRaBaseClass::SetCRC(){
   if( isTypeCRC(HeaderTX.Type) ){
      uint16_t _crc = calc_CRC(BufferTX,LengthTX);
      BufferTX[LengthTX++] = _crc%256;   
      BufferTX[LengthTX++] = _crc/256;  
//      Serial.printf(">>> Type=%d Len=%d crc=%u 0x%02x 0x%02x\n",(int)HeaderTX.Type,(int)LengthTX,_crc,(int)BufferTX[LengthTX-2],(int)BufferTX[LengthTX-1]);
   }
}

/**
 * Проверка контрольной сммы во входящем буфере
 */
bool MyLoRaBaseClass::CheckCRC(){
   if( isTypeCRC(HeaderRX.Type) ){
// Проверяем длину пакета
      if( LengthRX < sizeof(HeaderRX) + 2 )return true;      
// Вычисление контрольной суммы
      uint16_t _crc  = calc_CRC(BufferRX,LengthRX-2);
      uint16_t _crc0 = 0;
      _crc0 = BufferRX[LengthRX-2] + 256*BufferRX[LengthRX-1];
//      Serial.printf("<<< Type=%d Len=%d crc=%d %d 0x%02x 0x%02x\n",(int)HeaderRX.Type,(int)LengthRX,_crc,_crc0,(int)BufferRX[LengthRX-2],(int)BufferRX[LengthRX-1]);
      LengthRX -= 2;
      if( _crc != _crc0 ){
          Serial.printf("<<< Bad CRC\n");
           return false;
      }
   }
   return true;
}

/**
 * Упаковать JSON в буфер для отправки
 * 
 */
bool MyLoRaBaseClass::SetJson(){
// Упаковка JSON в буфер   
   String s;
   serializeJson(Json, s);
   int l = s.length();
   if( l <=0 || l > MAX_LEN_PAYLOAD - sizeof(MyLoRaHeader_t) -2 )return false;
   Serial.print(F(">>> JSON: "));
   Serial.println(s);
   memcpy(BufferTX+LengthTX, (uint8_t *)s.c_str(), l );
   LengthTX += l;
   SetCRC();
   return true;
}

/**
 * Распаковать JSON из приемного буфера
 * 
 */
bool MyLoRaBaseClass::GetJson(){
   BufferRX[LengthRX] = 0;
   uint16_t _off = 0;
   bool _is_compress = false;
   if( isRX_V3 ){
      _off = sizeof(MyLoRaHeaderV3_t);
      if( HeaderRX_V3.Type & PACKET_V3_COMPRESS_MASK )_is_compress = true;
   }
   else {
      _off = sizeof(MyLoRaHeader_t);
//      Serial.print("JSON v2 read off=");
//      Serial.println(_off);
   }
   char *p;   
//   char _buff[MAX_LEN_PAYLOAD];
   if( _is_compress ){
      int l1 = LengthRX-_off;
      char *p1 = (char *)BufferRX+_off;
      int l2 = unishox2_decompress_simple(p1, l1, (char *)BufferTX);
      BufferTX[l2] = '\0';
      p = (char *)BufferTX;
      Serial.print(F("<<< COMPRESS JSON: "));
      Serial.print(p);
      Serial.print(F(" RATIO "));
      Serial.print((float)l1/(float)l2*100,1);
      Serial.print(F(" "));
      Serial.print(l1);
      Serial.print(F("/"));
      Serial.println(l2);
   }
   else {
      p  = (char *)BufferRX+_off;
      Serial.print(F("<<< JSON: "));
      Serial.println(p);
   }
//   for( int i=0; i<strlen(p); i++){
//      Serial.print(p[i],HEX);
//      Serial.print(" ");
//   }
//   Serial.println();

   Json.clear();
   DeserializationError _err = deserializeJson(Json, p);  
  
   if( _err == DeserializationError::Ok ){
//         Serial.print("<<< JSON ERROR: ");
//         Serial.println(p);
      return true;
   }
   else {
//         Serial.print("<<< JSON eRROR: ");
//         Serial.println(p);
      return false;
         
   }
  
   return false;
}

/**
 * Обработка получаемого пакета
 * @param _payload входной буфер
 * @param _size пазмер входного буфера
 * @param _rssi уровень входящего сигнала
 * @return true - успешный прием
 * @return false - ошибка приема
 */
bool MyLoRaBaseClass::RX(uint8_t *_payload, uint16_t _size, int16_t _rssi){
   if( _size == 0 ){
      StateRX = NSRX_ERROR_SIZE;
      return false;        
   }

// Проверяем версию протокола
   if( (_payload[0] & PACKET_V3_MASK) )return(RX_V3(_payload,_size,_rssi));

   Rssi     = _rssi;
   LengthRX = _size;
   isRX_V3     = false;
   uint16_t _off = sizeof(MyLoRaHeader_t);

// Проверка на длину возвращаемого пакета
   if( _size < sizeof(MyLoRaHeader_t) || _size >= MAX_LEN_PAYLOAD ){
      StateRX = NSRX_ERROR_SIZE;
      return false;        
   }
// Сохраняем заголовок пакета   
    memcpy((uint8_t *)&HeaderRX,_payload,sizeof(MyLoRaHeader_t));
// Проверка на номер ноды возвращаемого пакета
   if( Node != HeaderRX.NodeRX ){
      StateRX = NSRX_ERROR_ALIEN;
      return false;        
   }
// Проверка на дубликат пакета    
   if( saveCount == HeaderRX.Count && saveNode == HeaderRX.NodeTX ){
      StateRX = NSRX_DUBLE;
      return true; //Пакет нормальный, но обрабатывать его не нужно
   }
   saveCount = HeaderRX.Count;
   saveNode  = HeaderRX.NodeTX;  
// Копирование входного буфера   

   memcpy(BufferRX,_payload,_size);
 // Проверка на контрольную сумму
   if( !CheckCRC() ){
      StateRX = NSRX_ERROR_CRC;
      return false;
   }
   
   LengthRX = _size-2 ;
   memcpy(BufferRX, _payload, LengthRX);

   StateRX = NSRX_OK;  
   return true;
}

/**
 * Печать отладки выходного сообщения
 * 
 */
void MyLoRaBaseClass::PrintTX(){
   if( isTX_V3 ){
      PrintTX_V3();
      return;
   }
   Serial.print(F(">>> Type="));
   Serial.print(HeaderTX.Type);
   Serial.print(F(" NodeTX="));
   Serial.print(HeaderTX.NodeTX);
   Serial.print(F(" NodeRX="));
   Serial.print(HeaderTX.NodeRX);
   Serial.print(" TTL=");
   Serial.print(HeaderTX.TTL);
   Serial.print(F(" Count="));
   Serial.print(HeaderTX.Count);
   Serial.print(F(" Size="));
   Serial.print(LengthTX);
   if( StateTX == NSTX_NONE )Serial.println(F(" TM"));
   else Serial.println(F(" OK"));
}

/**
 * Печать отладки входного сообщения
 * 
 */
void MyLoRaBaseClass::PrintRX(){
   if( isRX_V3 ){
      PrintRX_V3();
      return;
   }
   Serial.print(F("<<< "));
   switch( StateRX ){
      case NSRX_NONE :
         Serial.println(F("ERROR RX"));
         break;
      case NSRX_OK :
         Serial.print("Type=");
         Serial.print(HeaderRX.Type);
         Serial.print(" NodeTX=");
         Serial.print(HeaderRX.NodeTX);
         Serial.print(" NodeRX=");
         Serial.print(HeaderRX.NodeRX);
         Serial.print(" TTL=");
         Serial.print(HeaderRX.TTL);
         Serial.print(F(" Size="));
         Serial.print(LengthRX);
         Serial.print(F(" Rssi="));
         Serial.print(Rssi);
         Serial.print(F(" Count="));
         Serial.println(HeaderRX.Count);
         break;
      case NSRX_DUBLE :
         Serial.print(F("Dublicate Node="));
         Serial.print(HeaderRX.NodeTX);
         Serial.print(F(" Count="));
         Serial.println(HeaderRX.Count);
         break;
      case NSRX_ERROR_SIZE :
         Serial.print(F("ERROR Size="));
         Serial.println(LengthRX);
         break;
      case NSRX_ERROR_ALIEN :
         Serial.print(F("ERROR Other Node="));
         Serial.println(HeaderRX.NodeRX);
         break;
      case NSRX_ERROR_CRC :
         Serial.println(F("ERROR CRC"));
         break;
      case NSRX_ERROR_TM :
         Serial.println(F("TIMEOUT"));
         break;
      case NSRX_ERROR :
         Serial.println(F("ERROR"));
         break;
      default:
         Serial.print(F("ERROR UNKNOWN "));
        Serial.println(StateRX);

  }
}

/**
 * Проверка на специальный тип сообщения 
 * 
 * @param _type - проверяемы тип
 * @return true - специальный тип (не нужно подтверждение)
 * @return false-  информационный тип (нужно подтверждение)
 */
bool MyLoRaBaseClass::isTypeSpec( uint8_t _type){
   if( _type <= 40 )return true;
   return false;   
}

/**
 * Проверка нужно или не обрабатывать CRC
 * 
 * @param _type - проверяемый тип
 * @return true - CRC обрабатывать нужно
 * @return false - CRC обрабатывать не нужно
 */
bool MyLoRaBaseClass::isTypeCRC( uint8_t _type){
   if( _type == PACKET_TYPE_JSON_TELEMETRY || 
       _type == PACKET_TYPE_JSON_ATTRIBUTE || 
       _type == PACKET_TYPE_JSON_ATTRIBUTE_REQUEST || 
       _type == PACKET_TYPE_JSON_RPC )return true;
   return false;
}

/********************************************************************************************
 * Методы работы с протоколом V3
*********************************************************************************************/
/**
 * Формирование заголовка пакета
 * @param _type        - тип передваемого пакета
 * @param _addr_rx     - LoRaMAC адрес получателя
 * @param _is_compress - Флаг сжатия пакета
 */
void MyLoRaBaseClass::SetHeaderTX(uint8_t _type, uint8_t *_addr_rx, bool _is_compress){
   isTX_V3 = true;
// Формируем тип пакета   
   if( _is_compress )HeaderTX_V3.Type = PACKET_V3_MASK | PACKET_V3_COMPRESS_MASK | _type;
   else HeaderTX_V3.Type = PACKET_V3_MASK | _type;
   HeaderTX_V3.TTL    = TTL;
   HeaderTX_V3.Count  = Count++;
   MyLoRaAddress::Copy(HeaderTX_V3.AddrTX,Addr);
   MyLoRaAddress::Copy(HeaderTX_V3.AddrRX,_addr_rx);
   HeaderTX_V3.CRC    = 0;
   memcpy(BufferTX, &HeaderTX_V3, sizeof(MyLoRaHeaderV3_t) );
   LengthTX = sizeof(MyLoRaHeaderV3_t);
   Json.clear();
}

/**
 * Формирование тела пакета
 */
bool MyLoRaBaseClass::SetJsonBodyTX(){
// Упаковка JSON в буфер   
   String s;
   serializeJson(Json, s);
   int l1 = s.length();
   if( l1 >= MAX_LEN_PAYLOAD )return false;
   if( HeaderTX_V3.Type & PACKET_V3_COMPRESS_MASK ){
      char c_buf[MAX_LEN_PAYLOAD];
      int l2 = unishox2_compress_simple(s.c_str(),l1,c_buf);
       if( l2 <=0 || l2 > MAX_LEN_PAYLOAD - sizeof(MyLoRaHeaderV3_t) -2 )return false;
      memcpy(BufferTX+LengthTX, (uint8_t *)c_buf, l2 );
      LengthTX += l2;
      Serial.print(F(">>> COMPRESS JSON: "));
      Serial.print(s);
      Serial.print(F(" RATIO "));
      Serial.print((float)l2/(float)l1*100,1);
      Serial.print(F(" "));
      Serial.print(l2);
      Serial.print(F("/"));
      Serial.println(l1);
   }
   else {
      if( l1 <=0 || l1 > MAX_LEN_PAYLOAD - sizeof(MyLoRaHeaderV3_t) -2 )return false;
      memcpy(BufferTX+LengthTX, (uint8_t *)s.c_str(), l1 );
      LengthTX += l1;
      Serial.print(F(">>> JSON: "));
      Serial.println(s);
   }
   HeaderTX_V3.CRC = SetCRC_V3(BufferTX, LengthTX);
//   PrintTX_V3();
   return true;
}

/**
 * Формирование контрольной суммы
*/
uint16_t MyLoRaBaseClass::SetCRC_V3(uint8_t *_buff, uint16_t _len){
   uint16_t _crc   = 0;
   uint16_t _off   = sizeof(MyLoRaHeaderV3_t) - sizeof(_crc);
   for( int i=0; i<_len; i++)_crc += (uint16_t)_buff[i];
//   HeaderTX_V3.CRC = _crc;
//   _crc++;
   memcpy(_buff+_off, &_crc, sizeof(_crc)  );
//   Serial.print(F(">>> SET CRC: "));
//   Serial.println(_crc);
   return _crc;
}

/**
 * Проверка контрольной суммы
*/
bool MyLoRaBaseClass::CheckCRC_V3(uint8_t *_buff, uint16_t _len){
   uint16_t _crc   = 0;
   uint16_t _off   = sizeof(MyLoRaHeaderV3_t) - sizeof(_crc);
   uint16_t _crc_save = HeaderRX_V3.CRC;
   memcpy(_buff+_off, &_crc, sizeof(_crc)  );
   for( int i=0; i<_len; i++)_crc += (uint16_t )_buff[i];
   memcpy(_buff+_off, &_crc_save, sizeof(_crc)  );

//   Serial.print(F("!!! CRC "));
//   Serial.print(_crc);
 //  Serial.print(" ");
//   Serial.println(_crc_save);

   if( _crc == _crc_save )return true;
   else return false;
}

/**
 * Проверка на повтор пакета
*/
bool MyLoRaBaseClass::CheckDouble_V3(){
   bool _ret = false;
   if( saveCount == HeaderRX_V3.Count && MyLoRaAddress::Cmp(HeaderRX_V3.AddrTX,saveAddr) )_ret = true;
/*
   char s[16];
   Serial.print(F("!!! DUB "));
   Serial.print(saveCount);
   Serial.print(F(" "));
   Serial.print(HeaderRX_V3.Count);
   Serial.print(F(" "));
   Serial.print(MyLoRaAddress::Get(s,saveAddr));
   Serial.print(F(" "));
   Serial.print(MyLoRaAddress::Get(s,HeaderRX_V3.AddrTX));
   Serial.print(F(" "));
   Serial.println((int)_ret);
   */
   saveCount = HeaderRX_V3.Count;
   MyLoRaAddress::Copy(saveAddr,HeaderRX_V3.AddrTX);
   return _ret;
 }

/**
 * Обработка получаемого пакета
 * @param _payload входной буфер
 * @param _size пазмер входного буфера
 * @param _rssi уровень входящего сигнала
 * @return true - успешный прием
 * @return false - ошибка приема
 */
bool MyLoRaBaseClass::RX_V3(uint8_t *_payload, uint16_t _size, int16_t _rssi){
   isRX_V3 = true;

   uint16_t _off = sizeof(MyLoRaHeaderV3_t);

   Rssi     = _rssi;
   LengthRX = _size;
// Проверка на длину возвращаемого пакета
   if( _size < _off || _size >= MAX_LEN_PAYLOAD ){
      StateRX = NSRX_ERROR_SIZE;
      return false;        
   }

// Сохраняем заголовок пакета   
    memcpy((uint8_t *)&HeaderRX_V3,_payload,_off);

// Проверка на контрольную сумму
   if( !CheckCRC_V3( _payload, _size ) ){
      StateRX = NSRX_ERROR_CRC;
      return false;
   }    

   StateRX = NSRX_OK;  
   // Проверка на номер ноды возвращаемого пакета
   if( MyLoRaAddress::isBroadcast(HeaderRX_V3.AddrRX)){
      StateRX = NSRX_BROADCAST;    
   }
   else if( !MyLoRaAddress::Cmp(Addr,HeaderRX_V3.AddrRX) ){
      StateRX = NSRX_ERROR_ALIEN;
//      Serial.print("!!! Cmp RX: ");
//      char s[16];
//      Serial.print(MyLoRaAddress::Get(s,HeaderRX_V3.AddrRX));
//      Serial.print(" MY: ");
//      Serial.println(MyLoRaAddress::Get(s,Addr));
      return false;
   }

// Проверка на дубликат пакета    
   if( CheckDouble_V3() ){
      StateRX = NSRX_DUBLE;
//      Serial.println("!!! DUP");
//      return true; //Пакет нормальный, но обрабатывать его не нужно
   }

// Копирование входного буфера   
   LengthRX = _size;
   memcpy(BufferRX, _payload, LengthRX);
   
   return true;
}

/**
 * Отправить ответ в зависимости от версии протокола
*/
void MyLoRaBaseClass::SetRequest( bool _isOK ){
   if( isRX_V3 ){
      if( _isOK )SetHeaderTX(PACKET_V3_TYPE_ACK ,HeaderRX_V3.AddrTX,false);
      else SetHeaderTX(PACKET_V3_TYPE_ERROR ,HeaderRX_V3.AddrTX,false);
      HeaderTX_V3.CRC = SetCRC_V3(BufferTX,LengthTX);

   }
   else {
      if( _isOK )StartTX(PACKET_TYPE_ACK ,HeaderRX.NodeTX);
      else StartTX(PACKET_TYPE_CRC_ERROR ,HeaderRX.NodeTX);
   }
}

/**
 * Печать отладки выходного сообщения
 * 
 */
void MyLoRaBaseClass::PrintTX_V3(){
   char s[16];
   Serial.print(F(">>> Type="));
   Serial.print(HeaderTX_V3.Type,BIN);
   Serial.print(F(" TX="));
   Serial.print(MyLoRaAddress::Get(s,HeaderTX_V3.AddrTX));
   Serial.print(F(" RX="));
   Serial.print(MyLoRaAddress::Get(s,HeaderTX_V3.AddrRX));
   Serial.print(F(" TTL="));
   Serial.print(HeaderTX_V3.TTL);
   Serial.print(F(" Count="));
   Serial.print(HeaderTX_V3.Count);
   Serial.print(F(" CRC="));
   Serial.print(HeaderTX_V3.CRC);
   Serial.print(F(" Size="));
   Serial.print(LengthTX);
   if( StateTX == NSTX_NONE )Serial.println(F(" TM"));
   else Serial.println(F(" OK"));
}

/**
 * Печать отладки входного сообщения
 * 
 */
void MyLoRaBaseClass::PrintRX_V3(){
   char s[16];
   Serial.print(F("<<< "));
         Serial.print("Type=");
         Serial.print(HeaderRX_V3.Type,BIN);
         Serial.print(F(" TX="));
         Serial.print(MyLoRaAddress::Get(s,HeaderRX_V3.AddrTX));
         Serial.print(F(" RX="));
         Serial.print(MyLoRaAddress::Get(s,HeaderRX_V3.AddrRX));
         Serial.print(F(" CRC="));
         Serial.print(HeaderRX_V3.CRC);
   switch( StateRX ){
      case NSRX_NONE :
         Serial.println(F("ERROR RX"));
         break;
      case NSRX_OK :
      case NSRX_BROADCAST :
         Serial.print(" TTL=");
         Serial.print(HeaderRX_V3.TTL);
         Serial.print(F(" Size="));
         Serial.print(LengthRX);
         Serial.print(F(" Rssi="));
         Serial.print(Rssi);
         Serial.print(F(" Count="));
         Serial.println(HeaderRX_V3.Count);
         break;
      case NSRX_DUBLE :
         Serial.print(F(" Dublicate Addr="));
         Serial.print(MyLoRaAddress::Get(s,HeaderRX_V3.AddrTX));
         Serial.print(F(" Count="));
         Serial.println(HeaderRX_V3.Count);
         break;
      case NSRX_ERROR_SIZE :
         Serial.print(F(" ERROR Size="));
         Serial.println(LengthRX);
         break;
      case NSRX_ERROR_ALIEN :
         Serial.print(F(" ERROR Other Addr="));
         Serial.println(MyLoRaAddress::Get(s,HeaderRX_V3.AddrRX));
         break;
      case NSRX_ERROR_CRC :
         Serial.println(F(" ERROR CRC"));
         break;
      case NSRX_ERROR_TM :
         Serial.println(F(" TIMEOUT"));
         break;
      case NSRX_ERROR :
         Serial.println(F(" ERROR"));
         break;
      default:
         Serial.print(F(" ERROR UNKNOWN "));
         Serial.println(StateRX);

  }
}
