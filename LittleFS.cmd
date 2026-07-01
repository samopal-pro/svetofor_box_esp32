set FILE=mklittlefs.bin
set DIR=data
set COM=COM3

C:\Users\sav\AppData\Local\Arduino15\packages\esp32\tools\mklittlefs\4.0.2-db0513a\mklittlefs.exe ^
 -c %DIR% -p 256 -b 4096 -s 1441792 %FILE%

C:\Users\sav\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\5.3.0\esptool.exe ^
  --chip esp32 --port %COM% --baud 921600 --before default_reset --after hard_reset write_flash ^
  -z --flash_mode dio --flash_freq 80m --flash_size detect 2686976 %FILE%
rem del %FILE%
timeout /t 10
rem pause