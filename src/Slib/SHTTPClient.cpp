// ============================================
// Файл: src/Slib/SHTTPClient.cpp (дополненный)
// ============================================
#include "SHTTPClient.h"

HttpResponse SimpleHttpClient::GET(
    const char* host,
    uint16_t port,
    const String& path,
    const String& extraHeaders
) {
    WiFiClient client;
    HttpResponse response;
    if (!client.connect(host, port)) {
        return response;
    }
    client.printf(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n",
        path.c_str(),
        host
    );
    if (extraHeaders.length()) {
        client.print(extraHeaders);
    }
    client.print("\r\n");
    readResponse(client, response);
    return response;
}

HttpResponse SimpleHttpClient::POST(
    const char* host,
    uint16_t port,
    const String& path,
    const String& contentType,
    const String& payload,
    const String& extraHeaders
) {
    WiFiClient client;
    HttpResponse response;
    if (!client.connect(host, port)) {
        return response;
    }
    client.printf(
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %u\r\n",
        path.c_str(),
        host,
        contentType.c_str(),
        payload.length()
    );
    if (extraHeaders.length()) {
        client.print(extraHeaders);
    }
    client.print("\r\n");
    client.print(payload);
    readResponse(client, response);
    return response;
}

HttpResponse SimpleHttpClient::POST_JSON(
    const char* host,
    uint16_t port,
    const String& path,
    const String& json,
    const String& extraHeaders
) {
    return POST(host, port, path, "application/json", json, extraHeaders);
}

HttpResponse SimpleHttpClient::POST_TEXT(
    const char* host,
    uint16_t port,
    const String& path,
    const String& text,
    const String& extraHeaders
) {
    return POST(host, port, path, "text/plain", text, extraHeaders);
}

HttpResponse SimpleHttpClient::POST_STREAM(
    const char* host,
    uint16_t port,
    const String& path,
    const String& contentType,
    Stream& payloadStream,
    size_t contentLength,
    const String& extraHeaders
) {
    WiFiClient client;
    HttpResponse response;
    if (!client.connect(host, port)) {
        return response;
    }
    client.printf(
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %u\r\n",
        path.c_str(),
        host,
        contentType.c_str(),
        contentLength
    );
    if (extraHeaders.length()) {
        client.print(extraHeaders);
    }
    client.print("\r\n");
    uint8_t buffer[512];
    size_t bytesSent = 0;
    while (bytesSent < contentLength) {
        size_t toRead = min(sizeof(buffer), contentLength - bytesSent);
        size_t bytesRead = payloadStream.readBytes(buffer, toRead);
        if (bytesRead > 0) {
            client.write(buffer, bytesRead);
            bytesSent += bytesRead;
        } else {
            break;
        }
        yield();
    }
    readResponse(client, response);
    return response;
}

void SimpleHttpClient::readResponse(
    WiFiClient& client,
    HttpResponse& response
) {
    unsigned long timeout = millis();
    while (!client.available()) {
        if (millis() - timeout > 5000) {
            client.stop();
            return;
        }
        vTaskDelay(1);
    }
    String s = client.readString();
    parseHttpResponse(s, response);
    client.stop();
}

bool SimpleHttpClient::parseHttpResponse(const String& Str, HttpResponse& response) {
    response.statusCode = -1;
    response.headers = "";
    response.body = "";
    int firstLineEnd = Str.indexOf("\r\n");
    if (firstLineEnd == -1) {
        return false;
    }
    String statusLine = Str.substring(0, firstLineEnd);
    int firstSpace = statusLine.indexOf(' ');
    int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
    if (firstSpace != -1 && secondSpace != -1) {
        String codeStr = statusLine.substring(firstSpace + 1, secondSpace);
        response.statusCode = codeStr.toInt();
    } else if (firstSpace != -1) {
        String codeStr = statusLine.substring(firstSpace + 1);
        response.statusCode = codeStr.toInt();
    } else {
        return false;
    }
    int headersEnd = Str.indexOf("\r\n\r\n");
    if (headersEnd == -1) {
        headersEnd = Str.indexOf("\n\n");
        if (headersEnd == -1) {
            return false;
        }
        response.headers = Str.substring(firstLineEnd + 2, headersEnd);
        response.body = Str.substring(headersEnd + 2);
    } else {
        response.headers = Str.substring(firstLineEnd + 2, headersEnd);
        response.body = Str.substring(headersEnd + 4);
    }
    response.body.trim();
    return true;
}

