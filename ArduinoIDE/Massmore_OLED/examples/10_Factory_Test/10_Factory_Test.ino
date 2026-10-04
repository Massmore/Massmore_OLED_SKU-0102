/*
  10_Factory_Test — Massmore_OLED (Outgoing QA / QC, SKU-0102-1 … 12)
  ---------------------------------------------------------------------------
  ใช้ตรวจจอ OLED ก่อนส่งลูกค้า และให้ Massmore Web Serial Monitor อ่านผลอัตโนมัติ

  ขั้นตอน (Factory Test ตัวเดียวใช้ได้ทุกขนาด):
    1. เสียบจอ → บอร์ดสแกน I2C และ "ตรวจชิป" ให้ก่อน แล้วรายงาน #DETECT <ชิป> (แสดงบนจอด้วย)
    2. พิมพ์ขนาดจอใน Serial Monitor แล้ว Enter:
         0.91  0.96  1.3  1.5  1.54  2.42      (ต่อท้าย B = สีฟ้า, Y = ฟ้า-เหลือง เช่น "0.96B", "0.96Y")
       หรือ "SKU 0102-N" (N = 1…12) สำหรับ Massmore Web Serial Monitor
    3. ทดสอบตามขนาดที่เลือก — ถ้าขนาดไม่ตรงกับชิปที่ตรวจพบ CONTROLLER_ID จะ FAIL
    4. จบแล้ว: เสียบจอตัวถัดไปแล้วกด Enter (ตรวจชิป + ถามขนาดใหม่) · 'r' = ทดสอบจอเดิมซ้ำ

  Test sequence (#RESULT ละบรรทัด)
    1. BUS_SCAN       ACK ที่ 0x3C หรือ 0x3D                                     FAIL ถ้าไม่พบ
    2. RESET_PIN      (จอ 5 ขา) ชิปไม่ตอบขณะ RES = LOW และกลับมาตอบหลังปล่อย      WARN
    3. INIT_SEQ       ทุก byte ของ init sequence ได้ ACK                         FAIL
    4. CHIP_STATUS    อ่าน status byte ได้ (บันทึกค่า)                           WARN ถ้าอ่านไม่ได้ (SSD1306)
    5. CONTROLLER_ID  Controller Identity = VERIFIED / CONSISTENT และตรง SKU      FAIL ถ้า MISMATCH
                      (SKU ตระกูล SSD1306 รับ SSD1315 ได้ — value แสดงชิปที่ตรวจพบจริง)
    6. DISPLAY_ONOFF  บิต D6 ของ status กลับค่าตาม AEh / AFh                     FAIL (ถ้าอ่าน status ได้)
    7. FRAME_WRITE    ส่งเต็มเฟรมได้ ACK ครบ (บันทึกเวลา ms)                       FAIL
    8. FRAME_RATE     >= 30 fps (128x32/64) หรือ >= 15 fps (128x128) ที่ 400 kHz   WARN
    9. I2C_400K       ส่ง 50 เฟรมติดกันที่ 400 kHz ไม่มี NACK                     FAIL
   10. VISUAL         พนักงานดูภาพทดสอบ (all-on, checkerboard, border, inverse,
                      contrast sweep, ภาษาไทย, โซน Blue-Yellow) แล้วกด
                      'P' = ผ่าน / 'F' = ไม่ผ่าน ทาง Serial หรือกดปุ่ม BOOT (GPIO0) = ผ่าน
                      ไม่ตอบใน 30 วินาที = SKIP                                     FAIL / SKIP
       * จุดตาย (dead pixel) ตรวจทางไฟฟ้าไม่ได้ — ต้องดูด้วยตาในขั้นตอนนี้เท่านั้น

  Serial 115200 — ทุกบรรทัดที่เว็บ parse ขึ้นต้นด้วย '#' (English only):
    #MASSMORE_FACTORY_TEST v1.0
    #PRODUCT Massmore_OLED
    #DETECT <SSD1306 | SSD1309/SSD1315 | SH1106 | SH1107 | NONE> <addr> [ASSUMED]
    #PROMPT SIZE                          (รอขนาดจอ / "SKU 0102-N")
    #SKU <SKU-0102-N>
    #MCU <ESP32 | ESP32-S3 | AVR_NANO>
    #CHIP <ชิปของ SKU ที่เลือก>
    #RESULT <TEST_NAME> <PASS|FAIL|WARN|SKIP> <value>
    #VERDICT <PASS|FAIL> [<REASON>]
    [PASS] OLED QA PASSED - READY TO SHIP - <DRIVER>   หรือ   [FAIL] QA CHECK FAILED: <REASON> - <DRIVER>
    #DRIVER <SSD1306 | SSD1315 | SSD1309 | SH1106 | SH1107>   (driver ที่ตรวจพบจริง)
    PASS - <DRIVER>  /  FAIL - <DRIVER>                       (บรรทัดสรุป — แสดงบนจอด้วย)

  Default wiring (I2C / Qwiic)
    ESP32 Classic   : SDA -> GPIO 21, SCL -> GPIO 22, RES -> GPIO 17 (เฉพาะจอ 1.5" / 2.42")
    ESP32-S3 (MOMO) : SDA -> GPIO 14, SCL -> GPIO 15, RES -> GPIO 18
    Nano            : SDA -> A4,      SCL -> A5,      RES -> D4      (SKU-0102-8 ใช้ไม่ได้: RAM ไม่พอ)
    VCC -> 3V3, GND -> GND
  Pin ถูก hardcode ไว้ "เฉพาะใน sketch นี้" ไม่ใช่ในไลบรารี

  Designed and Manufactured by Massmore — https://www.massmore.shop
*/

