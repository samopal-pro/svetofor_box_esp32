import sys
import hashlib
import json
import requests
import struct
import os
from pathlib import Path
from datetime import datetime
import base64
import time

IMAGE_HEADER_SIZE = 24
SEGMENT_HEADER_SIZE = 8
APP_DESC_OFFSET = IMAGE_HEADER_SIZE + SEGMENT_HEADER_SIZE
APP_DESC_SIZE = 256
MAGIC_WORD = 0xABCD5432
CUSTOM_DESC_SIZE = 48
CUSTOM_DESC_OFFSET = APP_DESC_OFFSET + APP_DESC_SIZE


def read_string(buffer, offset, length):
    raw = buffer[offset:offset + length]
    null_byte_pos = raw.find(b'\x00')
    if null_byte_pos != -1:
        raw = raw[:null_byte_pos]
    return raw.decode('utf-8', errors='ignore')


def calculate_sha256(file_path):
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()


def read_app_description(firmware_path):
    with open(firmware_path, 'rb') as f:
        f.seek(APP_DESC_OFFSET)
        buffer = f.read(APP_DESC_SIZE)
    if len(buffer) < APP_DESC_SIZE:
        print(f"❌ Ошибка: файл слишком мал для чтения описания приложения")
        return None

    magic, = struct.unpack_from('<I', buffer, 0)
    if magic != MAGIC_WORD:
        print(f"❌ Ошибка: Неверное магическое число. Найдено: 0x{magic:08X}")
        return None

    result = {
        "magic_word": hex(magic),
        "secure_version": struct.unpack_from('<I', buffer, 4)[0],
        "reserv1": struct.unpack_from('<II', buffer, 8),
        "version": read_string(buffer, 16, 32),
        "project_name": read_string(buffer, 48, 32),
        "time": read_string(buffer, 80, 16),
        "date": read_string(buffer, 96, 16),
        "idf_ver": read_string(buffer, 112, 32),
        "app_elf_sha256": buffer[144:176].hex(),
        "min_efuse_blk_rev_full": struct.unpack_from('<H', buffer, 176)[0],
        "max_efuse_blk_rev_full": struct.unpack_from('<H', buffer, 178)[0],
        "mmu_page_size": buffer[180] if len(buffer) > 180 else 0,
    }
    return result


def read_custom_description(firmware_path, expected_magic):
    with open(firmware_path, 'rb') as f:
        f.seek(CUSTOM_DESC_OFFSET)
        buffer = f.read(CUSTOM_DESC_SIZE)
    if len(buffer) < CUSTOM_DESC_SIZE:
        print(f"❌ Ошибка: файл слишком мал для чтения пользовательских данных")
        return None

    custom_magic, = struct.unpack_from('<I', buffer, 0)
    if custom_magic != expected_magic:
        print(f"❌ Ошибка: Неверное магическое число в пользовательских данных. "
              f"Найдено: 0x{custom_magic:08X}, ожидалось: 0x{expected_magic:08X}")
        return None

    fw_name = read_string(buffer, 4, 28)
    fw_version = read_string(buffer, 32, 16)

    return {
        "fw_name": fw_name,
        "fw_version": fw_version,
        "fw_magic": hex(custom_magic)
    }


def get_firmware_size(file_path):
    return os.path.getsize(file_path)


def get_jwt_token(tb_url, username, password):
    print("--- ПОЛУЧЕНИЕ JWT ТОКЕНА ---")
    print(f"Сервер: {tb_url}")
    print(f"Пользователь: {username}")

    base_url = tb_url.rstrip('/')
    auth_url = f"{base_url}/api/auth/login"
    auth_data = {
        "username": username,
        "password": password
    }

    try:
        print(f"URL: {auth_url}")
        response = requests.post(
            auth_url,
            json=auth_data,
            headers={'Content-Type': 'application/json'},
            timeout=10
        )

        if response.status_code == 200:
            token_data = response.json()
            token = token_data.get('token')
            if token:
                print("✅ JWT токен получен успешно!")
                print(f"Токен: {token[:20]}...{token[-20:]}")
                print()
                return token
            else:
                print("❌ Токен не найден в ответе сервера")
                print(f"Ответ: {response.text}")
                return None
        else:
            print(f"❌ Ошибка аутентификации: {response.status_code}")
            print(f"Ответ: {response.text}")
            return None

    except requests.exceptions.Timeout:
        print("❌ Ошибка: Таймаут соединения")
        return None
    except requests.exceptions.ConnectionError:
        print("❌ Ошибка: Не удалось подключиться к серверу")
        return None
    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        return None


