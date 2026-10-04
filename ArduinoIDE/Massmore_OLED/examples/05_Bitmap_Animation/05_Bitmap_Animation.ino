/*
  05_Bitmap_Animation — Massmore_OLED (Intermediate)
  ---------------------------------------------------------------------------
  - drawBitmap()  : โลโก้ 32x32 ใน PROGMEM (รูปแบบ Adafruit / image2cpp)
  - drawXBitmap() : ไอคอนหัวใจ XBM 16x16 (export จาก GIMP ได้ตรง ๆ)
  - Sprite        : ใบพัด 4 เฟรมหมุน + โลโก้เด้งไปมา
  - FPS counter   : วัดเฟรมต่อวินาทีจริง (ขึ้นกับขนาดจอและความเร็ว I2C)

  ภาพอยู่ใน bitmaps.h (โฟลเดอร์เดียวกัน) — แปลงรูปของคุณเองได้ด้วยเว็บ image2cpp
  (เลือก "Arduino code", "Horizontal - 1 bit per pixel")

  Wiring: ดู 01_HelloWorld
  Designed and Manufactured by Massmore — https://www.massmore.shop
*/

#include <Wire.h>
#include <Massmore_OLED.h>
#include "bitmaps.h"

#define OLED_SKU    OLED096_White_SSD1306      // ← เปลี่ยนเป็นรุ่นของคุณ
#define OLED_ADDR   0x3C
#define OLED_RST    (-1)

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define I2C_SDA 14
  #define I2C_SCL 15
#elif defined(ARDUINO_ARCH_ESP32)
  #define I2C_SDA 21
  #define I2C_SCL 22
#endif

Massmore_OLED oled(OLED_SKU);

int16_t  bx = 0, by = 0, vx = 2, vy = 1;      // ตำแหน่ง/ความเร็วโลโก้
uint8_t  frame = 0;
uint16_t frames = 0, fps = 0;
uint32_t fpsTimer = 0;

void setup() {
  Serial.begin(115200);
#if defined(I2C_SDA)
  Wire.begin(I2C_SDA, I2C_SCL);
#else
  Wire.begin();
#endif
  Wire.setClock(400000);
  if (oled.begin(Wire, OLED_ADDR, OLED_RST) != MASSMORE_OLED_OK) {
    Serial.println(Massmore_OLED::errorString(oled.lastError()));
    while (true) delay(100);
  }

  // หน้าแรก: โลโก้ + ไอคอนนิ่ง ๆ
  oled.drawBitmap((oled.width() - LOGO_W) / 2, (oled.height() - LOGO_H) / 2, LOGO_32, LOGO_W, LOGO_H, WHITE);
  oled.drawXBitmap(0, 0, HEART_XBM, HEART_W, HEART_H, WHITE);
  oled.drawXBitmap(oled.width() - HEART_W, 0, HEART_XBM, HEART_W, HEART_H, WHITE);
  oled.display();
  delay(2000);
  fpsTimer = millis();
}

void loop() {
  int16_t W = oled.width(), H = oled.height();

  oled.clear();

  // โลโก้เด้งชนขอบ
  bx += vx;
  by += vy;
  if (bx <= 0 || bx >= W - LOGO_W) vx = -vx;
  int16_t top = (H - LOGO_H >= 9) ? 9 : 0;     // เว้นแถวบนให้ตัวเลข FPS (จอ 128x32 ไม่พอ → 0)
  if (by <= top || by >= H - LOGO_H) vy = -vy;
  bx = constrain(bx, 0, W - LOGO_W);
  by = constrain(by, top, H - LOGO_H);
  oled.drawBitmap(bx, by, LOGO_32, LOGO_W, LOGO_H, WHITE);

  // sprite ใบพัด (พื้นดำทึบด้วย bg → ไม่ทิ้งรอย)
  frame = (frame + 1) % FAN_FRAMES;
  oled.drawBitmap(W - FAN_W, H - FAN_H, FAN_SPRITE[frame], FAN_W, FAN_H, WHITE, BLACK);

  // หัวใจเต้น: สลับวาด/ไม่วาดทุก 8 เฟรม
  if ((frames & 8) == 0) oled.drawXBitmap(W - FAN_W - HEART_W - 2, H - HEART_H, HEART_XBM, HEART_W, HEART_H, WHITE);

  // FPS
  oled.setCursor(0, 0);
  oled.print(F("FPS "));
  oled.print(fps);
  oled.display();

  frames++;
  if (millis() - fpsTimer >= 1000) {
    fps = frames;
    frames = 0;
    fpsTimer += 1000;
    Serial.print(F("FPS: "));
    Serial.println(fps);
  }
}
