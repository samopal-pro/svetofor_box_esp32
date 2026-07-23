# Файл: utils/lib/web_response.py
# Формирует JSON-строки с сообщениями и командами для веб-интерфейса.
import json


class WebResponse:
    """Аналог C++ класса WebResponse для формирования JSON-ответов."""
    
    @staticmethod
    def log(text, type="info"):
        """Формирует JSON с лог-сообщением.
        
        Args:
            text: Текст сообщения.
            type: Тип сообщения (info, warning, error).
        
        Returns:
            JSON-строка с логом.
        """
        doc = {
            "log": {
                "text": text
            }
        }
        if type:
            doc["log"]["type"] = type
        return json.dumps(doc, ensure_ascii=False)
    
    @staticmethod
    def msg(text, type="info", timeout=4000):
        """Формирует JSON с всплывающим сообщением.
        
        Args:
            text: Текст сообщения.
            type: Тип сообщения (info, warning, error, success).
            timeout: Время показа в миллисекундах.
        
        Returns:
            JSON-строка с сообщением.
        """
        doc = {
            "msg": {
                "text": text,
                "type": type,
                "timeout": timeout
            }
        }
        return json.dumps(doc, ensure_ascii=False)
    
    @staticmethod
    def reload(timeout=0):
        """Формирует JSON с командой перезагрузки страницы.
        
        Args:
            timeout: Задержка перед перезагрузкой в миллисекундах.
        
        Returns:
            JSON-строка с командой reload.
        """
        doc = {
            "cmd": {
                "name": "reload"
            }
        }
        if timeout > 0:
            doc["cmd"]["timeout"] = timeout
        return json.dumps(doc, ensure_ascii=False)
    
    @staticmethod
    def load(url, timeout=0):
        """Формирует JSON с командой загрузки указанного URL.
        
        Args:
            url: URL для загрузки.
            timeout: Задержка перед загрузкой в миллисекундах.
        
        Returns:
            JSON-строка с командой load.
        """
        doc = {
            "cmd": {
                "name": "load",
                "url": url
            }
        }
        if timeout > 0:
            doc["cmd"]["timeout"] = timeout
        return json.dumps(doc, ensure_ascii=False)
    
    @staticmethod
    def config(section, param, value):
        """Формирует JSON с командой изменения конфигурации на клиенте.
        
        Args:
            section: Секция конфигурации.
            param: Имя параметра.
            value: Значение параметра.
        
        Returns:
            JSON-строка с командой config.
        """
        doc = {
            "config": {
                "section": section,
                "param": param,
                "value": value
            }
        }
        return json.dumps(doc, ensure_ascii=False)
    
    @staticmethod
    def combine(commands):
        """Объединяет несколько JSON-команд в один объект.
        
        Args:
            commands: Список JSON-строк для объединения.
        
        Returns:
            Объединённая JSON-строка.
        """
        result = {}
        for cmd in commands:
            if not cmd or cmd == "{}":
                continue
            try:
                data = json.loads(cmd)
                result.update(data)
            except json.JSONDecodeError:
                continue
        return json.dumps(result, ensure_ascii=False)