bool SimpleHttpClient::parseHttpResponseShared(const String& Str, HttpResponse& response) {
    return parseHttpResponse(Str, response);
}

/**
* Выполняет GET запрос и возвращает клиент для стримингового чтения
*/
bool SimpleHttpClient::connectAndSendRequest(
   WiFiClient& client,
   const char* host,
   uint16_t port,
   const String& path,
   const String& extraHeaders
) {
   if (!client.connect(host, port)) {
      return false;
   }
   client.printf(
      "GET %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "Connection: close\r\n",
      path.c_str(),
      host
   );
   if (extraHeaders.length()) {
      client.print(extraHeaders);
   }
   client.print("\r\n");
   return true;
}

/**
* Выполняет GET запрос с возможностью стримингового чтения ответа
* Возвращает HttpResponse с телом, если isChunked == false
* Если isChunked == true, тело не читается, а клиент остается открытым
*/
HttpResponse SimpleHttpClient::GET_STREAM(
   const char* host,
   uint16_t port,
   const String& path,
   const String& extraHeaders,
   bool* isChunked
) {
   WiFiClient client;
   HttpResponse response;
   
   if (!connectAndSendRequest(client, host, port, path, extraHeaders)) {
      return response;
   }
   
   // Ожидаем ответ
   unsigned long timeout = millis();
   while (!client.available()) {
      if (millis() - timeout > 10000) {
         client.stop();
         return response;
      }
      vTaskDelay(1);
   }
   
   // Читаем статусную строку
   String statusLine = client.readStringUntil('\n');
   if (!statusLine.startsWith("HTTP/1.1 200")) {
      client.stop();
      return response;
   }
   
   // Парсим заголовки
   String line;
   int contentLength = -1;
   bool chunked = false;
   
   while ((line = client.readStringUntil('\n')) != "\r" && line != "\n") {
      line.trim();
      response.headers += line + "\r\n";
      
      if (line.startsWith("Content-Length:")) {
         contentLength = line.substring(line.indexOf(':') + 1).toInt();
      } else if (line.startsWith("Transfer-Encoding:") && line.indexOf("chunked") >= 0) {
         chunked = true;
      }
   }
   
   response.statusCode = 200;
   
   // Если запрошен chunked режим, возвращаем клиент для стриминга
   if (isChunked != nullptr) {
      *isChunked = chunked;
      // Читаем тело только для обычного режима
      if (!chunked && contentLength > 0) {
         response.body.reserve(contentLength);
         uint8_t buffer[512];
         size_t remaining = contentLength;
         while (remaining > 0 && client.available()) {
            size_t toRead = min(sizeof(buffer), remaining);
            size_t bytesRead = client.readBytes(buffer, toRead);
            if (bytesRead > 0) {
               response.body.concat((char*)buffer, bytesRead);
               remaining -= bytesRead;
            }
         }
      } else if (chunked) {
         // Для chunked режима тело не читаем, клиент остается открытым
         // Возвращаем пустое тело, но клиент все еще подключен
         response.body = "";
      }
   } else {
      // Стандартный режим - читаем все тело
      if (!chunked && contentLength > 0) {
         response.body.reserve(contentLength);
         uint8_t buffer[512];
         size_t remaining = contentLength;
         while (remaining > 0 && client.available()) {
            size_t toRead = min(sizeof(buffer), remaining);
            size_t bytesRead = client.readBytes(buffer, toRead);
            if (bytesRead > 0) {
               response.body.concat((char*)buffer, bytesRead);
               remaining -= bytesRead;
            }
         }
      } else if (chunked) {
         // Читаем chunked ответ
         while (client.connected()) {
            String chunkHeader = client.readStringUntil('\n');
            chunkHeader.trim();
            if (chunkHeader.isEmpty()) {
               continue;
            }
            int chunkSize = strtol(chunkHeader.c_str(), NULL, 16);
            if (chunkSize == 0) {
               break;
            }
            uint8_t* buffer = new uint8_t[chunkSize];
            size_t bytesRead = client.readBytes(buffer, chunkSize);
            if (bytesRead == chunkSize) {
               response.body.concat((char*)buffer, bytesRead);
            }
            delete[] buffer;
            client.readStringUntil('\n'); // пропускаем CRLF
         }
      }
   }
   
   client.stop();
   return response;
}