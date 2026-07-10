#!/usr/bin/env python3
"""
Скрипт для загрузки прошивки (папки с файлами) на сервер ThingsBoard.
"""

import os
import json
import tarfile
import hashlib
import base64
import io
import requests
from typing import List, Optional

# Параметры скрипта
TB_URL = "http://109.172.115.70:8088"
TB_USERNAME = "sav@ellips.ru"
TB_PASSWORD = "_Sav59vas"
DEVICE_NAME = "68FE71AB1530"
HTTPD_PATH = "../data/httpd"  # Каталог для отправки
HTTPD_EXCLUDE = "config1.json,config2.json,config3.json"  # Список исключаемых файлов через запятую
HTTPD_VERSION = "10.1.0"  # Версия каталога по умолчанию


class ThingsBoardFirmwareUploader:
    """Класс для загрузки прошивки на ThingsBoard."""
    
    def __init__(self, tb_url: str, username: str, password: str):
        """
        Инициализация загрузчика.
        
        Args:
            tb_url: URL сервера ThingsBoard
            username: Имя пользователя
            password: Пароль
        """
        self.tb_url = tb_url.rstrip('/')
        self.username = username
        self.password = password
        self.token = None
        self.session = requests.Session()
        
    def login(self) -> bool:
        """
        Аутентификация в ThingsBoard.
        
        Returns:
            bool: True если вход успешен, иначе False
        """
        url = f"{self.tb_url}/api/auth/login"
        data = {
            "username": self.username,
            "password": self.password
        }
        
        try:
            response = self.session.post(url, json=data)
            response.raise_for_status()
            self.token = response.json().get("token")
            if self.token:
                self.session.headers.update({
                    "X-Authorization": f"Bearer {self.token}"
                })
                print("✓ Успешная аутентификация")
                return True
            else:
                print("✗ Токен не получен")
                return False
        except requests.exceptions.RequestException as e:
            print(f"✗ Ошибка аутентификации: {e}")
            return False
    
    def get_device_access_token(self, device_name: str) -> Optional[str]:
        """
        Получение ACCESS_TOKEN устройства по его имени.
        
        Args:
            device_name: Имя устройства
            
        Returns:
            ACCESS_TOKEN устройства или None
        """
        url = f"{self.tb_url}/api/tenant/devices"
        params = {
            "pageSize": 100,
            "page": 0,
            "textSearch": device_name
        }
        
        try:
            response = self.session.get(url, params=params)
            response.raise_for_status()
            devices = response.json().get("data", [])
            
            # Ищем устройство с точным совпадением имени
            for device in devices:
                if device.get("name") == device_name:
                    device_id = device.get("id", {}).get("id")
                    if device_id:
                        return self.get_device_credentials(device_id)
            
            print(f"✗ Устройство '{device_name}' не найдено")
            return None
            
        except requests.exceptions.RequestException as e:
            print(f"✗ Ошибка при получении списка устройств: {e}")
            return None
    
    def get_device_credentials(self, device_id: str) -> Optional[str]:
        """
        Получение учетных данных устройства.
        
        Args:
            device_id: ID устройства
            
        Returns:
            ACCESS_TOKEN устройства или None
        """
        url = f"{self.tb_url}/api/device/{device_id}/credentials"
        
        try:
            response = self.session.get(url)
            response.raise_for_status()
            credentials = response.json()
            return credentials.get("credentialsId")
        except requests.exceptions.RequestException as e:
            print(f"✗ Ошибка при получении учетных данных устройства: {e}")
            return None
    
    def update_device_attributes(self, access_token: str, attributes: dict) -> bool:
        """
        Обновление атрибутов устройства.
        
        Args:
            access_token: ACCESS_TOKEN устройства
            attributes: Словарь атрибутов для обновления
            
        Returns:
            bool: True если обновление успешно, иначе False
        """
        url = f"{self.tb_url}/api/v1/{access_token}/attributes"
        
        try:
            response = self.session.post(url, json=attributes)
            response.raise_for_status()
            print("✓ Атрибуты устройства успешно обновлены")
            return True
        except requests.exceptions.RequestException as e:
            print(f"✗ Ошибка при обновлении атрибутов: {e}")
            return False


