#define MODULE_NAME "NET"
#define MODULE_DEBUG_LEVEL DEBUG_INFO
#include "src/Slib/SDEBUG.h"
#include "WC_Net.h"
#include "WC_HttpSend.h"

bool isAP = false;
//bool isSTA = false;
bool isWiFiConnected = false;

static uint32_t msSTA = 0;
//static JsonDocument jsonData;
static MyHttpSend httpSend;
static bool webFilesSent = false;
static bool configSent = false;
static String g_serverFirmwareVersion = "";
bool isSendAttributeTB = false;

// Приоритеты задачи
static bool isHttpLowPriority      = true;


JsonDocument jsonData;
/**
* Задача менеджера WiFi
*/
void taskWiFiManager(void *pvParameters) {
    LOG_INFOLN("WiFi Manager task started");
    WiFi.mode(WIFI_OFF);
    EventRGB1->setColor0(COLOR_BLACK);
    EventRGB1->setColor1(COLOR_BLACK);
    isAP                = true;
    bool isAPactive     = false;
    T_STA_MODE modeSTA = (T_STA_MODE)config["config2"]["WIFI_MODE"].as<int>();
    bool isSTAconnected = false;
    bool isSTAactive    = false;
    int wifiError = 0;
//    uint32_t msLastStaCheck = 0;
//    wifi_mode_t lastWiFiMode = WIFI_OFF;
    
    if (!config["config2"]["WIFI_POWER"].isNull()) {
        wifi_power_t power = (wifi_power_t)config["config2"]["WIFI_POWER"].as<int>();
        WiFi.setTxPower(power);
        LOG_DEBUGLN("WiFi power set to: %d", power);
    }

   
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.persistent(true);

    while (true) {
        if (isSensorBlock || calibrMode == CM_WAIT_REBOOT) {
            vTaskDelay(1000);
            continue;
        }
        uint32_t ms = millis();
// Если нужно включить AP и AP не работает        
        if( isAP && ! isAPactive ){
            isAPactive = true;
            String ap_name = config["config2"]["ESP_NAME"].as<String>();
            if (ap_name == "") ap_name = deviceName();
            WiFi.softAP(ap_name);
            LOG_INFOLN("AP started: %s", ap_name.c_str());
            LOG_INFOLN("AP IP address: %s", WiFi.softAPIP().toString().c_str());
            if (bootCount < 0) {
                EventRGB1->setColor0(COLOR_WIFI_AP);
            } else {
                EventRGB1->setColor0(COLOR_WIFI_AP1);
            }
        }

// Если нужно нужно выключить AP
        else if(  !isAP && isAPactive ){
            WiFi.softAPdisconnect(true);
            isAPactive = false;
            EventRGB1->setColor0(COLOR_BLACK);
            LOG_INFOLN("AP stopped");
        }
      
        modeSTA = (T_STA_MODE)config["config2"]["WIFI_MODE"].as<int>();
// Если нужно включить STA п STA не включено 
        if( modeSTA != STA_OFF && !isSTAactive ){
            isSTAactive = true;
// Проверяем нужен ли статический IP
            if (config["config2"]["STATIC_IP"].as<bool>()) {
                IPAddress ip_addr, ip_mask, ip_gate, ip_dns;
                if (ip_addr.fromString(config["config2"]["IP_ADDR"].as<String>()) &&
                    ip_mask.fromString(config["config2"]["IP_MASK"].as<String>()) &&
                    ip_gate.fromString(config["config2"]["IP_GATE"].as<String>()) &&
                    ip_dns.fromString(config["config2"]["IP_DNS"].as<String>())) {
                        WiFi.config(ip_addr, ip_gate, ip_mask, ip_dns);
                        LOG_INFOLN("Static IP configured: %s",
                            config["config2"]["IP_ADDR"].as<String>().c_str());
                }
            }
// Определяем режим соединения
            if( modeSTA == STA_ON ){
                LOG_INFOLN("Connecting to primary WiFi: %s", config["config2"]["WIFI_NAME1"].as<String>().c_str());
                WiFi.begin(config["config2"]["WIFI_NAME1"].as<String>(), config["config2"]["WIFI_PASS1"].as<String>());
            } else {
                LOG_INFOLN("Auto-connecting to secondary WiFi: %s", config["config2"]["WIFI_NAME2"].as<String>().c_str());
                WiFi.begin(config["config2"]["WIFI_NAME2"].as<String>(),  config["config2"]["WIFI_PASS2"].as<String>());
            }   
            isSTAconnected = false;
            EventRGB1->setColor1(COLOR_WIFI_WAIT);
        }

// Если нужно STA включен
        else if(  isSTAactive ){
            wl_status_t status = WiFi.status();
// Требуется выключение            
            if( modeSTA == STA_OFF ){
                WiFi.setAutoReconnect(false);
                WiFi.disconnect(false, false);
                isSTAactive = false;
                isSTAconnected = false;  
                wifiError = 0;           
                EventRGB1->setColor1(COLOR_BLACK);
                LOG_INFOLN("WiFi disconnect");
                continue;
            }
// Если пока нет подключения
            if( status ==  WL_CONNECTED ){
                isWiFiConnected = true;
                if( !isSTAconnected ){
                    EventRGB1->setColor1(COLOR_WIFI_ON);
                    isSTAconnected = true;
                    isSendNet = true;
                    EventRGB1->setColor1(COLOR_WIFI_ON);
                    LOG_INFOLN("WiFi connected successfully!");
                    LOG_INFOLN("IP address: %s", WiFi.localIP().toString().c_str());
                    LOG_DEBUGLN("RSSI: %d dBm", WiFi.RSSI());
                }
                wifiError = 0;
            }
// Если нет подключения            
            else {
                isWiFiConnected = false;
                if( isSTAconnected ){
                    LOG_ERRORLN("WiFi lost!");
                    EventRGB1->setColor1(COLOR_WIFI_WAIT);
                    isSTAconnected = false;
                }
                wifiError++;
            }

// Считаем ошибки STA_AUTO
            if( modeSTA == STA_AUTO && wifiError > 20 ){
                wifiError = 0;
                isSTAactive = false;
                EventRGB1->setColor1(COLOR_WIFI_OFF);
                WiFi.setAutoReconnect(false);
                WiFi.disconnect(false, false);
                config["config2"]["WIFI_MODE"] = STA_ON;
                configWrite(); 
                LOG_ERRORLN("Change STA mode");
                systemMP3("60", 62, PRIORITY_MP3_MEDIUM);
            }                      

// Считаем ошибки STA_ON
            if( modeSTA == STA_ON && wifiError > 300 ){
                wifiError = 0;
                WiFi.mode(WIFI_OFF);
                vTaskDelay(100);
                WiFi.persistent(false);  // Отключаем сохранение в NVS
                vTaskDelay(100);
                WiFi.persistent(true);   // Возвращаем сохранение
                WiFi.mode(WIFI_AP_STA);   
                LOG_ERRORLN("WiFi reset");
                isSTAactive = false;
                EventRGB1->setColor1(COLOR_WIFI_OFF);
            }

        }
        vTaskDelay(1000);
    }
}

