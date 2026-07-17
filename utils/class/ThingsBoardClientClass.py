"""Клиент для работы с ThingsBoard REST API."""

import requests


class ThingsBoardClient:
    """HTTP клиент для взаимодействия с ThingsBoard сервером.
    
    Attributes:
        base_url: Базовый URL ThingsBoard сервера.
        token: JWT токен доступа (заполняется после auth).
    """
    
######################################################################################################
# Конструктор.         
######################################################################################################
    def __init__(self, url: str, username: str, password: str):
        """Инициализирует клиент ThingsBoard.
        
        Args:
            url: URL ThingsBoard сервера (например, http://192.168.1.1:8080).
            username: Имя пользователя (email).
            password: Пароль пользователя.
        """
        self.base_url = url.rstrip('/')
        self._username           = username
        self._password           = password
        self.token               = None
        self.device_name         = None
        self.device_id           = None
        self.device_token        = None
        self.device_profile_name = None
        self.device_profile_id   = None
        self.package_id          = None
        self.package_type        = "FIRMWARE"
    
######################################################################################################
# Получает JWT токен для доступа к API.         
######################################################################################################
    def auth(self) -> bool:
        """Получает JWT токен для доступа к API."""

        auth_url = f"{self.base_url}/api/auth/login"
        auth_data = {
            "username": self._username,
            "password": self._password
        }
        
        try:
            response = requests.post(
                auth_url,
                json=auth_data,
                headers={'Content-Type': 'application/json'},
                timeout=10
            )
            
            if response.status_code == 200:
                self.token = response.json().get('token')
                return True
            
            print(f"❌ Ошибка аутентификации: {response.status_code}")
            print(f"Ответ: {response.text}")
            return False
            
        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return False
        
######################################################################################################
# Получение ID Device Profile по его имени.         
######################################################################################################
    def get_device_profile_id(self, profile_name: str) -> bool:
        """Получение ID Device Profile по его имени. """
        headers = {
            'X-Authorization': f'Bearer {self.token}',
            'Accept': 'application/json'
        }
        self.device_profile_name = profile_name
        search_url = f"{self.base_url}/api/deviceProfiles?textSearch={profile_name}&pageSize=100&page=0"
        try:
            response = requests.get(search_url, headers=headers, timeout=10)
            if response.status_code == 200:
                data = response.json()
                profiles = data.get('data', [])
                for profile in profiles:
                    if profile.get('name') == profile_name:
                        self.device_profile_id = profile['id']['id']    
                        return True
            print(f"❌ Ошибка поиска Device Profile: {response.status_code}")
            return False
        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return False        
        
######################################################################################################
# Проверка существования OTA пакета по названию и версии.        
######################################################################################################
    def check_ota_package(self, title: str, version: str) -> bool:
        """Проверка существования OTA пакета по названию и версии."""
        if not self.token:
            print("❌ Токен не получен. Выполните auth()")
            return False
        self.package_id = None
        headers = {
            'X-Authorization': f'Bearer {self.token}',
            'Accept': 'application/json'
        }
        search_url = f"{self.base_url}/api/otaPackages?textSearch={title}&pageSize=100&page=0"
    
        try:
            response = requests.get(search_url, headers=headers, timeout=10)
            if response.status_code == 200:
                data = response.json()
                packages = data.get('data', [])
                for pkg in packages:
                    if pkg.get('title') == title and pkg.get('version') == version:
                        self.package_id =pkg['id']['id']
                        return True
                return False
            print(f"❌ Ошибка поиска OTA пакетов: {response.status_code}")
            return False
        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return False        

######################################################################################################
# Удаление OTA пакета по названию и опционально версии.        
######################################################################################################
    def delete_ota_package(self, title: str, version: str = None) -> bool:
        """Удаление OTA пакета по названию и опционально версии."""
        if not self.token:
            print("❌ Токен не получен. Выполните auth()")
            return False
        
        if self.package_id != None :
            self.check_ota_package(title,version)    

        if self.package_id == None  :
            print("❌ Пакет не найден")
            return False
       
        headers = {
            'X-Authorization': f'Bearer {self.token}',
            'Accept': 'application/json'
        }

        try:
            # Удаление
            success = True
            del_response = requests.delete(
                f"{self.base_url}/api/otaPackage/{self.package_id}",
                headers=headers,
                timeout=10
            )
            if del_response.status_code in [200, 204]:
                self.package_id = None
                print(f"✅ Удален: {title} v{self.package_id}")
            else:
                print(f"❌ Ошибка удаления {self.package_id}: {del_response.status_code}")
                success = False

            return success
        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return False        
        
