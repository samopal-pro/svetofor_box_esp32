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

        delay(1);
    }

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

    client.stop();
}