import sys
import json
import subprocess
import os
import serial.tools.list_ports
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                             QHBoxLayout, QPushButton, QComboBox, QLabel,
                             QGroupBox, QTextEdit, QMessageBox, QProgressBar,
                             QLineEdit, QTabWidget, QFileDialog, QCheckBox,
                             QDialog, QDialogButtonBox, QFormLayout)
from PyQt5.QtCore import QThread, pyqtSignal, Qt
from PyQt5.QtGui import QFont

# Определяем базовую директорию для ресурсов
def get_base_dir():
    """Возвращает базовую директорию приложения"""
    if getattr(sys, 'frozen', False):
        # Если приложение запущено как скомпилированный .exe
        return sys._MEIPASS
    else:
        # Если приложение запущено как обычный .py скрипт
        return os.path.dirname(os.path.abspath(__file__))

BASE_DIR = get_base_dir()
LIB_DIR = os.path.join(BASE_DIR, 'lib')
if LIB_DIR not in sys.path:
    sys.path.insert(0, LIB_DIR)

from FirmwareInfoClass import FirmwareInfo
from ThingsBoardClientClass import ThingsBoardClient
from HttpdBuilderClass import HttpdBuilder


class SettingsDialog(QDialog):
    """Диалог настроек ThingsBoard"""
    
    def __init__(self, config, parent=None):
        super().__init__(parent)
        self.config = config
        self.setWindowTitle("Настройки ThingsBoard")
        self.setModal(True)
        self.setGeometry(200, 200, 500, 300)
        
        layout = QVBoxLayout()
        form_layout = QFormLayout()
        
        self.server_edit = QLineEdit()
        self.server_edit.setText(config.get('tb_server', ''))
        self.server_edit.setPlaceholderText("http://109.172.115.70:8088")
        form_layout.addRow("Сервер:", self.server_edit)
        
        self.user_edit = QLineEdit()
        self.user_edit.setText(config.get('tb_user', ''))
        self.user_edit.setPlaceholderText("user@example.com")
        form_layout.addRow("Пользователь:", self.user_edit)
        
        self.pass_edit = QLineEdit()
        self.pass_edit.setText(config.get('tb_pass', ''))
        self.pass_edit.setPlaceholderText("password")
        self.pass_edit.setEchoMode(QLineEdit.Password)
        form_layout.addRow("Пароль:", self.pass_edit)
        
        self.profile_edit = QLineEdit()
        self.profile_edit.setText(config.get('tb_profile', ''))
        self.profile_edit.setPlaceholderText("ParkingSensor")
        form_layout.addRow("Профиль устройства:", self.profile_edit)
        
        layout.addLayout(form_layout)
        
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        
        self.setLayout(layout)
    
    def get_settings(self):
        """Возвращает настройки из диалога"""
        return {
            'tb_server': self.server_edit.text().strip(),
            'tb_user': self.user_edit.text().strip(),
            'tb_pass': self.pass_edit.text().strip(),
            'tb_profile': self.profile_edit.text().strip()
        }


