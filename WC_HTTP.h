#pragma once
#define WEBSERVER_NO_AUTH        // Без аутентификации (BASIC/DIGEST)
#define WEBSERVER_NO_CORS        // Без CORS заголовков
#define WEBSERVER_NO_ETAG        // Без ETag кеширования
#define WEBSERVER_NO_CHUNKED     // Без chunked encoding
#define WEBSERVER_NO_SSE         // Без Server-Sent Events
#define WEBSERVER_NO_RAW         // Без raw обработки запросов
#define WEBSERVER_NO_MIDDLEWARE  // Без middleware цепочки
#define WEBSERVER_NO_REGEX       // Без UriBraces/UriGlob/UriRegex

#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "WC_Config.h"
#include "WC_Task.h"
#include "MyConfig.h"

#include "src/WebServerLite/WebServerLite.h"

#include "src/Slib/SDEBUG.h"



#include "src/Slib/SHTTPResponce.h"
/*

class MonitoredWebServer : public WebServer {
private:
    uint32_t _lastRequestTime;  // Только время последнего запроса
    
public:
    // Конструкторы - пробрасывают параметры в WebServer
    MonitoredWebServer() : WebServer(80) {}
    MonitoredWebServer(uint16_t port) : WebServer(port) {}
    MonitoredWebServer(const IPAddress& addr, uint16_t port = 80) : WebServer(addr, port) {}



void handleClient() override {
    if (_currentStatus == HC_NONE) {
        _currentClient = _server.accept();
        if (!_currentClient) {
            if (_nullDelay) delay(1);
            return;
        }
        LOG_INFOLN("!!! HTTP SERVER NEW CONNECT\n");
//        _updateActivity();  // Новое подключение

        _currentStatus = HC_WAIT_READ;
        _statusChange = millis();
    }

    bool keepCurrentClient = false;
    bool callYield = false;

    if (_currentClient.connected()) {
        switch (_currentStatus) {
            case HC_NONE:
                break;
                
            case HC_WAIT_READ:
                if (_currentClient.available()) {
LOG_INFOLN("!!! HTTP SERVER REQUEST\n");
//                    _updateActivity();  // Получен запрос
                    
                    _currentClient.setTimeout(HTTP_MAX_SEND_WAIT);
                    if (_parseRequest(_currentClient)) {
                        _contentLength = CONTENT_LENGTH_NOT_SET;
                        _responseCode = 0;
                        _clearResponseHeaders();

                        if (_chain) {
                            _chain->runChain(*this, [this]() {
                                return _handleRequest();
                            });
                        } else {
                            _handleRequest();
                        }

                        if (_currentClient.isSSE()) {
                            _currentStatus = HC_WAIT_CLOSE;
                            _statusChange = millis();
                            keepCurrentClient = true;
                        }
                    }
                } else {
                    if (millis() - _statusChange <= HTTP_MAX_DATA_WAIT) {
                        keepCurrentClient = true;
                    }
                    callYield = true;
                }
                break;
                
            case HC_WAIT_CLOSE:
                if (_currentClient.isSSE()) {
                    _statusChange = millis();
                }
                if (millis() - _statusChange <= HTTP_MAX_CLOSE_WAIT) {
                    keepCurrentClient = true;
                    callYield = true;
                }
        }
    }

    if (!keepCurrentClient) {
        _currentClient = NetworkClient();
        _currentStatus = HC_NONE;
        _currentUpload.reset();
        _currentRaw.reset();
    }

    if (callYield) yield();
}

    // Метод для получения времени последней активности
    uint32_t getLastRequestTime() const {
        return _lastRequestTime;
    }
    
    // Метод для проверки простоя
    uint32_t getIdleTime() const {
        return millis() - _lastRequestTime;
    }
};
*/

void taskHttpServer(void *pvParameters);
bool setHttpActivity();

// ===== HTTP SERVER CORE =====
void HTTPD_start();        // ← ВОЗВРАЩАЕМ эту функцию
void HTTPD_begin();
void HTTPD_loop();
void HTTPD_stop();

// ===== FILE SERVING =====
void handleFile();
void HTTP_file(const String& uri);
String getContentType(const String& path);

// ===== AUTHENTICATION =====
bool isAuthenticated();
void handleAuth();

// ===== CONFIGURATION HANDLERS =====
void handleSave();
void handleConfigSelector();
void handleCommand();

// ===== FIRMWARE UPDATE =====
void handleFirmwareUpload();

// ===== RESPONSE HELPERS =====
void handleResponse();
void HTTP_sendResponse(const String& json);
void httpSetStatus(const String& s);

// ===== UI COMPONENTS =====
void handleHeader();
void handleTail();
void handleDistance();

// ===== UTILITY =====
bool HTTP_redirect();
void HTTP_notfound(const String& path);
String WiFi_ScanNetwork();
void printArg();
void saveConfigData();
void HTTP_playMP3();       // ← ДОБАВЛЯЕМ объявление

// ===== EXTERNAL FUNCTIONS (from other modules) =====
void API_fsBegin();        // ← ДОБАВЛЯЕМ объявление