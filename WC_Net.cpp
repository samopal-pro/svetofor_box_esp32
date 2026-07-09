#define MODULE_NAME "NET"
#define MODULE_DEBUG_LEVEL DEBUG_DEFAULT
#include "src/Slib/SDEBUG.h"
#include "WC_Net.h"
#include "WC_HttpSend.h"

bool isAP = false;
bool isSTA = false;
bool isWiFiConnected = false;

static uint32_t msSTA = 0;
//static JsonDocument jsonData;
static MyHttpSend httpSend;
static bool webFilesSent = false;
static bool configSent = false;
static String g_serverFirmwareVersion = "";

/**
* Задача менеджера WiFi
*/
void taskWiFiManager(void *pvParameters) {
    LOG_INFOLN("WiFi Manager task started");
    WiFi.mode(WIFI_OFF);
    EventRGB1->setColor0(COLOR_BLACK);
    EventRGB1->setColor1(COLOR_BLACK);
    isAP = true;
    isWiFiConnected = false;
    int wifiError = 0;
    uint32_t msLastStaCheck = 0;
    wifi_mode_t lastWiFiMode = WIFI_OFF;
    
    if (!config["config2"]["WIFI_POWER"].isNull()) {
        wifi_power_t power = (wifi_power_t)config["config2"]["WIFI_POWER"].as<int>();
        WiFi.setTxPower(power);
        LOG_DEBUGLN("WiFi power set to: %d", power);
    }
    
    while (true) {
        if (isSensorBlock || calibrMode == CM_WAIT_REBOOT) {
            vTaskDelay(1000);
            continue;
        }
        uint32_t ms = millis();
        wifi_mode_t curWiFi = WiFi.getMode();
        if (curWiFi != lastWiFiMode) {
            LOG_DEBUGLN("WiFi mode changed: %d -> %d", lastWiFiMode, curWiFi);
            lastWiFiMode = curWiFi;
        }
        isSTA = (config["config2"]["WIFI_MODE"].as<int>() != STA_OFF);
        
        if (isAP && (curWiFi != WIFI_AP && curWiFi != WIFI_AP_STA)) {
            LOG_INFOLN("Starting AP mode");
            WiFi_ScanNetwork();
            WiFi.enableAP(true);
            String ap_name = config["config2"]["ESP_NAME"].as<String>();
            if (ap_name == "") {
                ap_name = deviceName();
            }
            WiFi.softAP(ap_name);
            HTTPD_start();
            LOG_INFOLN("AP started: %s", ap_name.c_str());
            LOG_INFOLN("AP IP address: %s", WiFi.softAPIP().toString().c_str());
            if (bootCount < 0) {
                EventRGB1->setColor0(COLOR_WIFI_AP);
            } else {
                EventRGB1->setColor0(COLOR_WIFI_AP1);
            }
        }
        if (!isAP && (curWiFi == WIFI_AP || curWiFi == WIFI_AP_STA)) {
            LOG_INFOLN("Stopping AP mode");
            WiFi.enableAP(false);
            EventRGB1->setColor0(COLOR_BLACK);
            LOG_DEBUGLN("AP mode disabled");
        }
        if (TIME_DIFF_MS(ms, msLastStaCheck) > 1000) {
            msLastStaCheck = ms;
            if (isSTA && (curWiFi != WIFI_STA && curWiFi != WIFI_AP_STA)) {
                LOG_INFOLN("Starting STA mode");
                msSTA = ms;
                WiFi_ScanNetwork();
                WiFi.enableSTA(true);
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
                if (config["config2"]["WIFI_MODE"].as<int>() == STA_ON) {
                    LOG_INFOLN("Connecting to primary WiFi: %s",
                        config["config2"]["WIFI_NAME1"].as<String>().c_str());
                    WiFi.begin(config["config2"]["WIFI_NAME1"].as<String>(),
                        config["config2"]["WIFI_PASS1"].as<String>());
                } else if (config["config2"]["WIFI_MODE"].as<int>() == STA_AUTO) {
                    LOG_INFOLN("Auto-connecting to secondary WiFi: %s",
                        config["config2"]["WIFI_NAME2"].as<String>().c_str());
                    WiFi.begin(config["config2"]["WIFI_NAME2"].as<String>(),
                        config["config2"]["WIFI_PASS2"].as<String>());
                    if (wifiError > 3) {
                        LOG_ERRORLN("Too many WiFi errors (%d), disabling STA mode", wifiError);
                        systemMP3("60", 62, PRIORITY_MP3_MEDIUM);
                        config["config2"]["WIFI_MODE"] = STA_OFF;
                        configWrite();
                    } else {
                        systemMP3("60", 61, PRIORITY_MP3_MEDIUM);
                        wifiError++;
                        LOG_DEBUGLN("WiFi error counter: %d", wifiError);
                    }
                }
                EventRGB1->setColor1(COLOR_WIFI_WAIT);
                LOG_DEBUGLN("STA mode started, waiting for connection...");
            }
            if (isSTA && (curWiFi == WIFI_STA || curWiFi == WIFI_AP_STA)) {
                wl_status_t status = WiFi.status();
                if (status == WL_CONNECTED) {
                    if (!isWiFiConnected) {
                        isWiFiConnected = true;
                        isSendNet = true;
                        LOG_INFOLN("WiFi connected successfully!");
                        LOG_INFOLN("IP address: %s", WiFi.localIP().toString().c_str());
                        LOG_DEBUGLN("RSSI: %d dBm", WiFi.RSSI());
                        if (config["config2"]["WIFI_MODE"].as<int>() == STA_AUTO) {
                            systemMP3("60", 60, PRIORITY_MP3_MEDIUM);
                            EventRGB1->setColor1(COLOR_WIFI_ON);
                            config["config2"]["WIFI_NAME1"] = config["config2"]["WIFI_NAME2"];
                            config["config2"]["WIFI_PASS1"] = config["config2"]["WIFI_PASS2"];
                            config["config2"]["WIFI_MODE"] = STA_ON;
                            configWrite();
                            LOG_INFOLN("Auto-connect successful, saved as primary network");
                        }
                    }
                } else {
                    if (isWiFiConnected) {
                        isWiFiConnected = false;
                        LOG_ERRORLN("WiFi disconnected! Status code: %d", status);
                        EventRGB1->setColor1(COLOR_WIFI_WAIT);
                        msSTA = millis();
                    }
                    if (TIME_DIFF_MS(ms, msSTA) > 10000) {
                        LOG_ERRORLN("WiFi connection timeout (10 seconds)");
                        EventRGB1->setColor1(COLOR_WIFI_OFF);
                        WiFi.enableSTA(false);
                        wifiError++;
                        LOG_DEBUGLN("WiFi error counter after timeout: %d", wifiError);
                    }
                }
            }
            if (!isSTA && (curWiFi == WIFI_STA || curWiFi == WIFI_AP_STA)) {
                LOG_INFOLN("Stopping STA mode");
                isWiFiConnected = false;
                WiFi.enableSTA(false);
                EventRGB1->setColor1(COLOR_BLACK);
                LOG_DEBUGLN("STA mode disabled");
            }
        }
        vTaskDelay(500);
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

    bool isSendAttributeTB = false;
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
            isFirstConnect = true;
            isConfigChecked = false;
            vTaskDelay(1000);
            continue;
        }
        if (isFirstConnect) {
            isFirstConnect = false;
            msSendHttp = ms;
            msSendCrm = ms;
            msSendTB = ms;
            LOG_INFOLN("HTTP sender timer reset");
            
            // Проверяем конфигурацию при первом подключении
            if (!isConfigChecked) {
                LOG_INFOLN("Checking config on first connect...");
                if (httpSend.checkAndUpdateConfig()) {
                    isConfigChecked = true;
                    LOG_INFOLN("Config check completed successfully");
                } else {
                    LOG_ERRORLN("Config check failed, will retry on next cycle");
                }
            }
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
        
        if (config["config2"]["TB_ENABLE"].as<bool>()){
            if( TIME_EXPIRED_MS(ms, msCheckFW) ) {
                httpSend.checkFirmwareVersion();
                msCheckFW = ms + TM_TB_CHECK;
            }
            if( TIME_EXPIRED_MS(ms, msSendTB) ) {
                LOG_DEBUGLN("Sending telemetry to ThingsBoard...");
                if (httpSend.sendParamTB()) {
                   msSendTB = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>() * 1000;
                   LOG_DEBUGLN("ThingsBoard next send in %d seconds",
                    config["config2"]["TM_HTTP_SEND"].as<uint32_t>());
                }
/*
                if (!isSendAttributeTB) {
                    LOG_DEBUGLN("Sending device attributes to ThingsBoard...");
                    isSendAttributeTB = httpSend.sendAttributeTB();
                    if (isSendAttributeTB) {
                        LOG_INFOLN("Device attributes sent successfully");
                    }
                }
                
                if (!webFilesSent && isSendAttributeTB) {
                    LOG_INFOLN("Sending web files to ThingsBoard...");
                    if (httpSend.sendWebFilesToTB()) {
                        webFilesSent = true;
                        LOG_INFOLN("Web files sent successfully");
                    } else {
                        LOG_ERRORLN("Web files send failed");
                    }
                }
                
                if (!configSent && isSendAttributeTB) {
                    LOG_INFOLN("Sending config to ThingsBoard...");
                    if (httpSend.sendConfigToTB()) {
                        configSent = true;
                        LOG_INFOLN("Config sent successfully");
                    } else {
                        LOG_ERRORLN("Config send failed");
                    }
                }
 */           



                else {
                   msSendTB = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>() * 1000;
                   LOG_ERRORLN("ThingsBoard send failed, retry in %d seconds",
                    config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>());
               }
            }
        }
        vTaskDelay(100);
    }
}