#include <Wire.h>
#include <Massmore_OLED.h>

/* ---------------- Factory Test wiring (ปรับได้ที่นี่เท่านั้น) ------------------ */
#if defined(CONFIG_IDF_TARGET_ESP32S3)      // MOMO by Massmore (ESP32-S3 + CH343P)
  #define FT_SDA_PIN   14
  #define FT_SCL_PIN   15
  #define FT_RST_PIN   18
  #define FT_BTN_PIN   0                     // ปุ่ม BOOT
  #define FT_MCU_NAME  "ESP32-S3"
#elif defined(ARDUINO_ARCH_ESP32)
  #define FT_SDA_PIN   21
  #define FT_SCL_PIN   22
  #define FT_RST_PIN   17                    // Decision D3: ขาเดียวกับ jig BNO08x
  #define FT_BTN_PIN   0                     // ปุ่ม BOOT
  #define FT_MCU_NAME  "ESP32"
#elif defined(__AVR__)
  #define FT_RST_PIN   4
  #define FT_BTN_PIN   (-1)                  // Nano ไม่มีปุ่ม → ใช้ Serial 'P' / 'F'
  #define FT_MCU_NAME  "AVR_NANO"
#else
  #define FT_RST_PIN   (-1)
  #define FT_BTN_PIN   (-1)
  #define FT_MCU_NAME  "UNKNOWN"
#endif
#define FT_I2C_HZ          400000UL
#define FT_VISUAL_TIMEOUT  30000UL
#define FT_STRESS_FRAMES   50

// ตัวเลือกตอน build (ไม่จำเป็น):
//   -DFT_SKU=OLED130_White_SH1106 → ข้ามการถามขนาด ทดสอบรุ่นนี้ทุกครั้ง
//   -DFT_ADDR=0x78 หรือ 0x3C      → ตรวจเฉพาะ address นี้ (0x78 บน silkscreen = 0x3C แบบ 7 บิต)
#ifndef FT_SKU
  #define FT_SKU   MASSMORE_OLED_AUTO
#endif
#ifndef FT_ADDR
  #define FT_ADDR  0                          // 0 = สแกน 0x3C แล้ว 0x3D
#endif
#define FT_ADDR7   (((FT_ADDR) > 0x77) ? ((FT_ADDR) >> 1) : (FT_ADDR))   // รับแบบ 8 บิต (0x78) ได้ด้วย
/* -------------------------------------------------------------------------- */

Massmore_OLED oled(MASSMORE_OLED_AUTO);

Massmore_OLED_Model sku = FT_SKU;
Massmore_OLED_Controller probed = MASSMORE_CTRL_UNKNOWN;   // ชิปที่ตรวจพบตอนเสียบจอ (detectPanel)
Massmore_OLED_Controller driver = MASSMORE_CTRL_UNKNOWN;   // driver ที่รายงานตอนจบ (PASS - <driver>)
bool    anyFail = false;
char    failReason[24] = "";

