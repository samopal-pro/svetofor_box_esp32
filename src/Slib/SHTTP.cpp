/**
 * 
 * HTTP файловый сервер. Backend 
 * Frondend находится в файлах fs.html, update.html, update.css, httpd.js, update.js
 * Для инииализации необходимо вставить функцию API_fsBegin() там где вызываются функции webServer.on() 
 * 
 * Copyright (C) 2026 Шихарбеев Алексей 
 */


#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
//#include <Update.h>

#include "SHTTP.h"

extern WebServer webServer;

File uploadFile;



/**
 * Возвращает список файлов и каталогов
 * GET /api/list?path=/httpd
 */
void handle_fsList() {
    String path = webServer.arg("path");
    if (path.isEmpty()) path = "/httpd";

    File dir = LittleFS.open(path);
    if (!dir || !dir.isDirectory()) return webServer.send(404, "text/plain", "Directory not found");

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
        JsonObject obj = arr.add<JsonObject>();
        String full = file.path();
        obj["name"] = full.substring(full.lastIndexOf('/') + 1);
        obj["path"] = full;
        obj["dir"] = file.isDirectory();
        if (!file.isDirectory()) obj["size"] = file.size();
    }

    String json;
    serializeJson(doc, json);
    webServer.send(200, "application/json", json);
}

/**
 * Возвращает содержимое файла
 * GET /api/file?name=/httpd/test.txt
 */
void handle_fsFile() {
    String file = webServer.arg("name");
    if (file.isEmpty()) return webServer.send(400, "text/plain", "File name required");
    if (!LittleFS.exists(file)) return webServer.send(404, "text/plain", "File not found");

    File f = LittleFS.open(file, "r");
    if (!f) return webServer.send(500, "text/plain", "Open failed");

    webServer.send(200, "text/plain; charset=utf-8", f.readString());
    f.close();
}

/**
 * Удаляет файл
 * GET /api/delete?file=/httpd/test.txt
 */
void handle_fsDelete() {
    String file = webServer.arg("file");
    if (file.isEmpty()) return webServer.send(400, "text/plain", "File name required");
    if (!LittleFS.exists(file)) return webServer.send(404, "text/plain", "File not found");
    if (!LittleFS.remove(file)) return webServer.send(500, "text/plain", "Delete failed");
    webServer.send(200, "text/plain", "OK");
}

/**
 * Создает пустой файл
 * POST /api/create
 */
void handle_fsCreate() {
    String file = webServer.arg("file");
    if (file.isEmpty()) return webServer.send(400, "text/plain", "File name required");
    if (LittleFS.exists(file)) return webServer.send(409, "text/plain", "File already exists");

    File f = LittleFS.open(file, "w");
    if (!f) return webServer.send(500, "text/plain", "Create failed");

    f.close();
    webServer.send(200, "text/plain", "OK");
}

/**
 * Сохраняет файл
 * POST /api/save
 */
void handle_fsSave() {
    String file = webServer.arg("file");
    if (file.isEmpty()) return webServer.send(400, "text/plain", "File name required");

    File f = LittleFS.open(file, "w");
    if (!f) return webServer.send(500, "text/plain", "Open failed");

    f.print(webServer.arg("data"));
    f.close();

    webServer.send(200, "text/plain", "OK");
}

/**
 * Скачивание файла
 * GET /download?file=/httpd/test.txt
 */
void handle_fsDownload() {
    String file = webServer.arg("file");
    if (file.isEmpty()) return webServer.send(400, "text/plain", "File name required");
    if (!LittleFS.exists(file)) return webServer.send(404, "text/plain", "File not found");

    File f = LittleFS.open(file, "r");
    if (!f) return webServer.send(500, "text/plain", "Open failed");

    String name = file.substring(file.lastIndexOf('/') + 1);
    webServer.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    webServer.streamFile(f, "application/octet-stream");
    f.close();
}

/**
 * Загрузка файла
 * POST /api/upload
 */
void handle_fsUpload(){

    HTTPUpload& upload=webServer.upload();

//    static String basePath;

//    if(webServer.hasArg("path"))basePath=webServer.arg("path");

//    if(basePath.isEmpty())basePath="/httpd";
    switch(upload.status){

        case UPLOAD_FILE_START:{

            String fullPath=upload.filename;
//            Serial.printf("!!! upload: %s %s\n",basePath.c_str(),fullPath.c_str());
//            printArg();
            uploadFile=LittleFS.open(fullPath,"w");
           }break;

        case UPLOAD_FILE_WRITE:
            if(uploadFile)uploadFile.write(upload.buf,upload.currentSize);
            break;

        case UPLOAD_FILE_END:
        case UPLOAD_FILE_ABORTED:
            if(uploadFile) uploadFile.close();
            break;

        default:
            break;
    }
}




void API_fsBegin(){
    webServer.on("/api/list", HTTP_GET, handle_fsList);
    webServer.on("/api/file", HTTP_GET, handle_fsFile);
    webServer.on("/api/delete", HTTP_GET, handle_fsDelete);
    webServer.on("/api/create", HTTP_POST, handle_fsCreate);
    webServer.on("/api/save", HTTP_POST, handle_fsSave);
    webServer.on("/download", HTTP_GET, handle_fsDownload);
    webServer.on("/api/upload", HTTP_POST, [](){ webServer.send(200, "text/plain", "OK"); }, handle_fsUpload);

}