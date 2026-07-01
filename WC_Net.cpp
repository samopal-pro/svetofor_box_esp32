#include "WC_Net.h"


//HTTPClient httpClient;
SimpleHttpClient client;

bool isWiFiAlways1 = true; 
uint32_t msSendHttp = 0, msSendTB = 0, msSendLora = 0,msSendLoraAttr = 0, msSendCrm = 0;
bool isSendAttributeTB = false;
bool isLora            = false;
bool isLoraACK         = false;
bool isAP = false, isSTA = false;
uint32_t msSTA = 0;

JsonDocument jsonData;
volatile bool loraIrq = false;
//TaskHandle_t loraTaskHandle = NULL;
#ifdef IS_LORA
SPIClass LoraSPI(HSPI);
SX1262    radio = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, LoraSPI);
MyLoRaBaseClass myLora(chipID);
uint8_t loraGate[6];
#endif
/**
 * Задача работы с MP3
 * @param pvParameters
 */


void IRAM_ATTR onLoraIrq() {
   loraIrq = true;
   detachInterrupt(PIN_LORA_DIO1);
}

void initLora(){
#ifdef IS_LORA 
   pinMode(PIN_LORA_DIO1, INPUT);

   Serial.print(F("!!! LoRa init "));   
   LoraSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI);
   int state = radio.begin();
   if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
      isLora = true;
      radio.setFrequency(868.0);
      radio.setSpreadingFactor(7);
      radio.setBandwidth(125.0);
      radio.setCodingRate(5);
//      radio.invertIQ(true);
//      radio.
      MyLoRaAddress::Set(myLora.Addr,chipID);
//      MyLoRaAddress::Set(myLora.AddrRX,0,0,0,0,0,0);
      MyLoRaAddress::SetBroadcast(loraGate);
      setLoraReceive(true);

    }
   else {
      isLora = false;
      Serial.print(F("failed, code "));
      Serial.println(state);
   }

#endif
}

void readLora(){
#ifdef IS_LORA   
   if( !isLora )return;
   String s;
   uint8_t _buf[MAX_LEN_PAYLOAD];
   int n = radio.getPacketLength();
   if( n > MAX_LEN_PAYLOAD )n= MAX_LEN_PAYLOAD;
   int state = radio.readData(_buf,n);
   if (state == RADIOLIB_ERR_NONE && n>0 ) {
      myLora.RX(_buf, n, radio.getRSSI());

//      myLora.PrintRX_V3();
      if( myLora.StateRX == NSRX_OK && ((myLora.HeaderRX_V3.Type&B000111) ==  PACKET_V3_TYPE_ACK ) ){
         myLora.PrintRX_V3();
         Serial.println(F("!!! LORA RX ACK "));
         isLoraACK = true; 
      }           
      if( (myLora.StateRX == NSRX_OK || myLora.StateRX == NSRX_BROADCAST) && ((myLora.HeaderRX_V3.Type&B000111) ==  PACKET_V3_TYPE_JSON_RPC )){
         myLora.PrintRX_V3();
         myLora.GetJson();
         serializeJson(myLora.Json,s);
//            Serial.print(myLora.StateRX);
//            Serial.print(" ");
            
         Serial.println(s);
         if( myLora.Json["RPC"] == "test" ){
            Serial.println(F("!!! LORA RX RPC=test "));
            EventRGB1->setRainbow(true,1000);
            EventRGB2->setRainbow(true,1000);
            if( myLora.StateRX == NSRX_OK ){
               msSendLora     = 0;
               msSendLoraAttr = 0;
               systemMP3("100", 100, PRIORITY_MP3_MINIMAL );
            }
            else {
               msSendLora     = millis()+random(0,30)*1000;
               msSendLoraAttr = msSendLora;
            }
            lastSensorOn = SS_RESTORE;                
         }
         else if( myLora.Json["RPC"] == "reboot" ){
            systemMP3("89",86,PRIORITY_MP3_MAXIMAL);
            waitMP3andReboot();
         }
         else if( myLora.Json["RPC"] == "calibrate" ){
            systemMP3("89",85,PRIORITY_MP3_HIGH);
            startCalibrate(0,"97",97);
         }
      }
//      else {
//         myLora.GetJson();
//      }
   }  
   setLoraReceive(true);
#endif
}