/* ---------------- Report helpers ---------------- */
void result(const __FlashStringHelper *name, const __FlashStringHelper *status, const char *value) {
  Serial.print(F("#RESULT "));
  Serial.print(name);
  Serial.print(' ');
  Serial.print(status);
  Serial.print(' ');
  Serial.println(value);
  if (strcmp_P("FAIL", (PGM_P)status) == 0 && !anyFail) {
    anyFail = true;
    strncpy_P(failReason, (PGM_P)name, sizeof(failReason) - 1);
  }
}
#define PASS F("PASS")
#define FAIL F("FAIL")
#define WARN F("WARN")
#define SKIP F("SKIP")

void hex2(char *buf, uint8_t v) { snprintf(buf, 8, "0x%02X", v); }

/* ---------------- I2C scan ---------------- */
uint8_t scanAddr() {
  if (FT_RST_PIN >= 0) {                        // ปล่อย RES ก่อน (จอ 5 ขาไม่ตอบระหว่าง reset)
    pinMode(FT_RST_PIN, OUTPUT);
    digitalWrite(FT_RST_PIN, HIGH);
    delay(10);
  }
  for (uint8_t a = FT_ADDR7 ? FT_ADDR7 : MASSMORE_OLED_ADDR_DEFAULT;
       a <= (FT_ADDR7 ? FT_ADDR7 : MASSMORE_OLED_ADDR_ALT); a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) return a;
  }
  return 0;
}

/* ---------------- Serial line input ---------------- */
// อ่าน 1 บรรทัด (รอได้นาน) · คืนความยาว · พิมพ์ prompt ซ้ำทุก 20 วินาที
uint8_t readLine(char *buf, uint8_t size, const __FlashStringHelper *prompt) {
  uint8_t n = 0;
  uint32_t t = millis();
  while (Serial.available()) Serial.read();     // ทิ้งของค้าง
  for (;;) {
    if (millis() - t >= 20000UL) { t = millis(); Serial.println(prompt); }
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') { buf[n] = 0; return n; }
      if (n < size - 1) buf[n++] = c;
    }
    delay(5);
  }
}

/* ---------------- Detect controller ---------------- */
// ตรวจชิปด้วย AUTO แล้วรายงาน · คืน address (0 = ไม่พบจอ)
uint8_t detectPanel() {
  uint8_t addr = scanAddr();
  Serial.println();
  if (!addr) { Serial.println(F("#DETECT NONE")); return 0; }
  oled.setModel(MASSMORE_OLED_AUTO);
  if (oled.begin(Wire, addr, FT_RST_PIN) != MASSMORE_OLED_OK) { Serial.println(F("#DETECT NONE")); return 0; }
  Massmore_OLED_Controller c = oled.getDetectedController();
  probed = c;
  bool assumed = !oled.identityLoopback();      // อ่าน status ไม่ได้ → เดาว่า SSD1306
  Serial.print(F("#DETECT "));
  Serial.print(Massmore_OLED::controllerName(c));
  Serial.print(F(" 0x"));
  Serial.print(addr, HEX);
  Serial.println(assumed ? F(" ASSUMED") : F(""));

  Serial.print(F("Detected controller: "));
  Serial.print(Massmore_OLED::controllerName(c));
  if (assumed) Serial.print(F(" (status unreadable - assumed)"));
  Serial.print(F("  ->  likely size: "));
  switch (c) {
    case MASSMORE_CTRL_SH1106:  Serial.println(F("1.3"));          break;
    case MASSMORE_CTRL_SH1107:  Serial.println(F("1.5"));          break;
    case MASSMORE_CTRL_SSD1309: Serial.println(F("1.54 / 2.42"));  break;
    case MASSMORE_CTRL_SSD1309_1315: Serial.println(F("0.91 / 0.96 / 1.54 / 2.42"));  break;
    default:                    Serial.println(F("0.91 / 0.96"));  break;
  }
  // แสดงบนจอเฉพาะชิปที่รู้ขนาดแน่นอน — ตระกูล SSD1306/SSD1315 มีทั้ง 128x32 (0.91") และ 128x64 (0.96")
  // ซึ่งแยกทางไฟฟ้าไม่ได้: ถ้าวาดด้วยค่า 128x64 บนกระจก 0.91" ภาพจะเพี้ยน → ปล่อยจอว่างจนกว่าจะได้ขนาด
  oled.clear();
  if (c == MASSMORE_CTRL_SH1106 || c == MASSMORE_CTRL_SH1107 || c == MASSMORE_CTRL_SSD1309) {
    oled.setFont();
    oled.setCursor(0, 0);
    oled.print(F("Detected: "));
    oled.println(Massmore_OLED::controllerName(c));
    oled.println(F("Enter size in"));
    oled.println(F("Serial Monitor"));
  } else {
    Serial.println(F("(screen stays blank until the size is entered - this chip is used by several sizes)"));
  }
  oled.display();
  return addr;
}

