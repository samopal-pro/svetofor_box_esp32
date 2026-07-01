import sys
import os
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

class DeepSeekTemplateGenerator(QMainWindow):
    def __init__(self):
        super().__init__()
        self.files = []  # Список словарей {name: str, content: str, path: str, relative_path: str}
        self.base_dir = ""  # Базовая директория для относительных путей (корень проекта)
        self.project_structure = ""  # Сгенерированная структура проекта
        self.initUI()
        
    def initUI(self):
        self.setWindowTitle("DeepSeek Template Generator")
        self.setGeometry(100, 100, 1200, 800)
        
        # Центральный виджет
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # ===== Верхняя часть: Описание =====
        desc_label = QLabel("📝 Описание проекта (общий контекст):")
        main_layout.addWidget(desc_label)
        
        self.description_text = QTextEdit()
        self.description_text.setPlaceholderText("Опишите проект в целом, технологии, архитектуру...")
        self.description_text.setMaximumHeight(100)
        main_layout.addWidget(self.description_text)
        
        # ===== Средняя часть: Структура проекта =====
        structure_label = QLabel("📁 Структура проекта (генерируется автоматически, можно редактировать):")
        main_layout.addWidget(structure_label)
        
        # Кнопки для структуры
        structure_btn_layout = QHBoxLayout()
        
        self.generate_structure_btn = QPushButton("🔄 Сгенерировать структуру из файлов")
        self.generate_structure_btn.clicked.connect(self.generate_project_structure)
        self.generate_structure_btn.setToolTip("Обновить структуру на основе добавленных файлов")
        structure_btn_layout.addWidget(self.generate_structure_btn)
        
        self.set_root_btn = QPushButton("📂 Установить корень проекта")
        self.set_root_btn.clicked.connect(self.set_root_directory)
        self.set_root_btn.setToolTip("Установить корневую директорию для относительных путей")
        structure_btn_layout.addWidget(self.set_root_btn)
        
        structure_btn_layout.addStretch()
        main_layout.addLayout(structure_btn_layout)
        
        self.structure_text = QTextEdit()
        self.structure_text.setPlaceholderText("Структура будет сгенерирована автоматически после добавления файлов или каталога...\n\nВы также можете редактировать её вручную.")
        self.structure_text.setMaximumHeight(150)
        self.structure_text.textChanged.connect(self.on_structure_changed)
        main_layout.addWidget(self.structure_text)
        
        # ===== Информация о корне проекта =====
        root_info_layout = QHBoxLayout()
        root_info_layout.addWidget(QLabel("📂 Корень проекта:"))
        self.root_path_label = QLabel("Не установлен")
        self.root_path_label.setStyleSheet("color: #666; font-weight: normal;")
        root_info_layout.addWidget(self.root_path_label)
        root_info_layout.addStretch()
        main_layout.addLayout(root_info_layout)
        
        # ===== Файлы для анализа =====
        files_label = QLabel("📄 Файлы для анализа:")
        main_layout.addWidget(files_label)
        
        # Панель кнопок для файлов
        files_btn_layout = QHBoxLayout()
        
        self.add_file_btn = QPushButton("➕ Добавить файл")
        self.add_file_btn.clicked.connect(self.add_file)
        files_btn_layout.addWidget(self.add_file_btn)
        
        self.add_folder_btn = QPushButton("📁 Вставить каталог")
        self.add_folder_btn.clicked.connect(self.add_folder)
        files_btn_layout.addWidget(self.add_folder_btn)
        
        self.update_all_btn = QPushButton("🔄 Обновить все файлы")
        self.update_all_btn.clicked.connect(self.update_all_files)
        files_btn_layout.addWidget(self.update_all_btn)
        
        self.clear_files_btn = QPushButton("🗑️ Удалить все файлы")
        self.clear_files_btn.clicked.connect(self.clear_files)
        files_btn_layout.addWidget(self.clear_files_btn)
        
        files_btn_layout.addStretch()
        main_layout.addLayout(files_btn_layout)
        
        # Таблица файлов
        self.files_table = QTableWidget()
        self.files_table.setColumnCount(3)
        self.files_table.setHorizontalHeaderLabels(["Относительный путь", "Размер", "Действия"])
        self.files_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.files_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.files_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.files_table.setMaximumHeight(200)
        main_layout.addWidget(self.files_table)
        
        # ===== Задача =====
        task_label = QLabel("🎯 Задача (что нужно сделать):")
        main_layout.addWidget(task_label)
        
        self.task_text = QTextEdit()
        self.task_text.setPlaceholderText("Опишите конкретную задачу, которую нужно решить...")
        self.task_text.setMaximumHeight(100)
        main_layout.addWidget(self.task_text)
        
        # ===== Кнопки действий =====
        actions_layout = QHBoxLayout()
        
        self.generate_btn = QPushButton("🚀 Сформировать шаблон")
        self.generate_btn.clicked.connect(self.generate_template)
        self.generate_btn.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 10px;")
        actions_layout.addWidget(self.generate_btn)
        
        self.copy_btn = QPushButton("📋 Скопировать в буфер")
        self.copy_btn.clicked.connect(self.copy_to_clipboard)
        actions_layout.addWidget(self.copy_btn)
        
        self.clear_btn = QPushButton("🧹 Очистить всё")
        self.clear_btn.clicked.connect(self.clear_all)
        actions_layout.addWidget(self.clear_btn)
        
        actions_layout.addStretch()
        main_layout.addLayout(actions_layout)
        
        # ===== Результат =====
        result_label = QLabel("📤 Сгенерированный шаблон:")
        main_layout.addWidget(result_label)
        
        self.result_text = QTextEdit()
        self.result_text.setReadOnly(True)
        self.result_text.setFont(QFont("Courier New", 10))
        main_layout.addWidget(self.result_text)
        
        # ===== Строка статуса =====
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("✅ Готов к работе")
        
        # Применяем стиль
        self.apply_styles()
        
    def apply_styles(self):
        """Применяем стили для красивого вида"""
        style = """
            QMainWindow {
                background-color: #f0f0f0;
            }
            QTextEdit {
                border: 1px solid #ccc;
                border-radius: 5px;
                padding: 8px;
                background-color: white;
            }
            QTableWidget {
                border: 1px solid #ccc;
                border-radius: 5px;
                background-color: white;
            }
            QTableWidget::item {
                padding: 5px;
            }
            QPushButton {
                border: none;
                border-radius: 5px;
                padding: 8px 15px;
                font-weight: bold;
            }
            QPushButton:hover {
                opacity: 0.8;
            }
            QLabel {
                font-weight: bold;
                margin-top: 5px;
            }
            QStatusBar {
                background-color: #e0e0e0;
                padding: 5px;
                font-weight: bold;
            }
        """
        self.setStyleSheet(style)
    
    def set_status(self, message, is_error=False, is_success=False):
        """Установка сообщения в статус-бар с цветом"""
        if is_error:
            self.status_bar.setStyleSheet("QStatusBar { background-color: #ff6b6b; color: white; }")
        elif is_success:
            self.status_bar.setStyleSheet("QStatusBar { background-color: #51cf66; color: white; }")
        else:
            self.status_bar.setStyleSheet("QStatusBar { background-color: #e0e0e0; color: black; }")
        
        self.status_bar.showMessage(message)
        
        # Таймер для сброса цвета через 3 секунды
        QTimer.singleShot(3000, lambda: self.status_bar.setStyleSheet("QStatusBar { background-color: #e0e0e0; color: black; }"))
    
    def update_root_label(self):
        """Обновление отображения корневой директории"""
        if self.base_dir:
            self.root_path_label.setText(self.base_dir)
            self.root_path_label.setStyleSheet("color: #2b7a2b; font-weight: bold;")
        else:
            self.root_path_label.setText("Не установлен")
            self.root_path_label.setStyleSheet("color: #666; font-weight: normal;")
    
    def set_root_directory(self):
        """Установка корневой директории проекта вручную"""
        folder_path = QFileDialog.getExistingDirectory(
            self,
            "Выберите корневую директорию проекта",
            self.base_dir if self.base_dir else "",
            QFileDialog.ShowDirsOnly
        )
        
        if folder_path:
            self.base_dir = folder_path
            self.update_root_label()
            
            # Обновляем относительные пути для всех файлов
            for file_data in self.files:
                file_data['relative_path'] = self.get_relative_path(file_data['path'])
            
            self.update_table()
            self.generate_project_structure()
            self.set_status(f"✅ Корень проекта установлен: {folder_path}", is_success=True)
    
    def get_relative_path(self, file_path):
        """Получение относительного пути от базовой директории"""
        if self.base_dir:
            try:
                rel_path = os.path.relpath(file_path, self.base_dir)
                # Если путь начинается с .. значит файл вне корня проекта
                if rel_path.startswith('..'):
                    # Используем абсолютный путь как есть
                    return file_path
                return rel_path
            except ValueError:
                return os.path.basename(file_path)
        return os.path.basename(file_path)
    
    def on_structure_changed(self):
        """Сохранение изменений структуры при ручном редактировании"""
        self.project_structure = self.structure_text.toPlainText()
    
    def generate_project_structure(self):
        """Генерация структуры проекта на основе добавленных файлов"""
        if not self.files:
            self.structure_text.setText("Нет файлов для генерации структуры")
            self.project_structure = ""
            self.set_status("⚠️ Нет файлов для генерации структуры", is_error=True)
            return
        
        # Собираем все относительные пути
        paths = [file_data['relative_path'] for file_data in self.files]
        
        # Строим дерево
        tree = {}
        for path in paths:
            # Разбиваем путь на части
            parts = path.split(os.sep)
            current = tree
            for part in parts:
                if part not in current:
                    current[part] = {}
                current = current[part]
        
        # Формируем строку структуры
        def build_tree_string(node, prefix=""):
            lines = []
            items = list(node.items())
            for i, (name, subtree) in enumerate(items):
                is_last = (i == len(items) - 1)
                if is_last:
                    lines.append(f"{prefix}└── {name}")
                    if subtree:
                        lines.extend(build_tree_string(subtree, prefix + "    "))
                else:
                    lines.append(f"{prefix}├── {name}")
                    if subtree:
                        lines.extend(build_tree_string(subtree, prefix + "│   "))
            return lines
        
        structure_lines = build_tree_string(tree)
        
        # Добавляем корневую директорию
        if self.base_dir:
            root_name = os.path.basename(self.base_dir)
        else:
            root_name = "project"
        
        final_structure = f"{root_name}/\n" + "\n".join(structure_lines)
        self.structure_text.setText(final_structure)
        self.project_structure = final_structure
        self.set_status(f"✅ Структура сгенерирована! ({len(self.files)} файлов)", is_success=True)
    
    def add_file(self):
        """Добавление файла через диалог"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, 
            "Выберите файл", 
            "",
            "Все файлы (*);;Python (*.py);;JavaScript (*.js);;HTML (*.html);;CSS (*.css);;JSON (*.json);;Текстовые файлы (*.txt)"
        )
        
        if file_path:
            # Если базовая директория не установлена, предлагаем установить
            if not self.base_dir:
                reply = QMessageBox.question(
                    self,
                    "Корень проекта",
                    f"Установить корень проекта в папку файла?\n{os.path.dirname(file_path)}",
                    QMessageBox.Yes | QMessageBox.No
                )
                if reply == QMessageBox.Yes:
                    self.base_dir = os.path.dirname(file_path)
                    self.update_root_label()
                else:
                    # Предлагаем выбрать корень вручную
                    self.set_root_directory()
                    if not self.base_dir:
                        return
            
            self.add_single_file(file_path)
    
    def add_single_file(self, file_path):
        """Добавление одного файла"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            file_name = os.path.basename(file_path)
            relative_path = self.get_relative_path(file_path)
            
            # Проверяем, есть ли уже такой файл
            for i, file_data in enumerate(self.files):
                if file_data['path'] == file_path:
                    # Обновляем существующий
                    self.files[i] = {
                        'name': file_name,
                        'content': content,
                        'path': file_path,
                        'relative_path': relative_path
                    }
                    self.update_table()
                    self.generate_project_structure()
                    self.set_status(f"✅ Файл '{relative_path}' обновлён!", is_success=True)
                    return
            
            # Добавляем новый файл
            self.files.append({
                'name': file_name,
                'content': content,
                'path': file_path,
                'relative_path': relative_path
            })
            self.update_table()
            self.generate_project_structure()
            self.set_status(f"✅ Файл '{relative_path}' добавлен! (Всего: {len(self.files)})", is_success=True)
            
        except Exception as e:
            self.set_status(f"❌ Ошибка при чтении файла: {str(e)}", is_error=True)
    
    def add_folder(self):
        """Добавление всех файлов из каталога"""
        folder_path = QFileDialog.getExistingDirectory(
            self,
            "Выберите каталог для анализа",
            "",
            QFileDialog.ShowDirsOnly
        )
        
        if not folder_path:
            return
        
        # Сохраняем имя выбранной папки
        folder_name = os.path.basename(folder_path)
        
        # Устанавливаем базовую директорию как родительскую папку
        # чтобы относительные пути включали имя папки
        parent_dir = os.path.dirname(folder_path)
        self.base_dir = parent_dir
        self.update_root_label()
        
        # Собираем все файлы из каталога
        skipped_extensions = {'.pyc', '.pyo', '.exe', '.dll', '.so', '.dylib', '.class', '.o', '.obj', '.pdb'}
        skipped_dirs = {'.git', '__pycache__', 'node_modules', '.venv', 'venv', 'env', '.idea', '.vscode', '.pytest_cache'}
        
        file_count = 0
        skipped_count = 0
        
        for root, dirs, files in os.walk(folder_path):
            # Пропускаем системные директории
            dirs[:] = [d for d in dirs if d not in skipped_dirs]
            
            for file in files:
                file_path = os.path.join(root, file)
                
                # Пропускаем бинарные файлы
                ext = os.path.splitext(file)[1].lower()
                if ext in skipped_extensions:
                    skipped_count += 1
                    continue
                
                # Пропускаем слишком большие файлы (> 1 МБ)
                try:
                    if os.path.getsize(file_path) > 1024 * 1024:
                        skipped_count += 1
                        continue
                except:
                    skipped_count += 1
                    continue
                
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    
                    # Получаем относительный путь от родительской директории
                    # Теперь путь будет включать имя папки: my_app/src/main.py
                    relative_path = os.path.relpath(file_path, parent_dir)
                    
                    # Проверяем, есть ли уже такой файл
                    exists = False
                    for i, file_data in enumerate(self.files):
                        if file_data['path'] == file_path:
                            self.files[i] = {
                                'name': file,
                                'content': content,
                                'path': file_path,
                                'relative_path': relative_path
                            }
                            exists = True
                            break
                    
                    if not exists:
                        self.files.append({
                            'name': file,
                            'content': content,
                            'path': file_path,
                            'relative_path': relative_path
                        })
                    
                    file_count += 1
                    
                except UnicodeDecodeError:
                    # Пропускаем бинарные файлы
                    skipped_count += 1
                    continue
                except Exception as e:
                    skipped_count += 1
                    continue
        
        self.update_table()
        self.generate_project_structure()
        
        if file_count > 0:
            self.set_status(
                f"✅ Добавлено файлов из каталога: {file_count} (пропущено: {skipped_count}, всего: {len(self.files)})", 
                is_success=True
            )
        else:
            self.set_status(
                f"⚠️ Не удалось найти текстовые файлы в каталоге", 
                is_error=True
            )
    
    def update_all_files(self):
        """Обновление содержимого всех файлов"""
        if not self.files:
            self.set_status("⚠️ Нет файлов для обновления", is_error=True)
            return
        
        updated_count = 0
        failed_files = []
        
        for i, file_data in enumerate(self.files):
            try:
                with open(file_data['path'], 'r', encoding='utf-8') as f:
                    content = f.read()
                self.files[i]['content'] = content
                updated_count += 1
            except Exception as e:
                failed_files.append(f"{file_data['relative_path']}: {str(e)}")
        
        self.update_table()
        
        if failed_files:
            msg = f"⚠️ Обновлено: {updated_count}/{len(self.files)}. Ошибки: {', '.join(failed_files)}"
            self.set_status(msg, is_error=True)
        else:
            self.set_status(f"✅ Все {len(self.files)} файлов обновлены!", is_success=True)
    
    def clear_files(self):
        """Удаление всех файлов"""
        if not self.files:
            self.set_status("⚠️ Нет файлов для удаления", is_error=True)
            return
        
        self.files.clear()
        self.base_dir = ""
        self.update_root_label()
        self.update_table()
        self.structure_text.clear()
        self.project_structure = ""
        self.set_status("🗑️ Все файлы удалены", is_success=True)
    
    def remove_file(self, index):
        """Удаление конкретного файла"""
        relative_path = self.files[index]['relative_path']
        del self.files[index]
        self.update_table()
        self.generate_project_structure()
        self.set_status(f"🗑️ Файл '{relative_path}' удалён. Осталось: {len(self.files)}", is_success=True)
    
    def update_table(self):
        """Обновление таблицы файлов"""
        self.files_table.setRowCount(len(self.files))
        
        for i, file_data in enumerate(self.files):
            # Относительный путь
            path_item = QTableWidgetItem(file_data['relative_path'])
            path_item.setFlags(path_item.flags() & ~Qt.ItemIsEditable)
            self.files_table.setItem(i, 0, path_item)
            
            # Размер
            size = len(file_data['content'])
            size_str = f"{size} символов" if size < 1024 else f"{size/1024:.1f} КБ"
            size_item = QTableWidgetItem(size_str)
            size_item.setFlags(size_item.flags() & ~Qt.ItemIsEditable)
            self.files_table.setItem(i, 1, size_item)
            
            # Кнопка удаления
            remove_btn = QPushButton("❌ Удалить")
            remove_btn.clicked.connect(lambda checked, idx=i: self.remove_file(idx))
            self.files_table.setCellWidget(i, 2, remove_btn)
    
    def generate_template(self):
        """Генерация шаблона для DeepSeek"""
        description = self.description_text.toPlainText().strip()
        task = self.task_text.toPlainText().strip()
        
        if not self.files:
            self.set_status("⚠️ Добавьте хотя бы один файл для анализа!", is_error=True)
            return
        
        if not task:
            self.set_status("⚠️ Опишите задачу!", is_error=True)
            return
        
        # Получаем структуру из поля (может быть отредактирована пользователем)
        structure = self.structure_text.toPlainText().strip()
        
        # Формируем шаблон
        template_parts = []
        
        # 1. Описание
        if description:
            template_parts.append("📝 ОПИСАНИЕ ПРОЕКТА:")
            template_parts.append("=" * 50)
            template_parts.append(description)
            template_parts.append("")
        
        # 2. Структура проекта (используем то, что в поле, даже если отредактировано)
        if structure:
            template_parts.append("📁 СТРУКТУРА ПРОЕКТА:")
            template_parts.append("=" * 50)
            template_parts.append(structure)
            template_parts.append("")
        
        # 3. Файлы - используем относительные пути с именем папки
        template_parts.append("📄 ФАЙЛЫ ДЛЯ АНАЛИЗА:")
        template_parts.append("=" * 50)
        
        for file_data in self.files:
            # Используем относительный путь (включает имя папки)
            file_path = file_data['relative_path']
            content = file_data['content']
            
            # Определяем язык для подсветки по расширению
            ext = os.path.splitext(file_data['name'])[1].lower()
            lang_map = {
                '.py': 'python',
                '.js': 'javascript',
                '.html': 'html',
                '.css': 'css',
                '.json': 'json',
                '.xml': 'xml',
                '.java': 'java',
                '.cpp': 'cpp',
                '.c': 'c',
                '.h': 'c',
                '.go': 'go',
                '.rs': 'rust',
                '.ts': 'typescript',
                '.php': 'php',
                '.rb': 'ruby',
                '.sh': 'bash',
                '.txt': 'text',
                '.md': 'markdown',
                '.sql': 'sql',
                '.yml': 'yaml',
                '.yaml': 'yaml',
                '.ini': 'ini',
                '.cfg': 'ini',
                '.conf': 'ini',
                '.toml': 'toml',
                '.gitignore': 'text',
                '.env': 'text',
                '.dockerignore': 'text',
                '.editorconfig': 'text',
            }
            lang = lang_map.get(ext, '')
            
            template_parts.append(f"// ============================================")
            template_parts.append(f"// Файл: {file_path}")
            template_parts.append(f"// ============================================")
            template_parts.append(f"```{lang}")
            template_parts.append(content)
            template_parts.append("```")
            template_parts.append("")
        
        # 4. Задача
        template_parts.append("🎯 ЗАДАЧА:")
        template_parts.append("=" * 50)
        template_parts.append(task)
        
        # Объединяем всё
        result = "\n".join(template_parts)
        self.result_text.setText(result)
        
        self.set_status(f"✅ Шаблон сгенерирован! Файлов: {len(self.files)}, символов: {len(result)}", is_success=True)
    
    def copy_to_clipboard(self):
        """Копирование шаблона в буфер обмена"""
        text = self.result_text.toPlainText()
        if not text:
            self.set_status("⚠️ Сначала сгенерируйте шаблон!", is_error=True)
            return
        
        clipboard = QApplication.clipboard()
        clipboard.setText(text)
        self.set_status(f"✅ Шаблон скопирован в буфер! ({len(text)} символов)", is_success=True)
    
    def clear_all(self):
        """Очистка всех полей"""
        self.description_text.clear()
        self.structure_text.clear()
        self.files.clear()
        self.base_dir = ""
        self.update_root_label()
        self.task_text.clear()
        self.result_text.clear()
        self.project_structure = ""
        self.update_table()
        self.set_status("🧹 Всё очищено", is_success=True)

def main():
    app = QApplication(sys.argv)
    app.setStyle('Fusion')
    
    window = DeepSeekTemplateGenerator()
    window.show()
    
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()