class FirmwarePreparer:
    """Класс для подготовки прошивки."""
    
    def __init__(self, firmware_path: str, exclude_list: List[str], default_version: str):
        """
        Инициализация подготовки прошивки.
        
        Args:
            firmware_path: Путь к каталогу с прошивкой
            exclude_list: Список исключаемых файлов
            default_version: Версия по умолчанию
        """
        self.firmware_path = firmware_path
        self.exclude_list = exclude_list
        self.default_version = default_version
        
    def ensure_version_file(self) -> str:
        """
        Проверка и создание файла version.json.
        
        Returns:
            str: Версия прошивки
        """
        version_file = os.path.join(self.firmware_path, "version.json")
        
        # Проверяем существование файла
        if os.path.exists(version_file):
            try:
                with open(version_file, 'r') as f:
                    data = json.load(f)
                    version = data.get("version")
                    if version:
                        print(f"✓ Версия из файла: {version}")
                        return version
            except (json.JSONDecodeError, IOError) as e:
                print(f"⚠ Ошибка чтения version.json: {e}")
        
        # Создаем или перезаписываем файл
        version_data = {"version": self.default_version}
        with open(version_file, 'w') as f:
            json.dump(version_data, f, indent=2)
        
        print(f"✓ Создан файл version.json с версией: {self.default_version}")
        return self.default_version
    
    def create_tar_archive(self) -> bytes:
        """
        Создание TAR архива без сжатия.
        
        Returns:
            bytes: Содержимое архива
        """
        archive_buffer = io.BytesIO()
        
        with tarfile.open(fileobj=archive_buffer, mode='w') as tar:
            for root, dirs, files in os.walk(self.firmware_path):
                for file in files:
                    file_path = os.path.join(root, file)
                    
                    # Проверяем исключения
                    relative_name = os.path.relpath(file_path, self.firmware_path)
                    if file in self.exclude_list or relative_name in self.exclude_list:
                        print(f"  - Исключен: {relative_name}")
                        continue
                    
                    # Добавляем файл в архив
                    arcname = os.path.relpath(file_path, os.path.dirname(self.firmware_path))
                    tar.add(file_path, arcname=arcname)
                    print(f"  + Добавлен: {relative_name}")
        
        archive_data = archive_buffer.getvalue()
        archive_size = len(archive_data)
        print(f"✓ Создан архив размером: {archive_size} байт")
        
        return archive_data
    
    def calculate_sha256(self, data: bytes) -> str:
        """
        Вычисление SHA256 хеша.
        
        Args:
            data: Данные для хеширования
            
        Returns:
            str: SHA256 хеш в hex формате
        """
        sha256_hash = hashlib.sha256(data).hexdigest()
        print(f"✓ SHA256 сигнатура: {sha256_hash}")
        return sha256_hash
    
    def encode_base64(self, data: bytes) -> str:
        """
        Кодирование данных в Base64.
        
        Args:
            data: Данные для кодирования
            
        Returns:
            str: Base64 строка
        """
        encoded = base64.b64encode(data).decode('utf-8')
        print(f"✓ Данные закодированы в Base64 (длина: {len(encoded)} символов)")
        return encoded


def main():
    """Основная функция скрипта."""
    
    # Парсинг списка исключений
    exclude_files = [f.strip() for f in HTTPD_EXCLUDE.split(',') if f.strip()]
    
    print("=" * 60)
    print("ЗАГРУЗКА ПРОШИВКИ НА THINGSBOARD")
    print("=" * 60)
    print(f"Сервер: {TB_URL}")
    print(f"Устройство: {DEVICE_NAME}")
    print(f"Каталог прошивки: {HTTPD_PATH}")
    print(f"Исключаемые файлы: {exclude_files}")
    print(f"Версия по умолчанию: {HTTPD_VERSION}")
    print("=" * 60)
    
    # Проверка существования каталога
    if not os.path.exists(HTTPD_PATH):
        print(f"✗ Каталог '{HTTPD_PATH}' не существует!")
        return
    
    if not os.path.isdir(HTTPD_PATH):
        print(f"✗ '{HTTPD_PATH}' не является каталогом!")
        return
    
    # Подготовка прошивки
    preparer = FirmwarePreparer(HTTPD_PATH, exclude_files, HTTPD_VERSION)
    
    # Шаг 2-3: Работа с version.json
    version = preparer.ensure_version_file()
    
    # Шаг 4: Создание TAR архива
    print("\nСоздание архива...")
    archive_data = preparer.create_tar_archive()
    
    # Шаг 5: Вычисление SHA256
    checksum = preparer.calculate_sha256(archive_data)
    
    # Шаг 6: Кодирование в Base64
    encoded_data = preparer.encode_base64(archive_data)
    
    # Работа с ThingsBoard
    print("\nПодключение к ThingsBoard...")
    uploader = ThingsBoardFirmwareUploader(TB_URL, TB_USERNAME, TB_PASSWORD)
    
    # Шаг 0: Аутентификация и получение токена устройства
    if not uploader.login():
        print("✗ Не удалось войти в систему")
        return
    
    access_token = uploader.get_device_access_token(DEVICE_NAME)
    if not access_token:
        print(f"✗ Не удалось получить токен доступа для устройства '{DEVICE_NAME}'")
        return
    
    print(f"✓ Получен ACCESS_TOKEN устройства: {access_token[:10]}...")
    
    # Шаг 7-9: Отправка атрибутов
    attributes = {
        "httpd_data": encoded_data,
        "httpd_checksum": checksum,
        "httpd_version": version
    }
    
    if uploader.update_device_attributes(access_token, attributes):
        print("\n" + "=" * 60)
        print("✓ ПРОШИВКА УСПЕШНО ЗАГРУЖЕНА!")
        print("=" * 60)
    else:
        print("\n" + "=" * 60)
        print("✗ ОШИБКА ПРИ ЗАГРУЗКЕ ПРОШИВКИ")
        print("=" * 60)


if __name__ == "__main__":
    main()