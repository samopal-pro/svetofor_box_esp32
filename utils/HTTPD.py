# ============================================
# Файл: utils/HTTPD.py
# ============================================
"""
HTTP Server Module - Python implementation
Emulates ESP32 Web Server functionality
Copyright (C) 2026
"""
import json
import sys
import os
import time
import threading
from typing import Dict, Any, Optional
from flask import Flask, request, jsonify, send_file, make_response, redirect
from flask_cors import CORS
from werkzeug.utils import secure_filename
import logging
import uuid

# Добавляем путь к папке class
current_dir = os.path.dirname(os.path.abspath(__file__))
class_dir = os.path.join(current_dir, 'class')
if class_dir not in sys.path:
    sys.path.insert(0, class_dir)
    
from ThingsBoardClientClass import ThingsBoardClient

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ======================================================================
# SECTION 0: GLOBAL STATE
# ======================================================================

# Путь к корневой папке с файлами WEB сервера
HTTPD_PATH = "../data/httpd"

# Получаем абсолютный путь
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
WEB_ROOT = os.path.normpath(os.path.join(BASE_DIR, HTTPD_PATH))
UPLOAD_DIR = os.path.join(WEB_ROOT, 'uploads')
CONFIG_FILE = os.path.join(".", 'config.json')

# Конфигурация
TB_URL = "http://109.172.115.70:8088"
TB_USER = "sav@ellips.ru"
TB_PASS = "_Sav59vas"
DEVICE_NAME = "68FE71AB1530"
POLL_INTERVAL = 15  # секунды
CONFIG_UUID_KEY = "ConfigUUID"
CONFIG_KEY = "Config"


# Создаем директории если их нет
os.makedirs(WEB_ROOT, exist_ok=True)
os.makedirs(UPLOAD_DIR, exist_ok=True)

app = Flask(__name__)
CORS(app)

# Global state mimicking ESP32
http_status = ""
str_response = ""
is_authenticated_flag = False
is_first_play = True
active_config = 0

# Configuration storage
config = {
    "config2": {
        "ESP_PASS1": "admin",
        "ESP_PASS2": "user",
        "ESP_PASS3": "guest"
    }
}

config_selector = [
    {"file": "/config/config1.json", "mp3_num": 1},
    {"file": "/config/config2.json", "mp3_num": 2}
]

# Static configuration for web interface
SOFTWARE_V = "1.0.0"
strID = "ESP32_001"
serNo = "SN123456"


import sys
import os
import json
import time
from pathlib import Path
from typing import Optional

# Добавляем путь к папке class
current_dir = os.path.dirname(os.path.abspath(__file__))
class_dir = os.path.join(current_dir, 'class')
if class_dir not in sys.path:
    sys.path.insert(0, class_dir)
    
from ThingsBoardClientClass import ThingsBoardClient

# Конфигурация
TB_URL = "http://109.172.115.70:8088"
TB_USER = "sav@ellips.ru"
TB_PASS = "_Sav59vas"
DEVICE_NAME = "68FE71AB1530"
POLL_INTERVAL = 15  # секунды
CONFIG_FILE = "config.json"
CONFIG_UUID_KEY = "ConfigUUID"
CONFIG_KEY = "Config"

# Глобальные переменные
tb_client    = None
device_token = None
current_uuid = None

def check_updates_loop():
    """Синхронная функция, запущенная в отдельном потоке"""
    while True:
        try:
            if not current_uuid == None:
                print("!!! Check config update")
                check_and_update()
        except Exception as e:
            print(f"Ошибка: {e}")
            
        time.sleep(20)  # Спит только этот поток, Flask продолжает работать

# Создаем и запускаем поток ДО старта Flask

######################################################################################################
# Инициализация подключения к ThingsBoard.
######################################################################################################
def init_connection() -> bool:
    """Инициализация подключения к ThingsBoard."""
    global tb_client, device_token, current_uuid
    
    tb_client = ThingsBoardClient(TB_URL, TB_USER, TB_PASS)
    
    if not tb_client.auth():
        print("❌ Ошибка аутентификации")
        return False
        
    # Получаем ACCESS_TOKEN устройства
    ret = tb_client.get_device_access_token(DEVICE_NAME)
    if not ret:
        print(f"❌ Не удалось получить токен для устройства {DEVICE_NAME}")
        return False
        
    # Получаем начальное значение ConfigUUID
