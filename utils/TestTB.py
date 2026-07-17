import sys
import os
from pathlib import Path
# Добавляем путь к папке class
current_dir = os.path.dirname(os.path.abspath(__file__))
class_dir = os.path.join(current_dir, 'class')
if class_dir not in sys.path:
    sys.path.insert(0, class_dir)
    
from ThingsBoardClientClass import ThingsBoardClient

tb = ThingsBoardClient("http://109.172.115.70:8088", "sav@ellips.ru", "_Sav59vas")
flag = tb.auth()

device_name  = "68FE71AB1530"
device_token = ""

if flag:
    flag = tb.get_device_access_token("68FE71AB1530")

if flag:
    data = tb.get_device_attributes("ConfigUUID,Config","clientKeys")

print("!!! Attribute",data)
