#include "log.h"
#include "display.h"
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "board.h"
#include "buzzer.h"
#include "settings.h"

#if defined(BOARD_T_PANEL_S3)
#include "cst3240.h"
#else
#include "gt911.h"
#endif
#if defined(BOARD_WAVESHARE_4)
#include "wsio.h"
#endif

namespace {

#if defined(BOARD_T_PANEL_S3)

// Init-sequentie voor het YDP395BT001-paneel van de T-Panel, overgenomen uit LilyGO's
// eigen driver (gelijk aan hun ESP-IDF st7701_driver.cpp). De standaard type9-tabel van
// Arduino_GFX is voor een ander paneel.
const uint8_t t_panel_init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0xFF, WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
    WRITE_C8_D8, 0xEF, 0x08,
    WRITE_COMMAND_8, 0xFF, WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x10,
    WRITE_C8_D16, 0xC0, 0x3B, 0x00,
    WRITE_C8_D16, 0xC1, 0x0B, 0x02,
    WRITE_COMMAND_8, 0xC2, WRITE_BYTES, 3, 0x30, 0x02, 0x37,
    WRITE_C8_D8, 0xCC, 0x10,
    WRITE_COMMAND_8, 0xB0, WRITE_BYTES, 16,  // gamma +
    0x00, 0x0F, 0x16, 0x0E, 0x11, 0x07, 0x09, 0x09, 0x08, 0x23, 0x05, 0x11, 0x0F, 0x28, 0x2D, 0x18,
    WRITE_COMMAND_8, 0xB1, WRITE_BYTES, 16,  // gamma -
    0x00, 0x0F, 0x16, 0x0E, 0x11, 0x07, 0x09, 0x08, 0x09, 0x23, 0x05, 0x11, 0x0F, 0x28, 0x2D, 0x18,
    WRITE_COMMAND_8, 0xFF, WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x11,
    WRITE_C8_D8, 0xB0, 0x4D,
    WRITE_C8_D8, 0xB1, 0x33,
    WRITE_C8_D8, 0xB2, 0x87,
    WRITE_C8_D8, 0xB5, 0x4B,
    WRITE_C8_D8, 0xB7, 0x8C,
    WRITE_C8_D8, 0xB8, 0x20,
    WRITE_C8_D8, 0xC1, 0x78,
    WRITE_C8_D8, 0xC2, 0x78,
    WRITE_C8_D8, 0xD0, 0x88,
    WRITE_COMMAND_8, 0xE0, WRITE_BYTES, 3, 0x00, 0x00, 0x02,
    WRITE_COMMAND_8, 0xE1, WRITE_BYTES, 11, 0x02, 0xF0, 0x00, 0x00, 0x03, 0xF0, 0x00, 0x00, 0x00, 0x44, 0x44,
    WRITE_COMMAND_8, 0xE2, WRITE_BYTES, 12, 0x10, 0x10, 0x40, 0x40, 0xF2, 0xF0, 0x00, 0x00, 0xF2, 0xF0, 0x00, 0x00,
    WRITE_COMMAND_8, 0xE3, WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,
    WRITE_C8_D16, 0xE4, 0x44, 0x44,
    WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 16,
    0x07, 0xEF, 0xF0, 0xF0, 0x09, 0xF1, 0xF0, 0xF0, 0x03, 0xF3, 0xF0, 0xF0, 0x05, 0xED, 0xF0, 0xF0,
    WRITE_COMMAND_8, 0xE6, WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,
    WRITE_C8_D16, 0xE7, 0x44, 0x44,
    WRITE_COMMAND_8, 0xE8, WRITE_BYTES, 16,
    0x08, 0xF0, 0xF0, 0xF0, 0x0A, 0xF2, 0xF0, 0xF0, 0x04, 0xF4, 0xF0, 0xF0, 0x06, 0xEE, 0xF0, 0xF0,
    WRITE_COMMAND_8, 0xEB, WRITE_BYTES, 7, 0x00, 0x00, 0xE4, 0xE4, 0x44, 0x88, 0x40,
    WRITE_C8_D16, 0xEC, 0x78, 0x00,
    WRITE_COMMAND_8, 0xED, WRITE_BYTES, 16,
    0x20, 0xF9, 0x87, 0x76, 0x65, 0x54, 0x4F, 0xFF, 0xFF, 0xF4, 0x45, 0x56, 0x67, 0x78, 0x9F, 0x02,
    WRITE_COMMAND_8, 0xEF, WRITE_BYTES, 6, 0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F,
    WRITE_C8_D8, 0x3A, 0x55,  // RGB565
    WRITE_C8_D8, 0x36, 0x08,  // BGR-volgorde, hoort bij de omgewisselde R/B-pinnen hieronder
    WRITE_COMMAND_8, 0x11,    // sleep out
    END_WRITE,
    DELAY, 120,
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,    // display on
    END_WRITE,
};

