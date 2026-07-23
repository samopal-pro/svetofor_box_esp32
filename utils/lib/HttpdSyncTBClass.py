# Файл: utils/lib/tb_sync.py
# Синхронизация конфигурации с ThingsBoard: проверка обновлений, загрузка и отправка данных.
import json
import time
import os
import threading
import uuid
from pathlib import Path
from ThingsBoardClientClass import ThingsBoardClient
from HttpdResponseClass import WebResponse

class ThingsBoardSync:
    """Управляет синхронизацией конфигурации между локальным файлом и ThingsBoard."""
    
    def __init__(self, config_loader, server=None, config_file='config.json'):
        """Инициализирует синхронизацию с ThingsBoard.
        
        Args:
            config_loader: Загрузчик конфигурации.
            server: Экземпляр HttpServer для отправки ответов (опционально).
            config_file: Путь к файлу конфигурации.
        """
        self.cfg = config_loader
        self.server = server
        self.base_dir     = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.config_file = os.path.normpath(os.path.join(self.base_dir, self.cfg.device_config))

#        self.config_file = config_file
        self.client = None
        self.device_token = None
        self.current_uuid = None
        self._stop_event = threading.Event()
        self._updater_thread = None
    
    def init_thingsboard(self):
        """Инициализирует соединение с ThingsBoard и выполняет аутентификацию."""
        try:
            self.client = ThingsBoardClient(self.cfg.tb_server, self.cfg.tb_admin_user, self.cfg.tb_admin_pass)
            if not self.client.auth():
                print("❌ Ошибка аутентификации")
                return False
            print("✅ Аутентификация ThingsBoard успешна")
            return True
        except Exception as e:
            print(f"❌ Не удалось подключиться к серверу ThingsBoard: {e}")
            return False
        
    def init_device(self, device_name):
        """Получает токен устройства, перезапускает фоновую проверку обновлений.
        
        Args:
            device_name: Имя устройства в ThingsBoard.
        """
        self.stop_check_updates()
        
        ret = self.client.get_device_access_token(device_name)
        if not ret:
            print(f"❌ Не удалось получить токен для устройства {device_name}")
            return False
        
        self.check_and_update()
        if not self.current_uuid:
            print("⚠️ Начальное значение ConfigUUID не получено")
        
        print(f"✅ Инициализация устройства {device_name} завершена. ConfigUUID: {self.current_uuid}")
        self.start_check_updates()
        return True
    
    def stop_check_updates(self):
        """Останавливает фоновый поток проверки обновлений."""
        self._stop_event.set()
        if self._updater_thread and self._updater_thread.is_alive():
            self._updater_thread.join(timeout=2)
        self._stop_event.clear()
    
    def start_check_updates(self):
        """Запускает фоновый поток проверки обновлений."""
        self._stop_event.clear()
        self._updater_thread = threading.Thread(target=self._check_updates_loop, daemon=True)
        self._updater_thread.start()   
  
    def _check_updates_loop(self):
        """Фоновый цикл проверки обновлений конфигурации."""
        while not self._stop_event.is_set():
            try:
                if self.current_uuid is not None:
                    print("!!! Check config update")
                    self.check_and_update()
            except Exception as e:
                print(f"Ошибка: {e}")
            self._stop_event.wait(self.cfg.poll_interval)
    
    def load_config_from_device(self):
        """Загружает конфигурацию с устройства ThingsBoard."""
        try:
            data = self.client.get_device_attributes("Config", "clientKeys")
            return data["client"]["Config"]
        except Exception as e:
            print(f"❌ Ошибка загрузки конфигурации: {e}")
            return None
    
    def save_config_to_file(self, config_data):
        """Сохраняет конфигурацию в локальный JSON-файл."""
        try:
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(config_data, f, indent=2, ensure_ascii=False)
            print(f"✅ Конфигурация сохранена в {self.config_file}")
            return True
        except Exception as e:
            print(f"❌ Ошибка сохранения конфигурации: {e}")
            return False
    
    def check_and_update(self):
        """Проверяет изменение ConfigUUID и обновляет конфигурацию при необходимости."""
        try:
            data = self.client.get_device_attributes("ConfigUUID", "clientKeys")
            new_uuid = data["client"]["ConfigUUID"]
            if not new_uuid:
                print("⚠️ Новое значение ConfigUUID не получено")
                return False
            if new_uuid != self.current_uuid:
                print(f"🔄 Обнаружено изменение ConfigUUID: {self.current_uuid} -> {new_uuid}")
                config_data = self.load_config_from_device()
                if config_data and self.save_config_to_file(config_data):
                    self.current_uuid = new_uuid
                    print(f"✅ Конфигурация обновлена, новый UUID: {new_uuid}")
                    if self.server:
                        self.server.set_responcse(WebResponse.combine([
                           WebResponse.msg("Конфигурация обновлена на клиенте. Сейчас страница перезагрузится", "info", 4000),
                           WebResponse.reload(4000)
                        ]))
                            



                    return True
                print("❌ Не удалось загрузить новую конфигурацию")
                return False
            else:
                print(f"ℹ️ ConfigUUID не изменился: {self.current_uuid}")
            return False
        except Exception as e:
            print(f"❌ Ошибка при проверке обновлений: {e}")
            return False
    
    def send_config_update(self, config):
        """Отправляет обновлённую конфигурацию в ThingsBoard."""
        try:
            new_uuid = str(uuid.uuid4())
            print('!!! UUID', new_uuid)
            self.client.send_device_attribute("ConfigUUID", new_uuid)
            self.client.send_device_attribute("Config", config)
            self.current_uuid = new_uuid
            return new_uuid
        except Exception as e:
            print(f"❌ Ошибка отправки конфигурации: {e}")
            return None