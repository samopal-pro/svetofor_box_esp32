# Файл: utils/lib/http_server.py
# Класс HTTP-сервера с маршрутами для обработки запросов от веб-интерфейса.
import json
import os
import time
import uuid
import logging
from flask import Flask, request, jsonify, send_file, make_response, redirect
from flask_cors import CORS
from werkzeug.utils import secure_filename
from HttpdResponseClass import WebResponse

logger = logging.getLogger(__name__)
logger.setLevel(logging.ERROR)
logging.getLogger('werkzeug').setLevel(logging.ERROR)


class HttpServer:
    """HTTP-сервер для обслуживания веб-интерфейса и API."""
    
    def __init__(self, config_loader, tb_sync):
        logger.setLevel(logging.ERROR)
        self.cfg = config_loader
        self.tb_sync = tb_sync
        self.app = Flask(__name__)
        CORS(self.app)
        
        self.httpd_path   = self.cfg.web_root
        self.base_dir     = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.web_root     = os.path.normpath(os.path.join(self.base_dir,  self.httpd_path))
        self.admin_root   = os.path.normpath(os.path.join(self.base_dir, self.cfg.admin_root))
        self.config_root  = os.path.normpath(os.path.join(self.base_dir, self.cfg.config_root))
        self.upload_dir   = os.path.join(self.web_root, 'uploads')
        self.config_file = os.path.normpath(os.path.join(self.base_dir, self.cfg.device_config))
        self.software_v   = "-"
        
        os.makedirs(self.web_root, exist_ok=True)
        os.makedirs(self.upload_dir, exist_ok=True)
        
        self.http_status = ""
        self.str_response = ""
        self.is_authenticated_flag = False
        self.is_first_play = True
        self.active_config = 0
        self.strID = "ESP32_001"
        self.serNo = "SN123456"
        
        self.config = {
            "config2": {
                "ESP_PASS1": "admin",
                "ESP_PASS2": "superadmin",
                "ESP_PASS3": "sfetoforbox"
            }
        }
        
        self.config_selector = [
            {"file": "/config/config1.json", "mp3_num": 1},
            {"file": "/config/config2.json", "mp3_num": 2}
        ]
        
        self._register_routes()
    
    def _register_routes(self):
        """Регистрирует все маршруты приложения."""
        self.app.route('/auth', methods=['POST'])(self.handle_auth)
        self.app.route('/admin/config/<path:config_path>')(self.handle_config)
        self.app.route('/admin/', defaults={'admin_path': ''})(self.handle_admin)
        self.app.route('/admin/<path:admin_path>')(self.handle_admin)
        self.app.route('/', defaults={'path': ''})(self.handle_file)
        self.app.route('/<path:path>')(self.handle_file)
        self.app.route('/config', methods=['GET'])(self.handle_config_selector)
        self.app.route('/cmd', methods=['POST'])(self.handle_command)
        self.app.route('/save', methods=['POST'])(self.handle_save)
        self.app.route('/header', methods=['GET'])(self.handle_header)
        self.app.route('/tail', methods=['GET'])(self.handle_tail)
        self.app.route('/distance', methods=['GET'])(self.handle_distance)
        self.app.route('/response', methods=['GET'])(self.handle_response)

    def run(self, host='0.0.0.0', port=80, debug=True):
        """Запускает HTTP-сервер."""
        self.setup_app()
        self.app.run(host=host, port=port, debug=debug)
    
    def is_authenticated(self):
        """Проверяет наличие куки аутентификации."""
        cookie = request.cookies.get('AUTH')
        return cookie == 'true'
    
    def need_auth(self, content_type):
        """Определяет, требуется ли аутентификация для данного типа контента."""
        return any(content_type.startswith(t) for t in ['text/html', 'application/json', 'text/plain'])
    
    def get_content_type(self, path):
        """Возвращает MIME-тип на основе расширения файла."""
        if path.endswith(('.html', '.htm')):
            return 'text/html; charset=utf-8'
        if path.endswith('.json'):
            return 'application/json'
        if path.endswith('.js'):
            return 'application/javascript'
        if path.endswith('.css'):
            return 'text/css'
        if path.endswith('.png'):
            return 'image/png'
        if path.endswith('.svg'):
            return 'image/svg+xml'
        if path.endswith('.ico'):
            return 'image/x-icon'
        if path.endswith(('.jpg', '.jpeg')):
            return 'image/jpeg'
        if path.endswith('.gif'):
            return 'image/gif'
        if path.endswith('.txt'):
            return 'text/plain; charset=utf-8'
        if path.endswith('.woff2'):
            return 'font/woff2'
        if path.endswith('.woff'):
            return 'font/woff'
        return 'application/octet-stream'
    
    def handle_auth(self):
        """Обрабатывает вход и выход пользователя."""
        cmd = request.form.get('cmd', '')
        if cmd == 'LOGOUT':
            logger.info("User logged out")
            response = make_response(jsonify({"result": "logout"}))
            response.delete_cookie('AUTH')
            return response
        
        password = request.form.get('password', '')
        pass1 = self.config.get('config2', {}).get('ESP_PASS1', '')
        pass2 = self.config.get('config2', {}).get('ESP_PASS2', '')
        pass3 = self.config.get('config2', {}).get('ESP_PASS3', '')
        
        if password in (pass1, pass2, pass3):
            self.is_authenticated_flag = True
            logger.info("Authentication successful")
            response = make_response(jsonify({"result": "ok"}))
            response.set_cookie('AUTH', 'true', httponly=True, path='/')
            return response
        else:
            logger.error("Authentication failed")
            return jsonify({"result": "bad_password"}), 401
    
    def handle_file(self, path=''):
        """Обрабатывает запросы к статическим файлам с проверкой аутентификации."""
        if not path:
            path = 'index.html'
        if self.http_redirect():
            return
        
        # Блокируем доступ к странице обновления прошивки
        if path == 'update.html':
            self.set_responcse(WebResponse.msg("Эта функция работает только на устройстве", "warning"))
