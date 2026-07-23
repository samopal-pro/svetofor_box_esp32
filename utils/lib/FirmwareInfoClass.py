"""Модуль для извлечения метаданных из бинарного файла прошивки ESP32."""

import struct


class FirmwareInfo:
    """Извлекает и хранит метаданные из бинарного файла прошивки.
    
    Attributes:
        fw_name: Название прошивки из пользовательских данных.
        fw_version: Версия прошивки из пользовательских данных.
        project_name: Название проекта из стандартного описания.
        version: Версия проекта из стандартного описания.
        app_elf_sha256: SHA256 хеш ELF файла.
        build_date: Дата сборки.
        build_time: Время сборки.
        idf_ver: Версия ESP-IDF.
    """
    
    IMAGE_HEADER_SIZE = 24
    SEGMENT_HEADER_SIZE = 8
    APP_DESC_OFFSET = IMAGE_HEADER_SIZE + SEGMENT_HEADER_SIZE
    APP_DESC_SIZE = 256
    CUSTOM_DESC_SIZE = 48
    CUSTOM_DESC_OFFSET = APP_DESC_OFFSET + APP_DESC_SIZE
    MAGIC_WORD = 0xABCD5432

    def __init__(self, firmware_path: str):
        """Читает и парсит метаданные прошивки.
        
        Args:
            firmware_path: Путь к бинарному файлу прошивки.
            
        Raises:
            ValueError: Если файл повреждён или магические числа не совпадают.
        """
        with open(firmware_path, 'rb') as f:
            f.seek(self.APP_DESC_OFFSET)
            app_buffer = f.read(self.APP_DESC_SIZE)
            f.seek(self.CUSTOM_DESC_OFFSET)
            custom_buffer = f.read(self.CUSTOM_DESC_SIZE)

        # Парсинг стандартного описания
        magic, = struct.unpack_from('<I', app_buffer, 0)
        if magic != self.MAGIC_WORD:
            raise ValueError(f"Неверное магическое число: 0x{magic:08X}")

        self.version = self._read_string(app_buffer, 16, 32)
        self.project_name = self._read_string(app_buffer, 48, 32)
        self.build_time = self._read_string(app_buffer, 80, 16)
        self.build_date = self._read_string(app_buffer, 96, 16)
        self.idf_ver = self._read_string(app_buffer, 112, 32)
        self.app_elf_sha256 = app_buffer[144:176].hex()

        # Парсинг пользовательского описания
        custom_magic, = struct.unpack_from('<I', custom_buffer, 0)
        if custom_magic != magic:
            raise ValueError(f"Несовпадение магических чисел: custom=0x{custom_magic:08X}, app=0x{magic:08X}")

        self.fw_name = self._read_string(custom_buffer, 4, 28)
        self.fw_version = self._read_string(custom_buffer, 32, 16)

    @staticmethod
    def _read_string(buffer: bytes, offset: int, length: int) -> str:
        """Читает строку из буфера, обрезая по нулевому байту."""
        raw = buffer[offset:offset + length]
        null_pos = raw.find(b'\x00')
        if null_pos != -1:
            raw = raw[:null_pos]
        return raw.decode('utf-8', errors='ignore')