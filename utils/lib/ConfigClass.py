# Файл: utils/lib/config_loader.py
# Безопасная загрузка и доступ к настройкам из config/setting.json.
import json
import sys
from pathlib import Path
from types import SimpleNamespace


class ConfigLoader:
    """Загружает и предоставляет доступ к конфигурации приложения."""
    
    def __init__(self, config_path=None):
        if config_path is None:
            current_dir = Path(__file__).resolve().parent.parent
            config_path = current_dir / 'config' / 'setting.json'
        self.config_path = Path(config_path)
        self._setting = None
        self.load()
    
    def load(self):
        """Загружает конфигурацию из JSON-файла."""
        try:
            self._setting = json.loads(
                self.config_path.read_text(encoding="utf-8"),
                object_hook=lambda d: SimpleNamespace(**d)
            )
        except (FileNotFoundError, json.JSONDecodeError) as e:
            sys.exit(f"❌ Критическая ошибка: Не удалось прочитать {self.config_path.name} ({type(e).__name__}).")
    
    @property
    def setting(self):
        """Возвращает объект с настройками."""
        return self._setting
    
    @property
    def tb_server(self):
        """Адрес сервера ThingsBoard."""
        return self._setting.ThingsBoard.server
    
    @property
    def tb_admin_user(self):
        """Имя пользователя администратора ThingsBoard."""
        return self._setting.ThingsBoard.admin_user
    
    @property
    def tb_admin_pass(self):
        """Пароль администратора ThingsBoard."""
        return self._setting.ThingsBoard.admin_pass
    
    @property
    def device_config(self):
        """Файл config устройства."""
        return self._setting.SvetoforBox.device_config

    @property
    def device_profile(self):
        """Имя профайла устройств."""
        return self._setting.SvetoforBox.device_profile
    
    @property
    def poll_interval(self):
        """Интервал опроса в секундах."""
        return self._setting.SvetoforBox.poll_interval
    
    @property
    def web_root(self):
        """Путь к веб-корню относительно utils."""
        return self._setting.HTTPD.web_root
    
    @property
    def admin_root(self):
        """Путь к веб-корню админки относительно utils."""
        return self._setting.HTTPD.admin_root

    @property
    def config_root(self):
        """Путь к веб-корню конфигурации относительно utils."""
        return self._setting.HTTPD.config_root
    
    @property
    def software_version(self):
        """Версия программного обеспечения."""
        return self._setting.HTTPD.software_version