/**
* HTTP server
* 
* Copyright (C) 2024 Alexey Shikharbeev
* http://samopal.pro
*/
#include "WC_HTTP.h"
#include "src/Slib/SHTTP.h"

//#include <vector>
File updateFile;

JsonDocument doc;

String httpStatus = "";
String strResponse = "";;

bool is_auth = false;
#if defined(IS_DNS)
DNSServer dnsServer;
#endif
WebServer webServer(80);
String Content;
bool is_play_once = true;

//SHTTP http(&webServer);

void httpSetStatus(const String& s) {
  httpStatus = s;
  Serial.print("!!! Set http status ");
  Serial.println(s);
}

/**
 * Запуск HTTP сервера
 * 
 */
void HTTPD_start(){
// Стартуем DNS сервер   
#if defined(IS_DNS)
   dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

   dnsServer.start(53, "*", WiFi.softAPIP());
#endif
// Стартуем WEBсервер



   HTTPD_begin();
   webServer.begin();
   Serial.println(F("!!! Start HTTPD"));

 
// Запускаем цикл
}

void HTTPD_begin(){


  webServer.on("/save", HTTP_POST, handleSave);
  webServer.on("/cmd", HTTP_POST, handleCommand);
  webServer.on("/config", HTTP_GET, handleConfigSelector);
//  webServer.on("/channels", HTTP_GET, handleChannels);  
  webServer.on("/header", HTTP_GET, handleHeader );
  webServer.on("/tail", HTTP_GET, handleTail );
  webServer.on("/distance", HTTP_GET, handleDistance );
  webServer.on("/response", HTTP_GET,handleResponse);
//  webServer.on("/status", HTTP_GET, []() {  webServer.send(200, "text/plain; charset=utf-8", httpStatus); });
  webServer.on("/auth", HTTP_POST, handleAuth);
  webServer.on("/update",HTTP_POST,[]{},handle_firmwareUpdate);

  API_fsBegin(); 
//  webServer.on("/upload",HTTP_POST,  []() { webServer.send(200, "text/plain", "OK"); },  handleFileUpload);


/*
  webServer.on("/update",HTTP_POST,  []() {
    webServer.sendHeader("Connection", "close");
    webServer.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    if (!Update.hasError()) {
      ESP.restart();
    }
  },  handleFirmwareUpload );
  */
  webServer.onNotFound(handleFile); 
   const char * headerkeys[] = {"User-Agent","Cookie"} ;
   size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
   webServer.collectHeaders(headerkeys, headerkeyssize );


  webServer.begin();
}

/**
* Формируем строку с ответом
*/
void HTTP_setResponse(String _msg, String _cmd){
   if( _msg == "" && _cmd == "" ){
      strResponse = "";
      return;
   }
   doc.clear();
   doc["msg"] = _msg;
   doc["cmd"] = _cmd;
   serializeJson(doc, strResponse);
}

/**
 * Обработчик события на URL /response
*/
void handleResponse(){
   if( strResponse == "" ){
// Нет команды, возвращаем пустой ответ      
      webServer.send(204);
   }
   else {
// Посылаем команду и сбрасываем строку после отправки
      Serial.print("!!! Response: ");
      Serial.println(strResponse);
      webServer.send(200, "application/json", strResponse);
      strResponse = "";
   }
}


void handleCommand(){
   Serial.println("!!! /cmd:");
   printArg();
   int ret = 200;
   String cmd = webServer.arg("cmd"); 
   String out = "";
   if( cmd == "wifi_list" ){ out = WiFi_ScanNetwork(); }
   else if( cmd == "ap_name" ){ 
      out = "{\"value\": \"";
      out += deviceName();
      out += "\"}";  
   }
   else if( cmd == "calibrate"){
       systemMP3("89",85,PRIORITY_MP3_HIGH);
       startCalibrate(0,"97",97,CM_WAIT_WAIT);
   }
   else if( cmd == "play_once" ){ 
      if( is_play_once ) HTTP_playMP3();
      is_play_once = false; 
   }
   else if( cmd == "play" ){ 
      HTTP_playMP3(); 
      }
   else if( cmd == "default" ){      
      configSetDefault();  
      systemMP3("89",91,PRIORITY_MP3_MAXIMAL);
      waitMP3andReboot();
      }
   else if( cmd == "reboot" ){ 
      systemMP3("89",86,PRIORITY_MP3_MAXIMAL);
      waitMP3andReboot();

//      ESP.restart();  
      }
   else if( cmd == "config" ){
      String arg = webServer.arg("config");
      if( arg!= "" ){
         config_selector["active"] = arg;
         if(writeJson(CONFIG_SELECTOR,config_selector)){
            setActiveConfig();
            systemMP3("89",config_selector[activeConfig]["mp3_num"].as<int>(),PRIORITY_MP3_HIGH);
         }
         configRead();
      }      
      else ret = 400;
   }
   else if( cmd == "config_name" ){ out ="{\"value\":\"Тестовая конфигурация\"}"; }
   Serial.println(out);
   webServer.send(ret, "application/json", out);   
}

