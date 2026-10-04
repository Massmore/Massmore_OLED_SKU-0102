/*
  09_AutoDetect_Identity — Massmore_OLED (Advance)
  ---------------------------------------------------------------------------
  1. สแกน I2C หาจอที่ 0x3C / 0x3D
  2. MASSMORE_OLED_AUTO: probe ชนิดชิปจากพฤติกรรม แล้วเลือก profile เอง
  3. อ่าน status / ID byte และตรวจ "Controller Identity" แสดงผลบนจอ + Serial

  วิธีตรวจ (อิง datasheet ใน docs/datasheet):
    - SH1107 : Read ID 6 บิตล่าง = 000111b (07h)
    - SH1106 : status 3 บิตล่าง = 000
    - SSD1309: คำสั่ง Command Lock (FDh 16h) ทำให้ AEh ถูกเมิน — SSD1306 ไม่มีคำสั่งนี้
    - ทุกชิป : บิต D6 ของ status กลับค่าตาม AEh / AFh (ON/OFF loop-back)

  ขอบเขตที่ต้องรู้ (สำคัญ):
    - ชิปเหล่านี้ "ไม่มี" serial number / OTP ID — ผลนี้ยืนยันว่าชิป "ทำงานตรง datasheet ของ controller"
      และตรงกับ SKU ที่เลือก ไม่ใช่การพิสูจน์ของแท้แบบ cryptographic
    - SSD1306 ส่วนใหญ่ไม่ให้อ่าน status ผ่าน I2C → ผล CONSISTENT (ไม่ขัดแย้ง แต่ยืนยันไม่ได้)
    - SSD1306 128x32 กับ 128x64 แยกทางไฟฟ้าไม่ได้ → AUTO ถือเป็น 128x64 — เลือกตาม SKU จะแม่นกว่า

  จอ 5 ขา (1.5" / 2.42"): ต่อ RES แล้วตั้ง OLED_RST
  Wiring: ดู 01_HelloWorld
  Designed and Manufactured by Massmore — https://www.massmore.shop
*/

#include <Wire.h>
#include <Massmore_OLED.h>

#define OLED_RST    (-1)                       // จอ 5 ขา: ใส่เลขขา RES เช่น 17

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define I2C_SDA 14
  #define I2C_SCL 15
#elif defined(ARDUINO_ARCH_ESP32)
  #define I2C_SDA 21
  #define I2C_SCL 22
#endif

Massmore_OLED oled(MASSMORE_OLED_AUTO);

uint8_t scanBus() {
  uint8_t found = 0;
  Serial.println(F("I2C scan:"));
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  device at 0x"));
      if (a < 0x10) Serial.print('0');
      Serial.print(a, HEX);
      if (a == 0x3C || a == 0x3D) { Serial.print(F("  <- OLED")); if (!found) found = a; }
      Serial.println();
    }
  }
  return found;
}

void setup() {
  Serial.begin(115200);
  delay(300);
#if defined(I2C_SDA)
  Wire.begin(I2C_SDA, I2C_SCL);
#else
  Wire.begin();
#endif
  Wire.setClock(400000);

  // จอ 5 ขาจะไม่ตอบ I2C จนกว่าจะปล่อย RES → reset ก่อนสแกน
  if (OLED_RST >= 0) {
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(10);
    digitalWrite(OLED_RST, HIGH);
    delay(10);
  }

  uint8_t addr = scanBus();
  if (!addr) {
    Serial.println(F("No OLED at 0x3C/0x3D — check wiring (and RES on 5-pin modules)"));
    while (true) delay(100);
  }

  Massmore_OLED_Error e = oled.begin(Wire, addr, OLED_RST);
  if (e != MASSMORE_OLED_OK) {
    Serial.print(F("begin: "));
    Serial.println(Massmore_OLED::errorString(e));
    while (true) delay(100);
  }

  Serial.print(F("AUTO detected : "));
  Serial.print(oled.getControllerName());
  Serial.print(F(" "));
  Serial.print(oled.width());
  Serial.print('x');
  Serial.println(oled.height());

  int16_t id = oled.readChipID();
  Serial.print(F("readChipID()  : "));
  if (id < 0) Serial.println(F("not readable"));
  else { Serial.print(F("0x")); Serial.println(id, HEX); }

  oled.verifyController();
  Serial.println(F("---- Identity report ----"));
  oled.getIdentityReport(Serial);

  // แสดงบนจอ
  oled.clear();
  oled.setCursor(0, 0);
  oled.print(F("Addr 0x"));
  oled.println(addr, HEX);
  oled.print(F("Chip "));
  oled.print(oled.getControllerName());
  oled.print(' ');
  oled.print(oled.width());
  oled.print('x');
  oled.println(oled.height());
  oled.print(F("Status "));
  if (oled.identityStatusByte() < 0) oled.println(F("n/a"));
  else { oled.print(F("0x")); oled.println(oled.identityStatusByte(), HEX); }
  oled.println(F("Identity:"));
  oled.setTextSize(oled.height() >= 64 ? 2 : 1);
  oled.println(Massmore_OLED::identityString(oled.lastIdentity()));
  oled.setTextSize(1);
  oled.display();
}

void loop() {
  // ตรวจซ้ำทุก 10 วินาที (เช่น ถอดเปลี่ยนจอขณะทดสอบ)
  static uint32_t t = 0;
  if (millis() - t >= 10000) {
    t = millis();
    Serial.print(F("re-verify: "));
    Serial.println(Massmore_OLED::identityString(oled.verifyController()));
  }
}