/* ---------------- Size → SKU ---------------- */
// รับ "0.96", "096", "96", "1.3", "13", "130", "2.42" … + สี B / Y · หรือ "SKU 0102-N"
Massmore_OLED_Model parseSize(const char *in) {
  const char *p = strstr(in, "0102-");
  if (p) {
    int v = atoi(p + 5);
    return (v >= 1 && v <= 12) ? (Massmore_OLED_Model)v : MASSMORE_OLED_AUTO;
  }
  int size;
  if (strchr(in, '.')) {
    size = (int)(atof(in) * 100.0f + 0.5f);
  } else {
    size = atoi(in);
    if (size == 13 || size == 15) size *= 10;   // "13" = 1.3", "15" = 1.5"
  }
  char colour = 'W';
  for (const char *q = in; *q; q++) {
    char u = toupper(*q);
    if (u == 'B' || u == 'Y') colour = u;
  }
  switch (size) {
    case 91:  return colour == 'B' ? OLED091_Blue_SSD1306 : OLED091_White_SSD1306;
    case 96:  return colour == 'Y' ? OLED096_BlueYellow_SSD1306
                   : colour == 'B' ? OLED096_Blue_SSD1306 : OLED096_White_SSD1306;
    case 130: return colour == 'B' ? OLED130_Blue_SH1106 : OLED130_White_SH1106;
    case 150: return OLED150_White_SH1107;
    case 154: return colour == 'B' ? OLED154_Blue_SSD1309 : OLED154_White_SSD1309;
    case 242: return colour == 'B' ? OLED242_Blue_SSD1309 : OLED242_White_SSD1309;
    default:  return MASSMORE_OLED_AUTO;
  }
}

Massmore_OLED_Model askSize() {
  if (FT_SKU != MASSMORE_OLED_AUTO) return FT_SKU;
  char line[24];
  for (;;) {
    Serial.println(F("#PROMPT SIZE"));
    Serial.println(F("Enter panel size: 0.91 / 0.96 / 1.3 / 1.5 / 1.54 / 2.42  (add B = blue, Y = blue-yellow)"));
    readLine(line, sizeof(line), F("#PROMPT SIZE"));
    Massmore_OLED_Model m = parseSize(line);
    if (m != MASSMORE_OLED_AUTO) return m;
    Serial.print(F("Unknown size: "));
    Serial.println(line);
  }
}