def get_device_by_name(tb_url, token, device_name):
    base_url = tb_url.rstrip('/')
    headers = {
        'X-Authorization': f'Bearer {token}',
        'Accept': 'application/json'
    }
    search_url = f"{base_url}/api/tenant/devices?textSearch={device_name}&pageSize=100&page=0"

    try:
        print(f"Поиск устройства по имени: {device_name}")
        response = requests.get(search_url, headers=headers, timeout=10)

        if response.status_code == 200:
            data = response.json()
            devices = data.get('data', [])
            for device in devices:
                if device.get('name') == device_name:
                    device_id = device['id']['id']
                    print(f"✅ Устройство найдено, ID: {device_id}")
                    return device_id
            print(f"⚠️ Устройство '{device_name}' не найдено")
            return None
        else:
            print(f"❌ Ошибка поиска устройства: {response.status_code}")
            print(f"Ответ: {response.text}")
            return None

    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        return None


def get_device_profile(tb_url, token, device_id):
    base_url = tb_url.rstrip('/')
    headers = {
        'X-Authorization': f'Bearer {token}',
        'Accept': 'application/json'
    }
    url = f"{base_url}/api/device/{device_id}"

    try:
        print(f"Получение профиля устройства: {url}")
        response = requests.get(url, headers=headers, timeout=10)

        if response.status_code == 200:
            device_info = response.json()
            device_profile = device_info.get('deviceProfileId')
            if device_profile:
                if isinstance(device_profile, dict):
                    profile_id = device_profile.get('id')
                    if profile_id:
                        print(f"✅ Найден профиль устройства, ID: {profile_id}")
                        return profile_id
                elif isinstance(device_profile, str):
                    print(f"✅ Найден профиль устройства, ID: {device_profile}")
                    return device_profile
            print("⚠️ Профиль устройства не найден в ответе")
            return None
        else:
            print(f"❌ Ошибка получения профиля: {response.status_code}")
            print(f"Ответ: {response.text}")
            return None

    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        return None


def find_existing_packages(tb_url, token, title):
    base_url = tb_url.rstrip('/')
    headers = {
        'X-Authorization': f'Bearer {token}',
        'Accept': 'application/json'
    }
    search_url = f"{base_url}/api/otaPackages?textSearch={title}&pageSize=100&page=0"

    try:
        print(f"Проверка существующих OTA пакетов с названием: {title}")
        response = requests.get(search_url, headers=headers, timeout=10)

        if response.status_code == 200:
            data = response.json()
            packages = data.get('data', [])
            existing_packages = {}

            for pkg in packages:
                pkg_title = pkg.get('title')
                pkg_version = pkg.get('version')
                if pkg_title == title:
                    existing_packages[pkg_version] = {
                        'id': pkg['id']['id'],
                        'version': pkg_version,
                        'hasData': pkg.get('hasData', False)
                    }
                    print(f"   - Найден пакет с версией: {pkg_version}, ID: {pkg['id']['id']}")

            if existing_packages:
                print(f"✅ Найдено {len(existing_packages)} существующих пакетов")
                return existing_packages
            else:
                print("⚠️ Существующих пакетов не найдено")
                return {}
        else:
            print(f"❌ Ошибка поиска OTA пакетов: {response.status_code}")
            return {}

    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        return {}


def generate_unique_version(base_version, existing_versions):
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    if not base_version or base_version == "Unknown" or base_version == "unknown":
        return f"v{timestamp}"

    if base_version in existing_versions:
        new_version = f"{base_version}_{timestamp}"
        print(f"⚠️ Версия '{base_version}' уже существует. Используем: {new_version}")
        return new_version

    return base_version