class WorkerThread(QThread):
    """Выполняет длительные операции в отдельном потоке"""
    finished = pyqtSignal(bool, str)
    progress = pyqtSignal(int)
    log = pyqtSignal(str)
    serial_read = pyqtSignal(str)
    
    def __init__(self, operation, **kwargs):
        super().__init__()
        self.operation = operation
        self.kwargs = kwargs
        self.base_dir = BASE_DIR
    
    def _get_exe_path(self, exe_name):
        """Находит путь к исполняемому файлу"""
        # Проверяем в базовой директории
        base_path = os.path.join(self.base_dir, exe_name)
        if os.path.exists(base_path):
            return base_path
        
        # Проверяем в текущей директории (где запущен exe)
        current_path = os.path.join(os.path.dirname(sys.executable), exe_name)
        if os.path.exists(current_path):
            return current_path
        
        # Проверяем в PATH
        import shutil
        path_exe = shutil.which(exe_name)
        if path_exe:
            return path_exe
        
        # Возвращаем просто имя (будет искаться в PATH)
        return exe_name
    
    def run(self):
        try:
            self.log.emit(f"Запуск операции: {self.operation}")
            if self.operation == "read_block3":
                result = self._read_block3()
            elif self.operation == "write_block3":
                result = self._write_block3()
            elif self.operation == "flash_firmware":
                result = self._flash_firmware()
            elif self.operation == "flash_littlefs":
                result = self._flash_littlefs()
            elif self.operation == "ota_upload":
                result = self._ota_upload()
            elif self.operation == "ota_upload_httpd":
                result = self._ota_upload_httpd()
            else:
                self.finished.emit(False, "Неизвестная операция")
                return
            self.finished.emit(result[0], result[1] if result[0] else result[1])
        except Exception as e:
            self.finished.emit(False, str(e))
    
    def _read_block3(self):
        """Читает данные из BLOCK3"""
        port = self.kwargs.get('port')
        try:
            espefuse = self._get_exe_path('espefuse.exe')
            cmd = [espefuse, "-p", port, "-c", "auto", "summary"]
            self.log.emit(f"Выполнение команды: {' '.join(cmd)}")
            out = subprocess.check_output(cmd, text=True)
            flag = False
            block3 = []
            for line in out.splitlines():
                if flag:
                    flag = False
                    self.log.emit(f"Обработка строки BLOCK3: {line}")
                    fields = line.split(" ")
                    for field in fields:
                        if len(field) == 2 and all(c in "0123456789abcdefABCDEF" for c in field):
                            ch = chr(int(field, 16))
                            block3.append(ch)
                    block3.reverse()
                    text = ''.join(block3)
                    self.log.emit(f"Прочитан серийный номер: {text}")
                    self.serial_read.emit(text)
                    return (True, f"Серийный номер прочитан: {text}")
                if line.startswith("BLOCK3"):
                    flag = True
                    self.log.emit(f"Найдена строка BLOCK3: {line}")
            return (False, "BLOCK3 не найден")
        except subprocess.CalledProcessError as e:
            return (False, f"Ошибка чтения: {e.output}")
    
    def _write_block3(self):
        """Записывает данные в BLOCK3"""
        port = self.kwargs.get('port')
        serial_number = self.kwargs.get('serial_number')
        if not serial_number:
            return (False, "Серийный номер не задан")
        data_bytes = serial_number.encode('ascii')[:32]
        if len(data_bytes) < 32:
            data_bytes = data_bytes.ljust(32, b'\x00')
        hex_str = "0x" + data_bytes.hex().upper()
        espefuse = self._get_exe_path('espefuse.exe')
        cmd = [espefuse, "--do-not-confirm", "-p", port,
               "-c", "auto", "burn_efuse", "BLOCK3", hex_str]
        try:
            self.log.emit(f"Выполнение команды: {' '.join(cmd)}")
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                return (False, result.stdout)
            self.log.emit(f"Серийный номер '{serial_number}' записан успешно")
            return (True, f"Серийный номер '{serial_number}' записан успешно")
        except subprocess.CalledProcessError as e:
            return (False, str(e.output))
    
    def _flash_firmware(self):
        """Прошивает бинарный файл на ESP32"""
        port = self.kwargs.get('port')
        firmware_path = self.kwargs.get('firmware_path')
        esptool = self._get_exe_path('esptool.exe')
        cmd = [esptool, "-p", port, "-b", "460800",
               "write_flash", "0x10000", firmware_path]
        try:
            self.log.emit(f"Выполнение команды: {' '.join(cmd)}")
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                return (False, result.stdout)
            self.log.emit("Прошивка загружена успешно")
            return (True, "Прошивка загружена успешно")
        except subprocess.CalledProcessError as e:
            return (False, str(e.output))
    
    def _flash_littlefs(self):
        """Создает образ LittleFS и загружает на ESP32"""
        port = self.kwargs.get('port')
        fs_path = self.kwargs.get('fs_path')
        if os.path.basename(fs_path) == 'httpd':
            fs_path = os.path.dirname(fs_path)
            self.log.emit(f"Используется родительская директория для FS: {fs_path}")
        mklittlefs = self._get_exe_path('mklittlefs.exe')
        create_cmd = [mklittlefs, "-c", fs_path, "-b", "4096",
                      "-p", "256", "-s", "1441792", "littlefs.bin"]
        try:
            self.log.emit(f"Выполнение команды: {' '.join(create_cmd)}")
            result = subprocess.run(create_cmd, capture_output=True, text=True)
            if result.returncode != 0:
                return (False, f"Ошибка создания FS: {result.stdout}")
            self.log.emit("Образ LittleFS создан успешно")
            esptool = self._get_exe_path('esptool.exe')
            flash_cmd = [esptool, "-p", port, "-b", "921600",
                        "-c", "esp32", "--before", "default_reset", "--after", "hard_reset",
                        "write_flash", "2686976", "littlefs.bin"]
            self.log.emit(f"Выполнение команды: {' '.join(flash_cmd)}")
            result = subprocess.run(flash_cmd, capture_output=True, text=True)
            if result.returncode != 0:
                return (False, f"Ошибка загрузки FS: {result.stdout}")
            self.log.emit("Файловая система загружена успешно")
            return (True, "Файловая система загружена успешно")
        except subprocess.CalledProcessError as e:
            return (False, str(e.output))
    
    def _ota_upload(self):
        """Загружает прошивку через ThingsBoard OTA"""
        firmware_path = self.kwargs.get('firmware_path')
        tb_server = self.kwargs.get('tb_server')
        tb_user = self.kwargs.get('tb_user')
        tb_pass = self.kwargs.get('tb_pass')
        tb_profile = self.kwargs.get('tb_profile')
        try:
            self.log.emit("Чтение информации о прошивке...")
            fw_info = FirmwareInfo(firmware_path)
            fw_name = fw_info.fw_name
            fw_version = fw_info.fw_version
            self.log.emit(f"Прошивка: {fw_name} v{fw_version}")
            self.log.emit(f"Подключение к ThingsBoard: {tb_server}")
            tb = ThingsBoardClient(tb_server, tb_user, tb_pass)
            self.log.emit("Аутентификация...")
            if not tb.auth():
                return (False, "Ошибка аутентификации на ThingsBoard")
            self.log.emit("✅ Аутентификация успешна")
            self.log.emit(f"Поиск профиля устройства: {tb_profile}")
            if not tb.get_device_profile_id(tb_profile):
                return (False, f"Профиль устройства '{tb_profile}' не найден")
            self.log.emit(f"✅ Найден профиль: {tb.device_profile_name} ({tb.device_profile_id})")
            self.log.emit(f"Проверка наличия OTA пакета: {fw_name} v{fw_version}")
            exists = tb.check_ota_package(fw_name, fw_version)
            if exists:
                self.log.emit(f"⚠️ Пакет уже существует, удаляем...")
                if not tb.delete_ota_package(fw_name, fw_version):
                    return (False, "Ошибка удаления существующего пакета")
                self.log.emit("✅ Пакет удален")
            self.log.emit("Регистрация нового OTA пакета...")
            if not tb.insert_ota_package(fw_name, fw_version, f"{fw_name}_{fw_version}", "FIRMWARE"):
                return (False, "Ошибка регистрации OTA пакета")
            self.log.emit(f"✅ Пакет зарегистрирован, ID: {tb.package_id}")
            self.log.emit(f"Загрузка файла прошивки: {os.path.basename(firmware_path)}")
            if not tb.upload_ota_file(firmware_path):
                return (False, "Ошибка загрузки файла прошивки")
            self.log.emit("✅ Файл прошивки загружен")
            self.log.emit("Регистрация OTA пакета в профиле устройства...")
            if not tb.register_ota_to_device_profile():
                return (False, "Ошибка регистрации OTA пакета в профиле устройства")
            self.log.emit("✅ OTA пакет зарегистрирован в профиле устройства")
            return (True, f"Прошивка {fw_name} v{fw_version} успешно загружена в ThingsBoard OTA")
        except Exception as e:
            return (False, f"Ошибка OTA загрузки: {str(e)}")
    
    def _ota_upload_httpd(self):
        """Загружает контент HTTPD через ThingsBoard OTA"""
        httpd_dir = self.kwargs.get('httpd_dir')
        tb_server = self.kwargs.get('tb_server')
        tb_user = self.kwargs.get('tb_user')
        tb_pass = self.kwargs.get('tb_pass')
        tb_profile = self.kwargs.get('tb_profile')
        try:
            self.log.emit("Чтение информации о контенте...")
            tmp_dir = os.path.join(os.path.dirname(httpd_dir), 'tmp')
            os.makedirs(tmp_dir, exist_ok=True)
            tar_file = os.path.join(tmp_dir, 'httpd.tar')
            self.log.emit(f"Создание архива из: {httpd_dir}")
            httpd = HttpdBuilder(httpd_dir, tar_file)
            httpd.build()
            self.log.emit(f"Архив создан: {tar_file}")
            self.log.emit(f"Имя контента: {httpd.name}")
            self.log.emit(f"Версия контента: {httpd.version}")
            self.log.emit(f"Подключение к ThingsBoard: {tb_server}")
            tb = ThingsBoardClient(tb_server, tb_user, tb_pass)
            self.log.emit("Аутентификация...")
            if not tb.auth():
                return (False, "Ошибка аутентификации на ThingsBoard")
            self.log.emit("✅ Аутентификация успешна")
            self.log.emit(f"Поиск профиля устройства: {tb_profile}")
            if not tb.get_device_profile_id(tb_profile):
                return (False, f"Профиль устройства '{tb_profile}' не найден")
            self.log.emit(f"✅ Найден профиль: {tb.device_profile_name} ({tb.device_profile_id})")
            self.log.emit(f"Проверка наличия OTA пакета: {httpd.name} v{httpd.version}")
            exists = tb.check_ota_package(httpd.name, httpd.version)
            if exists:
                self.log.emit(f"⚠️ Пакет уже существует, удаляем...")
                if not tb.delete_ota_package(httpd.name, httpd.version):
                    return (False, "Ошибка удаления существующего пакета")
                self.log.emit("✅ Пакет удален")
            self.log.emit("Регистрация нового OTA пакета...")
            if not tb.insert_ota_package(httpd.name, httpd.version, f"{httpd.name}_{httpd.version}", "SOFTWARE"):
                return (False, "Ошибка регистрации OTA пакета")
            self.log.emit(f"✅ Пакет зарегистрирован, ID: {tb.package_id}")
            self.log.emit(f"Загрузка файла контента: {os.path.basename(tar_file)}")
            if not tb.upload_ota_file(tar_file):
                return (False, "Ошибка загрузки файла контента")
            self.log.emit("✅ Файл контента загружен")
            self.log.emit("Регистрация OTA пакета в профиле устройства...")
            if not tb.register_ota_to_device_profile():
                return (False, "Ошибка регистрации OTA пакета в профиле устройства")
            self.log.emit("✅ OTA пакет зарегистрирован в профиле устройства")
            try:
                os.remove(tar_file)
                self.log.emit("Временный файл удален")
            except:
                pass
            return (True, f"Контент {httpd.name} v{httpd.version} успешно загружен в ThingsBoard OTA")
        except Exception as e:
            return (False, f"Ошибка OTA загрузки контента: {str(e)}")