bool sendLora(){
#ifdef IS_LORA

    int _state = getStatus();

    myLora.SetHeaderTX(PACKET_V3_TYPE_JSON_TELEMETRY, loraGate, true);
    myLora.Json["Distance"] = (int)Distance;
    myLora.Json["State"]    = _state;
    myLora.Json["Uptime"]   = esp_timer_get_time()/1000000;
    myLora.Json["DN"]    = config["config2"]["N_DOGIVOR"].as<String>();
    myLora.Json["BN"]    = config["config2"]["N_BOX"].as<String>();

    if( serNo[0] != '\0' )myLora.Json["SN"] = serNo;
    if (myLora.SetJsonBodyTX()) {
       setLoraReceive(false);
       int state = radio.transmit(myLora.BufferTX,myLora.LengthTX);
       isLoraACK = false;
       setLoraReceive(true);
       if (state == RADIOLIB_ERR_NONE) {
          Serial.println("!!! LORA TX OK");
          myLora.PrintTX_V3();
       }
       else {
          Serial.print("??? LORA TX error: ");
          Serial.println(state);
       }         
   }
   setLoraReceive(true);
#endif
   return true;
}

bool sendLoraAttr(){
#ifdef IS_LORA
    myLora.SetHeaderTX(PACKET_V3_TYPE_JSON_ATTRIBUTE, myLora.AddrRX, true);
    myLora.Json["SerialNo"]    = serNo;
    myLora.Json["DogovorNo"]   = config["config2"]["N_DOGIVOR"].as<String>();
    myLora.Json["BoxNo"]       = config["config2"]["N_BOX"].as<String>();
    if (myLora.SetJsonBodyTX()) {
       setLoraReceive(false);
       int state = radio.transmit(myLora.BufferTX,myLora.LengthTX);
       isLoraACK = false;
       setLoraReceive(true);
       if (state == RADIOLIB_ERR_NONE) {
          Serial.println("!!! LORA TX OK");
          myLora.PrintTX_V3();
       }
       else {
          Serial.print("??? LORA TX error: ");
          Serial.println(state);
       }         
   }
   setLoraReceive(true);
#endif
   return true;
}



bool waitLoraRead(uint32_t _tm){
   for( uint32_t i=0; i<_tm; i+=10 ){
      if( loraIrq ){
         loraIrq = false;
         Serial.printf("!!! LORA wait %d\n",i);
         readLora();
         return true;
      }
      vTaskDelay(10);
   }
   return false;
}

void setLoraReceive(bool _flag){
 //   loraIrq = false;
 #ifdef IS_LORA
    if( _flag ){
       attachInterrupt(PIN_LORA_DIO1, onLoraIrq, RISING);
   radio.startReceive();          
    }
    else {
       radio.standby();
       detachInterrupt(PIN_LORA_DIO1);
     }
#endif     
}


/**
 * Задача передача данных через WiFi
 * @param pvParameters
 */