#            return jsonify({"status": "ok"}), 200        
            return "", 204        
        
        content_type = self.get_content_type(path)
        is_login_page = path in ('login.html', 'auth')
        
        if not is_login_page and self.need_auth(content_type) and not self.is_authenticated():
            logger.info("Redirecting to login page")
            return redirect('/login.html')
        
        path = path.lstrip('/')
        file_path = os.path.join(self.web_root, path)
        
        if not os.path.exists(file_path):
            logger.error(f"File not found: {file_path}")
            return self.http_notfound(path)
        if os.path.isdir(file_path):
            logger.error(f"Path is directory: {file_path}")
            return self.http_notfound(path)
        
        logger.info(f"Serving file: {file_path} [{content_type}]")
        return send_file(file_path, mimetype=content_type)
    
    def handle_config_selector(self):
        """Отдаёт файл config.json."""
        config_file_path = self.config_file
#        config_file_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), self.cfg.device_config)
        logger.info(f"!!! Open config file: {config_file_path}")
        
        if not os.path.exists(config_file_path):
            logger.error(f"Config file not found: {config_file_path}")
            return jsonify({"error": "Config not found"}), 404
        if os.path.isdir(config_file_path):
            logger.error(f"Config path is directory: {config_file_path}")
            return jsonify({"error": "Invalid config path"}), 404
        
        return send_file(config_file_path, mimetype='application/json')
    
    def handle_command(self):
        """Обрабатывает команды от клиента."""
        logger.info("Command received")
        self.print_arg()
        
        cmd = request.form.get('cmd', '')
        http_code = 200
        output = {}
        
        if cmd == 'wifi_list':
            output = {}
        elif cmd == 'calibrate':
            self.set_responcse(WebResponse.msg("Эта функция доступна только на устройстве", "info", 3000))
            logger.info("Calibration started (emulated)")
            self.system_mp3("89", 85)
        elif cmd == 'play_once':
            if self.is_first_play:
                self.http_play_mp3()
            self.is_first_play = False
        elif cmd == 'play':
            self.http_play_mp3()
        elif cmd == 'default':
#            self.set_responcse(WebResponse.msg("Эта ", "success", 3000))
            logger.info("Resetting to factory defaults (emulated)")
            self.config_set_default()
            self.system_mp3("89", 91)
            self.wait_mp3_and_reboot()
        elif cmd == 'reboot':
            self.set_responcse(WebResponse.msg("Эта функция доступна только на устройстве", "info", 3000))
            logger.info("System reboot requested (emulated)")
            self.system_mp3("89", 86)
#            self.wait_mp3_and_reboot()
        elif cmd == 'device':
            device_name = request.form.get('device_name', '')
            if device_name:
                self.strID = device_name
                self.serNo = request.form.get('serial_no', '')
