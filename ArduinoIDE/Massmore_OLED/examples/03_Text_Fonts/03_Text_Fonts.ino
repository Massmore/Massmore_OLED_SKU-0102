/*
  03_Text_Fonts — Massmore_OLED (Basic)
  ---------------------------------------------------------------------------
  ข้อความแบบต่าง ๆ ทีละหน้า:
    1. ฟอนต์ 5x7 ในตัว + setTextSize(1..3) + ขนาดแยกแกน setTextSize(sx, sy)
    2. ตัวเลข: print(int), print(float, digits), HEX/BIN, printf()
    3. ตัดบรรทัดอัตโนมัติ (setTextWrap) และสีพื้นหลัง setTextColor(fg, bg)
    4. จัดตำแหน่ง drawText(... ALIGN_LEFT / CENTER / RIGHT) + getTextBounds() กรอบข้อความ
    5. ฟอนต์รูปแบบ Adafruit GFXfont (MassmoreDigits26.h ในโฟลเดอร์นี้) — นาฬิกาตัวใหญ่

  Cursor: ฟอนต์ 5x7 → y = ขอบบน · GFXfont / ฟอนต์ไทย → y = baseline
  หมายเหตุ AVR: printf() ไม่รองรับ %f → ใช้ print(value, digits)

  Wiring: ดู 01_HelloWorld
  Designed and Manufactured by Massmore — https://www.massmore.shop
*/

#include <Wire.h>
#include <Massmore_OLED.h>
#include "MassmoreDigits26.h"

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

void waitPage() {
  oled.display();
  delay(2500);
  oled.clear();
  oled.setFont();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setTextWrap(true);
  oled.setCursor(0, 0);
}

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
}

void loop() {
  oled.clear();
  oled.setCursor(0, 0);

  // ---- 1) ขนาดตัวอักษร ----
  oled.println(F("Size 1: 5x7 font"));
  oled.setTextSize(2);
  oled.println(F("Size 2"));
  oled.setTextSize(3, 2);                      // กว้าง x3 สูง x2
  oled.println(F("3x2"));
  waitPage();

  // ---- 2) ตัวเลข ----
  oled.print(F("int  : ")); oled.println(-12345);
  oled.print(F("float: ")); oled.println(3.14159, 3);
  oled.print(F("hex  : 0x")); oled.println(0xBEEF, HEX);
  oled.print(F("bin  : ")); oled.println(0x5A, BIN);
  oled.printf("printf %d/%u", 42, 2026u);
  waitPage();

  // ---- 3) ตัดบรรทัด + สีพื้นหลัง ----
  oled.println(F("Long text wraps automatically at word boundaries on every panel."));
  oled.setTextColor(BLACK, WHITE);             // ตัวดำบนพื้นขาว (ฟอนต์ 5x7)
  oled.println(F(" Inverted label "));
  oled.setTextColor(WHITE);
  oled.setTextWrap(false);
  oled.println(F("No wrap: this line is cut at the edge"));
  waitPage();

  // ---- 4) จัดตำแหน่ง + กรอบข้อความ ----
  int16_t W = oled.width();
  oled.drawText(0, 0, "LEFT", ALIGN_LEFT);
  oled.drawText(W / 2, 12, "CENTER", ALIGN_CENTER);
  oled.drawText(W - 1, 24, "RIGHT", ALIGN_RIGHT);
  if (oled.height() >= 64) {
    oled.setFont(&Sarabun16);
    int16_t x1, y1;
    uint16_t bw, bh;
    const char *msg = "กรอบ Bounds";
    oled.getTextBounds(msg, 10, 56, &x1, &y1, &bw, &bh);   // กรอบหมึกจริง (รวมวรรณยุกต์)
    oled.drawRect(x1 - 2, y1 - 2, bw + 4, bh + 4, WHITE);
    oled.drawText(10, 56, msg);
  }
  waitPage();

  // ---- 5) GFXfont: นาฬิกาตัวเลขใหญ่ 5 วินาที ----
  for (uint8_t i = 0; i < 10; i++) {
    uint32_t s = millis() / 1000;
    char t[9];
    snprintf(t, sizeof(t), "%02lu:%02lu", (unsigned long)(s / 60) % 60, (unsigned long)s % 60);
    oled.clear();
    oled.setFont();
    oled.drawText(oled.width() / 2, 0, "GFXfont", ALIGN_CENTER);
    oled.setFont(&MassmoreDigits26);
    oled.drawText(oled.width() / 2, oled.height() >= 64 ? 44 : 30, t, ALIGN_CENTER);
    oled.display();
    delay(500);
  }
  oled.setFont();
}