#    data = tb_client.get_device_attributes("ConfigUUID","clientKeys")
#    current_uuid = data["client"]["ConfigUUID"]
    check_and_update() 
    if not current_uuid:
        print("⚠️ Начальное значение ConfigUUID не получено")
        
    print(f"✅ Инициализация завершена. ConfigUUID: {current_uuid}")
    updater_thread = threading.Thread(target=check_updates_loop, daemon=True)
    updater_thread.start()

    return True

######################################################################################################
# Загрузка конфигурации с устройства.
######################################################################################################
def load_config_from_device() -> dict | None:
    """Загрузка конфигурации с устройства."""
    try:
        # Получаем атрибут Config с устройства
        data        = tb_client.get_device_attributes("Config","clientKeys")
        config_data = data["client"]["Config"]
        return config_data           
    except Exception as e:
        print(f"❌ Ошибка загрузки конфигурации: {e}")
        return None

######################################################################################################
# Сохранение конфигурации в файл.
######################################################################################################
def save_config_to_file(config_data: dict) -> bool:
    """Сохранение конфигурации в файл."""
    try:
        with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
            json.dump(config_data, f, indent=2, ensure_ascii=False)
        print(f"✅ Конфигурация сохранена в {CONFIG_FILE}")
        return True
    except Exception as e:
        print(f"❌ Ошибка сохранения конфигурации: {e}")
        return False

######################################################################################################
# Проверка изменений ConfigUUID и обновление конфигурации при необходимости.
######################################################################################################
def check_and_update() -> bool:
    """Проверка изменений ConfigUUID и обновление конфигурации при необходимости."""
    global current_uuid
    new_uuid = None
    try:
    # Получаем начальное значение ConfigUUID
        data = tb_client.get_device_attributes("ConfigUUID","clientKeys")
        new_uuid = data["client"]["ConfigUUID"]
        if not new_uuid:
            print("⚠️ Новое значение ConfigUUID не получено")
            return False
                        
        # Проверяем, изменился ли UUID
        if new_uuid != current_uuid:
            print(f"🔄 Обнаружено изменение ConfigUUID: {current_uuid} -> {new_uuid}")
            
            # Загружаем новую конфигурацию
            config_data = load_config_from_device()
            if config_data:
                # Сохраняем в файл
                if save_config_to_file(config_data):
                    current_uuid = new_uuid
                    print(f"✅ Конфигурация обновлена, новый UUID: {new_uuid}")
                    return True
            else:
                print("❌ Не удалось загрузить новую конфигурацию")
                return False
        else:
            print(f"ℹ️ ConfigUUID не изменился: {current_uuid}")
            
        return False
        
    except Exception as e:
        print(f"❌ Ошибка при проверке обновлений: {e}")
        return False


# ======================================================================
# SECTION 1: AUTHENTICATION
# ======================================================================

def is_authenticated() -> bool:
    """Check if user is authenticated via cookie"""
    cookie = request.cookies.get('AUTH')
    return cookie == 'true'

def need_auth(content_type: str) -> bool:
    """Check if requested content requires authentication"""
    return any(content_type.startswith(t) for t in ['text/html', 'application/json', 'text/plain'])

@app.route('/auth', methods=['POST'])
def handle_auth():
    """Handle authentication request"""
    global is_authenticated_flag
    
    cmd = request.form.get('cmd', '')
    
    if cmd == 'LOGOUT':
        logger.info("User logged out")
        response = make_response(jsonify({"result": "logout"}))
        response.delete_cookie('AUTH')
        return response
    
    password = request.form.get('password', '')
    pass1 = config.get('config2', {}).get('ESP_PASS1', '')
    pass2 = config.get('config2', {}).get('ESP_PASS2', '')
    pass3 = config.get('config2', {}).get('ESP_PASS3', '')
    
    if password in (pass1, pass2, pass3):
        is_authenticated_flag = True
        logger.info("Authentication successful")
        response = make_response(jsonify({"result": "ok"}))
        response.set_cookie('AUTH', 'true', httponly=True, path='/')
        return response
    else:
        logger.error("Authentication failed")
        return jsonify({"result": "bad_password"}), 401