void taskNet( void *pvParameters ){
#if defined(DEBUG_SERIAL)
    Serial.println(F("!!! WiFi task start"));
#endif
   WiFi.mode(WIFI_OFF);
   uint32_t ms1 = 0, ms2 = 0, ms3 = 0;
   EventRGB1->setColor0(COLOR_BLACK);
   EventRGB1->setColor1(COLOR_BLACK);
   Network.onEvent(handleEventWiFi);
   isAP = true;
   initLora();
   bool isSendT = false;
   bool isSendA = false;
   bool isSendCurT = false;
   int wifiError = 0;
   if( !config["config2"]["WIFI_POWER"].isNull() )WiFi.setTxPower((wifi_power_t)config["config2"]["WIFI_POWER"].as<int>());
//   HTTP_begin();
   while(true){
      if(isSensorBlock || calibrMode == CM_WAIT_REBOOT ){ //Сенсор заблокирован до выключения питания
         vTaskDelay(1000);    
         continue;
      }

      uint32_t ms = millis();
      if( ms2 == 0 || ms < ms2 || (ms-ms2)>3000){
          ms2 = ms;
          wifi_mode_t curWiFi = WiFi.getMode(); 
          if( config["config2"]["WIFI_MODE"].as<int>() == STA_OFF )isSTA = false;
          else isSTA = true;  
//          if( !config["config2"]["WIFI"]["NAME"].isNull() )isSTA = true;
//          else isSTA = false;
// Стартуем точку доступа          
          if( isAP && ( curWiFi != WIFI_AP && curWiFi != WIFI_AP_STA) ){
             WiFi_ScanNetwork();
             WiFi.enableAP(true);
             String _ap = config["config2"]["ESP_NAME"].as<String>();
             if( _ap == "" )_ap = deviceName();
             WiFi.softAP(_ap);
             HTTPD_start();
//             msAP = millis();                
          }
// Гасим точку доступа
          if( !isAP && ( curWiFi == WIFI_AP || curWiFi == WIFI_AP_STA) ){
//             Serial.println(F("!!! Disable AP"));
             WiFi.enableAP(false);
          }
          if( ms1 == 0 || ms < ms1 || (ms-ms1)>1000){
           ms1= ms;


          if( isSTA && ( curWiFi != WIFI_STA && curWiFi != WIFI_AP_STA) ){
             Serial.println(F("!!! Start STA"));
              msSTA = ms;
             WiFi_ScanNetwork();
             WiFi.enableSTA(true);
             if( config["config2"]["STATIC_IP"].as<bool>() == true ){
                IPAddress ip_addr,ip_mask,ip_gate,ip_dns;
                if( ip_addr.fromString( config["config2"]["IP_ADDR"].as<String>() ) &&
                    ip_mask.fromString( config["config2"]["IP_MASK"].as<String>() ) &&
                    ip_gate.fromString( config["config2"]["IP_GATE"].as<String>() ) &&
                    ip_dns.fromString(  config["config2"]["IP_DNS"].as<String>()     ) ){
                   Serial.println("Config static IP address");
                   WiFi.config(ip_addr,ip_gate,ip_mask,ip_dns);
                }                 
             }
             if( config["config2"]["WIFI_MODE"].as<int>() == STA_ON ){
                WiFi.begin(config["config2"]["WIFI_NAME1"].as<String>(), config["config2"]["WIFI_PASS1"].as<String>());
             }
             else if( config["config2"]["WIFI_MODE"].as<int>() == STA_AUTO ){
                WiFi.begin(config["config2"]["WIFI_NAME2"].as<String>(), config["config2"]["WIFI_PASS2"].as<String>());
                if( wifiError>3 ){
                   systemMP3("60",62,PRIORITY_MP3_MEDIUM);
                   config["config2"]["WIFI_MODE"] = STA_OFF;
                   configWrite();               
                }
                else {
                  systemMP3("60",61,PRIORITY_MP3_MEDIUM);
                   wifiError++;
                }
             }
//             ledSTA(true);
             EventRGB1->setColor1(COLOR_WIFI_WAIT);
          }      
          if( isSTA && ( curWiFi == WIFI_STA || curWiFi == WIFI_AP_STA) ){
             if( WiFi.status() != WL_CONNECTED && (ms - msSTA)>10000 ){
                 Serial.println(F("!!! Error WiFi"));
                 EventRGB1->setColor1(COLOR_WIFI_OFF);
                 WiFi.enableSTA(false);
             }
          }
          if( !isSTA && ( curWiFi == WIFI_STA || curWiFi == WIFI_AP_STA) ){
             WiFi.enableSTA(false);
//             ledSTA( false );
             EventRGB1->setColor1(COLOR_BLACK);
          }
          }
// Стартуем подключение

/*
          wifi_mode_t modeWiFi =  WIFI_OFF;
          if( isAP && isSTA )modeWiFi = WIFI_AP_STA;
          else if( isAP )modeWiFi     = WIFI_AP;
          else if( isSTA )modeWiFi    = WIFI_STA; 
*/

      }
      if( (ms3 == 0 || ms < ms3 || (ms-ms3)>2000) ){
          ms3 = ms;
          if(WiFi.status() == WL_CONNECTED){
             if(  isSendNet ){
                isSendNet  = false;
                msSendHttp = 0;
                msSendCrm  = 0;
                msSendTB   = 0;
                msSendLora = 0;
             } 
             if( config["config2"]["CRM_ENABLE"].as<bool>() && ( msSendCrm == 0 || msSendCrm < ms ) ){
                if( sendCrmMoscowParam() )msSendCrm = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>()*1000;
                else msSendCrm = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>()*1000;
             }
             if( config["config2"]["HTTP_ENABLE"].as<bool>() && ( msSendHttp == 0 || msSendHttp < ms ) ){
                if( sendHttpParam() )msSendHttp = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>()*1000;
                else msSendHttp = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>()*1000;
             }
             if( config["config2"]["TB_ENABLE"].as<bool>() && ( msSendTB == 0 || msSendTB < ms ) ){
                if( sendParamTB() )msSendTB = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>()*1000;
                else msSendTB = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>()*1000;
             }
          }
          if( isLora && config["config2"]["LORA_ENABLE"].as<bool>() ){
             isSendT = ( msSendLora == 0 || msSendLora < ms );
//             isSendA = ( msSendLoraAttr == 0 || msSendLoraAttr < ms );
             isSendA = false;
// Требуется отправка только телеметрии
             if( isSendT && !isSendA ){
                if( isLoraACK && msSendLora != 0 ){
                   isLoraACK = false;
                   msSendLora = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>()*1000;   
                }
                else {
                   sendLora();
                   msSendLora = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>()*1000;
                }
             }
/*
// Требуется отправка только атрибутов
             if( isSendT && !isSendA ){
                if( isLoraACK && msSendLoraAttr != 0 ){
                   isLoraACK = false;
                   msSendLoraAttr = ms + config["config2"]["NET"]["T_SEND"].as<uint32_t>()*1000;   
                }
                else {
                   sendLoraAttr();
                   msSendLoraAttr = ms + config["config2"]["NET"]["T_RETRY"].as<uint32_t>()*1000;
                }
             }
// Требуется отпрвлять и телеметрию и атрибуты. Текущая отпрпвка телеметрия
             else if( isSendT && isSendA && isSendCurT ){
                if( isLoraACK && msSendLora != 0 ){
                   isLoraACK = false;
                   msSendLora = ms + config["config2"]["NET"]["T_SEND"].as<uint32_t>()*1000;
                }
                else {
                   msSendLora = ms + config["config2"]["NET"]["T_RETRY"].as<uint32_t>()*1000;
                }
                sendLoraAttr();
                msSendLoraAttr = ms + config["config2"]["NET"]["T_RETRY"].as<uint32_t>()*1000;
                isSendCurT = false;   
             }
// Требуется отпрвлять и телеметрию и атрибуты. Текущая отпрпвка атрибуты
             else if( isSendT && isSendA && !isSendCurT ){
                if( isLoraACK && msSendLoraAttr != 0 ){
                   isLoraACK = false;
                   msSendLoraAttr = ms + config["config2"]["NET"]["T_SEND"].as<uint32_t>()*1000;
                }
                else {
                   msSendLoraAttr = ms + config["config2"]["NET"]["T_RETRY"].as<uint32_t>()*1000;
                }
                sendLora();
                msSendLora = ms + config["config2"]["NET"]["T_RETRY"].as<uint32_t>()*1000;
                isSendCurT = true;   
             }
*/             
          }          
      }
      if( loraIrq ){
         loraIrq = false;
         readLora();
      }
// Перенес в taskButton
//      HTTP_loop();
      
      vTaskDelay(500);      
   }


}