/* ---------------- Visual patterns ---------------- */
void pattern(uint8_t idx) {
  int16_t W = oled.width(), H = oled.height();
  oled.allPixelsOn(false);
  oled.invert(false);
  oled.setContrast(0xFF);
  oled.clear();
  oled.setFont();
  switch (idx) {
    case 0:                                       // ทุกพิกเซลติด (A5h) — หาจุดตาย
      oled.display();
      oled.allPixelsOn(true);
      return;
    case 1:                                       // checkerboard 4x4
      for (int16_t y = 0; y < H; y += 4)
        for (int16_t x = 0; x < W; x += 4)
          if (((x + y) >> 2) & 1) oled.fillRect(x, y, 4, 4, WHITE);
      break;
    case 2:                                       // กรอบ + เส้นทแยง (ขอบครบ 4 ด้าน, ไม่มีคอลัมน์เพี้ยน)
      oled.drawRect(0, 0, W, H, WHITE);
      oled.drawLine(0, 0, W - 1, H - 1, WHITE);
      oled.drawLine(W - 1, 0, 0, H - 1, WHITE);
      break;
    case 3:                                       // inverse (A7h) ของ checkerboard
      pattern(1);
      oled.invert(true);
      return;
    case 4:                                       // contrast sweep (ทำใน loop รอ)
      oled.fillRect(0, 0, W, H, WHITE);
      break;
    case 5:                                       // ภาษาไทย
      oled.setFont(&Sarabun16);
      oled.drawText(W / 2, H >= 64 ? 30 : 22, F("ทดสอบ ภาษาไทย"), ALIGN_CENTER);
      if (H >= 64) oled.drawText(W / 2, 56, F("ปู่ ญี่ปุ่น น้ำ"), ALIGN_CENTER);
      oled.setFont();
      break;
    default:                                      // โซน Blue-Yellow: แถว 0–15 / 16–
      oled.fillRect(0, 0, W, MASSMORE_OLED_BY_YELLOW_H, WHITE);
      oled.drawRect(0, MASSMORE_OLED_BY_YELLOW_H + 2, W, H - MASSMORE_OLED_BY_YELLOW_H - 2, WHITE);
      oled.setCursor(4, MASSMORE_OLED_BY_YELLOW_H + 6);
      oled.print(F("ZONE 16px"));
      break;
  }
  oled.display();
}

// 'P' / BOOT = PASS, 'F' = FAIL, หมดเวลา = SKIP
char waitOperator() {
  Serial.println(F("VISUAL: check the patterns, then send 'P' (pass) / 'F' (fail) or press BOOT"));
  uint32_t t0 = millis(), tPat = 0;
  uint8_t idx = 0, contrast = 0;
  pattern(0);
  while (millis() - t0 < FT_VISUAL_TIMEOUT) {
    if (millis() - tPat >= 1500) {                // เปลี่ยนภาพทุก 1.5 s วนไปเรื่อย ๆ
      tPat = millis();
      pattern(idx);
      idx = (idx + 1) % 7;
    }
    if (idx == 5) {                               // ระหว่างแสดงภาพ contrast sweep
      oled.setContrast(contrast);
      contrast += 8;
    }
    while (Serial.available()) {
      char c = toupper(Serial.read());
      if (c == 'P' || c == 'F') return c;
    }
#if FT_BTN_PIN >= 0
    if (digitalRead(FT_BTN_PIN) == LOW) return 'P';
#endif
    delay(20);
  }
  return 'S';
}

