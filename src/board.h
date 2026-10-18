#pragma once
// Pinnen per board. Kies het board met een build-vlag in platformio.ini:
//   (geen vlag)           Waveshare ESP32-S3-Touch-LCD-4 (standaard)
//   -DBOARD_T_PANEL_S3    LilyGO T-Panel S3

#define LCD_WIDTH 480
#define LCD_HEIGHT 480

#if defined(BOARD_GUITION_4848)
#error "De Guition ESP32-4848S040 wordt niet meer ondersteund (geen CAN). Gebruik de Waveshare of de T-Panel."

#elif defined(BOARD_T_PANEL_S3)  // LilyGO T-Panel S3 (bron: github.com/Xinyuan-LilyGO/T-Panel)

#define BOARD_NAME "T_PANEL_S3"
#define LCD_BACKLIGHT 14

#define LCD_VSYNC 40
#define LCD_HSYNC 39
#define LCD_PCLK 41
#define LCD_R0 12
#define LCD_R1 13
#define LCD_R2 42
#define LCD_R3 46
#define LCD_R4 45
#define LCD_G0 6
#define LCD_G1 7
#define LCD_G2 8
#define LCD_G3 9
#define LCD_G4 10
#define LCD_G5 11
#define LCD_B0 1
#define LCD_B1 2
#define LCD_B2 3
#define LCD_B3 4
#define LCD_B4 5

// Gedeelde I2C-bus: XL9535 IO-expander (0x20) en CST3240 touch (0x5A)
#define I2C_SDA 17
#define I2C_SCL 18
#define TOUCH_INT 21
#define CST3240_ADDR 0x5A

// Pinnen op de XL9535 (0-7 = poort 0, 10-17 = poort 1)
#define XL_LCD_SPI_CS 17
#define XL_LCD_SPI_SCK 15
#define XL_LCD_SPI_MOSI 16
#define XL_TOUCH_RST 4
#define XL_LCD_RST 5

// CAN-module (T-Panel S3 CAN-versie, H720) voor NMEA 2000
#define CAN_TX_PIN 16
#define CAN_RX_PIN 15

#else  // Waveshare ESP32-S3-Touch-LCD-4 (bron: github.com/waveshareteam/ESP32-S3-Touch-LCD-4 en het V4.0-schema)

#define BOARD_WAVESHARE_4 1
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

#endif

#ifndef LCD_ROTATION
#define LCD_ROTATION 0
#endif

// Scherm gaat uit na zoveel seconden zonder aanraking (0 = altijd aan). Aanraken = weer aan.
#define SCREEN_TIMEOUT_S 60

// Zoemer. De Waveshare heeft er een op het board (via de hulpchip). Anders optioneel: een PCF8574 op de I2C-poort wordt automatisch gevonden (adres 0x21-0x27 of 0x38-0x3F):
//  - I2C-LCD-backpack (1602/2004): zoemer + op LCD-pin 15 (A), - op LCD-pin 16 (K) = backlight-transistor
//  - kale PCF8574-module: zoemer + op VCC, - op P0
// Of een zoemer direct op een GPIO:
#define BUZZER_GPIO -1        // bv. 10; -1 = niet gebruikt
#define BUZZER_ACTIVE_LOW 0   // voor BUZZER_GPIO: 1 als de zoemermodule op laag aangaat

// Pas aan als de touch niet overeenkomt met het beeld
#define TOUCH_SWAP_XY 0
#if defined(BOARD_WAVESHARE_4)  // touch hoort bij het ongedraaide paneel: mee draaien
#define TOUCH_INVERT_X 1
#define TOUCH_INVERT_Y 1
#else
#define TOUCH_INVERT_X 0
#define TOUCH_INVERT_Y 0
#endif