/**
* Выдача текущего файла конфигупации
*/
void handleConfigSelector(){
//   Serial.println("!!! Config selector");
//   printArg();
   String file = config_selector[activeConfig]["file"].as<String>();
   String path = "/httpd"+file;
   File f = LittleFS.open(path, "r");
   if (!f )configSetDefault();
   close(f);
   HTTP_file(file);

}


/**
* Выдача заголовка для всех страниц
*/
void handleHeader(){
   String out;
//   out += "<h2><img src=/img/logo.png> ДАТЧИК ПРИСУТСТВИЯ</h2>\n";
   out += "<p><img src=/img/logo1.png>\n";
   out += "<hr align=\"left\" width=\"600\">\n";
   out += "<p><b>ID:";
   out += strID;
   out += " &nbsp;&nbsp;&nbsp;S/N: ";
   out += serNo;
   out += " &nbsp;&nbsp;&nbsp;VER: :";
   out += SOFTWARE_V;
   out += "</b></p>\n";
//   if( is_auth )out += "<p>Права администратора</p>\n";
   if( WiFi.status() == WL_CONNECTED ){
      out += "<p><b>WiFi: ";
      out += WiFi.SSID();
      out += " RSSI: ";
      out += WiFi.RSSI();
      out += "</b></p>\n";
   }
   else {
      out += "<p>WiFi Не подключен</p>\n";

   } 
//     out += "<p>Дата: 30.05.2077 12:00:00\n";
//   Serial.println(F("!!! Header"));
   webServer.send(200, "text/plain; charset=utf-8", out);   
}

void handleTail(){
   String out;
   out += "<hr align=\"left\" width=\"600\">\n";
   out += "<p class=\"t1\">Copyright (C) Miller-Ti, A.Shikharbeev, 2026&nbsp;&nbsp;&nbsp;&nbsp;Made from Russia\n";
//   Serial.println(F("!!! Tail"));
   webServer.send(200, "text/plain; charset=utf-8", out);   
}

void handleDistance(){
   String out;
   out += "Расстояние от датчика до препятствия сейчас (мм): ";
   out += (int)Distance;
   webServer.send(200, "text/plain; charset=utf-8", out);   
}


void HTTPD_loop(){
#if defined(IS_DNS)
   dnsServer.processNextRequest();   
#endif
   webServer.handleClient(); 
}

void HTTPD_stop(){
#if defined(IS_DNS)
   dnsServer.stop();
#endif
   webServer.stop();
#if defined(DEBUG_SERIAL)
   Serial.println(F("!!! Stop HTTPD"));
#endif
}

void HTTP_notfound(String path){
   webServer.send(
      404,
      "text/html; charset=utf-8",
      "<!doctype html>"
      "<html><head><meta charset='utf-8'>"
      "<link rel=\"stylesheet\" href=\"/style.css\">"
      "<title>404</title></head>"
      "<body>"
      "<div class=\"main\">"
      "<h2>"+ path +" 404 - File not found</h2>"
      "</div></body></html>"
   );
   
}

String getContentType(const String& path) {
    if (path.endsWith(".html") || path.endsWith(".htm")) return "text/html; charset=utf-8";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".jpg"))  return "image/jpeg";
    if (path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".gif"))  return "image/gif";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    if (path.endsWith(".ico"))  return "image/x-icon";
    if (path.endsWith(".txt"))  return "text/plain; charset=utf-8";
    if (path.endsWith(".woff")) return "font/woff";
    if (path.endsWith(".woff2")) return "font/woff2";

    return "application/octet-stream";
}

void handleFile() {
    if( HTTP_redirect() )return;


    httpStatus = "";
    String uri = webServer.uri();
    if (uri == "/") uri = "/index.html";
    HTTP_file(uri);
}

