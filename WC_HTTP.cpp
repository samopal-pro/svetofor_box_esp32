/**
 * HTTP Server Module
 * Embedded web server for controller configuration
 * 
 * Copyright (C) 2024 Alexey Shikharbeev
 * http://samopal.pro
 */
// ===== MODULE DEBUG CONFIGURATION =====
#define MODULE_NAME "HTTP"
#define MODULE_DEBUG_LEVEL DEBUG_DEFAULT
#include "src/Slib/SDEBUG.h"
#define IS_SET_ACTIVITY

#include "WC_HTTP.h"

// ===== CONSTANTS =====
static const char* const CONTENT_TYPE_JSON = "application/json";
static const char* const CONTENT_TYPE_TEXT = "text/plain; charset=utf-8";
static const char* const CONTENT_TYPE_HTML = "text/html; charset=utf-8";
static const char* const COOKIE_AUTH = "AUTH=true; Path=/; HttpOnly";
static const char* const COOKIE_CLEAR = "AUTH=; Path=/; Max-Age=0; HttpOnly";
static const char* const HTTPD_PREFIX = "/httpd";

// ===== GLOBAL STATE =====
WebServerLite webServer(80);
#if defined(IS_DNS)
DNSServer dnsServer;
#endif

String httpStatus;
String strResponse;
bool isAuthenticatedFlag = false;
bool isFirstPlay = true;

// ===== DOCUMENT POOL =====
JsonDocument jsonDoc;

uint8_t httpActivity     = 1;
uint8_t httpLoopPriority = 2;
uint32_t httpLoopTimeout = 100;

// ======================================================================
// SECTION 0: Основная задача обработки цикла сервера
// ======================================================================
bool setHttpActivity(){

// Проверяем текущий уровень активности

   uint8_t _activity = httpActivity;
   uint32_t _idle    = webServer.getIdleTime();
   if( _idle > 30000 )_activity = 0;
   else if( _idle > 2000 )_activity = 1;
   else _activity = 2;

// Проверяем, изменился ли уровень активности
   if( _activity != httpActivity ){
      httpActivity = _activity; 
      switch( _activity ){
         case 0: 
            httpLoopPriority = 0;
            httpLoopTimeout  = 1000;
            break;
         case 1: 
            httpLoopPriority = 2;
            httpLoopTimeout  = 100;
            break;
         case 2: 
            httpLoopPriority = 4;
            httpLoopTimeout  = 20;
            break;           
      }
      LOG_DEBUGLN("HTTPD loop Change Activity %d %d %d", (int)uxTaskPriorityGet(NULL), (int)httpLoopPriority, (int)httpLoopTimeout);
      return true;
   }

   return false;

}




/**
 * Задача работы с MP3
 * @param pvParameters
 */
void taskHttpServer(void *pvParameters) {
   LOG_INFOLN("Starting loop HTTPS server %d",(int)uxTaskPriorityGet(NULL));
//   HTTPD_start();
   while (true) {
      if( setHttpActivity() ){
         vTaskPrioritySet(NULL, httpLoopPriority);
      }
      HTTPD_loop();      
      vTaskDelay(httpLoopTimeout);
   }
}

// ======================================================================
// SECTION 1: INITIALIZATION AND MAIN LOOP
// ======================================================================

void HTTPD_start() {
    LOG_INFOLN("Starting HTTP server...");
    
    #if defined(IS_DNS)
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(53, "*", WiFi.softAPIP());
        LOG_INFOLN("DNS server started on port 53");
    #endif
    
    HTTPD_begin();
    webServer.begin();
    LOG_INFOLN("HTTP server started successfully");
}