#                self.tb_sync.current_uuid = request.form.get('config_uuid', '')
                logger.info(f"Opening config for device: {device_name}")
                if self.tb_sync:
                    self.tb_sync.init_device(device_name)
                output = {"result": "ok"}
            else:
                http_code = 400
                output = {"result": "error", "message": "Device name required"}
        elif cmd == 'config':
            config_name = request.form.get('config', '')
            if config_name:
                for idx, cfg in enumerate(self.config_selector):
                    if cfg.get('file', '').find(config_name) != -1:
                        self.active_config = idx
                        break
                logger.info(f"Active config changed to: {config_name}")
                self.config_read()
            else:
                http_code = 400
        elif cmd == 'config_name':
            output = {"value": "Test configuration"}
        
        logger.debug(f"Command response: [{http_code}] {output}")
        return jsonify(output), http_code
    
    def handle_save(self):
        """Сохраняет конфигурацию по запросу от клиента."""
        if 'mode' not in request.form:
            return jsonify({"status": "Error: missing mode"})
        
        mode = request.form.get('mode')
        logger.info(f"Save mode: {mode}")
        
        if mode == 'save':
            self.save_config_data()
            logger.info("Configuration saved")
        
        self.print_arg()
        return jsonify({"status": "ok"})
    
    def handle_header(self):
        """Генерирует HTML-заголовок страницы."""
        out = []
        out.append("<p><img src=/img/logo1.png>\n")
        out.append("<hr align=\"left\" width=\"600\">\n")
        out.append(f"<p><b>ID:{self.strID} &nbsp;&nbsp;&nbsp;S/N: {self.serNo} &nbsp;&nbsp;&nbsp;VER: {self.software_v}</b></p>\n")
        out.append("<p><b>WiFi: Emulated RSSI: 0 dBm</b></p>\n")
        return ''.join(out)
    
    def handle_tail(self):
        """Генерирует HTML-подвал страницы."""
        out = []
        out.append("<hr align=\"left\" width=\"600\">\n")
        out.append("<p class=\"t1\">Copyright (C) Miller-Ti, A.Shikharbeev, 2026&nbsp;&nbsp;&nbsp;&nbsp;Made from Russia\n")
        return ''.join(out)
    
    def handle_distance(self):
        """Возвращает эмулированное расстояние до препятствия."""
        distance = 150
        return f"Расстояние от датчика до препятствия сейчас (мм): {distance}"
    
    def set_responcse(self, param: str):
        """Устанавливает строку с запросом для сервера"""
        self.str_response = param

    def handle_response(self):
        """Отправляет накопленный ответ клиенту."""
        if not self.str_response:
            return '', 204
        
