/*
  04_Thai_Sarabun — Massmore_OLED (Intermediate)
  ---------------------------------------------------------------------------
  แสดงภาษาไทยด้วยฟอนต์ Sarabun (SIL OFL 1.1) — พิมพ์ UTF-8 ตรง ๆ ด้วย print()
    1. ทุกขนาดที่มี: 12 / 16 / 20 / 24 / 32 px (+ setTextSize ขยายเท่าตัว)
    2. ไทย-อังกฤษปนกัน + ตัวเลขไทย
    3. ทดสอบสระ/วรรณยุกต์ซ้อน:  ปู่ ญี่ปุ่น ฏุ ป้ำ ฟ้า น้ำ ที่
       - วรรณยุกต์ลอยต่ำเมื่อไม่มีสระบน, ซ้อนเหนือสระบนเมื่อมี
       - ป ฝ ฟ ฬ เลื่อนเครื่องหมายไปทางซ้ายพ้นหาง
       - ฎ ฏ สระล่างลงใต้หาง · ญ ฐ ตัดเชิงเมื่อมีสระล่าง
       - ำ = นิคหิตบนพยัญชนะ (วรรณยุกต์ซ้อนเหนือ) + า
    4. จัดกึ่งกลาง / ชิดขวา และวัดความกว้าง thaiTextWidth()
    5. ตัดบรรทัด: ภาษาไทยไม่มีช่องว่างระหว่างคำ → ใส่ช่องว่าง หรือ ZWSP (U+200B, "\u200B") ตรงที่ตัดได้

  ขนาดฟอนต์ที่ใช้ได้:
    ESP32 / ESP32-S3 : ครบทุกขนาด (ฟอนต์ที่ไม่ได้ใช้ linker ตัดทิ้งเอง)
    Arduino Nano     : Sarabun16 อย่างเดียว (ประหยัด flash) — เพิ่มได้ด้วย
                       #define MASSMORE_THAI_FONT_12 (หรือ _20/_24/_32) ก่อน #include <Massmore_OLED.h>

  Cursor ของฟอนต์ไทย: y = baseline (เส้นฐานของตัวอักษร)
  F("...") เก็บข้อความใน flash — สำคัญบน Nano (ภาษาไทย 1 ตัว = 3 byte, RAM มีแค่ 2 KB)
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

struct FontItem { const MassmoreFont *font; const char *name; };
const FontItem FONTS[] = {
#ifdef MASSMORE_HAS_SARABUN12
  { &Sarabun12, "12px" },
#endif
#ifdef MASSMORE_HAS_SARABUN16
  { &Sarabun16, "16px" },
#endif
#ifdef MASSMORE_HAS_SARABUN20
  { &Sarabun20, "20px" },
#endif
#ifdef MASSMORE_HAS_SARABUN24
  { &Sarabun24, "24px" },
#endif
#ifdef MASSMORE_HAS_SARABUN32
  { &Sarabun32, "32px" },
#endif
};
const uint8_t FONT_COUNT = sizeof(FONTS) / sizeof(FONTS[0]);

void showPage(uint16_t ms = 3000) {
  oled.display();
  delay(ms);
  oled.clear();
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
  int16_t W = oled.width(), H = oled.height();

  // ---- 1) ทุกขนาด: "สวัสดี" ตามด้วยชื่อขนาด (ฟอนต์ 5x7 มุมขวาบน) ----
  for (uint8_t i = 0; i < FONT_COUNT; i++) {
    oled.clear();
    oled.setFont();
    oled.drawText(W - 1, 0, FONTS[i].name, ALIGN_RIGHT);
    oled.setFont(FONTS[i].font);
    oled.setCursor(0, oled.fontAscent());       // บรรทัดแรกชิดขอบบน
    oled.print(F("สวัสดี"));
    if (oled.fontHeight() * 2 <= H) {
      oled.setCursor(0, oled.fontAscent() + oled.fontHeight());
      oled.print(F("ภาษาไทย"));
    }
    showPage(2000);
  }

  // ---- 2) ไทยปนอังกฤษ + ตัวเลขไทย (Sarabun16) ----
  oled.setFont(&Sarabun16);
  oled.setCursor(0, oled.fontAscent());
  oled.println(F("Massmore ร้านอุปกรณ์"));
  oled.print(F("อุณหภูมิ ๒๕.๕ °C"));   // ° ไม่มีในฟอนต์ → ข้ามไปเอง
  showPage();

  // ---- 3) สระ/วรรณยุกต์ซ้อน ----
  oled.setCursor(0, oled.fontAscent());
  oled.println(F("ปู่ ญี่ปุ่น ฏุ ป้ำ"));
  oled.print(F("ฟ้า น้ำ ที่ ฐุ ฎู"));
  showPage(4000);

  // ---- 4) จัดตำแหน่ง ----
  oled.setFont(&Sarabun16);
  int16_t y = oled.fontAscent();
  oled.drawText(0, y, F("ซ้าย"), ALIGN_LEFT);
  oled.drawText(W / 2, y + 20, F("กึ่งกลาง"), ALIGN_CENTER);
  if (H >= 64) oled.drawText(W - 1, y + 40, F("ขวา"), ALIGN_RIGHT);
  Serial.print(F("thaiTextWidth(\"กึ่งกลาง\") = "));
  Serial.println(oled.textWidth(F("กึ่งกลาง")));
  showPage();

  // ---- 5) ตัดบรรทัดอัตโนมัติ: ZWSP (\u200B) บอกจุดที่ตัดได้โดยไม่เห็นช่องว่าง ----
  //      (ข้อความใน RAM ตัดทั้งคำได้แม่นที่สุด · F("...") ก็ใช้ได้)
  oled.setTextWrap(true);
  oled.setCursor(0, oled.fontAscent());
  oled.print("ไลบรารี\u200Bจอ\u200BOLED\u200Bของ\u200BMassmore\u200Bรองรับ\u200Bภาษาไทย");
  showPage(4000);

  // ---- 6) ขยายด้วย setTextSize(2) (เป็นบล็อก แต่ไม่เปลือง flash) ----
  oled.setTextSize(2);
  oled.drawText(W / 2, oled.fontAscent(), F("ไทย"), ALIGN_CENTER);
  oled.setTextSize(1);
  showPage();
}