void HTTPD_begin() {
    LOG_INFOLN("Configuring HTTP routes...");
    
    webServer.on("/save", HTTP_POST, handleSave);
    webServer.on("/cmd", HTTP_POST, handleCommand);
    webServer.on("/config", HTTP_GET, handleConfigSelector);
    webServer.on("/header", HTTP_GET, handleHeader);
    webServer.on("/tail", HTTP_GET, handleTail);
    webServer.on("/distance", HTTP_GET, handleDistance);
    webServer.on("/response", HTTP_GET, handleResponse);
    webServer.on("/auth", HTTP_POST, handleAuth);
    webServer.on("/update", HTTP_POST, []() {}, handleFirmwareUpload);
    
    API_fsBegin();
    
    webServer.onNotFound(handleFile);
    
    const char* headerKeys[] = {"User-Agent", "Cookie"};
    webServer.collectHeaders(headerKeys, 2);
    
    LOG_INFOLN("HTTP routes configured");
}

void HTTPD_loop() {
    #if defined(IS_DNS)
        dnsServer.processNextRequest();
    #endif
    webServer.handleClient();
}

void HTTPD_stop() {
    #if defined(IS_DNS)
        dnsServer.stop();
        LOG_INFOLN("DNS server stopped");
    #endif
    webServer.stop();
    LOG_INFOLN("HTTP server stopped");
}

// ======================================================================
// SECTION 2: STATUS AND RESPONSE MANAGEMENT
// ======================================================================

void httpSetStatus(const String& s) {
    httpStatus = s;
    LOG_INFOLN("Status set: %s", s.c_str());
}

void HTTP_sendResponse(const String& json) {
    if (json.isEmpty() || json == "{}") return;
    strResponse = json;
    LOG_DEBUGLN("Response queued: %s", json.c_str());
}

void handleResponse() {
    if (strResponse.isEmpty()) {
        webServer.send(204);
        return;
    }
    
    LOG_INFOLN("Sending: %s", strResponse.c_str());
    webServer.send(200, CONTENT_TYPE_JSON, strResponse);
    strResponse.clear();
}

// ======================================================================
// SECTION 3: AUTHENTICATION
// ======================================================================

bool isAuthenticated() {
    if (!webServer.hasHeader("Cookie")) {
        return false;
    }
    
    const String& cookie = webServer.header("Cookie");
    return cookie.indexOf("AUTH=true") >= 0;
}

void handleAuth() {
    isAuthenticatedFlag = false;
    
    const String& cmd = webServer.arg("cmd");
    
    if (cmd == "LOGOUT") {
        LOG_INFOLN("User logged out");
        webServer.sendHeader("Set-Cookie", COOKIE_CLEAR);
        webServer.send(200, CONTENT_TYPE_JSON, "{\"result\":\"logout\"}");
        return;
    }
    
    printArg();
    const String& password = webServer.arg("password");
    
    String pass1 = config["config2"]["ESP_PASS1"].as<String>();
    String pass2 = config["config2"]["ESP_PASS2"].as<String>();
    String pass3 = config["config2"]["ESP_PASS3"].as<String>();
    
    if (password == pass1 || password == pass2 || password == pass3) {
        isAuthenticatedFlag = true;
        LOG_INFOLN("Authentication successful");
        webServer.sendHeader("Set-Cookie", COOKIE_AUTH);
        webServer.send(200, CONTENT_TYPE_JSON, "{\"result\":\"ok\"}");
    } else {
        LOG_ERRORLN("Authentication failed");
        webServer.send(401, CONTENT_TYPE_JSON, "{\"result\":\"bad_password\"}");
    }
}

bool needAuth(const String& contentType) {
    return contentType.startsWith("text/html") ||
           contentType.startsWith("application/json") ||
           contentType.startsWith("text/plain");
}

// ======================================================================
// SECTION 4: FILE SERVING
// ======================================================================

String getContentType(const String& path) {
    if (path.endsWith(".html") || path.endsWith(".htm")) return CONTENT_TYPE_HTML;
    if (path.endsWith(".json")) return CONTENT_TYPE_JSON;
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    if (path.endsWith(".ico"))  return "image/x-icon";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".gif"))  return "image/gif";
    if (path.endsWith(".txt"))  return CONTENT_TYPE_TEXT;
    if (path.endsWith(".woff2")) return "font/woff2";
    if (path.endsWith(".woff")) return "font/woff";
    
    return "application/octet-stream";
}

