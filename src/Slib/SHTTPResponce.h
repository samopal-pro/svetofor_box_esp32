// ============================================
// Файл: WebResponse.h
// Версия: 2.1
// Исправлено: множественное определение статических членов
// ============================================

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Статический API для команд ESP32 → Браузер
 * Использование:
 *   HTTP_sendResponse(WebResponse::log("Готово"));
 *   HTTP_sendResponse(WebResponse::combine({log("text"), reload(1000)}));
 */
class WebResponse {
private:
    static constexpr size_t JSON_SIZE = 200;
    
public:
    // Удаляем конструктор (чисто статический класс)
    WebResponse() = delete;

    /**
     * Сообщение в статусную строку
     */
    static String log(const String& text, const String& type = "info") {
        StaticJsonDocument<JSON_SIZE> doc;  // Локальный статический документ
        JsonObject obj = doc["log"].to<JsonObject>();
        obj["text"] = text;
        if (!type.isEmpty()) obj["type"] = type;
        
        String result;
        result.reserve(JSON_SIZE);
        serializeJson(doc, result);
        return result;
    }

    /**
     * Всплывающее сообщение
     */
    static String msg(const String& text, const String& type = "info", uint32_t timeout = 4000) {
        StaticJsonDocument<JSON_SIZE> doc;
        JsonObject obj = doc["msg"].to<JsonObject>();
        obj["text"] = text;
        obj["type"] = type;
        obj["timeout"] = timeout;
        
        String result;
        result.reserve(JSON_SIZE);
        serializeJson(doc, result);
        return result;
    }

    /**
     * Перезагрузка страницы
     */
    static String reload(uint32_t timeout = 0) {
        StaticJsonDocument<JSON_SIZE> doc;
        JsonObject obj = doc["cmd"].to<JsonObject>();
        obj["name"] = "reload";
        if (timeout > 0) obj["timeout"] = timeout;
        
        String result;
        result.reserve(JSON_SIZE);
        serializeJson(doc, result);
        return result;
    }

    /**
     * Загрузка страницы
     */
    static String load(const String& url, uint32_t timeout = 0) {
        StaticJsonDocument<JSON_SIZE> doc;
        JsonObject obj = doc["cmd"].to<JsonObject>();
        obj["name"] = "load";
        obj["url"] = url;
        if (timeout > 0) obj["timeout"] = timeout;
        
        String result;
        result.reserve(JSON_SIZE);
        serializeJson(doc, result);
        return result;
    }

    /**
     * Обновление конфигурации
     */
    static String config(const String& section, const String& param, const String& value) {
        StaticJsonDocument<JSON_SIZE> doc;
        JsonObject obj = doc["config"].to<JsonObject>();
        obj["section"] = section;
        obj["param"] = param;
        obj["value"] = value;
        
        String result;
        result.reserve(JSON_SIZE);
        serializeJson(doc, result);
        return result;
    }

    /**
     * Комбинирование нескольких команд
     */
    static String combine(std::initializer_list<String> commands) {
        String result = "{";
        result.reserve(JSON_SIZE);
        bool first = true;
        
        for (const auto& cmd : commands) {
            if (cmd.isEmpty() || cmd == "{}") continue;
            
            if (!first) result += ',';
            result += cmd.substring(1, cmd.length() - 1);
            first = false;
        }
        
        result += '}';
        return result;
    }
};

// Больше не нужна глобальная инициализация!