# ======================================================================
# SECTION 2: FILE SERVING
# ======================================================================

def get_content_type(path: str) -> str:
    """Get MIME type based on file extension"""
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

@app.route('/', defaults={'path': ''})
@app.route('/<path:path>')
def handle_file(path):
    """Serve static files with authentication check"""
    if not path:
        path = 'index.html'
    
    # Check for authentication redirect
    if HTTP_redirect():
        return
    
    # Check authentication for protected content
    content_type = get_content_type(path)
    is_login_page = path in ('login.html', 'auth')
    
    if not is_login_page and need_auth(content_type) and not is_authenticated():
        logger.info("Redirecting to login page")
        return redirect('/login.html')
    
    # Нормализуем путь, убираем ведущие слеши
    path = path.lstrip('/')
    
    # Serve file from web root
    file_path = os.path.join(WEB_ROOT, path)
    
    # Проверяем существование файла
    if not os.path.exists(file_path):
        logger.error(f"File not found: {file_path}")
        return HTTP_notfound(path)
    
    # Проверяем что это файл, а не директория
    if os.path.isdir(file_path):
        logger.error(f"Path is directory: {file_path}")
        return HTTP_notfound(path)
    
    logger.info(f"Serving file: {file_path} [{content_type}]")
    return send_file(file_path, mimetype=content_type)

# ======================================================================
# SECTION 2: FILE SERVING
# ======================================================================

@app.route('/config', methods=['GET'])
def handle_config_selector():
    """Serve configuration file from current directory"""
    # Путь к файлу config.json в текущем каталоге
    config_file_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'config.json')
    
    logger.info(f"Serving config file: {config_file_path}")
    
    if not os.path.exists(config_file_path):
        logger.error(f"Config file not found: {config_file_path}")
        return jsonify({"error": "Config not found"}), 404
    
    if os.path.isdir(config_file_path):
        logger.error(f"Config path is directory: {config_file_path}")
        return jsonify({"error": "Invalid config path"}), 404
    
    return send_file(config_file_path, mimetype='application/json')

# ======================================================================
# SECTION 3: COMMAND HANDLER
# ======================================================================

@app.route('/cmd', methods=['POST'])
def handle_command():
    """Handle various system commands"""
    global is_first_play, active_config
    
    logger.info("Command received")
    print_arg()
    
    cmd = request.form.get('cmd', '')
    http_code = 200
    output = {}
    
    if cmd == 'wifi_list':
#        output = WiFi_scan_network()
        output = {}
    
    elif cmd == 'calibrate':
        logger.info("Calibration started (emulated)")
        system_mp3("89", 85)
    
    elif cmd == 'play_once':
        if is_first_play:
            HTTP_play_mp3()
        is_first_play = False
    
    elif cmd == 'play':
        HTTP_play_mp3()
    
    elif cmd == 'default':
        logger.info("Resetting to factory defaults (emulated)")
        config_set_default()
        system_mp3("89", 91)
        wait_mp3_and_reboot()
    
    elif cmd == 'reboot':
        logger.info("System reboot requested (emulated)")
        system_mp3("89", 86)
        wait_mp3_and_reboot()
    
    elif cmd == 'config':
        config_name = request.form.get('config', '')
        if config_name:
            for idx, cfg in enumerate(config_selector):
                if cfg.get('file', '').find(config_name) != -1:
                    active_config = idx
                    break
            logger.info(f"Active config changed to: {config_name}")
            config_read()
        else:
            http_code = 400
    
    elif cmd == 'config_name':
        output = {"value": "Test configuration"}
    
    logger.debug(f"Command response: [{http_code}] {output}")
    return jsonify(output), http_code

# ======================================================================
# SECTION 4: CONFIGURATION MANAGEMENT
# ======================================================================

