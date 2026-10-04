/*
  01_HelloWorld — Massmore_OLED (Basic)
  ---------------------------------------------------------------------------
  ตัวอย่างแรก: เริ่มจอ, ล้างจอ, พิมพ์ข้อความอังกฤษ + ไทย, ส่งขึ้นจอ และจัดการ error

  เลือกรุ่นจอที่บรรทัด OLED_SKU — ชื่อ = OLED<ขนาด>_<สี>_<ชิป> (ขนาด = นิ้ว x 100 เช่น 096 = 0.96"):
    OLED091_White_SSD1306      OLED091_Blue_SSD1306       0.91"  128x32
    OLED096_White_SSD1306      OLED096_Blue_SSD1306       0.96"  128x64
    OLED096_BlueYellow_SSD1306                            0.96"  128x64 (แถวบน 16 px สีเหลือง)
    OLED130_White_SH1106       OLED130_Blue_SH1106        1.3"   128x64
    OLED150_White_SH1107                                  1.5"   128x128 (5 ขา ต่อ RES)
    OLED154_White_SSD1309      OLED154_Blue_SSD1309       1.54"  128x64
    OLED242_White_SSD1309      OLED242_Blue_SSD1309       2.42"  128x64  (5 ขา ต่อ RES)
  ไม่แน่ใจรุ่น → ใช้ MASSMORE_OLED_AUTO (ตรวจชิปเอง) · ชื่อเดิม MASSMORE_OLED_SKU_0102_N ยังใช้ได้

  Wiring (I2C)
    ESP32 Classic   : SDA -> GPIO 21, SCL -> GPIO 22
    ESP32-S3 (MOMO) : SDA -> GPIO 14, SCL -> GPIO 15
    Arduino Nano    : SDA -> A4,      SCL -> A5
    VCC -> 3V3 (หรือ 5V), GND -> GND · จอ 5 ขา (1.5" / 2.42"): RES -> ขาที่ตั้งใน OLED_RST

  Designed and Manufactured by Massmore — https://www.massmore.shop
*/

#include <Wire.h>
#include <Massmore_OLED.h>

/* ---------------- ตั้งค่าที่นี่ ---------------- */
#define OLED_SKU    OLED096_White_SSD1306      // ← เปลี่ยนเป็นรุ่นของคุณ
#define OLED_ADDR   0x3C                       // 0x3C (ค่าเริ่มต้น) หรือ 0x3D
#define OLED_RST    (-1)                       // จอ 5 ขา: ใส่เลขขา เช่น 17 · จอ 4 ขา: -1

#if defined(CONFIG_IDF_TARGET_ESP32S3)          // MOMO by Massmore
  #define I2C_SDA 14
  #define I2C_SCL 15
#elif defined(ARDUINO_ARCH_ESP32)               // ESP32 Classic
  #define I2C_SDA 21
  #define I2C_SCL 22
#endif                                          // AVR: ใช้ขา A4/A5 ของฮาร์ดแวร์
/* ---------------------------------------------- */

Massmore_OLED oled(OLED_SKU);

void setup() {
  Serial.begin(115200);
  delay(200);

#if defined(I2C_SDA)
  Wire.begin(I2C_SDA, I2C_SCL);                 // sketch เป็นเจ้าของ bus — ไลบรารีไม่เรียก Wire.begin()
#else
  Wire.begin();
#endif
  Wire.setClock(400000);                        // 400 kHz = ความเร็วสูงสุดตาม datasheet

  Massmore_OLED_Error err = oled.begin(Wire, OLED_ADDR, OLED_RST);
  if (err != MASSMORE_OLED_OK) {
    Serial.print(F("OLED begin failed: "));
    Serial.println(Massmore_OLED::errorString(err));
    while (true) delay(100);                    // หยุด — ตรวจสาย / address / รุ่นจอ
  }
  Serial.print(F("OLED ready: "));
  Serial.print(oled.getModelName());
  Serial.print(F(" ("));
  Serial.print(oled.getControllerName());
  Serial.print(F(" "));
  Serial.print(oled.width());
  Serial.print('x');
  Serial.print(oled.height());
  Serial.println(F(")"));

  oled.clear();

  // ฟอนต์ 5x7 ในตัว: cursor = มุมซ้ายบนของตัวอักษร
  oled.setCursor(0, 0);
  oled.println(F("Hello, Massmore!"));
  oled.print(F("Controller: "));
  oled.println(oled.getControllerName());

  // ฟอนต์ไทย Sarabun: cursor y = baseline
  if (oled.height() >= 64) {
    oled.setFont(&Sarabun16);
    oled.setCursor(0, 46);
    oled.print("สวัสดีครับ");
    oled.setFont();                             // กลับไปใช้ฟอนต์ 5x7
  }

  err = oled.display();                         // ส่ง buffer ขึ้นจอ (เฉพาะ page ที่เปลี่ยน)
  if (err != MASSMORE_OLED_OK) Serial.println(Massmore_OLED::errorString(err));
}

void loop() {
  // นับวินาทีที่มุมขวาล่าง — วาดทับเฉพาะพื้นที่เล็ก ๆ แล้ว display() ส่งแค่ page นั้น
  static uint32_t last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    oled.fillRect(oled.width() - 36, oled.height() - 8, 36, 8, BLACK);
    oled.setCursor(oled.width() - 36, oled.height() - 8);
    oled.print(millis() / 1000);
    oled.print('s');
    oled.display();
  }
}