void handleEventWiFi(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:     Serial.println("STA Started"); break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: Serial.println("STA Connected"); break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("!!! STA Got IP");
      Serial.println(WiFi.STA);
      if( config["config2"]["WIFI_MODE"].as<int>() == STA_AUTO ){
         systemMP3("60",60,PRIORITY_MP3_MEDIUM);
         EventRGB1->setColor1(COLOR_WIFI_ON);
         config["config2"]["WIFI_NAME1"] = config["config2"]["WIFI_NAME2"];
         config["config2"]["WIFI_PASS1"] = config["config2"]["WIFI_PASS2"];
         config["config2"]["WIFI"]["WIFI_MODE"] = STA_ON;
         configWrite();
      }


//      WiFi.AP.enableNAPT(true);
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.println("!!! STA Lost IP");
     EventRGB1->setColor1(COLOR_WIFI_WAIT);          
      msSTA = millis();
//      WiFi.AP.enableNAPT(false);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("!!! STA Disconnected");
//      systemMP3("60",62,PRIORITY_MP3_MEDIUM);
//      WiFi.AP.enableNAPT(false);
      break;
    case ARDUINO_EVENT_WIFI_STA_STOP: Serial.println("STA Stopped"); break;

    case ARDUINO_EVENT_WIFI_AP_START:
//      w_stat2 = EWS_AP_MODE;
      if( bootCount<0 )   EventRGB1->setColor0(COLOR_WIFI_AP);
      else EventRGB1->setColor0(COLOR_WIFI_AP1);
      Serial.println("AP Started");
      Serial.println(WiFi.AP);
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:    
      Serial.println("AP STA Connected");
      EventRGB1->setColor0(COLOR_WIFI_ON,true); 
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
     Serial.println("AP STA Disconnected"); 
     EventRGB1->setColor0(COLOR_WIFI_AP1);
     break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      Serial.print("AP STA IP Assigned: ");
      Serial.println(IPAddress(info.wifi_ap_staipassigned.ip.addr));
      break;
    case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED: Serial.println("AP Probe Request Received"); break;
    case ARDUINO_EVENT_WIFI_AP_STOP:       
      Serial.println("AP Stopped"); 
      EventRGB1->setColor0(COLOR_BLACK);
      break;

    default: break;
  }
}