######################################################################################################
# Регистрация нового OTA пакета.       
######################################################################################################
    def insert_ota_package(self, title: str, version: str, tag: str, package_type: str = "FIRMWARE") -> bool:
        """Регистрация нового OTA пакета."""
        if not self.token:
            print("❌ Токен не получен. Выполните auth()")
            return False

        # Проверяем существование
        if self.package_id == None :
            self.check_ota_package(title, version)

        if self.package_id != None:
            print(f"⚠️ Пакет {title} v{version} уже существует (ID: {self.package_id})")
            return False
        
        if self.device_profile_id == None :
            print(f"⚠️ Не задан Device Profile")
            return False
        
        self.package_type = package_type
        
        headers = {
            'X-Authorization': f'Bearer {self.token}',
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        }

        ota_data = {
            "type": package_type.upper(),
            "title": title,
            "version": version,
            "tag": tag,
            "deviceProfileId": {
                "id": self.device_profile_id,
                "entityType": "DEVICE_PROFILE"
            }
        }
        try:
            response = requests.post(
                f"{self.base_url}/api/otaPackage",
                headers=headers,
                json=ota_data,
                timeout=30
            )
            
            if response.status_code in [200, 201]:
                package_id = response.json().get('id')
                if isinstance(package_id, dict):
                    self.package_id = package_id.get('id')
                if self.package_id != None:
                    print(f"✅ OTA пакет зарегистрирован, ID: {package_id}")
                    return True
                       
            print(f"❌ Ошибка регистрации OTA пакета: {response.status_code}")
            return False
        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return False        
        
######################################################################################################
# Расчет хэша SHA256 заданного файлп       
######################################################################################################
    def calculate_sha256(self, file_path):
        import hashlib
        sha256_hash = hashlib.sha256()
        with open(file_path, "rb") as f:
            for byte_block in iter(lambda: f.read(4096), b""):
                sha256_hash.update(byte_block)
        return sha256_hash.hexdigest()

        
######################################################################################################
# Загрузка файла прошивки в OTA пакет.       
######################################################################################################
    def upload_ota_file(self, file_path: str) -> bool:
        """Загрузка файла прошивки в OTA пакет."""
        if not self.token:
            print("❌ Токен не получен. Выполните auth()")
            return False
        
        if self.package_id == None :
            print("❌ Пакет не создан")
            return False
        import os
        firmware_sha = self.calculate_sha256(file_path)
          
        headers = {'X-Authorization': f'Bearer {self.token}'}
        upload_url = f"{self.base_url}/api/otaPackage/{self.package_id}?checksum={firmware_sha}&checksumAlgorithm=SHA256"        

        try:
            with open(file_path, 'rb') as f:
                files = {'file': (os.path.basename(file_path), f.read(), 'application/octet-stream')}
                response = requests.post(upload_url, headers=headers, files=files, timeout=300)
                
                if response.status_code in [200, 201]:
                    print("✅ Файл загружен")
                    return True
                print(f"❌ Ошибка загрузки: {response.status_code}")
                return False
        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return False
        
######################################################################################################
# Регистрация текущего OTA пакета в Device Profile.       
######################################################################################################
    def register_ota_to_device_profile(self) -> bool:
        """Регистрация OTA пакета в Device Profile."""
        if not self.token:
            print("❌ Токен не получен. Выполните auth()")
            return False

        if self.device_profile_id is None:
            print("❌ Device Profile не задан.")
            return False

        if self.package_id is None:
            print("❌ OTA пакет не бфл сформирован")
            return False

        headers = {
            'X-Authorization': f'Bearer {self.token}',
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        }

        try:
            # Получаем полную информацию Device Profile
            response = requests.get(
                f"{self.base_url}/api/deviceProfile/{self.device_profile_id}",
                headers=headers,
                timeout=10
            )
            if response.status_code != 200:
                print(f"❌ Ошибка получения Device Profile: {response.status_code}")
                return False

            device_profile = response.json()
            
            # Обновляем Device Profile
            ota_ref = {
                "entityType": "OTA_PACKAGE",
                "id": self.package_id
            }
            
            if self.package_type == "FIRMWARE":
                device_profile['firmwareId'] = ota_ref
                print(f"   Устанавливаем firmwareId: {self.package_id}")
            else:
                device_profile['softwareId'] = ota_ref
                print(f"   Устанавливаем softwareId: {self.package_id}")

            # Сохраняем Device Profile
            device_profile.pop('createdTime', None)
            device_profile.pop('version', None)

            response = requests.post(
                f"{self.base_url}/api/deviceProfile",
                headers=headers,
                json=device_profile,
                timeout=30
            )
            
            if response.status_code in [200, 201]:
                print("✅ Device Profile успешно обновлен!")
                print(f"📱 Пакет '{self.package_type}' назначен Device Profile.")
                print("   Все устройства этого профиля получат обновление при следующем подключении.")
                return True
            else:
                print(f"❌ Ошибка сохранения Device Profile: {response.status_code}")
                return False

        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return False        
        
