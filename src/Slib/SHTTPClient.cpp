#include "SHTTPClient.h"

/**
 * Выполняет HTTP GET запрос.
 *
 * @param host IP адрес или доменное имя сервера.
 * @param port TCP порт сервера.
 * @param path URI ресурса.
 * @param extraHeaders Дополнительные HTTP заголовки.
 *
 * @return Структура HttpResponse с кодом ответа,
 *         заголовками и телом ответа.
 */
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

/**
 * Выполняет HTTP POST запрос.
 *
 * @param host IP адрес или доменное имя сервера.
 * @param port TCP порт сервера.
 * @param path URI ресурса.
 * @param contentType MIME тип содержимого.
 * @param payload Передаваемые данные.
 * @param extraHeaders Дополнительные HTTP заголовки.
 *
 * @return Структура HttpResponse с результатом запроса.
 */
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

/**
 * Выполняет POST запрос с JSON содержимым.
 *
 * Content-Type автоматически устанавливается
 * в application/json.
 *
 * @param host Сервер назначения.
 * @param port TCP порт.
 * @param path URI ресурса.
 * @param json JSON строка.
 * @param extraHeaders Дополнительные HTTP заголовки.
 *
 * @return Структура HttpResponse.
 */
HttpResponse SimpleHttpClient::POST_JSON(
    const char* host,
    uint16_t port,
    const String& path,
    const String& json,
    const String& extraHeaders
) {
    return POST(
        host,
        port,
        path,
        "application/json",
        json,
        extraHeaders
    );
}

/**
 * Выполняет POST запрос с текстовым содержимым.
 *
 * Content-Type автоматически устанавливается
 * в text/plain.
 *
 * @param host Сервер назначения.
 * @param port TCP порт.
 * @param path URI ресурса.
 * @param text Текстовые данные.
 * @param extraHeaders Дополнительные HTTP заголовки.
 *
 * @return Структура HttpResponse.
 */
HttpResponse SimpleHttpClient::POST_TEXT(
    const char* host,
    uint16_t port,
    const String& path,
    const String& text,
    const String& extraHeaders
) {
    return POST(
        host,
        port,
        path,
        "text/plain",
        text,
        extraHeaders
    );
}

/**
 * Читает HTTP ответ от сервера.
 *
 * Извлекает:
 * - HTTP код ответа
 * - HTTP заголовки
 * - тело ответа
 *
 * @param client Подключенный WiFiClient.
 * @param response Структура для заполнения.
 */
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
    String s = "";
    s = client.readString();


/*    
    String statusLine = client.readStringUntil('\n');
    
    int p1 = statusLine.indexOf(' ');
    int p2 = statusLine.indexOf(' ', p1 + 1);
    if (p1 > 0 && p2 > p1) {
        response.statusCode =
            statusLine.substring(p1 + 1, p2).toInt();
    }

    while (client.connected()) {

        String line = client.readStringUntil('\n');

        if (line == "\r") {
            break;
        }

        response.headers += line;
    }

    while (client.connected() || client.available()) {

        while (client.available()) {
            response.body += (char)client.read();
        }

        delay(1);
    }
*/
    parseHttpResponse(s,response);
    client.stop();
}





bool SimpleHttpClient::parseHttpResponse(const String& Str, HttpResponse& response) {
    // Отладочный вывод входной строки
//    Serial.println("=== DEBUG: Input HTTP Response ===");
//    Serial.println(Str);
//    Serial.println("=== DEBUG: End of Input ===");
//    Serial.println();
    
    // Сбрасываем значения по умолчанию
    response.statusCode = -1;
    response.headers = "";
    response.body = "";
    
    // Находим конец первой строки (статусная строка)
    int firstLineEnd = Str.indexOf("\r\n");
    if (firstLineEnd == -1) {
        Serial.println("DEBUG: Failed to find first line ending (\\r\\n)");
        return false; // Невалидный ответ
    }
    
    // Парсим статусную строку: "HTTP/1.1 200 OK"
    String statusLine = Str.substring(0, firstLineEnd);
//    Serial.print("DEBUG: Status Line: ");
//    Serial.println(statusLine);
    
    int firstSpace = statusLine.indexOf(' ');
    int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
    
    if (firstSpace != -1 && secondSpace != -1) {
        String codeStr = statusLine.substring(firstSpace + 1, secondSpace);
        response.statusCode = codeStr.toInt();
//        Serial.print("DEBUG: Parsed Status Code: ");
//        Serial.println(response.statusCode);
    } else if (firstSpace != -1) {
        String codeStr = statusLine.substring(firstSpace + 1);
        response.statusCode = codeStr.toInt();
//        Serial.print("DEBUG: Parsed Status Code (no second space): ");
//        Serial.println(response.statusCode);
    } else {
//        Serial.println("DEBUG: Invalid status line format");
        return false; // Неверный формат статусной строки
    }
    
    // Находим разделитель между заголовками и телом (пустая строка)
    int headersEnd = Str.indexOf("\r\n\r\n");
    if (headersEnd == -1) {
//        Serial.println("DEBUG: \\r\\n\\r\\n separator not found, trying \\n\\n");
        headersEnd = Str.indexOf("\n\n");
        if (headersEnd == -1) {
//            Serial.println("DEBUG: Failed to find headers/body separator");
            return false; // Не можем найти тело
        }
        response.headers = Str.substring(firstLineEnd + 2, headersEnd);
        response.body = Str.substring(headersEnd + 2);
//        Serial.println("DEBUG: Used \\n\\n as separator");
    } else {
        response.headers = Str.substring(firstLineEnd + 2, headersEnd);
        response.body = Str.substring(headersEnd + 4);
//        Serial.println("DEBUG: Used \\r\\n\\r\\n as separator");
    }
    
    response.body.trim();
    
    // Отладочный вывод полей структуры
//    Serial.println("=== DEBUG: Parsed HttpResponse Fields ===");
//    Serial.print("Status Code: ");
//    Serial.println(response.statusCode);
//    Serial.println("Headers:");
//    Serial.println(response.headers);
//    Serial.println("Body:");
//    Serial.println(response.body);
//    Serial.println("=== DEBUG: End of Parsed Fields ===");
//    Serial.println();
    
    return true;
}