void HTTP_file(String uri){
    String path = "/httpd" + uri;

// login.html всегда доступна
    bool isLoginPage =  (uri == "/login.html") || (uri == "/auth");
    String type = getContentType(path);

    Serial.printf("!!! file=%s type=%s ",path.c_str(),type.c_str());

//    printHeaders();
 // авторизация только для html/json/txt
    if (!isLoginPage && needAuth(type) && !isAuth()) {
        webServer.sendHeader("Location", "/login.html");
        webServer.send(302, "text/plain", "");
        Serial.println("LOGIN");
        return;
    }

    File file = LittleFS.open(path, "r");
    if (!file || file.isDirectory()) {
        HTTP_notfound(path);
        Serial.println(F("NOT FOUND"));
        return;
    }
    Serial.println(F("OK"));
    webServer.streamFile(file, type);
    file.close();
}



/** 
* Редирект с любого домена на главную страницу сервера
* 
* @return - true если сработал редирект
* @return false - редирект не сработал
*/
bool HTTP_redirect() {
//  DEBUG_WM(DEBUG_DEV,"-> " + server->hostHeader());
  
//  if(!_enableCaptivePortal) return false; // skip redirections, @todo maybe allow redirection even when no cp ? might be useful
  
  String serverLoc =  webServer.client().localIP().toString();
  
  if (serverLoc != webServer.hostHeader() ) {
#if defined(DEBUG_SERIAL)
    Serial.print(F("HTTPD: redirect "));
    Serial.print(webServer.hostHeader());
    Serial.print(F(" to "));
    Serial.println(serverLoc);  
#endif    
    webServer.sendHeader(F("Location"), (String)F("http://") + serverLoc, true); // @HTTPHEAD send redirect
    webServer.send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    webServer.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}


void handleSave() {

  if( !webServer.hasArg("mode") ){
     webServer.send(200, "application/json", "{\"status\":\"Error data\"}"); 
     return;     
  }

  String mode = webServer.arg("mode");
  String name = webServer.arg("name");
/*
  if( mode == "cmd" ){
       Serial.printf("!!! CMD %s\n",name.c_str());
       if(name=="Reboot"){
          httpSetStatus("Перезагрузка контроллера через 5 сек ...");
       }
       else if(name=="Default"){
          httpSetStatus("Сброс всех значений на заводские. Страница обновится через 5 сек...");
       }
  }
  else */if( mode == "save" ){
     systemMP3("89",83,PRIORITY_MP3_MEDIUM);
     saveConfig();
 
  }

  printArg();


  webServer.send(200, "application/json", "{\"status\":\"ok\"}");
}


void saveConfig(){
   String file = "/httpd"+config_selector[activeConfig]["file"].as<String>();
   Serial.printf("!!! SAVE %s\n",file.c_str());

   if( !webServer.hasArg("name") )return;
   if( !webServer.hasArg("data") )return;

   String name = webServer.arg("name");
   String data = webServer.arg("data");

//   readJson(file.c_str(),doc);

    // Разбираем новую секцию config2
    JsonDocument docConfig2;
    DeserializationError err = deserializeJson(docConfig2, data);
    if (err) {
        Serial.println(err.c_str());
        return;
    }
    copyJson(docConfig2,config);
    configWrite();
    // Полностью заменяем объект config2
 //   doc[name.c_str()] = docConfig2.as<JsonObject>();

//    writeJson(file.c_str(),doc);
 
}


bool needAuth(const String& type){
//    Serial.printf("!!! NeedAuth = %s\n",type.c_str());
    return type.startsWith("text/html") ||
           type.startsWith("application/json") ||
           type.startsWith("text/plain");
}

bool isAuth(){
//    Serial.printf("!!! isdAuth\n");
    if (!webServer.hasHeader("Cookie"))return false;

    String cookie = webServer.header("Cookie");
//    Serial.printf("!!! isdAuth cookie = %s\n",cookie.c_str());
    return cookie.indexOf("AUTH=true") >= 0;
}

void handleAuth(){
    is_auth = false;
    String cmd = webServer.arg("cmd");
    // logout
    if (cmd == "LOGOUT") {
        Serial.println("!!! handleAuth LOGOUT");
        webServer.sendHeader("Set-Cookie", "AUTH=; Path=/; Max-Age=0; HttpOnly" );
        webServer.send(200, "application/json", "{\"result\":\"logout\"}");
        return;
    }
    printArg();
    String password = webServer.arg("password");
    if (password == config["config2"]["ESP_PASS1"].as<String>() ||
        password == config["config2"]["ESP_PASS2"].as<String>() ||
        password == config["config2"]["ESP_PASS3"].as<String>() )   {
        is_auth = true;
        Serial.println(F("!!! handleAuth OK"));
        webServer.sendHeader("Set-Cookie", "AUTH=true; Path=/; HttpOnly");
        webServer.send(200, "application/json", "{\"result\":\"ok\"}");
    }
    else {
         Serial.print(F("!!! handleAuth BAD_PASSPORT "));
         Serial.println(password);
         webServer.send(401, "application/json", "{\"result\":\"bad_password\"}");
    }
}

void printArg(){
  for (int i = 0; i < webServer.args(); i++) {
    Serial.printf("!!! Arg %d ",i);
    String name  = webServer.argName(i);
    String value = webServer.arg(i);

    Serial.print(name);
    Serial.print(" = ");
    Serial.println(value);
  }
}


void printHeaders(){
    Serial.println("=== HTTP Headers ===");
    int count = webServer.headers();
    for (int i = 0; i < count; i++){
        Serial.printf(
            "%s: %s\n",
            webServer.headerName(i).c_str(),
            webServer.header(i).c_str()
        );
    }
    Serial.println("====================");
}


String WiFi_ScanNetwork(){
   Serial.println("!!! Scan networks...");
   int n = WiFi.scanNetworks();
   int indices[n];
   String s;
   for (int i = 0; i < n; i++)indices[i] = i;

      // RSSI SORT
   for (int i = 0; i < n; i++) {
      for (int j = i + 1; j < n; j++) {
         if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
            std::swap(indices[i], indices[j]);
         }
      }
   }
   
   doc.clear();
   for (int i = 0; i < n; ++i) {
     
      String value = WiFi.SSID(indices[i]);
      int rssi = WiFi.RSSI(indices[i]);
      doc[i]["value"] = value;
      doc[i]["name"]  = value + "["+String(rssi)+"dB]";
   }
   serializeJson(doc, s); 
   Serial.print("!!! WiFi list: ");
   Serial.println(s);
   return s;
}

