import sys
import os
from pathlib import Path

fw_file = "../build/esp32.esp32.esp32/svetofor_box_esp32.ino.bin"

# Добавляем путь к папке class
current_dir = os.path.dirname(os.path.abspath(__file__))
class_dir = os.path.join(current_dir, 'class')
if class_dir not in sys.path:
    sys.path.insert(0, class_dir)
    
from FirmwareInfoClass import FirmwareInfo
from ThingsBoardClientClass import ThingsBoardClient

fw = FirmwareInfo(fw_file)
print(fw.fw_name)      # Название прошивки
print(fw.fw_version)   # Версия прошивки
print(fw.project_name) # Название проекта
print(fw.version)      # Версия проекта

tb = ThingsBoardClient("http://109.172.115.70:8088", "sav@ellips.ru", "_Sav59vas")
flag = tb.auth()
if flag :
    print(f"✅ JWT: {tb.token}")   
    dp_name = "ParkingSensor"
    flag   = tb.get_device_profile_id(dp_name)
    print(f"✅ Найден: {tb.device_profile_name} {tb.device_profile_id}")

if flag :
    ret = tb.check_ota_package(fw.fw_name,fw.fw_version)
    if ret:
        print(f"⚠️  Пакет {fw.fw_name} существует {tb.package_id}")   
    else :
        print(f"⚠️ Пакет {fw.fw_name} не существует")   
    
if flag :
    tb.delete_ota_package(fw.fw_name,fw.fw_version)

if flag :
    flag = tb.insert_ota_package(fw.fw_name,fw.fw_version,f"{fw.fw_name}_{fw.fw_version}","FIRMWARE")

if flag :
    flag = tb.upload_ota_file(fw_file)

if flag :
    flag = tb.register_ota_to_device_profile()