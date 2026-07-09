import sys
import os
import re
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *
FONT_FAMILY = "Segoe UI"
CODE_FONT_FAMILY = "Courier New"
FONT_SIZE = 10
CODE_FONT_SIZE = 10
class DeepSeekTemplateGenerator(QMainWindow):
    def __init__(self):
        super().__init__()
        self.files = []
        self.base_dir = ""
        self.project_structure = ""
        self.initUI()
    def initUI(self):
        self.setWindowTitle("DeepSeek Template Generator")
        self.setGeometry(100, 100, 1200, 800)
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        desc_label = QLabel("📝 Описание проекта (общий контекст):")
        main_layout.addWidget(desc_label)
        self.description_text = QTextEdit()
        self.description_text.setAcceptRichText(False)
        self.description_text.setPlaceholderText("Опишите проект в целом, технологии, архитектуру...")
        self.description_text.setMaximumHeight(100)
        self.description_text.setFont(QFont(FONT_FAMILY, FONT_SIZE))
        main_layout.addWidget(self.description_text)
        structure_label = QLabel("📁 Структура проекта (генерируется автоматически, можно редактировать):")
        main_layout.addWidget(structure_label)
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
        self.structure_text.setFont(QFont(CODE_FONT_FAMILY, CODE_FONT_SIZE))
        main_layout.addWidget(self.structure_text)
        root_info_layout = QHBoxLayout()
        root_info_layout.addWidget(QLabel("📂 Корень проекта:"))
        self.root_path_label = QLabel("Не установлен")
        self.root_path_label.setStyleSheet("color: #666; font-weight: normal;")
        root_info_layout.addWidget(self.root_path_label)
        root_info_layout.addStretch()
        main_layout.addLayout(root_info_layout)
        files_label = QLabel("📄 Файлы для анализа:")
        main_layout.addWidget(files_label)
        files_btn_layout = QHBoxLayout()
        self.add_file_btn = QPushButton("➕ Добавить файлы")
        self.add_file_btn.clicked.connect(self.add_files)
        self.add_file_btn.setToolTip("Выбрать один или несколько файлов")
        files_btn_layout.addWidget(self.add_file_btn)
        self.add_folder_btn = QPushButton("📁 Вставить каталоги")
        self.add_folder_btn.clicked.connect(self.add_folders)
        self.add_folder_btn.setToolTip("Выбрать один или несколько каталогов")
        files_btn_layout.addWidget(self.add_folder_btn)
        self.update_all_btn = QPushButton("🔄 Обновить все файлы")
        self.update_all_btn.clicked.connect(self.update_all_files)
        files_btn_layout.addWidget(self.update_all_btn)
        self.clear_files_btn = QPushButton("🗑️ Удалить все файлы")
        self.clear_files_btn.clicked.connect(self.clear_files)
        files_btn_layout.addWidget(self.clear_files_btn)
        files_btn_layout.addStretch()
        main_layout.addLayout(files_btn_layout)
        self.files_table = QTableWidget()
        self.files_table.setColumnCount(3)
        self.files_table.setHorizontalHeaderLabels(["Относительный путь", "Размер", "Действия"])
        self.files_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.files_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.files_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.files_table.setMaximumHeight(200)
        main_layout.addWidget(self.files_table)
        options_layout = QHBoxLayout()
        self.clean_comments_checkbox = QCheckBox("🧹 Очищать комментарии и лишние пробелы в файлах")
        self.clean_comments_checkbox.setToolTip("Удалять комментарии и лишние пробелы/переносы для уменьшения размера шаблона")
        self.clean_comments_checkbox.setChecked(True)
        options_layout.addWidget(self.clean_comments_checkbox)
        options_layout.addStretch()
        main_layout.addLayout(options_layout)
        task_label = QLabel("🎯 Задача (что нужно сделать):")
        main_layout.addWidget(task_label)
        self.task_text = QTextEdit()
        self.task_text.setAcceptRichText(False)
        self.task_text.setPlaceholderText("Опишите конкретную задачу, которую нужно решить...")
        self.task_text.setMaximumHeight(100)
        self.task_text.setFont(QFont(FONT_FAMILY, FONT_SIZE))
        main_layout.addWidget(self.task_text)
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
        result_label = QLabel("📤 Сгенерированный шаблон:")
        main_layout.addWidget(result_label)
        self.result_text = QTextEdit()
        self.result_text.setReadOnly(True)
        self.result_text.setFont(QFont(CODE_FONT_FAMILY, CODE_FONT_SIZE))
        main_layout.addWidget(self.result_text)
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("✅ Готов к работе")
        self.apply_styles()
    def apply_styles(self):
        style = f"""
            QMainWindow {{
                background-color: #f5f5f5;
            }}
            QTextEdit {{
                border: 1px solid #ccc;
                border-radius: 5px;
                padding: 8px;
                background-color: white;
                font-family: {FONT_FAMILY};
                font-size: {FONT_SIZE}pt;
            }}
            QTableWidget {{
                border: 1px solid #ccc;
                border-radius: 5px;
                background-color: white;
            }}
            QTableWidget::item {{
                padding: 5px;
            }}
            QPushButton {{
                border: none;
                border-radius: 5px;
                padding: 8px 15px;
                font-weight: bold;
                font-family: {FONT_FAMILY};
                font-size: {FONT_SIZE}pt;
            }}
            QLabel {{
                font-weight: bold;
                margin-top: 5px;
                font-family: {FONT_FAMILY};
                font-size: {FONT_SIZE}pt;
            }}
            QStatusBar {{
                background-color: #e0e0e0;
                padding: 5px;
                font-weight: bold;
                font-family: {FONT_FAMILY};
                font-size: {FONT_SIZE}pt;
            }}
            QCheckBox {{
                font-family: {FONT_FAMILY};
                font-size: {FONT_SIZE}pt;
            }}
        """
        self.setStyleSheet(style)
    def clean_code_content(self, content, file_path):
        if not self.clean_comments_checkbox.isChecked():
            return content
        lines = content.split('\n')
        cleaned_lines = []
        in_multiline_comment = False
        ext = os.path.splitext(file_path)[1].lower()
        for line in lines:
            stripped = line.strip()
            if not stripped:
                continue
            if ext in ['.py']:
                if '"""' in line or "'''" in line:
                    if line.count('"""') == 2 or line.count("'''") == 2:
                        continue
                    if in_multiline_comment:
                        in_multiline_comment = False
                        continue
                    else:
                        in_multiline_comment = True
                        continue
                if in_multiline_comment:
                    continue
                if '#' in line:
                    in_string = False
                    for i, char in enumerate(line):
                        if char in ['"', "'"] and (i == 0 or line[i-1] != '\\'):
                            in_string = not in_string
                        elif char == '#' and not in_string:
                            line = line[:i]
                            break
                line = line.rstrip()
                if line and not line.startswith(' '):
                    line = re.sub(r' +', ' ', line)
                    cleaned_lines.append(line)
                elif line:
                    cleaned_lines.append(line)
            elif ext in ['.js', '.ts', '.java', '.c', '.cpp', '.h', '.go', '.rs']:
                if '/*' in line and '*/' in line:
                    continue
                if '/*' in line:
                    in_multiline_comment = True
                    continue
                if '*/' in line:
                    in_multiline_comment = False
                    continue
                if in_multiline_comment:
                    continue
                if '//' in line:
                    in_string = False
                    for i, char in enumerate(line):
                        if char in ['"', "'"] and (i == 0 or line[i-1] != '\\'):
                            in_string = not in_string
                        elif char == '/' and i+1 < len(line) and line[i+1] == '/' and not in_string:
                            line = line[:i]
                            break
                line = line.rstrip()
                if line:
                    line = re.sub(r' +', ' ', line)
                    cleaned_lines.append(line)
            elif ext in ['.html', '.xml']:
                if '<!--' in line and '-->' in line:
                    continue
                if '<!--' in line:
                    in_multiline_comment = True
                    continue
                if '-->' in line:
                    in_multiline_comment = False
                    continue
                if in_multiline_comment:
                    continue
                if line:
                    line = re.sub(r'>\s+<', '><', line)
                    cleaned_lines.append(line)
            elif ext in ['.css']:
                if '/*' in line and '*/' in line:
                    continue
                if '/*' in line:
                    in_multiline_comment = True
                    continue
                if '*/' in line:
                    in_multiline_comment = False
                    continue
                if in_multiline_comment:
                    continue
                if line:
                    line = re.sub(r' +', ' ', line)
                    cleaned_lines.append(line)
            elif ext in ['.json']:
                if line:
                    line = re.sub(r'"\s*:\s*"', '":"', line)
                    line = re.sub(r'"\s*:\s*\{', '":{', line)
                    line = re.sub(r'"\s*:\s*\[', '":[', line)
                    line = re.sub(r'"\s*:\s*null', '":null', line)
                    line = re.sub(r'"\s*:\s*true', '":true', line)
                    line = re.sub(r'"\s*:\s*false', '":false', line)
                    line = re.sub(r'"\s*:\s*(\d+)', '":\\1', line)
                    cleaned_lines.append(line)
            else:
                if line:
                    line = re.sub(r' +', ' ', line)
                    cleaned_lines.append(line)
        result = '\n'.join(cleaned_lines)
        result = re.sub(r'\n{3,}', '\n\n', result)
        return result
    def set_status(self, message, is_error=False, is_success=False):
        if is_error:
            self.status_bar.setStyleSheet("QStatusBar { background-color: #ff6b6b; color: white; }")
        elif is_success:
            self.status_bar.setStyleSheet("QStatusBar { background-color: #51cf66; color: white; }")
        else:
            self.status_bar.setStyleSheet("QStatusBar { background-color: #e0e0e0; color: black; }")
        self.status_bar.showMessage(message)
        QTimer.singleShot(3000, lambda: self.status_bar.setStyleSheet("QStatusBar { background-color: #e0e0e0; color: black; }"))
    def update_root_label(self):
        if self.base_dir:
            self.root_path_label.setText(self.base_dir)
            self.root_path_label.setStyleSheet("color: #2b7a2b; font-weight: bold;")
        else:
            self.root_path_label.setText("Не установлен")
            self.root_path_label.setStyleSheet("color: #666; font-weight: normal;")
    def set_root_directory(self):
        folder_path = QFileDialog.getExistingDirectory(
            self,
            "Выберите корневую директорию проекта",
            self.base_dir if self.base_dir else "",
            QFileDialog.ShowDirsOnly
        )
        if folder_path:
            self.base_dir = folder_path
            self.update_root_label()
            for file_data in self.files:
                file_data['relative_path'] = self.get_relative_path(file_data['path'])
            self.update_table()
            self.generate_project_structure()
            self.set_status(f"✅ Корень проекта установлен: {folder_path}", is_success=True)
    def get_relative_path(self, file_path):
        if self.base_dir:
            try:
                rel_path = os.path.relpath(file_path, self.base_dir)
                if rel_path.startswith('..'):
                    return file_path
                return rel_path
            except ValueError:
                return os.path.basename(file_path)
        return os.path.basename(file_path)
    def on_structure_changed(self):
        self.project_structure = self.structure_text.toPlainText()
    def generate_project_structure(self):
        if not self.files:
            self.structure_text.setText("Нет файлов для генерации структуры")
            self.project_structure = ""
            self.set_status("⚠️ Нет файлов для генерации структуры", is_error=True)
            return
        paths = [file_data['relative_path'] for file_data in self.files]
        tree = {}
        for path in paths:
            parts = path.split(os.sep)
            current = tree
            for part in parts:
                if part not in current:
                    current[part] = {}
                current = current[part]
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
        if self.base_dir:
            root_name = os.path.basename(self.base_dir)
        else:
            root_name = "project"
        final_structure = f"{root_name}/\n" + "\n".join(structure_lines)
        self.structure_text.setText(final_structure)
        self.project_structure = final_structure
        self.set_status(f"✅ Структура сгенерирована! ({len(self.files)} файлов)", is_success=True)
    def add_files(self):
        file_paths, _ = QFileDialog.getOpenFileNames(
            self,
            "Выберите файлы",
            "",
            "Все файлы (*);;Python (*.py);;JavaScript (*.js);;HTML (*.html);;CSS (*.css);;JSON (*.json);;Текстовые файлы (*.txt)"
        )
        if not file_paths:
            return
        if not self.base_dir:
            common_parent = os.path.commonpath(file_paths) if len(file_paths) > 1 else os.path.dirname(file_paths[0])
            reply = QMessageBox.question(
                self,
                "Корень проекта",
                f"Установить корень проекта в общую папку?\n{common_parent}",
                QMessageBox.Yes | QMessageBox.No
            )
            if reply == QMessageBox.Yes:
                self.base_dir = common_parent
                self.update_root_label()
            else:
                self.set_root_directory()
                if not self.base_dir:
                    return
        added_count = 0
        for file_path in file_paths:
            if self.add_single_file(file_path, show_status=False):
                added_count += 1
        self.update_table()
        self.generate_project_structure()
        self.set_status(f"✅ Добавлено файлов: {added_count} (всего: {len(self.files)})", is_success=True)
    def add_single_file(self, file_path, show_status=True):
        try:
            if not os.path.exists(file_path):
                if show_status:
                    self.set_status(f"❌ Файл не существует: {file_path}", is_error=True)
                return False
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            content = self.clean_code_content(content, file_path)
            file_name = os.path.basename(file_path)
            relative_path = self.get_relative_path(file_path)
            for i, file_data in enumerate(self.files):
                if file_data['path'] == file_path:
                    self.files[i] = {
                        'name': file_name,
                        'content': content,
                        'path': file_path,
                        'relative_path': relative_path
                    }
                    if show_status:
                        self.set_status(f"✅ Файл '{relative_path}' обновлён!", is_success=True)
                    return True
            self.files.append({
                'name': file_name,
                'content': content,
                'path': file_path,
                'relative_path': relative_path
            })
            return True
        except Exception as e:
            if show_status:
                self.set_status(f"❌ Ошибка при чтении файла {os.path.basename(file_path)}: {str(e)}", is_error=True)
            return False
    def add_folders(self):
        dialog = QFileDialog(self)
        dialog.setFileMode(QFileDialog.Directory)
        dialog.setOption(QFileDialog.ShowDirsOnly, True)
        dialog.setOption(QFileDialog.DontUseNativeDialog, True)
        list_view = dialog.findChild(QListView, "listView")
        if list_view:
            list_view.setSelectionMode(QAbstractItemView.MultiSelection)
        tree_view = dialog.findChild(QTreeView)
        if tree_view:
            tree_view.setSelectionMode(QAbstractItemView.MultiSelection)
        dialog.setWindowTitle("Выберите каталоги")
        if dialog.exec_() == QDialog.Accepted:
            folder_paths = dialog.selectedFiles()
            if folder_paths:
                self.process_folders(folder_paths)
    def process_folders(self, folder_paths):
        if not folder_paths:
            return
        if len(folder_paths) == 1:
            parent_dir = os.path.dirname(folder_paths[0])
        else:
            try:
                parent_dir = os.path.commonpath(folder_paths)
            except ValueError:
                parent_dir = os.path.dirname(folder_paths[0])
        if self.base_dir and self.base_dir != parent_dir:
            reply = QMessageBox.question(
                self,
                "Корень проекта",
                f"Текущий корень: {self.base_dir}\n"
                f"Новый корень: {parent_dir}\n\n"
                "Заменить корень проекта?",
                QMessageBox.Yes | QMessageBox.No
            )
            if reply == QMessageBox.Yes:
                self.base_dir = parent_dir
                self.update_root_label()
        else:
            self.base_dir = parent_dir
            self.update_root_label()
        skipped_extensions = {'.pyc', '.pyo', '.exe', '.dll', '.so', '.dylib', '.class', '.o', '.obj', '.pdb'}
        skipped_dirs = {'.git', '__pycache__', 'node_modules', '.venv', 'venv', 'env', '.idea', '.vscode', '.pytest_cache'}
        file_count = 0
        skipped_count = 0
        for folder_path in folder_paths:
            for root, dirs, files in os.walk(folder_path):
                dirs[:] = [d for d in dirs if d not in skipped_dirs]
                for file in files:
                    file_path = os.path.join(root, file)
                    ext = os.path.splitext(file)[1].lower()
                    if ext in skipped_extensions:
                        skipped_count += 1
                        continue
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
                        content = self.clean_code_content(content, file_path)
                        relative_path = os.path.relpath(file_path, self.base_dir)
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
                        skipped_count += 1
                        continue
                    except Exception as e:
                        skipped_count += 1
                        continue
        self.update_table()
        self.generate_project_structure()
        if file_count > 0:
            self.set_status(
                f"✅ Добавлено файлов из каталогов: {file_count} (пропущено: {skipped_count}, всего: {len(self.files)})",
                is_success=True
            )
        else:
            self.set_status(
                f"⚠️ Не удалось найти текстовые файлы в выбранных каталогах",
                is_error=True
            )
    def add_folder_old(self):
        folder_path = QFileDialog.getExistingDirectory(
            self,
            "Выберите каталог для анализа",
            "",
            QFileDialog.ShowDirsOnly
        )
        if folder_path:
            self.process_folders([folder_path])
    def update_all_files(self):
        if not self.files:
            self.set_status("⚠️ Нет файлов для обновления", is_error=True)
            return
        updated_count = 0
        failed_files = []
        for i, file_data in enumerate(self.files):
            try:
                with open(file_data['path'], 'r', encoding='utf-8') as f:
                    content = f.read()
                content = self.clean_code_content(content, file_data['path'])
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
        relative_path = self.files[index]['relative_path']
        del self.files[index]
        self.update_table()
        self.generate_project_structure()
        self.set_status(f"🗑️ Файл '{relative_path}' удалён. Осталось: {len(self.files)}", is_success=True)
    def update_table(self):
        self.files_table.setRowCount(len(self.files))
        for i, file_data in enumerate(self.files):
            path_item = QTableWidgetItem(file_data['relative_path'])
            path_item.setFlags(path_item.flags() & ~Qt.ItemIsEditable)
            self.files_table.setItem(i, 0, path_item)
            size = len(file_data['content'])
            size_str = f"{size} символов" if size < 1024 else f"{size/1024:.1f} КБ"
            size_item = QTableWidgetItem(size_str)
            size_item.setFlags(size_item.flags() & ~Qt.ItemIsEditable)
            self.files_table.setItem(i, 1, size_item)
            remove_btn = QPushButton("❌ Удалить")
            remove_btn.clicked.connect(lambda checked, idx=i: self.remove_file(idx))
            self.files_table.setCellWidget(i, 2, remove_btn)
    def generate_template(self):
        description = self.description_text.toPlainText().strip()
        task = self.task_text.toPlainText().strip()
        if not self.files:
            self.set_status("⚠️ Добавьте хотя бы один файл для анализа!", is_error=True)
            return
        if not task:
            self.set_status("⚠️ Опишите задачу!", is_error=True)
            return
        structure = self.structure_text.toPlainText().strip()
        template_parts = []
        if description:
            template_parts.append("📝 ОПИСАНИЕ ПРОЕКТА:")
            template_parts.append("=" * 50)
            template_parts.append(description)
            template_parts.append("")
        if structure:
            template_parts.append("📁 СТРУКТУРА ПРОЕКТА:")
            template_parts.append("=" * 50)
            template_parts.append(structure)
            template_parts.append("")
        template_parts.append("📄 ФАЙЛЫ ДЛЯ АНАЛИЗА:")
        template_parts.append("=" * 50)
        for file_data in self.files:
            file_path = file_data['relative_path']
            content = file_data['content']
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
        template_parts.append("🎯 ЗАДАЧА:")
        template_parts.append("=" * 50)
        template_parts.append(task)
        result = "\n".join(template_parts)
        self.result_text.setText(result)
        self.set_status(f"✅ Шаблон сгенерирован! Файлов: {len(self.files)}, символов: {len(result)}", is_success=True)
    def copy_to_clipboard(self):
        text = self.result_text.toPlainText()
        if not text:
            self.set_status("⚠️ Сначала сгенерируйте шаблон!", is_error=True)
            return
        clipboard = QApplication.clipboard()
        clipboard.setText(text)
        self.set_status(f"✅ Шаблон скопирован в буфер! ({len(text)} символов)", is_success=True)
    def clear_all(self):
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