def create_ota_package(tb_url, token, custom_info, fw_version, device_profile_id):
    base_url = tb_url.rstrip('/')
    headers = {
        'X-Authorization': f'Bearer {token}',
        'Content-Type': 'application/json',
        'Accept': 'application/json'
    }

    ota_package_data = {
        "type": "FIRMWARE",
        "title": custom_info['fw_name'],
        "version": fw_version,
        "tag": custom_info['fw_name']
    }

    if device_profile_id:
        ota_package_data["deviceProfileId"] = {
            "id": device_profile_id,
            "entityType": "DEVICE_PROFILE"
        }

    upload_url = f"{base_url}/api/otaPackage"
    print(f"URL: {upload_url}")
    print(f"Создание OTA пакета с версией: {fw_version}")
    print(f"Данные: {json.dumps(ota_package_data, indent=2)}")

    try:
        response = requests.post(upload_url, headers=headers, json=ota_package_data, timeout=30)

        if response.status_code in [200, 201]:
            package_info = response.json()
            package_id = package_info.get('id')
            if isinstance(package_id, dict):
                package_id = package_id.get('id')

            if not package_id:
                print("❌ Не удалось получить ID OTA пакета")
                print(f"Ответ: {package_info}")
                return None

            print(f"✅ OTA пакет создан, ID: {package_id}")
            return package_id
        else:
            print(f"❌ Ошибка создания OTA пакета: {response.status_code}")
            print(f"Ответ сервера: {response.text}")

            if "deviceProfileId" in str(response.text):
                print("Пробуем создать OTA пакет без deviceProfileId...")
                del ota_package_data["deviceProfileId"]
                response = requests.post(upload_url, headers=headers, json=ota_package_data, timeout=30)

                if response.status_code in [200, 201]:
                    package_info = response.json()
                    package_id = package_info.get('id')
                    if isinstance(package_id, dict):
                        package_id = package_id.get('id')
                    print(f"✅ OTA пакет создан, ID: {package_id}")
                    return package_id
                else:
                    print(f"❌ Ошибка создания OTA пакета: {response.status_code}")
                    print(f"Ответ сервера: {response.text}")
                    return None
            return None

    except requests.exceptions.Timeout:
        print("❌ Ошибка: Таймаут соединения")
        return None
    except requests.exceptions.ConnectionError:
        print("❌ Ошибка: Не удалось подключиться к серверу")
        return None
    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        import traceback
        traceback.print_exc()
        return None


def upload_file_to_package(tb_url, token, package_id, firmware_path, firmware_sha):
    base_url = tb_url.rstrip('/')
    headers = {
        'X-Authorization': f'Bearer {token}'
    }
    upload_url = f"{base_url}/api/otaPackage/{package_id}?checksum={firmware_sha}&checksumAlgorithm=SHA256"

    print(f"Загрузка файла на: {upload_url}")
    print(f"Размер файла: {os.path.getsize(firmware_path):,} байт")
    print(f"SHA256 (файла): {firmware_sha}")

    try:
        with open(firmware_path, 'rb') as f:
            file_data = f.read()
            files = {
                'file': (
                    os.path.basename(firmware_path),
                    file_data,
                    'application/octet-stream'
                )
            }

            file_response = requests.post(
                upload_url,
                headers=headers,
                files=files,
                timeout=300
            )

            if file_response.status_code in [200, 201]:
                print("✅ Файл прошивки успешно загружен!")
                return True
            else:
                print(f"❌ Ошибка загрузки файла: {file_response.status_code}")
                print(f"Ответ: {file_response.text}")
                return False

    except requests.exceptions.Timeout:
        print("❌ Ошибка: Таймаут соединения при загрузке файла")
        return False
    except requests.exceptions.ConnectionError:
        print("❌ Ошибка: Не удалось подключиться к серверу")
        return False
    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        import traceback
        traceback.print_exc()
        return False


def update_device_attributes(tb_url, token, device_id, attributes_data):
    """
    Обновляет атрибуты устройства в SERVER_SCOPE.
    Правильный формат URL для ThingsBoard 4.3+:
    /api/plugins/telemetry/DEVICE/{deviceId}/attributes/SERVER_SCOPE
    """
    base_url = tb_url.rstrip('/')
    headers = {
        'X-Authorization': f'Bearer {token}',
        'Content-Type': 'application/json',
        'Accept': 'application/json'
    }

    # ПРАВИЛЬНЫЙ URL: указываем entityType = DEVICE
    attributes_url = f"{base_url}/api/plugins/telemetry/DEVICE/{device_id}/attributes/SERVER_SCOPE"

    print(f"Обновление атрибутов устройства: {attributes_url}")

    try:
        response = requests.post(
            attributes_url,
            headers=headers,
            json=attributes_data,
            timeout=30
        )

        if response.status_code in [200, 201]:
            print("✅ Атрибуты устройства обновлены!")
            return True
        else:
            print(f"⚠️ Не удалось обновить атрибуты: {response.status_code}")
            print(f"Ответ: {response.text}")
            return False

    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        return False


def get_package_info(tb_url, token, package_id):
    """Получает информацию об OTA пакете по его ID."""
    base_url = tb_url.rstrip('/')
    headers = {
        'X-Authorization': f'Bearer {token}',
        'Accept': 'application/json'
    }
    url = f"{base_url}/api/otaPackage/info/{package_id}"

    try:
        response = requests.get(url, headers=headers, timeout=10)
        if response.status_code == 200:
            return response.json()
        else:
            print(f"❌ Ошибка получения информации о пакете: {response.status_code}")
            print(f"Ответ: {response.text}")
            return None
    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        return None


