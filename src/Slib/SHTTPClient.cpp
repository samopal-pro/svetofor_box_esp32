// ============================================
// Файл: src/Slib/SHTTPClient.cpp (исправленный)
// ============================================
#define MODULE_NAME "HTTP_CLIENT"
#define MODULE_DEBUG_LEVEL DEBUG_INFO
#include "SDEBUG.h"


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
    LOG_INFOLN("HTTP GET: %s:%d%s", host, port, path.c_str());
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
        stop();
        return false;
    }
    if (!readStatusCode()) {
        stop();
        return false;
    }
    if (!readHeader()) {
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
    LOG_INFOLN("HTTP POST: %s:%d%s", host, port, path.c_str());
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
        stop();
        return false;
    }
    if (!readStatusCode()) {
        stop();
        return false;
    }
    if (!readHeader()) {
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
            LOG_ERRORLN("Timeout Response");
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
        LOG_ERRORLN("Not connect Status Code");
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
        LOG_ERRORLN("Error Status Code");
        return false;
    }
    return true;
}

/**
 * Читает заголовки HTTP в m_response.headers
 */
bool SimpleHttpClient::readHeader() {
    if (!m_client.connected() && !m_client.available()) {
        LOG_ERRORLN("Not connect Headers");
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
        LOG_ERRORLN("Not connect Body");
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