/**
* Отправка на CRM.MOSCOW по протоколу HTTP GET
*/
bool sendCrmMoscowParam(){
   bool ret = false;
   char s[64];
   uint32_t tm = millis()/1000;
   int _dist = (int)Distance;
   int _stat = getStatus();
   sprintf(s,"%s;%ld;%d;%d;%d",strID,tm,_dist,tm,0);
   uint16_t crc = KeyGen(s);

   String str = "";
   str += "http://";
   str += config["config2"]["CRM_HOST"].as<String>();
   str += ":";
   str += config["config2"]["TB_PORT"].as<int>();
   str = CRM_MOSCOW_PATH;
   str += "?id=";
   str += config["config2"]["N_DOGIVOR"].as<String>();
   str += "_";
   str += config["config2"]["N_BOX"].as<String>();
//   str += strID;
   str += "&temp=0&hum=0&dist=";
   str += _dist;
   str += "&tm=";
   str += String(millis()/1000);
   str += "&btn=";
   str += _stat;
   str += "&uptime=";
   str += String(millis()/1000);
   str += "&key=";
   str += (int)crc;

   Serial.println(str);


   HttpResponse response = SimpleHttpClient::GET(config["config2"]["CRM_HOST"].as<const char *>(),config["config2"]["TB_PORT"].as<int>(),s);

/*

   httpClient.begin(str);
   int httpCode = httpClient.GET();
   Serial.print(F("!!! HTTP send "));
   Serial.println(config["config2"]["CRM_HOST"].as<String>());
   
   if( httpCode == HTTP_CODE_OK ){
        String payload = httpClient.getString();
        Serial.print(" success: ");
        Serial.println(payload);
        ret = true;
   }
   else {
        Serial.print(" error: ");
        Serial.println(httpCode);
   }
//   sprintf(sbuf,"GET http://%s:%d%s?id=%s&temp=%d&hum=%d&dist=%d&tm=%ld&btn=%d&uptime=%ld&key=%d HTTP/1.0\r\n\r\n",
//      EA_Config.SERVER,EA_Config.PORT,HTTP_PATH,SensorID,_temp,_hum,
//      _dist,_time,(int)_btn,_uptime,(int)KeyGen());
   httpClient.end();
*/

if (response.statusCode == 200) {
    Serial.println("Success");
    Serial.println(response.body);
} else {
    Serial.printf("HTTP error: %d\n", response.statusCode);
}


   return ret;    
}