void handleFile() {
    if (HTTP_redirect()) return;
    
    httpStatus.clear();
    String uri = webServer.uri();
    if (uri == "/") {
        uri = "/index.html";
    }
    
    HTTP_file(uri);
}

void HTTP_file(const String& uri) {
    String path = String(HTTPD_PREFIX) + uri;
    String contentType = getContentType(path);
    
    LOG_DEBUGLN("Requested file: %s [%s]", path.c_str(), contentType.c_str());
    
    bool isLoginPage = (uri == "/login.html" || uri == "/auth");
    if (!isLoginPage && needAuth(contentType) && !isAuthenticated()) {
        LOG_INFOLN("Redirecting to login page");
        webServer.sendHeader("Location", "/login.html");
        webServer.send(302, CONTENT_TYPE_TEXT, "");
        return;
    }
    
    File file = LittleFS.open(path, "r");
    if (!file || file.isDirectory()) {
        HTTP_notfound(path);
        LOG_ERRORLN("File not found: %s", path.c_str());
        return;
    }
    
    LOG_DEBUGLN("Serving file: %s (%d bytes)", path.c_str(), file.size());
    webServer.streamFile(file, contentType);
    file.close();
}

void HTTP_notfound(const String& path) {
    String html;
    html.reserve(300);
    html += "<!doctype html><html><head><meta charset='utf-8'>";
    html += "<link rel=\"stylesheet\" href=\"/style.css\">";
    html += "<title>404</title></head><body>";
    html += "<div class=\"main\"><h2>";
    html += path;
    html += " - 404 Not Found</h2></div></body></html>";
    
    webServer.send(404, CONTENT_TYPE_HTML, html);
}

bool HTTP_redirect() {
    String serverLoc = webServer.client().localIP().toString();
    
    if (serverLoc != webServer.hostHeader()) {
        LOG_INFOLN("Redirect: %s -> %s", webServer.hostHeader().c_str(), serverLoc.c_str());
        webServer.sendHeader("Location", String("http://") + serverLoc, true);
        webServer.send(302, CONTENT_TYPE_TEXT, "");
        webServer.client().stop();
        return true;
    }
    return false;
}

// ======================================================================
// SECTION 5: CONFIGURATION MANAGEMENT
// ======================================================================

void handleConfigSelector() {
    String configFile = config_selector[activeConfig]["file"].as<String>();
    LOG_INFOLN("Serving config file: %s", configFile.c_str());
    
    String fullPath = String(HTTPD_PREFIX) + configFile;
    if (!LittleFS.exists(fullPath)) {
        LOG_ERRORLN("Config file not found, resetting to default");
        configSetDefault();
    }
    
    HTTP_file(configFile);
}

void handleSave() {
    if (!webServer.hasArg("mode")) {
        webServer.send(200, CONTENT_TYPE_JSON, "{\"status\":\"Error: missing mode\"}");
        return;
    }
    
    const String& mode = webServer.arg("mode");
    
    if (mode == "save") {
        systemMP3((char*)"89", 83, PRIORITY_MP3_MEDIUM);
        saveConfigData();
        LOG_INFOLN("Configuration saved");
    }
    
    printArg();
    webServer.send(200, CONTENT_TYPE_JSON, "{\"status\":\"ok\"}");
}

void saveConfigData() {
    if (!webServer.hasArg("name") || !webServer.hasArg("data")) {
        LOG_ERRORLN("Missing name or data in save request");
        return;
    }
    
    const String& sectionName = webServer.arg("name");
    const String& jsonData = webServer.arg("data");
    
    String configFileName = config_selector[activeConfig]["file"].as<String>();
    String filePath = String(HTTPD_PREFIX) + configFileName;
    
    LOG_INFOLN("Saving section '%s' to: %s", sectionName.c_str(), filePath.c_str());
    
    JsonDocument docConfig;
    DeserializationError err = deserializeJson(docConfig, jsonData);
    if (err) {
        LOG_ERRORLN("JSON parse error: %s", err.c_str());
        return;
    }
    
    if (docConfig.containsKey(sectionName)) {
        JsonObject sourceSection = docConfig[sectionName].as<JsonObject>();
        JsonObject targetSection = config[sectionName].as<JsonObject>();
        
        for (JsonPair kv : sourceSection) {
            targetSection[kv.key()] = kv.value();
            LOG_DEBUGLN("Updated %s.%s", sectionName.c_str(), kv.key().c_str());
        }
    } else {
        copyJson(docConfig, config);
    }
    
    configWrite();
}

