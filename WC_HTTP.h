#pragma once

#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "WC_Config.h"
#include "WC_Task.h"
#include "MyConfig.h"

// ===== MODULE DEBUG CONFIGURATION =====
#define MODULE_NAME "HTTP"
#define MODULE_DEBUG_LEVEL DEBUG_DEFAULT
#include "src/Slib/SDEBUG.h"

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
void HTTP_setResponse(const String& msg = "", const String& cmd = "");
void httpSetStatus(const String& s);

// ===== UI COMPONENTS =====
void handleHeader();
void handleTail();
void handleDistance();

// ===== UTILITY =====
bool HTTP_redirect();
void HTTP_notfound(const String& path);
String WiFi_ScanNetwork();
String deviceName();
void printArg();
void saveConfigData();
void HTTP_playMP3();       // ← ДОБАВЛЯЕМ объявление

// ===== EXTERNAL FUNCTIONS (from other modules) =====
void API_fsBegin();        // ← ДОБАВЛЯЕМ объявление