int getStatus(){
   int _stat = -2;
   switch(SensorOn){
      case SS_BUSY:
      case SS_NAN_BUSY: _stat = 1;break;
      case SS_FREE:   
      case SS_NAN_FREE: _stat = 0;break;  
      default: _stat = -1;
   }     
   return _stat;
}


/**
* Отправка на шлюз по протоколу HTTP GET
*/
bool sendHttpParam(){

    bool ret = true;
    int start = -1;
    String _hosts = config["config2"]["HTTP_SERVERS"].as<String>();
    for (int i = 0; i <= _hosts.length(); i++) {

        bool separator =
            (i == _hosts.length()) ||
            (_hosts[i] == ' ') ||
            (_hosts[i] == ',') ||
            (_hosts[i] == '\n') ||
            (_hosts[i] == '\r');

        if (start < 0) {
            if (!separator)
                start = i;
        }
        else {
            if (separator) {

                String _host = _hosts.substring(start, i);

                // Использование host
                Serial.print("!!! HTTP send: ");
                Serial.println(_host);
                sendHttpParamOne(_host);

                start = -1;
            }
        }
    }
}

void sendHttpParamOne(String &_host){

   char s[64];
   uint32_t tm = millis()/1000;
   int _dist = (int)Distance;
   int _stat = getStatus();
   sprintf(s,"%s;%ld;%d",strID,tm,_dist);
   uint16_t crc = KeyGen(s);

   String str = "";
   str += HTTP_PATH;
   str += "?id=";
   str += strID;
   str += "&dist=";
   str += _dist;
   str += "&sn=";
   str += serNo;
   str += "&dn=";
   str += config["config2"]["N_DOGIVOR"].as<String>();
   str += "&bn=";
   str += config["config2"]["N_BOX"].as<String>();  
   str += "&tm=";
   str += String(millis()/1000);
   str += "&stat=";
   str += _stat;
   str += "&uptime=";
   str += String(millis()/1000);
   str += "&rssi=";
   str += WiFi.RSSI();
   str += "&key=";
   str += (int)crc;
   Serial.printf("!!! HTTP send: %s ",_host.c_str());
   Serial.print(str);
   HttpResponse response = SimpleHttpClient::GET(_host.c_str(),80,str);
   if (response.statusCode == 200) {
      Serial.println(" Success");
      Serial.println(response.body);
   } 
   else {
      Serial.printf(" HTTP error: %d\n", response.statusCode);
   }
   return;    
}


/**
 * Генерация контрольной суммы 
 */
 
