#include "log.h"
#include "display.h"
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "board.h"
#include "buzzer.h"
#include "settings.h"

#include "gt911.h"
#include "wsio.h"

namespace {

// Waveshare ESP32-S3-Touch-LCD-4 (waarden uit Waveshare's eigen Arduino-voorbeelden)

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

// De achtergrondverlichting dimt via de hulpchip
void setBacklight(int level) { wsio_backlight(level); }

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