class ESP32Flasher(QMainWindow):
    """Главное окно программы для работы с ESP32"""
    
    def __init__(self):
        super().__init__()
        self.base_dir = BASE_DIR
        self.config = self._load_config()
        self.worker = None
        self.init_ui()
        self.refresh_ports()
    
    def _get_relative_path(self, path):
        """Преобразует абсолютный путь в относительный относительно базовой директории"""
        if not path:
            return ""
        try:
            base = os.path.dirname(sys.executable) if getattr(sys, 'frozen', False) else self.base_dir
            return os.path.relpath(path, base)
        except ValueError:
            return path
    
    def _get_absolute_path(self, path):
        """Преобразует относительный путь в абсолютный относительно базовой директории"""
        if not path:
            return ""
        if os.path.isabs(path):
            return path
        # Сначала проверяем в базовой директории (MEIPASS для exe)
        base_path = os.path.join(self.base_dir, path)
        if os.path.exists(base_path):
            return base_path
        # Если не найден, проверяем в текущей директории
        current_path = os.path.join(os.path.dirname(sys.executable), path)
        if os.path.exists(current_path):
            return current_path
        # Возвращаем путь относительно base_dir
        return os.path.join(self.base_dir, path)
    
    def _load_config(self):
        """Загружает конфигурацию из JSON файла"""
        # Проверяем в базовой директории
        config_path = os.path.join(self.base_dir, "config", "esp32.json")
        try:
            with open(config_path, 'r') as f:
                config = json.load(f)
                if 'firmware_path' in config:
                    config['firmware_path_abs'] = self._get_absolute_path(config['firmware_path'])
                if 'filesystem_path' in config:
                    config['filesystem_path_abs'] = self._get_absolute_path(config['filesystem_path'])
                if 'httpd_path' in config:
                    config['httpd_path_abs'] = self._get_absolute_path(config['httpd_path'])
                return config
        except FileNotFoundError:
            # Пробуем найти конфиг в текущей директории
            alt_path = os.path.join(os.path.dirname(sys.executable), "config", "esp32.json")
            if os.path.exists(alt_path):
                with open(alt_path, 'r') as f:
                    config = json.load(f)
                    if 'firmware_path' in config:
                        config['firmware_path_abs'] = self._get_absolute_path(config['firmware_path'])
                    if 'filesystem_path' in config:
                        config['filesystem_path_abs'] = self._get_absolute_path(config['filesystem_path'])
                    if 'httpd_path' in config:
                        config['httpd_path_abs'] = self._get_absolute_path(config['httpd_path'])
                    return config
            return {}
        except json.JSONDecodeError as e:
            QMessageBox.critical(None, "Ошибка", f"Ошибка парсинга JSON: {e}")
            return {}
    
    def _save_config(self):
        """Сохраняет конфигурацию в JSON файл"""
        # Сохраняем в директорию, где запущен exe
        save_dir = os.path.dirname(sys.executable) if getattr(sys, 'frozen', False) else self.base_dir
        config_path = os.path.join(save_dir, "config", "esp32.json")
        try:
            os.makedirs(os.path.dirname(config_path), exist_ok=True)
            save_config = {}
            if 'firmware_path' in self.config:
                save_config['firmware_path'] = self._get_relative_path(self.config['firmware_path'])
            if 'filesystem_path' in self.config:
                save_config['filesystem_path'] = self._get_relative_path(self.config['filesystem_path'])
            if 'httpd_path' in self.config:
                save_config['httpd_path'] = self._get_relative_path(self.config['httpd_path'])
            if 'tb_server' in self.config:
                save_config['tb_server'] = self.config['tb_server']
            if 'tb_user' in self.config:
                save_config['tb_user'] = self.config['tb_user']
            if 'tb_pass' in self.config:
                save_config['tb_pass'] = self.config['tb_pass']
            if 'tb_profile' in self.config:
                save_config['tb_profile'] = self.config['tb_profile']
            with open(config_path, 'w') as f:
                json.dump(save_config, f, indent=4, ensure_ascii=False)
            self.log_text.append(f"✅ Конфигурация сохранена в {config_path}")
            return True
        except Exception as e:
            self.log_text.append(f"❌ Ошибка сохранения: {e}")
            QMessageBox.critical(self, "Ошибка", f"Не удалось сохранить конфигурацию: {e}")
            return False
    
    def _read_firmware_info(self, file_path):
        """Извлекает название, версию и размер из файла прошивки используя FirmwareInfo"""
        try:
            file_size = os.path.getsize(file_path)
            fw_info = FirmwareInfo(file_path)
            if file_size < 1024:
                size_str = f"{file_size} B"
            elif file_size < 1024 * 1024:
                size_str = f"{file_size / 1024:.2f} KB"
            else:
                size_str = f"{file_size / (1024 * 1024):.2f} MB"
            return {
                'name': fw_info.fw_name if hasattr(fw_info, 'fw_name') else "Неизвестно",
                'version': fw_info.fw_version if hasattr(fw_info, 'fw_version') else "Неизвестно",
                'project_name': fw_info.project_name if hasattr(fw_info, 'project_name') else "Неизвестно",
                'size': size_str,
                'size_bytes': file_size,
                'build_date': fw_info.build_date if hasattr(fw_info, 'build_date') else "Неизвестно",
                'build_time': fw_info.build_time if hasattr(fw_info, 'build_time') else "Неизвестно"
            }
        except Exception as e:
            self.log_text.append(f"⚠️ Ошибка чтения информации о прошивке: {e}")
            return {
                'name': "Ошибка чтения",
                'version': "Ошибка чтения",
                'project_name': "Ошибка чтения",
                'size': "Ошибка чтения",
                'size_bytes': 0,
                'build_date': "Ошибка чтения",
                'build_time': "Ошибка чтения"
            }
    
    def _read_httpd_info(self, dir_path):
        """Извлекает информацию о контенте из version.json"""
        try:
            version_file = os.path.join(dir_path, 'version.json')
            if os.path.exists(version_file):
                with open(version_file, 'r') as f:
                    data = json.load(f)
                return {
                    'name': data.get('name', 'Неизвестно'),
                    'version': data.get('version', 'Неизвестно')
                }
            return {
                'name': 'Неизвестно',
                'version': 'Неизвестно'
            }
        except Exception as e:
            self.log_text.append(f"⚠️ Ошибка чтения информации о контенте: {e}")
            return {
                'name': 'Ошибка чтения',
                'version': 'Ошибка чтения'
            }
    
    def init_ui(self):
        """Инициализация интерфейса пользователя"""
        self.setWindowTitle("ESP32 Flasher")
        self.setGeometry(100, 100, 950, 750)
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout()
        central_widget.setLayout(main_layout)
        
        # Верхняя панель с выбором порта и настройками
        top_layout = QHBoxLayout()
        
        # Группа COM-порта
        port_group = QGroupBox("COM-порт")
        port_layout = QHBoxLayout()
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(120)
        port_layout.addWidget(self.port_combo)
        
        self.refresh_btn = QPushButton("Обновить")
        self.refresh_btn.clicked.connect(self.refresh_ports)
        port_layout.addWidget(self.refresh_btn)
        port_group.setLayout(port_layout)
        top_layout.addWidget(port_group)
        
        # Группа ThingsBoard
        tb_group = QGroupBox("ThingsBoard OTA")
        tb_layout = QHBoxLayout()
        self.tb_status_label = QLabel()
        self.update_tb_status()
        tb_layout.addWidget(self.tb_status_label)
        
        self.tb_settings_btn = QPushButton("Настройки")
        self.tb_settings_btn.clicked.connect(self.show_tb_settings)
        tb_layout.addWidget(self.tb_settings_btn)
        
        tb_group.setLayout(tb_layout)
        top_layout.addWidget(tb_group)
        
        top_layout.addStretch()
        main_layout.addLayout(top_layout)
        
        # Вкладки с операциями
        self.tabs = QTabWidget()
        
        # Вкладка 1: Обновление прошивки
        firmware_tab = QWidget()
        firmware_layout = QVBoxLayout()
        firmware_tab.setLayout(firmware_layout)
        
        fw_group = QGroupBox("Обновление прошивки")
        fw_layout = QVBoxLayout()
        
        # Выбор файла прошивки
        fw_file_layout = QHBoxLayout()
        fw_file_layout.addWidget(QLabel("Файл прошивки:"))
        
        display_path = self.config.get('firmware_path', 'Не выбран')
        self.fw_path_label = QLabel(display_path)
        self.fw_path_label.setStyleSheet("border: 1px solid #ccc; padding: 5px;")
        self.fw_path_label.setMinimumWidth(300)
        fw_file_layout.addWidget(self.fw_path_label)
        
        self.fw_browse_btn = QPushButton("Обзор...")
        self.fw_browse_btn.clicked.connect(self.browse_firmware)
        fw_file_layout.addWidget(self.fw_browse_btn)
        fw_layout.addLayout(fw_file_layout)
        
        # Информация о прошивке
        info_group = QGroupBox("Информация о прошивке")
        info_layout = QVBoxLayout()
        
        info_name_layout = QHBoxLayout()
        info_name_layout.addWidget(QLabel("Название FW:"))
        self.fw_name_label = QLabel("Не выбрано")
        self.fw_name_label.setStyleSheet("border: 1px solid #ccc; padding: 5px; background-color: #f0f0f0;")
        info_name_layout.addWidget(self.fw_name_label)
        info_layout.addLayout(info_name_layout)
        
        info_version_layout = QHBoxLayout()
        info_version_layout.addWidget(QLabel("Версия FW:"))
        self.fw_version_label = QLabel("Не выбрано")
        self.fw_version_label.setStyleSheet("border: 1px solid #ccc; padding: 5px; background-color: #f0f0f0;")
        info_version_layout.addWidget(self.fw_version_label)
        info_layout.addLayout(info_version_layout)
        
        info_project_layout = QHBoxLayout()
        info_project_layout.addWidget(QLabel("Проект:"))
        self.fw_project_label = QLabel("Не выбрано")
        self.fw_project_label.setStyleSheet("border: 1px solid #ccc; padding: 5px; background-color: #f0f0f0;")
        info_project_layout.addWidget(self.fw_project_label)
        info_layout.addLayout(info_project_layout)
        
        info_size_layout = QHBoxLayout()
        info_size_layout.addWidget(QLabel("Размер:"))
        self.fw_size_label = QLabel("Не выбрано")
        self.fw_size_label.setStyleSheet("border: 1px solid #ccc; padding: 5px; background-color: #f0f0f0;")
        info_size_layout.addWidget(self.fw_size_label)
        info_layout.addLayout(info_size_layout)
        
        info_date_layout = QHBoxLayout()
        info_date_layout.addWidget(QLabel("Дата сборки:"))
        self.fw_date_label = QLabel("Не выбрано")
        self.fw_date_label.setStyleSheet("border: 1px solid #ccc; padding: 5px; background-color: #f0f0f0;")
        info_date_layout.addWidget(self.fw_date_label)
        info_layout.addLayout(info_date_layout)
        
        info_group.setLayout(info_layout)
        fw_layout.addWidget(info_group)
        
        # Кнопки действий
        buttons_layout = QHBoxLayout()
        self.flash_btn = QPushButton("Загрузить прошивку (USB)")
        self.flash_btn.clicked.connect(self.start_flash_firmware)
        buttons_layout.addWidget(self.flash_btn)
        
        self.ota_btn = QPushButton("Загрузить прошивку (OTA)")
        self.ota_btn.clicked.connect(self.start_ota_upload)
        buttons_layout.addWidget(self.ota_btn)
        fw_layout.addLayout(buttons_layout)
        
        fw_group.setLayout(fw_layout)
        firmware_layout.addWidget(fw_group)
        firmware_layout.addStretch()
        
        self.tabs.addTab(firmware_tab, "Обновление прошивки")
        
        # Если в конфиге есть путь, читаем информацию
        if self.config.get('firmware_path_abs') and os.path.exists(self.config.get('firmware_path_abs')):
            fw_info = self._read_firmware_info(self.config['firmware_path_abs'])
            self.fw_name_label.setText(fw_info['name'])
            self.fw_version_label.setText(fw_info['version'])
            self.fw_project_label.setText(fw_info['project_name'])
            self.fw_size_label.setText(fw_info['size'])
            self.fw_date_label.setText(f"{fw_info['build_date']} {fw_info['build_time']}")
        
        # Вкладка 2: Обновление контента
        content_tab = QWidget()
        content_layout = QVBoxLayout()
        content_tab.setLayout(content_layout)
        
        content_group = QGroupBox("Обновление контента")
        content_layout_inner = QVBoxLayout()
        
        # Выбор каталога с контентом
        content_path_layout = QHBoxLayout()
        content_path_layout.addWidget(QLabel("Каталог контента:"))
        
        display_path = self.config.get('httpd_path', 'Не выбран')
        self.content_path_label = QLabel(display_path)
        self.content_path_label.setStyleSheet("border: 1px solid #ccc; padding: 5px;")
        self.content_path_label.setMinimumWidth(300)
        content_path_layout.addWidget(self.content_path_label)
        
        self.content_browse_btn = QPushButton("Обзор...")
        self.content_browse_btn.clicked.connect(self.browse_content)
        content_path_layout.addWidget(self.content_browse_btn)
        content_layout_inner.addLayout(content_path_layout)
        
        # Информация о контенте
        content_info_group = QGroupBox("Информация о контенте")
        content_info_layout = QVBoxLayout()
        
        content_name_layout = QHBoxLayout()
        content_name_layout.addWidget(QLabel("Название:"))
        self.content_name_label = QLabel("Не выбрано")
        self.content_name_label.setStyleSheet("border: 1px solid #ccc; padding: 5px; background-color: #f0f0f0;")
        content_name_layout.addWidget(self.content_name_label)
        content_info_layout.addLayout(content_name_layout)
        
        content_version_layout = QHBoxLayout()
        content_version_layout.addWidget(QLabel("Версия:"))
        self.content_version_label = QLabel("Не выбрано")
        self.content_version_label.setStyleSheet("border: 1px solid #ccc; padding: 5px; background-color: #f0f0f0;")
        content_version_layout.addWidget(self.content_version_label)
        content_info_layout.addLayout(content_version_layout)
        
        content_info_group.setLayout(content_info_layout)
        content_layout_inner.addWidget(content_info_group)
        
        # Кнопки действий для контента
        content_buttons_layout = QHBoxLayout()
        
        self.content_flash_btn = QPushButton("Загрузить контент (USB)")
        self.content_flash_btn.clicked.connect(self.start_flash_littlefs)
        content_buttons_layout.addWidget(self.content_flash_btn)
        
        self.content_ota_btn = QPushButton("Загрузить контент (OTA)")
        self.content_ota_btn.clicked.connect(self.start_ota_upload_httpd)
        content_buttons_layout.addWidget(self.content_ota_btn)
        
        content_layout_inner.addLayout(content_buttons_layout)
        
        content_group.setLayout(content_layout_inner)
        content_layout.addWidget(content_group)
        content_layout.addStretch()
        
        self.tabs.addTab(content_tab, "Обновление контента")
        
        # Если в конфиге есть путь, читаем информацию
        if self.config.get('httpd_path_abs') and os.path.exists(self.config.get('httpd_path_abs')):
            info = self._read_httpd_info(self.config['httpd_path_abs'])
            self.content_name_label.setText(info['name'])
            self.content_version_label.setText(info['version'])
        
        # Вкладка 3: Прошивка серийного номера
        serial_tab = QWidget()
        serial_layout = QVBoxLayout()
        serial_tab.setLayout(serial_layout)
        
        serial_group = QGroupBox("Прошивка серийного номера")
        serial_layout_inner = QVBoxLayout()
        
        # Чтение BLOCK3
        read_layout = QHBoxLayout()
        self.read_btn = QPushButton("Прочитать серийный номер")
        self.read_btn.clicked.connect(self.start_read_block3)
        read_layout.addWidget(self.read_btn)
        self.serial_display = QLineEdit()
        self.serial_display.setReadOnly(True)
        self.serial_display.setPlaceholderText("Серийный номер")
        read_layout.addWidget(self.serial_display)
        serial_layout_inner.addLayout(read_layout)
        
        # Запись BLOCK3
        write_layout = QHBoxLayout()
        self.write_btn = QPushButton("Записать серийный номер")
        self.write_btn.clicked.connect(self.start_write_block3)
        write_layout.addWidget(self.write_btn)
        self.serial_input = QLineEdit()
        self.serial_input.setPlaceholderText("Введите серийный номер (до 32 символов)")
        write_layout.addWidget(self.serial_input)
        serial_layout_inner.addLayout(write_layout)
        
        serial_group.setLayout(serial_layout_inner)
        serial_layout.addWidget(serial_group)
        serial_layout.addStretch()
        
        self.tabs.addTab(serial_tab, "Прошивка серийного номера")
        
        main_layout.addWidget(self.tabs)
        
        # Кнопка сохранения конфига
        config_layout = QHBoxLayout()
        config_layout.addStretch()
        self.save_config_btn = QPushButton("💾 Сохранить конфигурацию")
        self.save_config_btn.clicked.connect(self.save_config)
        config_layout.addWidget(self.save_config_btn)
        main_layout.addLayout(config_layout)
        
        # Прогресс-бар
        self.progress = QProgressBar()
        self.progress.setVisible(False)
        main_layout.addWidget(self.progress)
        
        # Лог
        log_group = QGroupBox("Лог операций")
        log_layout = QVBoxLayout()
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setFont(QFont("Courier New", 9))
        log_layout.addWidget(self.log_text)
        log_group.setLayout(log_layout)
        main_layout.addWidget(log_group)
        
        self.update_buttons_state()
    
    def update_tb_status(self):
        """Обновляет отображение статуса ThingsBoard"""
        server = self.config.get('tb_server', '')
        profile = self.config.get('tb_profile', '')
        if server and profile:
            self.tb_status_label.setText(f"Сервер: {server} | Профиль: {profile}")
            self.tb_status_label.setStyleSheet("color: green;")
        else:
            self.tb_status_label.setText("⚠️ Не настроено")
            self.tb_status_label.setStyleSheet("color: red;")
    
    def show_tb_settings(self):
        """Показывает диалог настроек ThingsBoard"""
        dialog = SettingsDialog(self.config, self)
        if dialog.exec_() == QDialog.Accepted:
            settings = dialog.get_settings()
            self.config['tb_server'] = settings['tb_server']
            self.config['tb_user'] = settings['tb_user']
            self.config['tb_pass'] = settings['tb_pass']
            self.config['tb_profile'] = settings['tb_profile']
            self.update_tb_status()
            self.log_text.append("✅ Настройки ThingsBoard обновлены")
    
    def browse_firmware(self):
        """Открывает диалог выбора файла прошивки"""
        start_dir = os.path.dirname(sys.executable) if getattr(sys, 'frozen', False) else self.base_dir
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Выберите файл прошивки", start_dir,
            "Binary files (*.bin);;All files (*.*)"
        )
        if file_path:
            rel_path = self._get_relative_path(file_path)
            self.fw_path_label.setText(rel_path)
            self.config['firmware_path'] = file_path
            self.config['firmware_path_abs'] = file_path
            self.log_text.append(f"Выбран файл прошивки: {rel_path}")
            try:
                fw_info = self._read_firmware_info(file_path)
                self.fw_name_label.setText(fw_info['name'])
                self.fw_version_label.setText(fw_info['version'])
                self.fw_project_label.setText(fw_info['project_name'])
                self.fw_size_label.setText(fw_info['size'])
                self.fw_date_label.setText(f"{fw_info['build_date']} {fw_info['build_time']}")
                self.log_text.append(f"Информация о прошивке: Название={fw_info['name']}, Версия={fw_info['version']}, Проект={fw_info['project_name']}, Размер={fw_info['size']}")
            except Exception as e:
                self.log_text.append(f"⚠️ Ошибка чтения информации: {e}")
                QMessageBox.warning(self, "Предупреждение", f"Не удалось прочитать информацию о прошивке: {e}")
    
    def browse_content(self):
        """Открывает диалог выбора каталога с контентом"""
        start_dir = os.path.dirname(sys.executable) if getattr(sys, 'frozen', False) else self.base_dir
        dir_path = QFileDialog.getExistingDirectory(
            self, "Выберите каталог с контентом", start_dir
        )
        if dir_path:
            rel_path = self._get_relative_path(dir_path)
            self.content_path_label.setText(rel_path)
            self.config['httpd_path'] = dir_path
            self.config['httpd_path_abs'] = dir_path
            self.log_text.append(f"Выбран каталог контента: {rel_path}")
            info = self._read_httpd_info(dir_path)
            self.content_name_label.setText(info['name'])
            self.content_version_label.setText(info['version'])
            self.log_text.append(f"Информация о контенте: Название={info['name']}, Версия={info['version']}")
    
    def browse_filesystem(self):
        """Открывает диалог выбора каталога файловой системы"""
        start_dir = os.path.dirname(sys.executable) if getattr(sys, 'frozen', False) else self.base_dir
        dir_path = QFileDialog.getExistingDirectory(
            self, "Выберите каталог файловой системы", start_dir
        )
        if dir_path:
            rel_path = self._get_relative_path(dir_path)
            self.content_path_label.setText(rel_path)
            self.config['filesystem_path'] = dir_path
            self.config['filesystem_path_abs'] = dir_path
            self.log_text.append(f"Выбран каталог FS: {rel_path}")
    
    def save_config(self):
        """Сохраняет конфигурацию в JSON файл"""
        if self._save_config():
            QMessageBox.information(self, "Успех", "Конфигурация сохранена")
    
    def refresh_ports(self):
        """Обновляет список доступных COM-портов"""
        self.port_combo.clear()
        ports = [port.device for port in serial.tools.list_ports.comports()]
        if ports:
            self.port_combo.addItems(ports)
        else:
            self.port_combo.addItem("Нет доступных портов")
        self.update_buttons_state()
    
    def update_buttons_state(self):
        """Обновляет состояние кнопок в зависимости от выбранного порта"""
        enabled = self.port_combo.currentText() != "Нет доступных портов"
        self.flash_btn.setEnabled(enabled)
        self.content_flash_btn.setEnabled(enabled)
        self.read_btn.setEnabled(enabled)
        self.write_btn.setEnabled(enabled)
    
    def get_selected_port(self):
        """Возвращает выбранный COM-порт"""
        port = self.port_combo.currentText()
        if port == "Нет доступных портов":
            return None
        return port
    
    def start_operation(self, operation, **kwargs):
        """Запускает операцию в отдельном потоке"""
        if self.worker and self.worker.isRunning():
            QMessageBox.warning(self, "Внимание", "Операция уже выполняется")
            return
        
        port = self.get_selected_port()
        if operation not in ["ota_upload", "ota_upload_httpd"] and not port:
            QMessageBox.warning(self, "Внимание", "Выберите COM-порт")
            return
        
        self.progress.setVisible(True)
        self.progress.setRange(0, 0)
        self.log_text.append(f"\n=== Начало операции: {operation} ===")
        
        self.worker = WorkerThread(operation, port=port, **kwargs)
        self.worker.log.connect(self.append_log)
        self.worker.finished.connect(self.on_operation_finished)
        self.worker.serial_read.connect(self.on_serial_read)
        self.worker.start()
        
        self.set_buttons_enabled(False)
    
    def set_buttons_enabled(self, enabled):
        """Включает/отключает все кнопки"""
        self.flash_btn.setEnabled(enabled)
        self.content_flash_btn.setEnabled(enabled)
        self.content_ota_btn.setEnabled(enabled)
        self.read_btn.setEnabled(enabled)
        self.write_btn.setEnabled(enabled)
        self.refresh_btn.setEnabled(enabled)
        self.fw_browse_btn.setEnabled(enabled)
        self.content_browse_btn.setEnabled(enabled)
        self.save_config_btn.setEnabled(enabled)
        self.ota_btn.setEnabled(enabled)
        self.tb_settings_btn.setEnabled(enabled)
    
    def on_serial_read(self, serial_number):
        """Обработчик получения серийного номера"""
        if serial_number:
            self.serial_display.setText(serial_number)
            self.log_text.append(f"✅ Серийный номер установлен: {serial_number}")
    
    def on_operation_finished(self, success, message):
        """Обработчик завершения операции"""
        self.progress.setVisible(False)
        self.set_buttons_enabled(True)
        self.update_buttons_state()
        
        if success:
            self.log_text.append(f"✅ {message}")
            if "серийный" in message.lower() and "прочитан" in message.lower():
                if not self.serial_display.text():
                    serial = message.split(":")[-1].strip()
                    if serial:
                        self.serial_display.setText(serial)
                        self.log_text.append(f"Серийный номер установлен: {serial}")
            QMessageBox.information(self, "Успех", message)
        else:
            self.log_text.append(f"❌ Ошибка: {message}")
            QMessageBox.critical(self, "Ошибка", message)
        
        self.log_text.append("=== Операция завершена ===\n")
        self.worker = None
    
    def append_log(self, text):
        """Добавляет текст в лог"""
        self.log_text.append(text)
    
    def start_read_block3(self):
        """Запускает чтение BLOCK3"""
        port = self.get_selected_port()
        if not port:
            QMessageBox.warning(self, "Внимание", "Выберите COM-порт")
            return
        msg = QMessageBox()
        msg.setWindowTitle("Чтение серийного номера")
        msg.setText("Выполняется чтение серийного номера из ESP32...")
        msg.setIcon(QMessageBox.Information)
        msg.setStandardButtons(QMessageBox.Ok | QMessageBox.Cancel)
        msg.setDefaultButton(QMessageBox.Ok)
        result = msg.exec()
        if result == QMessageBox.Ok:
            self.start_operation("read_block3")
    
    def start_write_block3(self):
        """Запускает запись BLOCK3"""
        serial_number = self.serial_input.text().strip()
        if not serial_number:
            QMessageBox.warning(self, "Внимание", "Введите серийный номер")
            return
        if len(serial_number) > 32:
            QMessageBox.warning(self, "Внимание",
                              "Серийный номер не должен превышать 32 символа")
            return
        port = self.get_selected_port()
        if not port:
            QMessageBox.warning(self, "Внимание", "Выберите COM-порт")
            return
        msg = QMessageBox()
        msg.setWindowTitle("ВНИМАНИЕ! Действие необратимо! ВНИМАНИЕ!")
        msg.setText(f"Вы точно хотите записать серийный номер \"{serial_number}\" в ESP32?")
        msg.setIcon(QMessageBox.Critical)
        msg.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        msg.setDefaultButton(QMessageBox.No)
        result = msg.exec()
        if result == QMessageBox.Yes:
            self.start_operation("write_block3", serial_number=serial_number)
    
    def start_flash_firmware(self):
        """Запускает прошивку через USB"""
        firmware_path = self.config.get('firmware_path_abs')
        if not firmware_path or not os.path.exists(firmware_path):
            QMessageBox.critical(self, "Ошибка", "Путь к прошивке не выбран или файл не существует")
            return
        try:
            fw_info = self._read_firmware_info(firmware_path)
            self.log_text.append(f"Прошивка: {fw_info['name']} v{fw_info['version']} ({fw_info['size']})")
            self.log_text.append(f"Проект: {fw_info['project_name']}, Собрана: {fw_info['build_date']} {fw_info['build_time']}")
        except:
            pass
        self.start_operation("flash_firmware", firmware_path=firmware_path)
    
    def start_ota_upload(self):
        """Запускает загрузку прошивки через ThingsBoard OTA"""
        firmware_path = self.config.get('firmware_path_abs')
        if not firmware_path or not os.path.exists(firmware_path):
            QMessageBox.critical(self, "Ошибка", "Путь к прошивке не выбран или файл не существует")
            return
        tb_server = self.config.get('tb_server', '')
        tb_user = self.config.get('tb_user', '')
        tb_pass = self.config.get('tb_pass', '')
        tb_profile = self.config.get('tb_profile', '')
        if not tb_server:
            QMessageBox.critical(self, "Ошибка", "Не указан сервер ThingsBoard. Настройте в параметрах.")
            return
        if not tb_user:
            QMessageBox.critical(self, "Ошибка", "Не указан пользователь ThingsBoard. Настройте в параметрах.")
            return
        if not tb_pass:
            QMessageBox.critical(self, "Ошибка", "Не указан пароль ThingsBoard. Настройте в параметрах.")
            return
        if not tb_profile:
            QMessageBox.critical(self, "Ошибка", "Не указан профиль устройства. Настройте в параметрах.")
            return
        try:
            fw_info = self._read_firmware_info(firmware_path)
            self.log_text.append(f"Подготовка OTA загрузки для: {fw_info['name']} v{fw_info['version']}")
        except:
            pass
        self.start_operation("ota_upload",
                           firmware_path=firmware_path,
                           tb_server=tb_server,
                           tb_user=tb_user,
                           tb_pass=tb_pass,
                           tb_profile=tb_profile)
    
    def start_flash_littlefs(self):
        """Запускает создание и загрузку LittleFS через USB"""
        fs_path = self.config.get('httpd_path_abs')
        if not fs_path or not os.path.exists(fs_path):
            QMessageBox.critical(self, "Ошибка", "Путь к контенту не выбран или каталог не существует")
            return
        if os.path.basename(fs_path) == 'httpd':
            fs_path = os.path.dirname(fs_path)
            self.log_text.append(f"Используется родительская директория для FS: {fs_path}")
        self.start_operation("flash_littlefs", fs_path=fs_path)
    
    def start_ota_upload_httpd(self):
        """Запускает загрузку контента через ThingsBoard OTA"""
        httpd_path = self.config.get('httpd_path_abs')
        if not httpd_path or not os.path.exists(httpd_path):
            QMessageBox.critical(self, "Ошибка", "Путь к контенту не выбран или каталог не существует")
            return
        tb_server = self.config.get('tb_server', '')
        tb_user = self.config.get('tb_user', '')
        tb_pass = self.config.get('tb_pass', '')
        tb_profile = self.config.get('tb_profile', '')
        if not tb_server:
            QMessageBox.critical(self, "Ошибка", "Не указан сервер ThingsBoard. Настройте в параметрах.")
            return
        if not tb_user:
            QMessageBox.critical(self, "Ошибка", "Не указан пользователь ThingsBoard. Настройте в параметрах.")
            return
        if not tb_pass:
            QMessageBox.critical(self, "Ошибка", "Не указан пароль ThingsBoard. Настройте в параметрах.")
            return
        if not tb_profile:
            QMessageBox.critical(self, "Ошибка", "Не указан профиль устройства. Настройте в параметрах.")
            return
        info = self._read_httpd_info(httpd_path)
        self.log_text.append(f"Подготовка OTA загрузки контента: {info['name']} v{info['version']}")
        self.start_operation("ota_upload_httpd",
                           httpd_dir=httpd_path,
                           tb_server=tb_server,
                           tb_user=tb_user,
                           tb_pass=tb_pass,
                           tb_profile=tb_profile)


def main():
    app = QApplication(sys.argv)
    app.setStyle('Fusion')
    window = ESP32Flasher()
    window.show()
    sys.exit(app.exec_())


if __name__ == '__main__':
    main()