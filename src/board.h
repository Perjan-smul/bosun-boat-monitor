#pragma once
// Pinnen van het board: Bosun draait op de Waveshare ESP32-S3-Touch-LCD-4.

#define LCD_WIDTH 480
#define LCD_HEIGHT 480

// Waveshare ESP32-S3-Touch-LCD-4 (bron: github.com/waveshareteam/ESP32-S3-Touch-LCD-4 en het V4.0-schema)
#define BOARD_NAME "WAVESHARE_4"

// RGB-paneel (ST7701, type1-init) en 3-draads SPI voor de init
#define LCD_DE 40
#define LCD_VSYNC 39
#define LCD_HSYNC 38
#define LCD_PCLK 41
#define LCD_SPI_CS 42
#define LCD_SPI_SCK 2
#define LCD_SPI_MOSI 1
#define LCD_ROTATION 2  // het paneel zit 180 graden gedraaid in de behuizing

// Gedeelde I2C-bus: hulpchip (V4: CH32V003 op 0x24, V3: TCA9554 op 0x20), GT911-touch, PCF85063-klok (0x51)
#define I2C_SDA 15
#define I2C_SCL 7
#define TOUCH_SDA I2C_SDA
#define TOUCH_SCL I2C_SCL

// CAN (TJA1051) voor NMEA 2000
#define CAN_TX_PIN 6
#define CAN_RX_PIN 0

// Micro-SD (SD_MMC, 1-bit). CLK en CMD zijn ook de SPI-pinnen van de scherm-init: pas na display_begin() starten.
#define SD_CLK 2
#define SD_CMD 1
#define SD_D0 4

// Scherm gaat uit na zoveel seconden zonder aanraking (0 = altijd aan). Aanraken = weer aan.
#define SCREEN_TIMEOUT_S 60

// Zoemer: op het board, via de hulpchip (zie wsio.cpp)

// Pas aan als de touch niet overeenkomt met het beeld
#define TOUCH_SWAP_XY 0
// De touch hoort bij het ongedraaide paneel: mee draaien
#define TOUCH_INVERT_X 1
#define TOUCH_INVERT_Y 1
