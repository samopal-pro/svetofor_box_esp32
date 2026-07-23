# Файл: utils/HTTPD.py
# Основной модуль для запуска HTTP-сервера.
import sys
import os
import logging
from pathlib import Path

current_dir = Path(__file__).resolve().parent
lib_dir = current_dir / 'lib'
if str(lib_dir) not in sys.path:
    sys.path.insert(0, str(lib_dir))

from ConfigClass import ConfigLoader
from HttpdSyncTBClass import ThingsBoardSync
from HttpdServerClass import HttpServer

#logging.basicConfig(level=logging.INFO)
logging.basicConfig(level=logging.ERROR)

# В HTTPD.py передать server в tb_sync
if __name__ == '__main__':
    cfg = ConfigLoader()
    server = HttpServer(cfg, None)
    tb_sync = ThingsBoardSync(cfg, server)
    server.tb_sync = tb_sync
    if not tb_sync.init_thingsboard():
        print("⚠️ Не удалось инициализировать соединение с ThingsBoard")
    server.run(host='0.0.0.0', port=80, debug=True)