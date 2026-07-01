#pragma once

#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
//#if defined(IS_DNS)
#include <DNSServer.h>

//#include <ESPmDNS.h>
//#endif
#include <Update.h>
#include <ArduinoJson.h>

#include "WC_Config.h"
#include "WC_Task.h"
#include "MyConfig.h"

extern String strResponse;

void HTTPD_start();
void HTTPD_loop();
void HTTPD_stop();
void HTTPD_begin();


void handleSave();

bool HTTP_redirect();
void HTTP_notfound(String path);
void HTTP_file(String uri);

String getContentType(const String& path);

String readConfig();
String readConfig1();
void writeConfig(String s);
void writeConfig1(String s);

void handleFile();
void handleFirmwareUpload();
void httpSetStatus(const String& s);

void handleConfigSelector();
void handleHeader();
void handleTail();
void handleDistance();
void handleAuth();
void handleCommand();
void handleResponse();
void HTTP_setResponse(String _msg="", String _cmd="");

bool needAuth(const String& type);
bool isAuth();
void printHeaders();
void printArg();
String WiFi_ScanNetwork();
String deviceName();
void saveConfig();
void HTTP_playMP3();
void handle_firmwareUpdate();