@app.route('/save', methods=['POST'])
def handle_save():
    """Save configuration data"""
    if 'mode' not in request.form:
        return jsonify({"status": "Error: missing mode"})
    
    mode = request.form.get('mode')
    logger.info(f"Save mode: {mode}")
    
    if mode == 'save':
        save_config_data()
        logger.info("Configuration saved")
    
    print_arg()
    return jsonify({"status": "ok"})

def save_config_data():
    """Save configuration section data"""
    if 'name' not in request.form or 'data' not in request.form:
        logger.error("Missing name or data in save request")
        return
    
    section_name = request.form.get('name')
    json_data = request.form.get('data')
    
    logger.info(f"Saving section '{section_name}'")
    
    try:
        doc_config = json.loads(json_data)
        
        if section_name in doc_config:
            # Update existing section
            if section_name not in config:
                config[section_name] = {}
            config[section_name].update(doc_config[section_name])
        else:
            # Merge entire config
            config.update(doc_config)
        
        # Save to file
        with open(CONFIG_FILE, 'w') as f:
            json.dump(config, f, indent=2)
        
        logger.info(f"Configuration saved successfully")
        # Send config to ThingsBoard
        current_uuid = str(uuid.uuid4())
        print('!!! UUID', current_uuid)
        tb_client.send_device_attribute(CONFIG_UUID_KEY, current_uuid)
        tb_client.send_device_attribute(CONFIG_KEY,config)


    except Exception as e:
        logger.error(f"Error saving config: {e}")

# ======================================================================
# SECTION 5: UI COMPONENTS
# ======================================================================

@app.route('/header', methods=['GET'])
def handle_header():
    """Return header HTML fragment"""
    out = []
    out.append("<p><img src=/img/logo1.png>\n")
    out.append("<hr align=\"left\" width=\"600\">\n")
    out.append(f"<p><b>ID:{strID} &nbsp;&nbsp;&nbsp;S/N: {serNo} &nbsp;&nbsp;&nbsp;VER: {SOFTWARE_V}</b></p>\n")
    out.append("<p><b>WiFi: Emulated RSSI: -45 dBm</b></p>\n")
    
    return ''.join(out)

@app.route('/tail', methods=['GET'])
def handle_tail():
    """Return footer HTML fragment"""
    out = []
    out.append("<hr align=\"left\" width=\"600\">\n")
    out.append("<p class=\"t1\">Copyright (C) Miller-Ti, A.Shikharbeev, 2026&nbsp;&nbsp;&nbsp;&nbsp;Made from Russia\n")
    return ''.join(out)

@app.route('/distance', methods=['GET'])
def handle_distance():
    """Return distance reading"""
    # Simulate distance reading
    distance = 150  # emulated distance in mm
    return f"Расстояние от датчика до препятствия сейчас (мм): {distance}"

@app.route('/response', methods=['GET'])
def handle_response():
    """Return queued response"""
    global str_response
    
    if not str_response:
        return '', 204
    
    logger.info(f"Sending: {str_response}")
    response_text = str_response
    str_response = ""
    return response_text

# ======================================================================
# SECTION 6: FIRMWARE UPDATE
# ======================================================================

@app.route('/update', methods=['POST'])
def handle_firmware_upload():
    """Handle firmware upload (emulated)"""
    if 'file' not in request.files:
        return "No file uploaded", 400
    
    file = request.files['file']
    if file.filename == '':
        return "No file selected", 400
    
    if not file.filename.endswith('.bin'):
        logger.error(f"Invalid firmware file type: {file.filename}")
        return "Only .bin files allowed", 400
    
    logger.info(f"Firmware update started: {file.filename}")
    
    try:
        filename = secure_filename(file.filename)
        file.save(os.path.join(UPLOAD_DIR, filename))
        logger.info(f"Firmware uploaded: {filename}")
        
        logger.info("Firmware update successful")
        system_mp3("89", 88)
        wait_mp3_and_reboot()
        return "OK reboot", 200
        
    except Exception as e:
        logger.error(f"Firmware update failed: {e}")
        system_mp3("89", 87)
        return "Update failed", 500

# ======================================================================
# SECTION 7: UTILITY FUNCTIONS
# ======================================================================