#        logger.info(f"Sending: {self.str_response}")
        response_text = self.str_response
        self.str_response = ""
        return response_text
       
    def save_config_data(self):
        """Сохраняет секцию конфигурации в файл и синхронизирует с ThingsBoard."""
        if 'name' not in request.form or 'data' not in request.form:
            logger.error("Missing name or data in save request")
            return
        
        section_name = request.form.get('name')
        json_data = request.form.get('data')
        logger.info(f"Saving section '{section_name}'")
        
        try:
            doc_config = json.loads(json_data)
            if section_name in doc_config:
                if section_name not in self.config:
                    self.config[section_name] = {}
                self.config[section_name].update(doc_config[section_name])
            else:
                self.config.update(doc_config)
            
            with open(self.config_file, 'w') as f:
                json.dump(self.config, f, indent=2)
            
            logger.info("Configuration saved successfully")
            self.tb_sync.send_config_update(self.config)
        except Exception as e:
            logger.error(f"Error saving config: {e}")
    
    def wifi_scan_network(self):
        """Эмулирует сканирование WiFi-сетей."""
        logger.info("Scanning WiFi networks...")
        networks = [
            {"value": "WiFi_Network_1", "name": "WiFi_Network_1[-45dB]"},
            {"value": "WiFi_Network_2", "name": "WiFi_Network_2[-55dB]"},
            {"value": "WiFi_Network_3", "name": "WiFi_Network_3[-65dB]"}
        ]
        logger.debug(f"WiFi scan result: {len(networks)} networks")
        return networks
    
    def http_play_mp3(self):
        """Эмулирует воспроизведение MP3-файла."""
        name = request.form.get('name', '')
        if not name:
            logger.error("Missing 'name' argument for MP3 playback")
            return
        num = int(name)
        arg1 = request.form.get('arg1', '')
        dir_num = 1 if num < 20 else 2
        arg2_val = request.form.get('arg2', '1')
        priority = int(arg2_val) if arg2_val and arg2_val.strip() else 1
        logger.debug(f"Playing MP3: dir={dir_num}, num={num}, priority={priority}")
        if not arg1:
            logger.info(f"Play MP3: {num} (emulated)")
        else:
            logger.info(f"System MP3: {arg1}, {num} (emulated)")
    
    def http_redirect(self):
        """Проверяет необходимость редиректа (заглушка)."""
        return False
    
    def http_notfound(self, path):
        """Возвращает ответ 404 для несуществующих путей."""
        return "Not found", 404
    
    def print_arg(self):
        """Логирует аргументы запроса."""
        args = {}
        if request.method == 'POST':
            args = request.form.to_dict()
        elif request.method == 'GET':
            args = request.args.to_dict()
        
        for key, value in args.items():
            logger.debug(f"Arg: {key} = {value}")
    
    def system_mp3(self, arg1, num, priority=1):
        """Эмулирует системное воспроизведение MP3."""
        logger.info(f"System MP3: {arg1}, {num}, priority={priority} (emulated)")
    
    def config_set_default(self):
        """Сбрасывает конфигурацию к значениям по умолчанию."""
        self.config = {
            "config2": {
                "ESP_PASS1": "admin",
                "ESP_PASS2": "superadmin",
                "ESP_PASS3": "svetoforbox"
            }
        }
        logger.info("Configuration reset to default")
    
    def config_read(self):
        """Загружает конфигурацию из локального файла."""
        try:
            with open(self.config_file, 'r') as f:
                self.config.update(json.load(f))
            logger.info("Configuration loaded from file")
        except:
            logger.warning("No config file found, using defaults")
    
    def wait_mp3_and_reboot(self):
        """Эмулирует ожидание окончания MP3 и перезагрузку."""
        logger.info("Waiting for MP3 to finish and rebooting (emulated)")
        time.sleep(1)
        logger.info("System reboot (emulated)")
    
    def setup_app(self):
        """Настраивает и выводит информацию о запуске HTTP-сервера."""
        logger.info("Starting HTTP server...")
        logger.info(f"Web root directory: {self.web_root}")
        logger.info(f"Upload directory: {self.upload_dir}")
        
        if not os.path.exists(self.web_root):
            logger.warning(f"Web root directory does not exist: {self.web_root}")
            os.makedirs(self.web_root, exist_ok=True)
            logger.info(f"Created web root directory: {self.web_root}")
        
        self.config_read()
        logger.info("HTTP server started successfully")
        
        for rule in self.app.url_map.iter_rules():
            logger.debug(f"Route: {rule}")

#####################################################################################################################################
# Методы для работы с админкой
#####################################################################################################################################
    def handle_admin(self, admin_path=''):
        """Обрабатывает запросы к админ-панели из каталога ./httpd/."""
        if not admin_path:
            admin_path = 'index.html'
              
        content_type = self.get_content_type(admin_path)
              
        admin_path = admin_path.lstrip('/')
        file_path = os.path.join(self.admin_root, admin_path)
        
        if not os.path.exists(file_path):
            logger.error(f"Admin file not found: {file_path}")
            return self.http_notfound(admin_path)

        if os.path.isdir(file_path):
            logger.error(f"Admin path is directory: {file_path}")
            return self.http_notfound(admin_path)
        
        logger.info(f"Serving admin file: {file_path} [{content_type}]")
        return send_file(file_path, mimetype=content_type)            
    
    def handle_config(self, config_path=''):
        """Обрабатывает запросы к админ-панели из каталога ./config/."""
        
        content_type = self.get_content_type(config_path)
              
        config_path = config_path.lstrip('/')
        file_path = os.path.join(self.config_root, config_path)
        
        if not os.path.exists(file_path):
            logger.error(f"Config file not found: {file_path}")
            return self.http_notfound(config_path)
        if os.path.isdir(file_path):
            logger.error(f"Config path is directory: {file_path}")
            return self.http_notfound(config_path)
        
        logger.info(f"Serving admin file: {file_path} [{content_type}]")
        return send_file(file_path, mimetype=content_type)                