String deviceName(){
//    doc.clear();
    String s = "";
    if( strlen(serNo) >0 )s += serNo;
    else s += DEVICE_NAME_YEAR;
    s += DEVICE_NAM_SUFFIX;
//    s += "\" }";
    Serial.println(s);
    return s;
}

/**
* Проигрывание звукового файла по параметра запроса POST
*/
void HTTP_playMP3(){
   if( !webServer.hasArg("name") )return;
   int _num = webServer.arg("name").toInt();
   String arg1 = "";
   if( !webServer.hasArg("arg1") )arg1 = webServer.arg("arg1");
   String arg2 = "";
   if( !webServer.hasArg("arg2") )arg2 = webServer.arg("arg2");
   int _dir = MP3_BASE_DIR;
   if( _num < 20 )_dir = MP3_BASE_DIR;
   else _dir = MP3_ADD_DIR;
   int _priority = PRIORITY_MP3_MEDIUM;
   if( arg2 != "" )int _priority = arg2.toInt();
   if( arg1 == "" )playMP3(_dir, _num, _priority);
   else systemMP3((char *)arg1.c_str(),_num,_priority);
}

/**
* Обновление ESP из файла
*/
void handle_firmwareUpdate(){
    HTTPUpload& upload = webServer.upload();

    switch(upload.status){
        case UPLOAD_FILE_START:
            if(!upload.filename.endsWith(".bin")){
//                webServer.send(400,"text/plain","Only .bin allowed");
                Serial.printf("!!! Update error type\n");
                systemMP3("89",87,PRIORITY_MP3_MEDIUM);
    
                return;
            }
            Serial.printf("filename: %s\n", upload.filename.c_str());
            if(!Update.begin(UPDATE_SIZE_UNKNOWN))
                Update.printError(Serial);
            else {
               Serial.printf("!!! Update start %d\n",(int)upload.totalSize);
               systemMP3("89",89,PRIORITY_MP3_MEDIUM);
            }   
            break;

        case UPLOAD_FILE_WRITE:
            if(Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                Update.printError(Serial);
            break;

        case UPLOAD_FILE_END:
            if(Update.end(true)){
//                HTTP_setResponse("Прошивка успешно обновлена","home");
                webServer.send(200,"text/plain","OK reboot");             
                Serial.printf("!!! Update success\n");
                systemMP3("89",88,PRIORITY_MP3_MEDIUM);
//                waitMP3(5000);
//                systemMP3("89",86,PRIORITY_MP3_MAXIMAL);
                waitMP3andReboot();
            }
            else {
                Update.printError(Serial);
//                HTTP_setResponse("Ошибка обновления прошивки","home");
                webServer.send(500,"text/plain","Update failed");

                Serial.printf("!!! Update error\n");
                systemMP3("89",87,PRIORITY_MP3_MEDIUM);
            }
            break;

        case UPLOAD_FILE_ABORTED:
            Update.abort();
            break;
    }
}