// SPI naar het paneel loopt via de XL9535 IO-expander
Arduino_XL9535SWSPI *bus = new Arduino_XL9535SWSPI(I2C_SDA, I2C_SCL, GFX_NOT_DEFINED /* PWD */,
                                                   XL_LCD_SPI_CS, XL_LCD_SPI_SCK, XL_LCD_SPI_MOSI);

// Net als LilyGO's Arduino-voorbeeld: B-pinnen op de R-plaatsen + MADCTL BGR
Arduino_ESP32RGBPanel *panel = new Arduino_ESP32RGBPanel(
    GFX_NOT_DEFINED /* DE */, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,
    LCD_B0, LCD_B1, LCD_B2, LCD_B3, LCD_B4,
    LCD_G0, LCD_G1, LCD_G2, LCD_G3, LCD_G4, LCD_G5,
    LCD_R0, LCD_R1, LCD_R2, LCD_R3, LCD_R4,
    1, 20, 2, 0,          // hsync: polariteit, front porch, pulsbreedte, back porch
    1, 30, 8, 1,          // vsync: polariteit, front porch, pulsbreedte, back porch
    1 /* pclk_active_neg */, 6000000L /* pclk */);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    LCD_WIDTH, LCD_HEIGHT, panel, 0 /* rotatie */, true /* auto flush */, bus, GFX_NOT_DEFINED /* RST */,
    t_panel_init_operations, sizeof(t_panel_init_operations));

CST3240 touch;

void boardInit() {
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  // Arduino_GFX 1.4.9 zet in begin() alleen poort 0 van de XL9535 op output; de
  // SPI-pinnen van het paneel zitten op poort 1, dus die zetten we hier zelf goed.
  bus->begin();
  bus->pinMode(XL_LCD_SPI_CS, OUTPUT);
  bus->pinMode(XL_LCD_SPI_SCK, OUTPUT);
  bus->pinMode(XL_LCD_SPI_MOSI, OUTPUT);

  // Hardware-reset van het paneel
  bus->digitalWrite(XL_LCD_RST, LOW);
  delay(20);
  bus->digitalWrite(XL_LCD_RST, HIGH);
  delay(120);
}

bool touchInit() {
  bus->digitalWrite(XL_TOUCH_RST, LOW);
  delay(20);
  bus->digitalWrite(XL_TOUCH_RST, HIGH);
  delay(200);
  return touch.begin(CST3240_ADDR, TOUCH_INT);
}

#else  // Waveshare ESP32-S3-Touch-LCD-4 (waarden uit Waveshare's eigen Arduino-voorbeelden)

Arduino_DataBus *bus = new Arduino_SWSPI(GFX_NOT_DEFINED /* DC */, LCD_SPI_CS, LCD_SPI_SCK, LCD_SPI_MOSI,
                                         GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *panel = new Arduino_ESP32RGBPanel(
    LCD_DE, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,
    46, 3, 8, 18, 17,       // R0-R4
    14, 13, 12, 11, 10, 9,  // G0-G5
    5, 45, 48, 47, 21,      // B0-B4
    1, 10, 8, 50,           // hsync: polariteit, front porch, pulsbreedte, back porch
    1, 10, 8, 20);          // vsync: polariteit, front porch, pulsbreedte, back porch

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    LCD_WIDTH, LCD_HEIGHT, panel, LCD_ROTATION, true /* auto flush */, bus, GFX_NOT_DEFINED /* RST */,
    st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

GT911 touch;

// Hangt SDA nog laag (bv. na een herstart midden in een overdracht), dan SCL een paar keer
// pulsen tot de chip hem loslaat. Waveshare doet dit ook vóór elke start.
void recoverI2c() {
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SCL, HIGH);
  delayMicroseconds(10);
  for (int i = 0; i < 9 && digitalRead(I2C_SDA) == LOW; i++) {
    digitalWrite(I2C_SCL, LOW);
    delayMicroseconds(10);
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(10);
  }
  pinMode(I2C_SCL, INPUT_PULLUP);
}

