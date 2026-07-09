import struct
from pathlib import Path

PATH = "../build/esp32.esp32.esp32/svetofor_box_esp32.ino.bin"

# Константы для заголовков
IMAGE_HEADER_SIZE = 24
SEGMENT_HEADER_SIZE = 8
APP_DESC_OFFSET = IMAGE_HEADER_SIZE + SEGMENT_HEADER_SIZE
APP_DESC_SIZE = 256
MAGIC_WORD = 0xABCD5432

# Размер пользовательской структуры custom_app_desc_t
# fw_magic (4) + fw_name[28] + fw_version[16] = 48 байт
CUSTOM_DESC_SIZE = 48
# Пользовательские данные находятся сразу после esp_app_desc_t
CUSTOM_DESC_OFFSET = APP_DESC_OFFSET + APP_DESC_SIZE  # 0x20 + 0x100 = 0x120


def read_string(buffer, offset, length):
    """
    Читает строку из буфера до первого нулевого байта, декодирует в UTF-8.
    """
    raw = buffer[offset:offset + length]
    null_byte_pos = raw.find(b'\x00')
    if null_byte_pos != -1:
        raw = raw[:null_byte_pos]
    return raw.decode('utf-8', errors='ignore')


def read_app_description(firmware_path):
    """
    Читает структуру esp_app_desc_t из файла прошивки.
    Возвращает словарь с данными.
    """
    with open(firmware_path, 'rb') as f:
        # Читаем esp_app_desc_t
        f.seek(APP_DESC_OFFSET)
        buffer = f.read(APP_DESC_SIZE)

    if len(buffer) < APP_DESC_SIZE:
        print("Ошибка: файл слишком мал для чтения описания приложения.")
        return None

    # Проверяем магическое число
    magic, = struct.unpack_from('<I', buffer, 0)
    if magic != MAGIC_WORD:
        print(f"Ошибка: Неверное магическое число. Найдено: 0x{magic:08X}, ожидалось: 0x{MAGIC_WORD:08X}")
        print("Возможно, файл не является прошивкой ESP-IDF или поврежден.")
        return None

    result = {
        "magic_word": hex(magic),
        "secure_version": struct.unpack_from('<I', buffer, 4)[0],
        "reserv1": struct.unpack_from('<II', buffer, 8),
        "version": read_string(buffer, 16, 32),
        "project_name": read_string(buffer, 48, 32),
        "time": read_string(buffer, 80, 16),
        "date": read_string(buffer, 96, 16),
        "idf_ver": read_string(buffer, 112, 32),
        "app_elf_sha256": buffer[144:176].hex(),
        "min_efuse_blk_rev_full": struct.unpack_from('<H', buffer, 176)[0],
        "max_efuse_blk_rev_full": struct.unpack_from('<H', buffer, 178)[0],
        "mmu_page_size": buffer[180],
    }
    return result


def read_custom_description(firmware_path, expected_magic):
    """
    Читает пользовательскую структуру custom_app_desc_t из файла прошивки.
    Проверяет совпадение fw_magic с expected_magic.
    Возвращает словарь с данными или None при ошибке.
    """
    with open(firmware_path, 'rb') as f:
        # Читаем пользовательские данные сразу после esp_app_desc_t
        f.seek(CUSTOM_DESC_OFFSET)
        buffer = f.read(CUSTOM_DESC_SIZE)

    if len(buffer) < CUSTOM_DESC_SIZE:
        print(f"Ошибка: файл слишком мал для чтения пользовательских данных. "
              f"Требуется {CUSTOM_DESC_SIZE} байт, доступно {len(buffer)}")
        return None

    # Читаем магическое число из пользовательской структуры
    custom_magic, = struct.unpack_from('<I', buffer, 0)

    # Проверяем совпадение магического числа
    if custom_magic != expected_magic:
        print(f"Ошибка: Неверное магическое число в пользовательских данных. "
              f"Найдено: 0x{custom_magic:08X}, ожидалось: 0x{expected_magic:08X}")
        return None

    # Извлекаем строки из структуры
    # Смещения: fw_magic (0-3), fw_name (4-31), fw_version (32-47)
    fw_name = read_string(buffer, 4, 28)    # 28 байт для имени
    fw_version = read_string(buffer, 32, 16)  # 16 байт для версии

    result = {
        "fw_magic": hex(custom_magic),
        "fw_name": fw_name,
        "fw_version": fw_version,
        "fw_magic_match": True
    }
    return result


if __name__ == "__main__":
    fw_path = Path(PATH)

    if not fw_path.exists():
        print(f"Файл не найден: {fw_path}")
        print("Пожалуйста, укажите правильный путь к файлу прошивки.")
        exit(1)

    print("=" * 60)
    print("АНАЛИЗ ПРОШИВКИ ESP32")
    print("=" * 60)
    print(f"Файл: {fw_path}")
    print(f"Размер: {fw_path.stat().st_size:,} байт")
    print()

    # 1. Читаем стандартное описание приложения
    print("--- СТАНДАРТНОЕ ОПИСАНИЕ (esp_app_desc_t) ---")
    app_info = read_app_description(fw_path)

    if app_info:
        print(f"Название проекта:    {app_info['project_name']}")
        print(f"Версия:             {app_info['version']}")
        print(f"Дата сборки:        {app_info['date']} {app_info['time']}")
        print(f"Версия IDF:         {app_info['idf_ver']}")
        print(f"SHA256 ELF файла:   {app_info['app_elf_sha256']}")
        print(f"Magic word:         {app_info['magic_word']}")
        print(f"Secure version:     {app_info['secure_version']}")
        print(f"MMU page size:      {app_info['mmu_page_size']}")
        print(f"Min eFuse rev:      {app_info['min_efuse_blk_rev_full']}")
        print(f"Max eFuse rev:      {app_info['max_efuse_blk_rev_full']}")

        # Получаем ожидаемое магическое число для проверки
        expected_magic = int(app_info['magic_word'], 16)
        print()

        # 2. Читаем пользовательские данные
        print("--- ПОЛЬЗОВАТЕЛЬСКИЕ ДАННЫЕ (custom_app_desc_t) ---")
        custom_info = read_custom_description(fw_path, expected_magic)

        if custom_info:
            print(f"FW Name:            {custom_info['fw_name']}")
            print(f"FW Version:         {custom_info['fw_version']}")
            print(f"FW Magic:           {custom_info['fw_magic']}")
            print(f"Magic match:        {'✓ ДА' if custom_info['fw_magic_match'] else '✗ НЕТ'}")
            print(f"Размер структуры:   {CUSTOM_DESC_SIZE} байт")
            print(f"Смещение в файле:   0x{CUSTOM_DESC_OFFSET:04X} ({CUSTOM_DESC_OFFSET} байт)")
        else:
            print("❌ Не удалось прочитать пользовательские данные")
        print()

        # 3. Дополнительная информация о структуре файла
        print("--- СТРУКТУРА ФАЙЛА ---")
        print(f"Заголовок образа:       0x00-0x17 ({IMAGE_HEADER_SIZE} байт)")
        print(f"Заголовок сегмента:     0x18-0x1F ({SEGMENT_HEADER_SIZE} байт)")
        print(f"esp_app_desc_t:          0x20-0x11F ({APP_DESC_SIZE} байт)")
        print(f"custom_app_desc_t:       0x120-0x14F ({CUSTOM_DESC_SIZE} байт)")
        print(f"Общий размер файла:     {fw_path.stat().st_size:,} байт")

    else:
        print("❌ Ошибка чтения прошивки")