// ======================================================================
// SECTION 6: COMMAND HANDLER
// ======================================================================

void handleCommand() {
    LOG_INFOLN("Command received");
    printArg();
    
    int httpCode = 200;
    const String& cmd = webServer.arg("cmd");
    String output;
    output.reserve(512);
    
    if (cmd == "wifi_list") {
        output = WiFi_ScanNetwork();
    }
    else if (cmd == "calibrate") {
        systemMP3((char*)"89", 85, PRIORITY_MP3_HIGH);
        startCalibrate(0, (char*)"97", 97, CM_WAIT_WAIT);
    }
    else if (cmd == "play_once") {
        if (isFirstPlay) {
            HTTP_playMP3();
        }
        isFirstPlay = false;
    }
    else if (cmd == "play") {
        HTTP_playMP3();
    }
    else if (cmd == "default") {
        LOG_INFOLN("Resetting to factory defaults");
        configSetDefault();
        systemMP3((char*)"89", 91, PRIORITY_MP3_MAXIMAL);
        waitMP3andReboot();
    }
    else if (cmd == "reboot") {
        LOG_INFOLN("System reboot requested");
        systemMP3((char*)"89", 86, PRIORITY_MP3_MAXIMAL);
        waitMP3andReboot();
    }
    else if (cmd == "config") {
        const String& configName = webServer.arg("config");
        if (!configName.isEmpty()) {
            config_selector["active"] = configName;
            if (writeJson(CONFIG_SELECTOR_PATH, config_selector)) {
                setActiveConfig();
                int mp3num = config_selector[activeConfig]["mp3_num"].as<int>();
                systemMP3((char*)"89", mp3num, PRIORITY_MP3_HIGH);
                LOG_INFOLN("Active config changed to: %s", configName.c_str());
            }
            configRead();
        } else {
            httpCode = 400;
        }
    }
    else if (cmd == "config_name") {
        output = "{\"value\":\"Тестовая конфигурация\"}";
    }
    
    LOG_DEBUGLN("Command response: [%d] %s", httpCode, output.c_str());
    webServer.send(httpCode, CONTENT_TYPE_JSON, output);
}

// ======================================================================
// SECTION 7: UI COMPONENTS
// ======================================================================

void handleHeader() {
    String out;
    out.reserve(400);
    
    out += "<p><img src=/img/logo1.png>\n";
    out += "<hr align=\"left\" width=\"600\">\n";
    out += "<p><b>ID:";
    out += strID;
    out += " &nbsp;&nbsp;&nbsp;S/N: ";
    out += serNo;
    out += " &nbsp;&nbsp;&nbsp;VER: ";
    out += SOFTWARE_V;
    out += "</b></p>\n";
    
    if (WiFi.status() == WL_CONNECTED) {
        out += "<p><b>WiFi: ";
        out += WiFi.SSID();
        out += " RSSI: ";
        out += WiFi.RSSI();
        out += " dBm</b></p>\n";
    } else {
        out += "<p>WiFi: Not connected</p>\n";
    }
    
    webServer.send(200, CONTENT_TYPE_TEXT, out);
}

void handleTail() {
    String out;
    out.reserve(200);
    out += "<hr align=\"left\" width=\"600\">\n";
    out += "<p class=\"t1\">Copyright (C) Miller-Ti, A.Shikharbeev, 2026&nbsp;&nbsp;&nbsp;&nbsp;Made from Russia\n";
    
    webServer.send(200, CONTENT_TYPE_TEXT, out);
}

void handleDistance() {
    String out;
    out.reserve(100);
    out += "Расстояние от датчика до препятствия сейчас (мм): ";
    out += static_cast<int>(Distance);
    
    webServer.send(200, CONTENT_TYPE_TEXT, out);
}