void boardInit() {
  recoverI2c();
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  wsio_begin();  // systeemvoeding aan, scherm en touch uit reset
}

bool touchInit() { return touch.begin(TOUCH_SDA, TOUCH_SCL); }

#endif

#if !defined(BOARD_WAVESHARE_4)
constexpr int BL_CHANNEL = 0;  // LEDC-kanaal voor de achtergrondverlichting
#endif

void setBacklight(int level) {
#if defined(BOARD_WAVESHARE_4)
  wsio_backlight(level);
#else
  ledcWrite(BL_CHANNEL, level);
#endif
}

lv_disp_draw_buf_t drawBuf;
lv_disp_drv_t dispDrv;
lv_indev_drv_t indevDrv;

void flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&px->full, w, h);
  lv_disp_flush_ready(drv);
}

// ---------- Schermtimer: backlight dimt uit na SCREEN_TIMEOUT_S, aanraken = aan ----------

int blLevel = 0, blTarget = 255, blMax = 255;
bool asleep = false;
bool swallowTouch = false;  // de aanraking die het scherm wekt, telt niet als klik
bool keepAwake = false;

void backlightTimer(lv_timer_t *) {
  if (!asleep && blTarget != blMax) blTarget = blMax;  // ander thema: naar de nieuwe helderheid faden
  uint32_t timeout = settings().screenTimeoutS;
  if (timeout > 0 && !keepAwake && !asleep && lv_disp_get_inactive_time(nullptr) > timeout * 1000UL) {
    asleep = true;
    blTarget = 0;
  }
  if (blLevel != blTarget) {  // zacht in- en uitfaden
    blLevel = blTarget > blLevel ? min(blTarget, blLevel + 40) : max(blTarget, blLevel - 10);
    setBacklight(blLevel);
  }
}

void touchCb(lv_indev_drv_t *, lv_indev_data_t *data) {
  uint16_t x, y;
  bool touched = touch.read(x, y);
  if (asleep || swallowTouch) {
    if (touched && asleep) {
      asleep = false;
      swallowTouch = true;
      blTarget = blMax;
      lv_disp_trig_activity(nullptr);
    }
    if (!touched) swallowTouch = false;
    data->state = LV_INDEV_STATE_REL;
    return;
  }
  if (touched) {
    if (TOUCH_SWAP_XY) std::swap(x, y);
    if (TOUCH_INVERT_X) x = LCD_WIDTH - 1 - x;
    if (TOUCH_INVERT_Y) y = LCD_HEIGHT - 1 - y;
    data->point.x = min<uint16_t>(x, LCD_WIDTH - 1);
    data->point.y = min<uint16_t>(y, LCD_HEIGHT - 1);
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

}  // namespace

void display_wake() {
  if (asleep) {
    asleep = false;
    blTarget = blMax;
  }
  lv_disp_trig_activity(nullptr);
}

void display_keep_awake(bool keep) {
  if (keep && !keepAwake) display_wake();
  keepAwake = keep;
}

void display_begin() {
  boardInit();
  if (!gfx->begin()) log_line("[display] gfx->begin() mislukt");
  gfx->fillScreen(BLACK);
#if !defined(BOARD_WAVESHARE_4)  // de Waveshare dimt via zijn hulpchip
  ledcSetup(BL_CHANNEL, 20000, 8);
  ledcAttachPin(LCD_BACKLIGHT, BL_CHANNEL);
#endif
  setBacklight(0);  // fadet in via backlightTimer

  if (!touchInit()) log_line("[display] touchcontroller niet gevonden");

  lv_init();

  // Tekenbuffer van 40 regels in intern RAM (sneller dan PSRAM)
  const size_t bufPixels = LCD_WIDTH * 40;
  auto *buf = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf) buf = (lv_color_t *)ps_malloc(bufPixels * sizeof(lv_color_t));
  lv_disp_draw_buf_init(&drawBuf, buf, nullptr, bufPixels);

  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = LCD_WIDTH;
  dispDrv.ver_res = LCD_HEIGHT;
  dispDrv.flush_cb = flushCb;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);

  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = touchCb;
  lv_indev_drv_register(&indevDrv);

  lv_timer_create(backlightTimer, 20, nullptr);
  buzzer_begin();  // zit op dezelfde I2C-bus
}

void display_set_max_brightness(uint8_t level) { blMax = level < 20 ? 20 : level; }