void setHttpActivity(){
// Проверяем текущий уровень активности

   bool _lowPriority = isHttpLowPriority;
   if( config["config2"]["CRM_ENABLE"].as<bool>() || config["config2"]["HTTP_ENABLE"].as<bool>() || config["config2"]["TB_ENABLE"].as<bool>() )isHttpLowPriority = false;
   else isHttpLowPriority = true;

// Проверяем, изменился ли уровень активности
   if( _lowPriority != isHttpLowPriority ){
      isHttpLowPriority = _lowPriority; 
      if( isHttpLowPriority )vTaskPrioritySet(NULL,HTTP_LOW_PRIORITY);
      else vTaskPrioritySet(NULL,HTTP_HIGH_PRIORITY);
      LOG_DEBUGLN("HTTP loop Change Activity %d", (int)uxTaskPriorityGet(NULL));
   }
}




/**
* Задача отправки HTTP данных
*/
void taskHttpSender(void *pvParameters) {
    LOG_INFOLN("HTTP Sender task started");
    uint32_t msSendHttp = millis();
    uint32_t msSendTB = msSendHttp;
    uint32_t msSendCrm = msSendHttp;
    uint32_t msCheckFW = millis();


    bool isFirstConnect = true;
    bool isConfigChecked = false;
    
    if (!httpSend.begin()) {
        LOG_ERRORLN("HTTP Sender: Failed to initialize MyHttpSend");
    }
    
    while (true) {
        if (isSensorBlock || calibrMode == CM_WAIT_REBOOT) {
            vTaskDelay(1000);
            continue;
        }
        uint32_t ms = millis();
        if (!isWiFiConnected) {
//            isFirstConnect = true;
            isConfigChecked = false;
            vTaskDelay(1000);
            continue;
        }
        setHttpActivity();

        if (isFirstConnect) {
            isFirstConnect = false;
            isSendNet = true;
//            msSendHttp = ms;
//            msSendCrm = ms;
//            msSendTB = ms;
            LOG_INFOLN("HTTP sender timer reset");
            
        }
        if(  isSendNet ){
            isSendNet  = false;
            msSendHttp = ms;
            msSendCrm  = ms;
            msSendTB   = ms;
//           msSendLora = ms;
            LOG_INFOLN("HTTP send activate");
        } 
        
        if (config["config2"]["CRM_ENABLE"].as<bool>() &&
            TIME_EXPIRED_MS(ms, msSendCrm)) {
            LOG_DEBUGLN("Sending data to CRM Moscow...");
            if (httpSend.sendCrmMoscowParam()) {
                msSendCrm = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>() * 1000;
                LOG_DEBUGLN("CRM next send in %d seconds",
                    config["config2"]["TM_HTTP_SEND"].as<uint32_t>());
            } else {
                msSendCrm = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>() * 1000;
                LOG_ERRORLN("CRM send failed, retry in %d seconds",
                    config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>());
            }
        }
        
        if (config["config2"]["HTTP_ENABLE"].as<bool>() &&
            TIME_EXPIRED_MS(ms, msSendHttp)) {
            LOG_DEBUGLN("Sending data to HTTP Gateways...");
            if (httpSend.sendHttpParam()) {
                msSendHttp = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>() * 1000;
                LOG_DEBUGLN("HTTP Gateways next send in %d seconds",
                    config["config2"]["TM_HTTP_SEND"].as<uint32_t>());
            } else {
                msSendHttp = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>() * 1000;
                LOG_ERRORLN("HTTP Gateway send failed, retry in %d seconds",
                    config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>());
            }
        }
        
        if (config["config2"]["TB_ENABLE"].as<bool>() ){
            if (!config["config2"]["TB_TOKEN"].isNull() && config["config2"]["TB_TOKEN"] != ""){
               if( TIME_EXPIRED_MS(ms, msCheckFW) ) {
 //                  httpSend.checkFirmwareVersionTB();
 //                  httpSend.checkHttpdVersionTB();
                   if (!isSendAttributeTB) {
                      LOG_DEBUGLN("Sending device attributes to ThingsBoard...");
                      isSendAttributeTB = httpSend.sendAttributeTB();
                      if (isSendAttributeTB) {
                         LOG_INFOLN("Device attributes sent successfully");
                      }
                   }


                   httpSend.checkUpdateTB(isSendAttributeTB);
                   msCheckFW = millis() + TM_TB_CHECK;
               }

               if( TIME_EXPIRED_MS(ms, msSendTB) ) {
                  LOG_INFOLN("Sending telemetry to ThingsBoard...");
                  if (httpSend.sendParamTB()) {
                     msSendTB = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>() * 1000;
                     LOG_DEBUGLN("ThingsBoard next send in %d seconds",
                      config["config2"]["TM_HTTP_SEND"].as<uint32_t>());
                  }
                  else {
                     msSendTB = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>() * 1000;
                     LOG_ERRORLN("ThingsBoard send failed, retry in %d seconds",
                    config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>());
                  }
               }
              
            }
            else {
               if( TIME_EXPIRED_MS(ms, msCheckFW) ) {  
                  httpSend.authTB(TB_PROVISION_KEY, TB_PROVISION_SECRET);
                  msCheckFW = millis() + TM_TB_CHECK;
               }   
            }

            




        }
        if( isHttpLowPriority ){
            vTaskDelay(HTTP_LOW_TM);
        }   
        else {
            vTaskDelay(HTTP_HIGH_TM);
        }
    }
}