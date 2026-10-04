/*
  06_Display_Control_Scroll — Massmore_OLED (Intermediate)
  ---------------------------------------------------------------------------
  ควบคุมจอระดับฮาร์ดแวร์ ทีละหัวข้อ (รายงานผลทาง Serial 115200):
    1. setContrast() / setBrightness() ไล่ความสว่าง
    2. invert()           กลับสีทั้งจอ (ฮาร์ดแวร์ ไม่แตะ buffer)
    3. setRotation(0..3)  หมุนภาพทีละ 90° (ซอฟต์แวร์)
    4. flipHorizontal / flipVertical  กลับด้านด้วยฮาร์ดแวร์ (SEG/COM remap)
    5. sleep() / wake()   ปิดจอ + ปิด charge pump / DC-DC (กินไฟต่ำสุด)
    6. allPixelsOn()      ทุกพิกเซลติด (ใช้หาจุดตาย)
    7. HW scroll          SSD1306 / SSD1309 เท่านั้น — SH1106/SH1107 คืน ERR_NOT_SUPPORTED อย่างสุภาพ
    8. scrollContent()    SSD1309 เลื่อน RAM ทีละคอลัมน์
    9. scrollBuffer()     software scroll ใช้ได้ทุกชิป

  Wiring: ดู 01_HelloWorld
  Designed and Manufactured by Massmore — https://www.massmore.shop
*/

#include <Wire.h>
#include <Massmore_OLED.h>

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

// พิมพ์ผลลัพธ์: "<ชื่อ> -> OK" หรือ "-> Not supported by this controller"
void report(const __FlashStringHelper *what, Massmore_OLED_Error e) {
  Serial.print(what);
  Serial.print(F(" -> "));
  Serial.println(Massmore_OLED::errorString(e));
}

// ภาพทดสอบ: กรอบ + ลูกศรชี้ขวาบน + ข้อความ
void drawTestImage(const char *label) {
  int16_t W = oled.width(), H = oled.height();
  oled.clear();
  oled.drawRect(0, 0, W, H, WHITE);
  oled.fillTriangle(W - 3, 2, W - 14, 2, W - 3, 13, WHITE);
  oled.setCursor(4, 4);
  oled.print(label);
  oled.setCursor(4, H - 10);
  oled.print(W);
  oled.print('x');
  oled.print(H);
  oled.display();
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
  Serial.print(F("Controller: "));
  Serial.println(oled.getControllerName());
}

void loop() {
  // ---- 1) ความสว่าง ----
  drawTestImage("Brightness");
  for (int p = 100; p >= 0; p -= 10) { oled.setBrightness(p); delay(150); }
  for (int c = 0; c <= 255; c += 15) { oled.setContrast(c); delay(60); }
  report(F("setBrightness/Contrast"), oled.lastError());
  oled.setBrightness(80);

  // ---- 2) invert ----
  drawTestImage("Invert");
  report(F("invert(true)"), oled.invert(true));
  delay(1500);
  oled.invert(false);

  // ---- 3) software rotation ----
  for (uint8_t r = 0; r < 4; r++) {
    oled.setRotation(r);
    char t[12];
    snprintf(t, sizeof(t), "Rot %u", r);
    drawTestImage(t);
    delay(1200);
  }
  oled.setRotation(0);

  // ---- 4) hardware flip ----
  drawTestImage("Flip H");
  report(F("flipHorizontal(true)"), oled.flipHorizontal(true));
  delay(1500);
  oled.flipHorizontal(false);
  drawTestImage("Flip V");
  report(F("flipVertical(true)"), oled.flipVertical(true));
  delay(1500);
  oled.flipVertical(false);

  // ---- 5) sleep / wake ----
  drawTestImage("Sleep 2s");
  delay(800);
  report(F("sleep()"), oled.sleep());
  delay(2000);
  report(F("wake()"), oled.wake());
  delay(800);

  // ---- 6) all pixels on ----
  report(F("allPixelsOn(true)"), oled.allPixelsOn(true));
  delay(1500);
  oled.allPixelsOn(false);

  // ---- 7) hardware scroll ----
  drawTestImage("HW scroll");
  Massmore_OLED_Error e = oled.scrollRight(0, oled.getPages() - 1, 5);
  report(F("scrollRight"), e);
  if (e == MASSMORE_OLED_OK) {
    delay(2500);
    oled.scrollLeft(0, oled.getPages() - 1, 5);
    delay(2500);
    report(F("scrollDiagRight"), oled.scrollDiagRight(0, oled.getPages() - 1, 6, 1));
    delay(2500);
    report(F("scrollStop"), oled.scrollStop());   // คืนภาพเดิมให้อัตโนมัติ
  } else {
    oled.setCursor(4, 14);
    oled.print(F("HW scroll: N/A"));
    oled.display();
    delay(1500);
  }

  // ---- 8) SSD1309 content scroll ----
  drawTestImage("Content");
  e = oled.scrollContent(-1);
  report(F("scrollContent"), e);
  if (e == MASSMORE_OLED_OK) {
    for (int i = 0; i < 64; i++) { oled.scrollContent(-1); delay(15); }
    oled.displayAll();                            // RAM เลื่อนไปแล้ว → เขียน buffer กลับ
  }

  // ---- 9) software scroll (ทุกชิป) ----
  drawTestImage("SW scroll");
  for (int i = 0; i < 32; i++) {
    oled.scrollBuffer(-2, 0);                    // เลื่อนซ้าย 2 px
    oled.display();
  }
  delay(1000);
}
