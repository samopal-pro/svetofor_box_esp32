// ============================================
// Файл: src/Slib/SHTTPClient.cpp (исправленный)
// ============================================
#include "SHTTPClient.h"

//*********************************************************************************************************************
// Конструктор/деструктор
//*********************************************************************************************************************

// Конструктор
SimpleHttpClient::SimpleHttpClient() {
    m_connected = false;
    m_chunkedMode = false;
    m_currentChunkSize = 0;
}

// Деструктор
SimpleHttpClient::~SimpleHttpClient() {
    stop();
}

// Инициализация клиента
bool SimpleHttpClient::begin() {
    stop();
    m_connected = false;
    m_chunkedMode = false;
    m_currentChunkSize = 0;
    return true;
}

// Закрывает соединение
void SimpleHttpClient::stop() {
    if (m_client.connected()) {
        m_client.stop();
    }
    m_connected = false;
    m_chunkedMode = false;
    m_currentChunkSize = 0;
}

//*********************************************************************************************************************
// GET запросы
//*********************************************************************************************************************

// ============================================
// Файл: src/Slib/SHTTPClient.cpp (добавить после блока GET запросы)
// ============================================

//*********************************************************************************************************************
// Стриминговые запросы
//*********************************************************************************************************************

/**
 * Отправляет GET запрос, читает статус и заголовки, оставляет соединение открытым для чтения тела
 */
bool SimpleHttpClient::GET_STREAM(
    const char* host,
    uint16_t port,
    const String& path,
    const String& extraHeaders,
    const uint32_t waitTM
) {
    resetResponse();
    begin();
    if (!m_client.connect(host, port)) {
        return false;
    }
//    Serial.printf("!!! GET %s:%d %s\n",host,port,path.c_str());
    m_connected = true;
    m_client.printf(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n",
        path.c_str(),
        host
    );
    if (extraHeaders.length()) {
        m_client.print(extraHeaders);
    }
    m_client.print("\r\n");

    if (!waitResponse(waitTM)) {
        Serial.println("!!! GET not response");
        stop();
        return false;
    }
    if (!readStatusCode()) {
        Serial.println("!!! GET not status");
        stop();
        return false;
    }
    if (!readHeader()) {
        Serial.println("!!! GET not header");
        stop();
        return false;
    }
    return true;
}



/**
 * Отправляет GET запрос и закрываеи соединение
 */
bool SimpleHttpClient::GET(
    const char* host,
    uint16_t port,
    const String& path,
    const String& extraHeaders,
    const uint32_t waitTM
) {

    bool ret = GET_STREAM(host,port,path,extraHeaders,waitTM);
//    Serial.printf("!!! GET body %d\n",(int)ret);
    if( ret  )ret = readBody();
    stop();
    return ret;
}

//*********************************************************************************************************************
// POST запросы
//*********************************************************************************************************************

// Выполняет POST запрос
bool SimpleHttpClient::POST(
    const char* host,
    uint16_t port,
    const String& path,
    const String& contentType,
    const String& payload,
    const String& extraHeaders,
    const uint32_t waitTM
) {
    resetResponse();
    begin();
    if (!m_client.connect(host, port)) {
        return false;
    }
    m_connected = true;
    m_client.printf(
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
        m_client.print(extraHeaders);
    }
    m_client.print("\r\n");
    m_client.print(payload);
    if (!waitResponse(waitTM)) {
        Serial.println("!?!?! Error wait");
        stop();
        return false;
    }
    if (!readStatusCode()) {
        Serial.println("!?!?! Error status");
        stop();
        return false;
    }
    if (!readHeader()) {
        Serial.println("!?!?! Error header");
        stop();
        return false;
    }
    readBody();
    return true;
}

// Выполняет POST запрос с JSON данными
bool SimpleHttpClient::POST_JSON(
    const char* host,
    uint16_t port,
    const String& path,
    const String& json,
    const String& extraHeaders,
    const uint32_t waitTM
) {
    return POST(host, port, path, "application/json", json, extraHeaders);
}

// Выполняет POST запрос с текстовыми данными
bool SimpleHttpClient::POST_TEXT(
    const char* host,
    uint16_t port,
    const String& path,
    const String& text,
    const String& extraHeaders,
    const uint32_t waitTM
) {
    return POST(host, port, path, "text/plain", text, extraHeaders);
}

// Выполняет POST запрос с потоковыми данными
/*
HttpResponse SimpleHttpClient::POST_STREAM(
    const char* host,
    uint16_t port,
    const String& path,
    const String& contentType,
    Stream& payloadStream,
    size_t contentLength,
    const String& extraHeaders
) {
    HttpResponse response;
    if (!m_client.connect(host, port)) {
        return response;
    }
    m_connected = true;
    m_client.printf(
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
        m_client.print(extraHeaders);
    }
    m_client.print("\r\n");
    uint8_t buffer[512];
    size_t bytesSent = 0;
    while (bytesSent < contentLength) {
        size_t toRead = min(sizeof(buffer), contentLength - bytesSent);
        size_t bytesRead = payloadStream.readBytes(buffer, toRead);
        if (bytesRead > 0) {
            m_client.write(buffer, bytesRead);
            bytesSent += bytesRead;
        } else {
            break;
        }
        yield();
    }
    readResponse(response);
    return response;
}
*/

