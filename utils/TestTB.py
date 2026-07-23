import sys
import json
from pathlib import Path
from types import SimpleNamespace

# 1. НАСТРОЙКА ПУТЕЙ И ИМПОРТОВ
current_dir = Path(__file__).resolve().parent
class_dir = current_dir / 'class'
if str(class_dir) not in sys.path:
    sys.path.insert(0, str(class_dir))

from ThingsBoardClientClass import ThingsBoardClient

# Настройка рабочих папок
config_dir = Path.cwd() / "config"
config_dir.mkdir(parents=True, exist_ok=True)
file_setting = config_dir / "setting.json"

# Функция для красивой конвертации словарей в объекты SimpleNamespace
def to_obj(data_dict):
    return json.loads(json.dumps(data_dict), object_hook=lambda d: SimpleNamespace(**d))


# 2. ЗАГРУЗКА КОНФИГУРАЦИИ
try:
    setting = json.loads(
        file_setting.read_text(encoding="utf-8"), 
        object_hook=lambda d: SimpleNamespace(**d)
    )
except (FileNotFoundError, json.JSONDecodeError) as e:
    sys.exit(f"❌ Критическая ошибка: Не удалось прочитать settings.json ({type(e).__name__}).")

# Безопасно формируем путь для вывода устройств после проверки конфига
file_devices = config_dir / setting.SvetoforBox.device_data


# 3. АВТОРИЗАЦИЯ В THINGSBOARD
try:
    tb = ThingsBoardClient(
        setting.ThingsBoard.server, 
        setting.ThingsBoard.admin_user, 
        setting.ThingsBoard.admin_pass
    )
    if not tb.auth():
        sys.exit("❌ Ошибка: Неверный логин или пароль для ThingsBoard.")
except Exception as e:
    sys.exit(f"❌ Не удалось подключиться к серверу ThingsBoard: {e}")


# 4. ПОЛУЧЕНИЕ СПИСКА УСТРОЙСТВ
try:
    devices = tb.get_devices_by_profile(setting.SvetoforBox.device_profile)
    if not devices:
        sys.exit("⚠️ Устройства с указанным профилем не найдены на сервере.")
except Exception as e:
    sys.exit(f"❌ Ошибка при запросе списка устройств: {e}")


# 5. СБОР ДАННЫХ И ОБРАБОТКА В ЦИКЛЕ
result = {}

for device_dict in devices:
    try:
        # Безопасно парсим устройство в объект
        device = to_obj(device_dict)
        device_id = device.id.id
        device_name = device.name
    except AttributeError:
        # Если структура ответа сервера неожиданно изменилась, пропускаем это устройство
        print(f"⚠️ Пропущено устройство с невалидной структурой в ответе API")
        continue

    # Инициализируем значения по умолчанию
    access_token = ""
    serial_no    = ""
    dogovor_no   = ""
    box_no       = ""
    config_uuid  = ""
    # Получаем токен устройства
    try:
        access_token = tb.get_device_access_token_by_id(device_id)
    except Exception as e:
        print(f"⚠️ Не удалось получить токен для {device_name}: {e}")

    # Получаем клиентские атрибуты (с защитой от отсутствия ключей)
    if access_token:
        try:
            # Передаем device_id, если ваш метод ThingsBoardClient требует этого
            raw_data = tb.get_device_attributes("SerialNo,DogovorNo,BoxNo,ConfigUUID", "clientKeys")
            
            if raw_data:
                data = to_obj(raw_data)
                # Защита через try/except на случай, если ключей client или вложенных параметров нет
                try:
                    serial_no = data.client.SerialNo
                except AttributeError:
                    pass
                
                try:
                    dogovor_no = data.client.DogovorNo
                except AttributeError:
                    pass
                
                try:
                    box_no = data.client.BoxNo
                except AttributeError:
                    pass

                try:
                    config_uuid = data.client.ConfigUUID
                except AttributeError:
                    pass

        except Exception as e:
            print(f"⚠️ Ошибка при запросе атрибутов для {device_name}: {e}")

    # Сохраняем собранные данные устройства в итоговый словарь
    result[device_name] = {
        "id": device_id,
        "token": access_token if access_token else "",
        "serial_no": serial_no,
        "dogovor_no": dogovor_no,
        "box_no": box_no,
        "config_uuid": config_uuid
    }


# 6. СОХРАНЕНИЕ РЕЗУЛЬТАТА В ФАЙЛ
try:
    with open(file_devices, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=4)
    print(f"✅ Сформирован список из {len(result)} устройств в файл {file_devices.name}")
except IOError as e:
    print(f"❌ Не удалось сохранить итоговый файл устройств: {e}")
