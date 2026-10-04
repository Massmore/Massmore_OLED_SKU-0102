/*
  02_Graphics_Primitives — Massmore_OLED (Basic)
  ---------------------------------------------------------------------------
  สาธิตฟังก์ชันวาดทุกตัว (API เดียวกับ Adafruit-GFX) ทีละหน้า หน้าละ 2 วินาที:
    pixel / line / rect / roundRect / circle / ellipse / triangle / arc / สี INVERSE

  สี: BLACK (0) · WHITE (1) · INVERSE (2 = กลับสีพิกเซลเดิม)
  ทุกรูปแบบเติมสีวาดแถวละครั้งเดียว → ใช้ INVERSE ซ้อนกันได้ถูกต้อง

  Wiring: ดู 01_HelloWorld (ESP32 21/22 · ESP32-S3 14/15 · Nano A4/A5)
  Designed and Manufactured by Massmore — https://www.massmore.shop
*/

#include <Wire.h>
#include <Massmore_OLED.h>

#define OLED_SKU    OLED096_White_SSD1306      // ← เปลี่ยนเป็นรุ่นของคุณ
#define OLED_ADDR   0x3C
#define OLED_RST    (-1)                       // จอ 5 ขา: ใส่เลขขา RES

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define I2C_SDA 14
  #define I2C_SCL 15
#elif defined(ARDUINO_ARCH_ESP32)
  #define I2C_SDA 21
  #define I2C_SCL 22
#endif

Massmore_OLED oled(OLED_SKU);
int16_t W, H;                                   // ขนาดจอ (ใช้วาดให้พอดีทุกรุ่น)

void title(const __FlashStringHelper *t) {
  oled.clear();
  oled.setCursor(0, 0);
  oled.print(t);
}

void pagePixels() {
  title(F("drawPixel"));
  for (int16_t i = 0; i < 300; i++) oled.drawPixel(random(W), 10 + random(H - 10), WHITE);
}

void pageLines() {
  title(F("drawLine"));
  for (int16_t x = 0; x < W; x += 8) oled.drawLine(0, H - 1, x, 10, WHITE);
  for (int16_t y = 10; y < H; y += 6) oled.drawLine(W - 1, H - 1, 0, y, WHITE);
  oled.drawFastHLine(0, 9, W, WHITE);
  oled.drawFastVLine(W / 2, 10, H - 10, INVERSE);
}

void pageRects() {
  title(F("Rect / RoundRect"));
  oled.drawRect(2, 12, W / 3, H - 14, WHITE);
  oled.fillRect(6, 16, W / 3 - 8, (H - 14) / 3, WHITE);
  oled.drawRoundRect(W / 3 + 6, 12, W / 3, H - 14, 6, WHITE);
  oled.fillRoundRect(2 * W / 3 + 8, 12, W / 3 - 10, H - 14, 8, WHITE);
  oled.fillRect(W / 4, H / 2, W / 2, 8, INVERSE);           // แถบกลับสีพาดทับ
}

void pageCircles() {
  title(F("Circle / Ellipse"));
  int16_t r = (H - 12) / 2 - 1;
  oled.drawCircle(r + 2, 11 + r, r, WHITE);
  oled.fillCircle(r + 2, 11 + r, r / 2, WHITE);
  oled.drawEllipse(W / 2 + 10, 11 + r, r + 10, r / 2, WHITE);
  oled.fillEllipse(W - 14, 11 + r, 10, r - 2, WHITE);
  oled.fillCircle(r + 2 + r / 2, 11 + r, r / 2, INVERSE);    // วงกลมกลับสีซ้อน
}

void pageTriangles() {
  title(F("Triangle / Arc"));
  oled.drawTriangle(4, H - 2, 24, 12, 44, H - 2, WHITE);
  oled.fillTriangle(30, H - 2, 50, 14, 70, H - 6, WHITE);
  oled.fillTriangle(10, H - 10, 60, H - 20, 40, H - 2, INVERSE);
  int16_t r = (H - 14) / 2;
  oled.drawArc(W - r - 4, 12 + r, r, 180, 360, WHITE, 3);    // ครึ่งวงบน หนา 3 px
  oled.drawArc(W - r - 4, 12 + r, r - 6, 0, 270, WHITE);      // 3/4 วง
}

void pageInverse() {
  title(F("INVERSE"));
  for (int16_t i = 0; i < 6; i++) {
    oled.fillCircle(random(W), 10 + random(H - 10), 6 + random(H / 4), INVERSE);
  }
  oled.fillRect(0, 10, W, H - 10, INVERSE);                  // กลับทั้งพื้นที่
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
  W = oled.width();
  H = oled.height();
  randomSeed(analogRead(0));
}

void loop() {
  void (*pages[])() = { pagePixels, pageLines, pageRects, pageCircles, pageTriangles, pageInverse };
  for (uint8_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
    pages[i]();
    uint32_t t0 = millis();
    oled.display();
    Serial.print(F("page "));
    Serial.print(i);
    Serial.print(F(" sent in "));
    Serial.print(millis() - t0);
    Serial.println(F(" ms"));
    delay(2000);
  }
}
