import sys
import os
from pathlib import Path

http_dir = "../data/httpd"
http_tar_file = "../tmp/httpd.tar"

# Добавляем путь к папке class
current_dir = os.path.dirname(os.path.abspath(__file__))
class_dir = os.path.join(current_dir, 'class')
if class_dir not in sys.path:
    sys.path.insert(0, class_dir)
    
from HttpdBuilderClass import HttpdBuilder
from ThingsBoardClientClass import ThingsBoardClient

httpd = HttpdBuilder(http_dir,http_tar_file)
httpd.build()
print(httpd.name)     
print(httpd.version)  
print(httpd.exclude) 

tb = ThingsBoardClient("http://109.172.115.70:8088", "sav@ellips.ru", "_Sav59vas")
flag = tb.auth()
if flag :
    print(f"✅ JWT: {tb.token}")   
    dp_name = "ParkingSensor"
    flag   = tb.get_device_profile_id(dp_name)
    print(f"✅ Найден: {tb.device_profile_name} {tb.device_profile_id}")

if flag :
    ret = tb.check_ota_package(httpd.name,httpd.version)
    if ret:
        print(f"⚠️  Пакет {http_tar_file} существует {tb.package_id}")   
    else :
        print(f"⚠️ Пакет {http_tar_file} не существует")   
    
if flag :
    tb.delete_ota_package(httpd.name,httpd.version)

if flag :
    flag = tb.insert_ota_package(httpd.name,httpd.version,f"{httpd.name}_{httpd.version}","SOFTWARE")

if flag :
    flag = tb.upload_ota_file(http_tar_file)

if flag :
    flag = tb.register_ota_to_device_profile()