uint16_t KeyGen(char *str){
//   char s[64];
//   sprintf(s,"%s;%ld;%d;%d;%d",strID,Time,Distance,Time,Hum);
//   Serial.printf("!!! CRC %s %d\n",str,strlen(str));
   uint16_t crc = 0;
   for( int i=0; i< strlen(str); i++ ){
       crc += (int)str[i];
   }    
   crc = ( ~ crc )&0xfff;   
   return crc;
  
}

bool sendParamTB(){
    bool ret = false;
    bool is_over_gate = config["config2"]["TB"]["GATEWAY"].as<bool>();
    if( !is_over_gate){
       if( config["config2"]["TB_TOKEN"].isNull() || config["config2"]["TB_TOKEN"] == "" )if(!authTB(TB_PROVISION_KEY,TB_PROVISION_SECRET))return false;
    }
    String _url = "http://";
//    _url += config["config2"]["TB_HOST"].as<String>();
//    _url += ":";
//    _url += config["config2"]["TB_PORT"].as<int>();
    _url = "/api/v1/";
    if( is_over_gate ){
       _url += strID;
    }
    else {
       _url += config["config2"]["TB_TOKEN"].as<String>();
    }
    _url += "/telemetry";

    int _state = getStatus();
    jsonData.clear();
    jsonData["Distance"] = (int)Distance;
    jsonData["State"]    = _state;
    jsonData["Uptime"]   = esp_timer_get_time()/1000000;
    if( serNo[0] != '\0' )jsonData["SN"] = serNo;
    String _data;
    serializeJson(jsonData, _data); 

    String _headers =
      "Content-Type: application/json\r\n";
//      "Authorization: Bearer token\r\n";
    HttpResponse response = SimpleHttpClient::POST(
      config["config2"]["TB_HOST"].as<const char *>(),
      config["config2"]["TB_PORT"].as<int>(),
      _url,
      "application/json",
      _data,
      _headers
    );
    if (response.statusCode >= 200 && response.statusCode < 300) {

      Serial.println("Success");
      deserializeJson(jsonData,response.body);
      Serial.println(response.body);
      if( jsonData["status"].as<String>() == "SUCCESS" ){
         config["config2"]["TB_TOKEN"] =  jsonData["credentialsValue"].as<String>();
         configWrite();
         ret = true;  
      }
    }
    else {
       Serial.println("Failed");
    }

//    httpClient.begin(_url);
//    httpClient.addHeader("Content-Type", "application/json");     
//    int httpCode = httpClient.POST(_data);
//    Serial.print(F("!!! TB auth send "));
//    Serial.print(_url);
//    Serial.print(" ");
//    Serial.println(_data);
   
//   if( httpCode == HTTP_CODE_OK ){
//        String _payload = httpClient.getString();
//        Serial.print(" success");
//        Serial.println(_payload);
//        ret = true;
 //  }
 //  else {
//        Serial.print(" error: ");
//        Serial.println(httpCode);
//   }
//   httpClient.end();

   if( !isSendAttributeTB && ret )isSendAttributeTB = sendAttributeTB();
   return ret;    


}