def WiFi_scan_network() -> Dict[str, Any]:
    """Simulate WiFi network scan"""
    logger.info("Scanning WiFi networks...")
    
    # Simulated networks
    networks = [
        {"value": "WiFi_Network_1", "name": "WiFi_Network_1[-45dB]"},
        {"value": "WiFi_Network_2", "name": "WiFi_Network_2[-55dB]"},
        {"value": "WiFi_Network_3", "name": "WiFi_Network_3[-65dB]"}
    ]
    
    logger.debug(f"WiFi scan result: {len(networks)} networks")
    return networks

def HTTP_play_mp3():
    """Simulate MP3 playback"""
    name = request.form.get('name', '')
    if not name:
        logger.error("Missing 'name' argument for MP3 playback")
        return
    
    num = int(name)
    arg1 = request.form.get('arg1', '')
    
    dir_num = 1 if num < 20 else 2
    priority = int(request.form.get('arg2', 1))
    
    logger.debug(f"Playing MP3: dir={dir_num}, num={num}, priority={priority}")
    
    if not arg1:
        logger.info(f"Play MP3: {num} (emulated)")
    else:
        logger.info(f"System MP3: {arg1}, {num} (emulated)")

def HTTP_redirect() -> bool:
    """Check and handle redirects (emulated)"""
    # In Python server, we don't need IP-based redirect
    return False

def HTTP_notfound(path: str):
    """Return 404 page"""
    html = f"""
    <!doctype html>
    <html>
        <head>
            <meta charset='utf-8'>
            <link rel="stylesheet" href="/style.css">
            <title>404</title>
        </head>
        <body>
            <div class="main">
                <h2>{path} - 404 Not Found</h2>
                <p>File not found in: {WEB_ROOT}</p>
            </div>
        </body>
    </html>
    """
    return html, 404

def print_arg():
    """Debug function to print all request arguments"""
    args = {}
    if request.method == 'POST':
        args = request.form.to_dict()
    elif request.method == 'GET':
        args = request.args.to_dict()
    
    for key, value in args.items():
        logger.debug(f"Arg: {key} = {value}")

# ======================================================================
# SECTION 8: SYSTEM EMULATION FUNCTIONS
# ======================================================================

def system_mp3(arg1: str, num: int, priority: int = 1):
    """Emulate system MP3 playback"""
    logger.info(f"System MP3: {arg1}, {num}, priority={priority} (emulated)")

def config_set_default():
    """Reset configuration to defaults"""
    global config
    config = {
        "config2": {
            "ESP_PASS1": "admin",
            "ESP_PASS2": "user",
            "ESP_PASS3": "guest"
        }
    }
    logger.info("Configuration reset to default")

def config_read():
    """Read configuration from file"""
    try:
        with open(CONFIG_FILE, 'r') as f:
            global config
            config.update(json.load(f))
        logger.info("Configuration loaded from file")
    except:
        logger.warning("No config file found, using defaults")

def wait_mp3_and_reboot():
    """Emulate waiting for MP3 and reboot"""
    logger.info("Waiting for MP3 to finish and rebooting (emulated)")
    time.sleep(1)
    logger.info("System reboot (emulated)")

# ======================================================================
# SECTION 9: MAIN APPLICATION
# ======================================================================

def setup_app():
    """Initialize the application"""
    logger.info("Starting HTTP server...")
    logger.info(f"Web root directory: {WEB_ROOT}")
    logger.info(f"Upload directory: {UPLOAD_DIR}")
    
    # Проверяем существование корневой директории
    if not os.path.exists(WEB_ROOT):
        logger.warning(f"Web root directory does not exist: {WEB_ROOT}")
        os.makedirs(WEB_ROOT, exist_ok=True)
        logger.info(f"Created web root directory: {WEB_ROOT}")
    
    # Load initial config
    config_read()
    
    logger.info("HTTP server started successfully")
    
    # Log all routes
    for rule in app.url_map.iter_rules():
        logger.debug(f"Route: {rule}")

if __name__ == '__main__':
    init_connection()    
    setup_app()
    app.run(host='0.0.0.0', port=80, debug=True)
