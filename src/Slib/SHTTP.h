#pragma once

#include "../Webserver/WebServer.h"

#include <LittleFS.h>
#include <ArduinoJson.h>
//#include <Update.h>

extern WebServer webServer;

void handle_fsList();
void handle_fsFile();
void handle_fsDelete();
void handle_fsCreate();
void handle_fsSave();
void handle_fsDownload();
void handle_fsUpload();

void API_fsBegin();