//*********************************************************************************************************************
// Стриминговые запросы
//*********************************************************************************************************************

// Устанавливает соединение и отправляет GET запрос
/*
bool SimpleHttpClient::connectAndSendRequest(
    const char* host,
    uint16_t port,
    const String& path,
    const String& extraHeaders
) {
    if (!m_client.connect(host, port)) {
        return false;
    }
    m_connected = true;
    m_client.printf(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n",
        path.c_str(),
        host
    );
    if (extraHeaders.length()) {
        m_client.print(extraHeaders);
    }
    m_client.print("\r\n");
    return true;
}
*/

//*********************************************************************************************************************
// Вспомогательные методы
//*********************************************************************************************************************
/*
// Читает ответ от сервера
void SimpleHttpClient::readResponse(HttpResponse& response) {
    unsigned long timeout = millis();
    while (!m_client.available()) {
        if (millis() - timeout > 5000) {
            m_client.stop();
            m_connected = false;
            return;
        }
        vTaskDelay(1);
    }
    String s = m_client.readString();
    parseHttpResponse(s, response);
    m_client.stop();
    m_connected = false;
}

// Разбирает HTTP ответ
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

// Публичный метод для разбора HTTP ответа из внешнего кода
bool SimpleHttpClient::parseHttpResponseShared(const String& Str, HttpResponse& response) {
    SimpleHttpClient client;
    return client.parseHttpResponse(Str, response);
}
*/
//*********************************************************************************************************************
// Вспомогательные методы
//*********************************************************************************************************************

/**
 * Сбрасывает m_response в начальное состояние
 */
void SimpleHttpClient::resetResponse() {
    m_response.statusCode = -1;
    m_response.headers = "";
    m_response.body = "";
}

/**
 * Ожидает ответ сервера tm миллисекунд
 */
bool SimpleHttpClient::waitResponse(uint32_t tm) {
    unsigned long timeout = millis();
    while (!m_client.available()) {
        if (millis() - timeout > tm) {
            Serial.println("?!?!?! Stop Wait");
            return false;
        }
        vTaskDelay(1);
    }
    return true;
}

/**
 * Читает код HTTP в m_response.statusCode
 */
bool SimpleHttpClient::readStatusCode() {
    if (!m_client.connected()) {
        return false;
    }
    String statusLine = m_client.readStringUntil('\r');
    m_client.read(); // пропустить '\n'
    
    int firstSpace = statusLine.indexOf(' ');
    int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
    if (firstSpace != -1 && secondSpace != -1) {
        m_response.statusCode = statusLine.substring(firstSpace + 1, secondSpace).toInt();
    } else if (firstSpace != -1) {
        m_response.statusCode = statusLine.substring(firstSpace + 1).toInt();
    } else {
        return false;
    }
    return true;
}

/**
 * Читает заголовки HTTP в m_response.headers
 */
bool SimpleHttpClient::readHeader() {
    if (!m_client.connected() && !m_client.available()) {
        return false;
    }
    m_response.headers = "";
    while (m_client.connected() || m_client.available()) {
        if (!m_client.available()) {
            vTaskDelay(1);
            continue;
        }       
        String line = m_client.readStringUntil('\r');
        m_client.read(); // пропустить '\n'
        if (line.length() == 0) {
            break;
        }
        if (m_response.headers.length() > 0) {
            m_response.headers += "\r\n";
        }
        m_response.headers += line;
    }
//    Serial.println("!!! Headers");
//    Serial.printf("!!! Headers: %s\n",m_response.headers.c_str());
//    Serial.println("!!! Headers");
    return true;
}

/**
 * Извлекает значение HTTP-заголовка по имени из строки ответа
 */
String SimpleHttpClient::getHeaderValue(const String& name) const {
  if (m_response.headers.isEmpty() || name.isEmpty()) {
    return "";
  }
  int pos = 0;
  while (pos < m_response.headers.length()) {
    int lineEnd = m_response.headers.indexOf('\n', pos);
    if (lineEnd == -1) {
      lineEnd = m_response.headers.length();
    }
    String line = m_response.headers.substring(pos, lineEnd);
    line.trim();
    int colonPos = line.indexOf(':');
    if (colonPos != -1) {
      String headerName = line.substring(0, colonPos);
      headerName.trim();
      if (headerName.equalsIgnoreCase(name)) {
        String value = line.substring(colonPos + 1);
        value.trim();
        return value;
      }
    }
    pos = lineEnd + 1;
  }
  return "";
}


/**
 * Читает тело HTTP в m_response.body
 */
bool SimpleHttpClient::readBody() {
    if (!m_client.connected() && !m_client.available()) {
        return false;
    }
    m_response.body = "";
    while (m_client.connected() || m_client.available()) {
        if (!m_client.available()) {
            vTaskDelay(1);
            continue;
        }       
        m_response.body += m_client.readString();
    }
    m_response.body.trim();
    return true;
}