// Работа с TB
// Получаеи JWT_TOKEN
bool authTB(const char *_key, const char *_secret){
    bool ret = false;
    String _url = "http://";
//    _url += config["config2"]["TB_HOST"].as<String>();
//    _url += ":";
//    _url += config["config2"]["TB_PORT"].as<int>();
    _url = "/api/v1/provision";

    jsonData.clear();
    jsonData["deviceName"]            = strID;
    jsonData["provisionDeviceKey"]    = _key;
    jsonData["provisionDeviceSecret"] = _secret;
    String _data;
    serializeJson(jsonData, _data); 
    String _headers =
      "Content-Type: application/json\r\n";
//      "Authorization: Bearer token\r\n";
    HttpResponse response = SimpleHttpClient::POST(
      config["config2"]["TB_HOST"].as<const char *>(),
      config["config2"]["TB_PORT"].as<int>(),
      _url,
      "application/json",
      _data,
      _headers
    );

//    httpClient.begin(_url);
//    httpClient.addHeader("Content-Type", "application/json");     
//    int httpCode = httpClient.POST(_data);
//    Serial.print(F("!!! TB auth send "));
//    Serial.print(_url);
//    Serial.print(" ");
//    Serial.println(_data);

    if (response.statusCode >= 200 && response.statusCode < 300) {

      Serial.println("Success");
      deserializeJson(jsonData,response.body);
      Serial.println(response.body);
      if( jsonData["status"].as<String>() == "SUCCESS" ){
         config["config2"]["TB_TOKEN"] =  jsonData["credentialsValue"].as<String>();
         configWrite();
         ret = true;  
      }
    }
    else {
       Serial.println("Failed");
    }

//   Serial.println(response.body);

//   if( httpCode == HTTP_CODE_OK ){
//      String _payload = httpClient.getString();
//      jsonData.clear();
//      deserializeJson(jsonData,_payload);
//      Serial.println(_payload);
//      if( jsonData["status"].as<String>() == "SUCCESS" ){
//         config["config2"]["TB_TOKEN"] =  jsonData["credentialsValue"].as<String>();
//         configWrite();
//         ret = true;  
//      }
//   }
   return ret;
}


bool sendAttributeTB(){
    bool ret = false;

    bool is_over_gate = config["config2"]["TB"]["GATEWAY"].as<bool>();
    if( !is_over_gate){
       if( config["config2"]["TB_TOKEN"].isNull() || config["config2"]["TB_TOKEN"] == "" )if(!authTB(TB_PROVISION_KEY,TB_PROVISION_SECRET))return false;
    }


    String _url = "http://";
//    _url += config["config2"]["TB_HOST"].as<String>();
//    _url += ":";
//    _url += config["config2"]["TB_PORT"].as<int>();
    _url = "/api/v1/";
    if( is_over_gate ){
       _url += strID;
    }
    else {
       _url += config["config2"]["TB_TOKEN"].as<String>();
    }
    _url += "/attributes?clientKeys";
    jsonData.clear();
    jsonData["SerialNo"]    = serNo;
    jsonData["DogovorNo"]   = config["config2"]["N_DOGIVOR"].as<String>();
    jsonData["BoxNo"]       = config["config2"]["N_BOX"].as<String>();
    String _data;
    serializeJson(jsonData, _data); 

   String _headers =
      "Content-Type: application/json\r\n";
    HttpResponse response = SimpleHttpClient::POST(
      config["config2"]["TB_HOST"].as<const char *>(),
      config["config2"]["TB_PORT"].as<int>(),
      _url,
      "application/json",
      _data,
      _headers
    );


    if (response.statusCode >= 200 && response.statusCode < 300) {

      Serial.println("Success");
      deserializeJson(jsonData,response.body);
      Serial.println(response.body);
      if( jsonData["status"].as<String>() == "SUCCESS" ){
         config["config2"]["TB_TOKEN"] =  jsonData["credentialsValue"].as<String>();
         configWrite();
         ret = true;  
      }
    }
    else {
       Serial.println("Failed");
    }
//    httpClient.begin(_url);
//    httpClient.addHeader("Content-Type", "application/json");     
//    int httpCode = httpClient.POST(_data);
//    Serial.print(F("!!! TB send token="));
//    Serial.print(config["config2"]["TB_TOKEN"].as<String>());
//    Serial.print(" Data=");
//    Serial.println(_data);
   
//   if( httpCode == HTTP_CODE_OK ){
//        String _payload = httpClient.getString();
//        Serial.print(" success");
//        Serial.println(_payload);
//        ret = true;
//   }
//   else {
//        Serial.print(" error: ");
//        Serial.println(httpCode);
//   }
//   httpClient.end();

   return ret;    
}