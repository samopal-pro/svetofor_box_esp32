#pragma once


#define SDEBUG_LEVEL  3
// Выдача сообщение сенсоров 1- иницилизация и ошибки, 2 - измерение параметров
// Устаревшие макросы
#define DEBUG_SENSORS 1
#define DEBUG_SERIAL 
#define IS_LORA
#define IS_DNS

#define SOFTWARE_V           "10.1.1"
#define HARDWARE_V           "10.0.0"



#define PIN_LORA_MOSI        23
#define PIN_LORA_MISO        19
#define PIN_LORA_SCK         18
#define PIN_LORA_CS          5
#define PIN_LORA_RST         2
#define PIN_LORA_DIO1        4
#define PIN_LORA_DIO2        14
#define PIN_LORA_BUSY        -1

#define PIN_I2C_SDA          21
#define PIN_I2C_SCL          22

#define PIN_SONAR_TRIG       17 //TRIG/SCL/RX
#define PIN_SONAR_ECHO       16 //ECHO/SDA/TX

#define PIN_RGB1             13
#define PIN_RGB2             15

#define PIN_TX1              25
#define PIN_RX1              26

#define PIN_OUT1             32
#define PIN_OUT2             33

#define PIN_GPIO             27

#define PIN_BTN              0
#define PIN_TOUCH            PIN_GPIO
#define PIN_BTN_ADD          35


#define CORE                 1
#define SIMPLE_SIZE          10

//#define DEFAULT_SENSOR_INSTALL_TYPE INSTALL_TYPE_NORMAL
//#define DEFAULT_SENSOR_GROUND       1500
//#define DEFAULT_SENSOR_LIMIT        250
//#define DEFAULT_SENSOR_MIN_DIST     1250
//#define DEFAULT_SENSOR_MAX_DIST     1750
#define COLOR_NONE             0xFFFFFFFF  //Цвет прозрачный
#define COLOR_BLACK            0x000000    //Цвет "никакой" (черный)
#define COLOR_FREE1            0x0000FF    //Цвет "свободно" №1
#define COLOR_FREE2            0x00FF00    //Цвет "свободно" №2
#define COLOR_FREE_DEFAULT     COLOR_FREE1 //Цвет "свободно" по умолчанию
#define COLOR_BLINK1           0x7F7F7F    //Цвет "свободно мигание" (если включен) №1
#define COLOR_BLINK2           0xFF007F    //Цвет "свободно мигание" (если включен) №2
#define COLOR_BLINK_DEFAULT    COLOR_BLINK1//Цвет "свободно мигание" по умолчанию
#define COLOR_BUSY1            0xFF0000    //Цвет "занято" №1
#define COLOR_BUSY2            0x000000    //Цвет "занято" №2
#define COLOR_BUSY_DEFAULT     COLOR_BUSY1 //Цвет "занято" по умолчанию
#define COLOR_NAN              0xFF007F    //Цвет "NAN"
#define COLOR_GROUND           0xA5FF00    //Цвет "установка земли"
#define COLOR_SAVE             0xFFFFFF    //Цвет "сохранение"
#define COLOR_ERROR            0xFF7F00    //Цвет "ошибка"
#define COLOR_MP3_1            0x7F7F7F    //Цвет мигания во время произрывания звукового файла
#define COLOR_MP3_2            0xFFFF00    //Цвет мигания во время произрывания звукового файла
#define COLOR_MP3_DEFAULT      COLOR_MP3_1    //Цвет мигания во время произрывания звукового файла

#define COLOR_WIFI_NONE        0x000000    //Цвет "WiFi не конфигуен"
#define COLOR_WIFI_OFF         0xFF0000    //Цвет "WiFi не подключен"
#define COLOR_WIFI_ON          0x00FF00    //Цвет "WiFi подключен"
#define COLOR_WIFI_WAIT        0xFFFF00    //Цвет "WiFi попытка подключения"
#define COLOR_WIFI_AP          0x00FF7F    //Цвет "Режим точки доступа"
#define COLOR_WIFI_AP1         0xFFFFFF    //Цвет "Точка доступа всегда включена"

#define LED_STAT_AP2            14
#define LED_STAT_STA2           15
#define LED_STAT_AP1            0
#define LED_STAT_STA1           1
#define LED_STAT_BRIGHTNESS     1           //Яркость 0-10 для двух светодиодов

#define DEFAULT_NAN_VALUE_FLAG NAN_VALUE_IGNORE

#define DEVICE_NAM_SUFFIX      "_SVETOFORBOX.RU_192.168.4.1"
#define DEVICE_NAME_YEAR       "2026"

//#define DEVICE_PASSS           "svetoforbox"
//#define DEVICE_PASS0           "superadmin"
//#define DEVICE_PASS1           "admin"
//#define SENSOR_GROUND_STATE    SS_FREE;
#define COUNT_RGB1             50   //Число светодиодов
#define COUNT_RGB2             50   //Число светодиодов

#define CRM_MOSCOW_PATH       "/api/1/sensor/99/"
#define HTTP_PATH             "/api/v1/sensor/telemetry"

#define MP3_BASE_DIR           2
#define MP3_SYSTEM_FULL_DIR    3
#define MP3_SYSTEM_SHORT_DIR   4

#define TB_HOST                "109.172.115.70"
#define TB_PORT                8088

#define TB_TOKEN               ""

#define TB_PROVISION_KEY       "_Svetofor10box_key"
#define TB_PROVISION_SECRET    "_Svetofor10box_secret"

// ESM_NONE - перемычки BUSY-SDA нет, статус MP3 Плеера определяется через UART 
// ESM_ENABLE - перемычка BUSY-SDA есть, статус MP3 Плеера определяется через PIN_I2C_SDA
// ESM_AUTO - автоопределение перемычки (первый цикл параллельно UART и PIN) 
#define DEFAULT_MP3_GPIO       ESM_AUTO