######################################################################################################
# Получение ACCESS_TOKEN устройства по его имени.         
######################################################################################################
    def get_device_access_token(self, device_name: str) -> bool:
        """Получение ACCESS_TOKEN устройства по его имени.
        
        Args:
            device_name: Имя устройства.
            
        Returns:
            ACCESS_TOKEN устройства или None в случае ошибки.
        """
        self.device_name  = device_name
        self.device_token = None
        self.device_id    = None

        if not self.token:
            print("❌ Токен не получен. Выполните auth()")
            return False

        headers = {
            'X-Authorization': f'Bearer {self.token}',
            'Accept': 'application/json'
        }

        try:
            # Поиск устройства по имени
            search_url = f"{self.base_url}/api/tenant/devices?textSearch={device_name}&pageSize=100&page=0"
            response = requests.get(search_url, headers=headers, timeout=10)
            
            if response.status_code != 200:
                print(f"❌ Ошибка поиска устройства: {response.status_code}")
                return False

            data = response.json()
            devices = data.get('data', [])
            
            # Ищем устройство с точным совпадением имени
            device_id = None
            for device in devices:
                if device.get('name') == device_name:
                    self.device_id = device['id']['id']
                    break
            
            if not self.device_id:
                print(f"❌ Устройство '{device_name}' не найдено")
                return False

            # Получение ACCESS_TOKEN устройства
            token_url = f"{self.base_url}/api/device/{self.device_id}/credentials"
            response = requests.get(token_url, headers=headers, timeout=10)
            
            if response.status_code != 200:
                print(f"❌ Ошибка получения токена устройства: {response.status_code}")
                return False

            credentials = response.json()
            self.device_token = credentials.get('credentialsId')
            
            if self.device_token:
                print(f"✅ ACCESS_TOKEN для '{device_name}' получен")
                return True
            else:
                print(f"❌ ACCESS_TOKEN для '{device_name}' не найден")
                return False

        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return False        
        
######################################################################################################
# Получение значений атрибутов устройства по ACCESS_TOKEN.         
######################################################################################################
    def get_device_attributes(self,  attributes: str, attribute_type: str = "sharedKeys") -> dict | None:
        """Получение значений атрибутов устройства по ACCESS_TOKEN.
        
        Args:
            access_token: ACCESS_TOKEN устройства.
            attributes: Список атрибутов через запятую (например, "firmware_version,model").
            attribute_type: Тип атрибута (SHARED_SCOPE, CLIENT_SCOPE, SERVER_SCOPE).
            
        Returns:
            Словарь с значениями атрибутов или None в случае ошибки.
        """
        headers = {
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        }
#   url += "/attributes?sharedKeys=fw_title,fw_version,fw_checksum";

        try:
            url = f"{self.base_url}/api/v1/{self.device_token}/attributes?{attribute_type}={attributes}"
          
            response = requests.get(url, headers=headers, timeout=10)
            
            if response.status_code != 200:
                print(f"❌ Ошибка получения атрибутов: {response.status_code}")
                return None

            data = response.json()
            return data

        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return None        
        
######################################################################################################
# Отправка клиентского атрибута на устройство       
######################################################################################################
    def send_device_attribute(self, key: str, value) -> bool:
        """Отправка клиентского атрибута на устройство."""
        if not self.device_token:
            print("❌ Токен устройства не получен. Выполните get_device_access_token()")
            return False

        headers = {
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        }

        try:
            url = f"{self.base_url}/api/v1/{self.device_token}/attributes"
            response = requests.post(
                url,
                headers=headers,
                json={key: value},
                timeout=10
            )
            
            if response.status_code in [200, 204]:
                print(f"✅ Атрибут '{key}' отправлен")
                return True
                
            print(f"❌ Ошибка отправки атрибута: {response.status_code}")
            return False

        except Exception as e:
            print(f"❌ Ошибка: {e}")
            return False