// ======================================================================
// SECTION 8: FIRMWARE UPDATE
// ======================================================================

void handleFirmwareUpload() {
    HTTPUploadLite& upload = webServer.upload();
    
    switch (upload.status) {
        case UPLOAD_FILE_START: {
            if (!upload.filename.endsWith(".bin")) {
                LOG_ERRORLN("Invalid firmware file type: %s", upload.filename.c_str());
                systemMP3((char*)"89", 87, PRIORITY_MP3_MEDIUM);
                webServer.send(400, CONTENT_TYPE_TEXT, "Only .bin files allowed");
                return;
            }
            
            LOG_INFOLN("Firmware update started: %s (%u bytes)", 
                      upload.filename.c_str(), upload.totalSize);
            
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            } else {
                systemMP3((char*)"89", 89, PRIORITY_MP3_MEDIUM);
            }
            break;
        }
        
        case UPLOAD_FILE_WRITE: {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
            break;
        }
        
        case UPLOAD_FILE_END: {
            if (Update.end(true)) {
                LOG_INFOLN("Firmware update successful");
                webServer.send(200, CONTENT_TYPE_TEXT, "OK reboot");
                systemMP3((char*)"89", 88, PRIORITY_MP3_MEDIUM);
                waitMP3andReboot();
            } else {
                Update.printError(Serial);
                LOG_ERRORLN("Firmware update failed");
                webServer.send(500, CONTENT_TYPE_TEXT, "Update failed");
                systemMP3((char*)"89", 87, PRIORITY_MP3_MEDIUM);
            }
            break;
        }
        
        case UPLOAD_FILE_ABORTED: {
            Update.abort();
            LOG_ERRORLN("Firmware update aborted");
            break;
        }
    }
}

// ======================================================================
// SECTION 9: UTILITY FUNCTIONS
// ======================================================================

String WiFi_ScanNetwork() {
    LOG_INFOLN("Scanning WiFi networks...");
    
    int n = WiFi.scanNetworks();
    if (n <= 0) {
        LOG_ERRORLN("No networks found");
        return "[]";
    }
    
    int* indices = new int[n];
    for (int i = 0; i < n; i++) {
        indices[i] = i;
    }
    
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                std::swap(indices[i], indices[j]);
            }
        }
    }
    
    jsonDoc.clear();
    JsonArray array = jsonDoc.to<JsonArray>();
    
    for (int i = 0; i < n; i++) {
        JsonObject obj = array.add<JsonObject>();
        String ssid = WiFi.SSID(indices[i]);
        obj["value"] = ssid;
        obj["name"] = ssid + "[" + String(WiFi.RSSI(indices[i])) + "dB]";
    }
    
    String result;
    serializeJson(jsonDoc, result);
    
    delete[] indices;
    
    LOG_DEBUGLN("WiFi scan result: %d networks", n);
    return result;
}

void HTTP_playMP3() {
    if (!webServer.hasArg("name")) {
        LOG_ERRORLN("Missing 'name' argument for MP3 playback");
        return;
    }
    
    int num = webServer.arg("name").toInt();
    const String& arg1 = webServer.hasArg("arg1") ? webServer.arg("arg1") : "";
    
    int dir = (num < 20) ? MP3_BASE_DIR : MP3_ADD_DIR;
    int priority = PRIORITY_MP3_MEDIUM;
    
    if (webServer.hasArg("arg2")) {
        priority = webServer.arg("arg2").toInt();
    }
    
    LOG_DEBUGLN("Playing MP3: dir=%d, num=%d, priority=%d", dir, num, priority);
    
    if (arg1.isEmpty()) {
        playMP3(dir, num, priority);
    } else {
        char* arg1_ptr = const_cast<char*>(arg1.c_str());
        systemMP3(arg1_ptr, num, priority);
    }
}

void printArg() {
    int count = webServer.args();
    for (int i = 0; i < count; i++) {
        LOG_DEBUGLN("Arg[%d]: %s = %s", 
                   i, 
                   webServer.argName(i).c_str(), 
                   webServer.arg(i).c_str());
    }
}