def get_device_profile_by_id(tb_url, token, device_profile_id):
    """Получает Device Profile по ID."""
    base_url = tb_url.rstrip('/')
    headers = {
        'X-Authorization': f'Bearer {token}',
        'Accept': 'application/json'
    }
    url = f"{base_url}/api/deviceProfile/{device_profile_id}"

    try:
        response = requests.get(url, headers=headers, timeout=10)
        if response.status_code == 200:
            return response.json()
        else:
            print(f"❌ Ошибка получения Device Profile: {response.status_code}")
            print(f"Ответ: {response.text}")
            return None
    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        return None


def save_device_profile(tb_url, token, device_profile):
    """Сохраняет (создает или обновляет) Device Profile."""
    base_url = tb_url.rstrip('/')
    headers = {
        'X-Authorization': f'Bearer {token}',
        'Content-Type': 'application/json',
        'Accept': 'application/json'
    }
    url = f"{base_url}/api/deviceProfile"

    # Удаляем поля, которые не должны отправляться при обновлении
    device_profile.pop('createdTime', None)
    device_profile.pop('version', None)

    try:
        response = requests.post(url, headers=headers, json=device_profile, timeout=30)

        if response.status_code in [200, 201]:
            return True
        else:
            print(f"❌ Ошибка сохранения Device Profile: {response.status_code}")
            print(f"Ответ: {response.text}")
            return False

    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        return False


def assign_package_via_device_profile(tb_url, token, package_id, device_profile_id):
    """
    Назначает OTA пакет через обновление Device Profile.
    Это правильный способ в ThingsBoard 4.3+.
    """
    print(f"Назначение OTA пакета через Device Profile (ID: {device_profile_id})...")

    # 1. Получаем информацию о пакете
    package_info = get_package_info(tb_url, token, package_id)
    if not package_info:
        print("❌ Не удалось получить информацию о пакете")
        return False

    package_type = package_info.get('type')  # 'FIRMWARE' или 'SOFTWARE'
    if package_type not in ['FIRMWARE', 'SOFTWARE']:
        print(f"❌ Неизвестный тип пакета: {package_type}")
        return False

    # 2. Получаем текущий Device Profile
    device_profile = get_device_profile_by_id(tb_url, token, device_profile_id)
    if not device_profile:
        print("❌ Не удалось получить Device Profile")
        return False

    # 3. Обновляем профиль: добавляем/заменяем firmwareId или softwareId
    ota_ref = {
        "entityType": "OTA_PACKAGE",
        "id": package_id
    }

    if package_type == "FIRMWARE":
        device_profile['firmwareId'] = ota_ref
        print(f"   Устанавливаем firmwareId: {package_id}")
    else:  # SOFTWARE
        device_profile['softwareId'] = ota_ref
        print(f"   Устанавливаем softwareId: {package_id}")

    # 4. Сохраняем обновленный Device Profile
    success = save_device_profile(tb_url, token, device_profile)
    if not success:
        print("❌ Не удалось сохранить обновленный Device Profile")
        return False

    print("✅ Device Profile успешно обновлен!")
    print(f"📱 Пакет '{package_type}' назначен Device Profile.")
    print("   Все устройства этого профиля получат обновление при следующем подключении.")
    return True


