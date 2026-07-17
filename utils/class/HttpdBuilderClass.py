import json
import tarfile
import os
from pathlib import Path
import io

class HttpdBuilder:
    """Класс для сборки firmware в бинарный файл."""

    def __init__(self, source_dir: str, output_file: str):
        """
        Инициализация сборщика.

        Args:
            source_dir: Путь к папке с файлами данных
            output_dir: Путь для сохранения архива
        """
        self.source_dir  = Path(source_dir)
        self.output_file = Path(output_file)
        self.version = None
        self.name = None
        self.exclude = []

    def build(self) -> Path:
        """
        Выполняет сборку firmware.

        Returns:
            Path: Путь к созданному архивному файлу

        Raises:
            FileNotFoundError: Если version.json не найден
        """
        self._load_version()
        return self.create_tar_archive()

    def _load_version(self) -> None:
        """Загружает данные из version.json."""
        version_file = self.source_dir / 'version.json'
        if not version_file.exists():
            raise FileNotFoundError(f'version.json not found in {self.source_dir}')

        with open(version_file, 'r') as f:
            data = json.load(f)

        self.version = data.get('version')
        self.name = data.get('name')
        self.exclude = data.get('exclude', [])

  
    def create_tar_archive(self) -> bool:
        """
        Создание TAR архива без сжатия.
        
        Returns:
            bytes: Содержимое архива
        """

        
        with tarfile.open(self.output_file, mode='w') as tar:
            for root, dirs, files in os.walk(self.source_dir):
                for file in files:
                    file_path = os.path.join(root, file)
                    
                    # Проверяем исключения
                    relative_name = os.path.relpath(file_path, self.source_dir)
                    if file in self.exclude or relative_name in self.exclude:
                        print(f"  - Исключен: {relative_name}")
                        continue
                    
                    # Добавляем файл в архив
                    arcname = os.path.relpath(file_path, os.path.dirname(self.source_dir))
                    tar.add(file_path, arcname=arcname)
                    print(f"  + Добавлен: {relative_name}")
        
        
        return True