/* ---------------- Test run ---------------- */
void runTest() {
  char v[32];
  anyFail = false;
  failReason[0] = 0;
  driver = probed;                              // ค่าเริ่มต้น = ชิปที่ตรวจพบตอนเสียบ (อัปเดตที่ CONTROLLER_ID)

  Serial.println();
  Serial.println(F("#MASSMORE_FACTORY_TEST v1.0"));
  Serial.println(F("#PRODUCT Massmore_OLED"));
  Serial.print(F("#SKU SKU-0102-"));
  Serial.println((int)sku);
  Serial.print(F("#MCU "));
  Serial.println(F(FT_MCU_NAME));

  oled.setModel(sku);
  bool fivePin = oled.hasFeature(MASSMORE_F_NEEDS_RESET);   // ตาม SKU

  // ---- 1) BUS_SCAN ----
  uint8_t addr = scanAddr();
  if (!addr) {
    result(F("BUS_SCAN"), FAIL, "NO_ACK");
    goto verdict;
  }
  hex2(v, addr);
  result(F("BUS_SCAN"), PASS, v);

  // ---- 2) RESET_PIN ----
  if (FT_RST_PIN < 0) {
    result(F("RESET_PIN"), SKIP, "NO_PIN");
  } else {
    digitalWrite(FT_RST_PIN, LOW);
    delay(5);
    Wire.beginTransmission(addr);
    bool nackLow = (Wire.endTransmission() != 0);
    digitalWrite(FT_RST_PIN, HIGH);
    delay(10);
    Wire.beginTransmission(addr);
    bool ackHigh = (Wire.endTransmission() == 0);
    if (nackLow && ackHigh)  result(F("RESET_PIN"), PASS, "NACK_IN_RESET");
    else if (fivePin)        result(F("RESET_PIN"), WARN, nackLow ? "NO_RECOVER" : "NO_EFFECT");
    else                     result(F("RESET_PIN"), SKIP, "4PIN_MODULE");
  }

  // ---- 3) INIT_SEQ ----
  {
    Massmore_OLED_Error e = oled.begin(Wire, addr, FT_RST_PIN);
    if (e != MASSMORE_OLED_OK) {
      strncpy_P(v, (PGM_P)Massmore_OLED::errorString(e), sizeof(v) - 1);
      v[sizeof(v) - 1] = 0;
      for (char *p = v; *p; p++) if (*p == ' ') *p = '_';
      result(F("INIT_SEQ"), FAIL, v);
      goto verdict;
    }
    result(F("INIT_SEQ"), PASS, oled.getModelName());
    Serial.print(F("#CHIP "));
    Serial.println(oled.getControllerName());
  }

  // ---- 4) CHIP_STATUS ----
  {
    int16_t st = oled.readStatus();
    if (st < 0) result(F("CHIP_STATUS"), WARN, "UNREADABLE");
    else { hex2(v, (uint8_t)st); result(F("CHIP_STATUS"), PASS, v); }
  }

  // ---- 5) CONTROLLER_ID + 6) DISPLAY_ONOFF ----
  {
    Massmore_OLED_Identity id = oled.verifyController();
    Massmore_OLED_Controller det = oled.getDetectedController();      // ชิปที่ตรวจพบจริง (เช่น SSD1315)
    driver = (det != MASSMORE_CTRL_UNKNOWN) ? det : probed;
    strcpy_P(v, (PGM_P)Massmore_OLED::controllerName(det != MASSMORE_CTRL_UNKNOWN ? det : oled.getController()));
    strcat(v, "_");
    strcat_P(v, (PGM_P)Massmore_OLED::identityString(id));
    if (id == MASSMORE_IDENTITY_VERIFIED || id == MASSMORE_IDENTITY_CONSISTENT) {
      result(F("CONTROLLER_ID"), PASS, v);
    } else {
      // ขนาดที่พิมพ์ไม่ตรงกับชิปจริง → บอกทั้งสองฝั่ง เช่น EXPECT_SH1106_GOT_SSD1315
      Massmore_OLED_Controller got = (det != MASSMORE_CTRL_UNKNOWN && det != oled.getController()) ? det : probed;
      strcpy_P(v, PSTR("EXPECT_"));
      strcat_P(v, (PGM_P)oled.getControllerName());
      strcat_P(v, PSTR("_GOT_"));
      strcat_P(v, (PGM_P)Massmore_OLED::controllerName(got));
      result(F("CONTROLLER_ID"), FAIL, v);
    }

    if (oled.identityStatusByte() < 0) result(F("DISPLAY_ONOFF"), SKIP, "STATUS_UNREADABLE");
    else if (oled.identityLoopback())  result(F("DISPLAY_ONOFF"), PASS, "D6_TOGGLES");
    else                               result(F("DISPLAY_ONOFF"), FAIL, "D6_STUCK");
  }

  // ---- 7) FRAME_WRITE ----
  {
    Wire.setClock(FT_I2C_HZ);
    oled.fill(WHITE);
    uint32_t t0 = micros();
    Massmore_OLED_Error e = oled.displayAll();
    uint32_t us = micros() - t0;
    snprintf(v, sizeof(v), "%lu.%lums", (unsigned long)(us / 1000), (unsigned long)(us % 1000) / 100);
    result(F("FRAME_WRITE"), e == MASSMORE_OLED_OK ? PASS : FAIL, v);
  }

  // ---- 8) FRAME_RATE ----
  {
    uint32_t t0 = millis();
    for (uint8_t i = 0; i < 20; i++) { oled.fill(i & 1 ? WHITE : BLACK); oled.displayAll(); }
    uint32_t ms = millis() - t0;
    uint16_t fps = ms ? (uint16_t)(20000UL / ms) : 999;
    uint16_t minFps = oled.height() >= 128 ? 15 : 30;
    snprintf(v, sizeof(v), "%ufps", fps);
    result(F("FRAME_RATE"), fps >= minFps ? PASS : WARN, v);
  }

  // ---- 9) I2C_400K ----
  {
    uint8_t ok = 0;
    for (uint8_t i = 0; i < FT_STRESS_FRAMES; i++) {
      oled.fill(i & 1 ? WHITE : BLACK);
      if (oled.displayAll() == MASSMORE_OLED_OK) ok++;
    }
    snprintf(v, sizeof(v), "%u/%u", ok, FT_STRESS_FRAMES);
    result(F("I2C_400K"), ok == FT_STRESS_FRAMES ? PASS : FAIL, v);
  }

  // ---- 10) VISUAL ----
  {
    char c = waitOperator();
    oled.allPixelsOn(false);
    oled.invert(false);
    oled.setContrast(0xCF);
    if (c == 'P')      result(F("VISUAL"), PASS, "OPERATOR_OK");
    else if (c == 'F') result(F("VISUAL"), FAIL, "OPERATOR_REJECT");
    else               result(F("VISUAL"), SKIP, "TIMEOUT");
  }

verdict:
  // ชื่อ driver ที่ตรวจพบจริงต่อท้ายผล เช่น "PASS - SSD1315"
  char drv[12];
  strcpy_P(drv, (PGM_P)Massmore_OLED::controllerName(driver));
  if (anyFail) {
    Serial.print(F("#VERDICT FAIL "));
    Serial.println(failReason);
    Serial.print(F("[FAIL] QA CHECK FAILED: "));
    Serial.print(failReason);
    Serial.print(F(" - "));
    Serial.println(drv);
  } else {
    Serial.println(F("#VERDICT PASS"));
    Serial.print(F("[PASS] OLED QA PASSED - READY TO SHIP - "));
    Serial.println(drv);
  }
  Serial.print(F("#DRIVER "));
  Serial.println(drv);
  Serial.print(anyFail ? F("FAIL - ") : F("PASS - "));
  Serial.println(drv);

  // สรุปบนจอ (ถ้าจอทำงาน): "PASS - SSD1315" ตัวใหญ่ถ้าพอดีจอ
  if (oled.isStarted()) {
    char head[24];
    snprintf(head, sizeof(head), "%s - %s", anyFail ? "FAIL" : "PASS", drv);
    oled.clear();
    oled.setFont();
    oled.setTextSize(2);
    if (oled.height() < 64 || oled.textWidth(head) > (uint16_t)oled.width()) oled.setTextSize(1);
    oled.drawText(oled.width() / 2, 4, head, ALIGN_CENTER);
    oled.setTextSize(1);
    oled.drawText(oled.width() / 2, oled.height() >= 64 ? 28 : 14, oled.getModelName(), ALIGN_CENTER);
    if (anyFail) oled.drawText(oled.width() / 2, oled.height() >= 64 ? 40 : 23, failReason, ALIGN_CENTER);
    oled.display();
  }
}