def upload_firmware_to_thingsboard(firmware_path, tb_url, token, device_name):
    print("=" * 60)
    print("ЗАГРУЗКА ПРОШИВКИ НА THINGSBOARD (OTA UPDATE)")
    print("=" * 60)
    print(f"Файл: {firmware_path}")
    print(f"Сервер: {tb_url}")
    print(f"Устройство: {device_name}")
    print()

    if not os.path.exists(firmware_path):
        print(f"❌ Ошибка: Файл не найден: {firmware_path}")
        return False

    print("--- ИЗВЛЕЧЕНИЕ ДАННЫХ ИЗ ПРОШИВКИ ---")
    app_info = read_app_description(firmware_path)
    if not app_info:
        print("❌ Ошибка: Не удалось прочитать стандартное описание")
        return False

    expected_magic = int(app_info['magic_word'], 16)
    custom_info = read_custom_description(firmware_path, expected_magic)
    if not custom_info:
        print("❌ Ошибка: Не удалось прочитать пользовательские данные")
        return False

    firmware_sha = calculate_sha256(firmware_path)
    elf_sha256 = app_info['app_elf_sha256']
    firmware_size = get_firmware_size(firmware_path)
    fw_version = custom_info['fw_version'] if custom_info['fw_version'] else app_info['version']

    print(f"✓ Название проекта:    {app_info['project_name']}")
    print(f"✓ Версия проекта:      {app_info['version']}")
    print(f"✓ FW Name:             {custom_info['fw_name']}")
    print(f"✓ FW Version:          {fw_version}")
    print(f"✓ Размер прошивки:     {firmware_size:,} байт")
    print(f"✓ SHA256 (файла):      {firmware_sha}")
    print(f"✓ SHA256 (ELF):        {elf_sha256}")
    print(f"✓ Дата сборки:         {app_info['date']} {app_info['time']}")
    print(f"✓ Версия IDF:          {app_info['idf_ver']}")
    print()

    print("--- ЗАГРУЗКА НА THINGSBOARD ---")

    print("Шаг 1: Поиск устройства...")
    device_id = get_device_by_name(tb_url, token, device_name)
    if not device_id:
        print("❌ Не удалось найти устройство")
        return False
    print()

    print("Шаг 2: Получение профиля устройства...")
    device_profile_id = get_device_profile(tb_url, token, device_id)
    if not device_profile_id:
        print("⚠️ Не удалось получить профиль устройства. Продолжаем без него...")
    print()

    print("Шаг 3: Проверка существующих OTA пакетов...")
    existing_packages = find_existing_packages(tb_url, token, custom_info['fw_name'])

    if fw_version in existing_packages:
        package_info = existing_packages[fw_version]
        print(f"\n⚠️ ПАКЕТ С ВЕРСИЕЙ '{fw_version}' УЖЕ СУЩЕСТВУЕТ!")
        print(f"   ID пакета: {package_info['id']}")
        print(f"   Версия: {package_info['version']}")
        print(f"   Данные загружены: {'Да' if package_info['hasData'] else 'Нет'}")
        print(f"\n❌ Загрузка отменена. Пакет с версией '{fw_version}' уже существует.")
        print("   Используйте существующий пакет или измените версию прошивки.")
        return False
    print()

    print("Шаг 4: Создание OTA пакета...")
    package_id = create_ota_package(
        tb_url, token,
        custom_info,
        fw_version,
        device_profile_id
    )
    if not package_id:
        print("❌ Не удалось создать OTA пакет")
        return False
    print()

    print("Шаг 5: Загрузка файла прошивки...")
    if not upload_file_to_package(tb_url, token, package_id, firmware_path, firmware_sha):
        print("❌ Не удалось загрузить файл прошивки")
        return False
    print()

    print("Шаг 6: Обновление атрибутов устройства...")
    attributes_data = {
        "fw_title": custom_info['fw_name'],
        "fw_version": fw_version,
        "fw_sha256": firmware_sha,
        "fw_elf_sha256": elf_sha256,
        "fw_size": firmware_size,
        "fw_project_name": app_info['project_name'],
        "fw_project_version": app_info['version'],
        "fw_build_date": f"{app_info['date']} {app_info['time']}",
        "fw_idf_version": app_info['idf_ver'],
        "fw_upload_date": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "fw_package_id": package_id,
        "fw_secure_version": app_info.get('secure_version', 0),
        "fw_mmu_page_size": app_info.get('mmu_page_size', 0),
    }
    update_device_attributes(tb_url, token, device_id, attributes_data)
    print()

    print("Шаг 7: Назначение OTA пакета через Device Profile...")
    assign_package_via_device_profile(tb_url, token, package_id, device_profile_id)

    return True


def main():
    TB_URL = "http://109.172.115.70:8088"
    TB_USERNAME = "sav@ellips.ru"
    TB_PASSWORD = "_Sav59vas"
    DEVICE_NAME = "68FE71AB1530"
    FIRMWARE_PATH = "../build/esp32.esp32.esp32/svetofor_box_esp32.ino.bin"

    if len(sys.argv) > 1:
        FIRMWARE_PATH = sys.argv[1]
    if len(sys.argv) > 2:
        TB_URL = sys.argv[2]
    if len(sys.argv) > 3:
        TB_USERNAME = sys.argv[3]
    if len(sys.argv) > 4:
        TB_PASSWORD = sys.argv[4]
    if len(sys.argv) > 5:
        DEVICE_NAME = sys.argv[5]

    token = get_jwt_token(TB_URL, TB_USERNAME, TB_PASSWORD)
    if not token:
        print("\n❌ Не удалось получить JWT токен. Проверьте учетные данные.")
        sys.exit(1)

    success = upload_firmware_to_thingsboard(
        FIRMWARE_PATH,
        TB_URL,
        token,
        DEVICE_NAME
    )

    if success:
        print("\n✅ Загрузка завершена успешно!")
        sys.exit(0)
    else:
        print("\n❌ Загрузка не удалась!")
        sys.exit(1)


if __name__ == "__main__":
    main()