// ตรวจชิป → ถามขนาด → ทดสอบ
void cycle() {
  while (!detectPanel()) {
    Serial.println(F("No OLED found at 0x3C/0x3D - plug a display (RES -> GPIO for 5-pin) and press Enter"));
    char line[8];
    readLine(line, sizeof(line), F("Waiting for a display - press Enter to scan again"));
  }
  sku = askSize();
  runTest();
}

void setup() {
  Serial.begin(115200);
  delay(300);
#if FT_BTN_PIN >= 0
  pinMode(FT_BTN_PIN, INPUT_PULLUP);
#endif
#if defined(FT_SDA_PIN)
  Wire.begin(FT_SDA_PIN, FT_SCL_PIN);
#else
  Wire.begin();
#endif
  Wire.setClock(FT_I2C_HZ);
  Serial.println(F("Massmore_OLED Factory Test - one firmware for all SKU-0102 panels"));
  cycle();
}

void loop() {
  Serial.println(F("Next: plug the next display and press Enter  |  'r' = repeat this display  |  or type a size"));
  char line[24];
  readLine(line, sizeof(line), F("#PROMPT NEXT"));
  if (line[0] == 'r' || line[0] == 'R') {
    runTest();
  } else if (line[0]) {
    Massmore_OLED_Model m = parseSize(line);    // พิมพ์ขนาดตรง ๆ = ทดสอบจอที่ต่ออยู่ด้วยขนาดนั้น
    if (m != MASSMORE_OLED_AUTO) { sku = m; runTest(); }
    else cycle();
  } else {
    cycle();
  }
}
