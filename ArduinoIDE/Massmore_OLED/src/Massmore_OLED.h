/**
 * @file    Massmore_OLED.h
 * @brief   Massmore_OLED — ไลบรารีเดียวสำหรับจอ OLED I2C ทั้ง 12 SKU ของ Massmore (SKU-0102-1 … 12)
 *          SSD1306 (0.91" / 0.96") · SH1106 (1.3") · SH1107 (1.5") · SSD1309 (1.54" / 2.42")
 *
 * - include ไฟล์นี้ไฟล์เดียว: driver + กราฟิก (Adafruit-GFX compatible) + ภาษาไทย Sarabun + UI widgets
 * - ไม่มี dependency นอกจาก Wire, ไม่มี malloc / String, ไม่ hardcode ขา
 * - Bus ownership: sketch เป็นเจ้าของ I2C — เรียก Wire.begin(sda, scl) และ Wire.setClock() เอง
 *   ก่อน oled.begin(Wire, 0x3C, rstPin) (ไลบรารีไม่เรียก Wire.begin())
 *
 * Quick start
 * @code
 *   #include <Wire.h>
 *   #include <Massmore_OLED.h>
 *   Massmore_OLED oled(OLED096_White_SSD1306);          // 0.96" สีขาว SSD1306 128x64
 *
 *   void setup() {
 *     Wire.begin(21, 22);                               // ESP32: sketch กำหนดขาเอง
 *     Wire.setClock(400000);
 *     if (oled.begin(Wire, 0x3C) != MASSMORE_OLED_OK) { while (1) delay(10); }
 *     oled.setFont(&Sarabun16);
 *     oled.setCursor(0, 20);
 *     oled.print("สวัสดี Massmore");
 *     oled.display();
 *   }
 * @endcode
 *
 * Thai fonts (header-only, เลือกตอน compile — ฟอนต์ที่ไม่ได้ใช้ linker ตัดทิ้งเอง):
 *   ESP32 / อื่น ๆ : Sarabun12 / 16 / 20 / 24 / 32 พร้อมใช้ทั้งหมด
 *   AVR (Nano)    : Sarabun16 อย่างเดียวเป็นค่าเริ่มต้น
 *                   #define MASSMORE_THAI_FONT_12 (…_20/_24/_32) หรือ MASSMORE_THAI_FONT_ALL ก่อน include เพื่อเพิ่ม
 *   #define MASSMORE_OLED_NO_THAI_FONTS → ไม่ include ฟอนต์ไทยเลย
 *
 * Designed and Manufactured by Massmore — https://www.massmore.shop
 * License: MIT (code) · Sarabun bitmaps: SIL OFL 1.1 (src/fonts/OFL.txt)
 */
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Massmore_OLED_Defs.h"
#include "Massmore_OLED_Profiles.h"
#include "Massmore_GFX.h"
#include "Massmore_ThaiText.h"

/* ========================================================================== */
/*  Massmore_OLED                                                             */
/* ========================================================================== */
class Massmore_OLED : public Massmore_GFX {
public:
  /** เลือกจอ: OLED096_White_SSD1306 ฯลฯ (แนะนำ) / MASSMORE_OLED_SKU_0102_N / ตามชิป / MASSMORE_OLED_AUTO */
  explicit Massmore_OLED(Massmore_OLED_Model model = MASSMORE_OLED_AUTO);

  /* ---------------- Start-up ---------------- */
  /**
   * @brief  เริ่มจอ: reset (ถ้ามีขา RES) → ตรวจ ACK → (AUTO: probe ชิป) → init → ล้างจอ → เปิดจอ
   * @param  wire    I2C bus ที่ sketch begin() แล้ว (Wire / Wire1)
   * @param  addr    0x3C (ค่าเริ่มต้น) หรือ 0x3D
   * @param  rstPin  ขา RES ของโมดูล 5 ขา (1.5" / 2.42") · -1 = ไม่ต่อ
   */
  Massmore_OLED_Error begin(TwoWire &wire = Wire, uint8_t addr = MASSMORE_OLED_ADDR_DEFAULT, int8_t rstPin = -1);
  void     end();                           ///< ปิดจอ (sleep) และเลิกใช้งาน
  /** เปลี่ยนรุ่นจอขณะรันไทม์ (เช่น Factory Test รับ SKU จาก Serial) — เรียกก่อน begin() · false = model ไม่รู้จัก */
  bool     setModel(Massmore_OLED_Model model);
  bool     reset();                         ///< pulse ขา RES (false = ไม่ได้กำหนดขา)
  void     setI2CClock(uint32_t hz);        ///< เปลี่ยน clock ของ bus (spec ชิป = 400 kHz)
  bool     isConnected();                   ///< มี ACK ที่ address หรือไม่
  bool     isStarted() const { return _started; }

  Massmore_OLED_Model      getModel() const      { return _model; }
  Massmore_OLED_Controller getController() const { return (Massmore_OLED_Controller)_p.controller; }
  Massmore_Str getControllerName() const         { return controllerName(getController()); }
  const char *getModelName() const;         ///< "SKU-0102-3" หรือ "SSD1306 128x64" (เลือกตามชิป/AUTO)
  const char *getPanelSize() const;         ///< "0.96" (นิ้ว) · "" ถ้าไม่ได้เลือกตาม SKU
  const char *getPanelColour() const;       ///< "White" / "Blue" / "Blue-Yellow" · ""
  uint8_t     getAddress() const { return _addr; }
  uint8_t     getPages() const   { return _p.pages; }
  bool        hasFeature(uint16_t flag) const { return (_p.flags & flag) != 0; }

  /* ---------------- Buffer → จอ ---------------- */
  Massmore_OLED_Error display();            ///< ส่งเฉพาะ page ที่เปลี่ยน (dirty)
  Massmore_OLED_Error displayAll();         ///< ส่งทุก page
  /** ส่ง dirty page ทีละ 1 page ต่อการเรียก (cooperative non-blocking) · true = ยังมีค้าง */
  bool     displayStep();
  bool     isBusy() const { return _dirty != 0; }   ///< ยังมี page ที่ยังไม่ได้ส่ง
  uint16_t getDirtyMask() const { return _dirty; }
  void     setAutoDisplay(bool on) { _autoDisplay = on; }  ///< print() แล้ว display() ให้อัตโนมัติ

  /* ---------------- Display control ---------------- */
  Massmore_OLED_Error setContrast(uint8_t value);          ///< 0–255
  Massmore_OLED_Error setBrightness(uint8_t percent);      ///< 0–100 % → contrast
  Massmore_OLED_Error invert(bool on);                     ///< A7h / A6h (ทั้งจอ ฮาร์ดแวร์)
  Massmore_OLED_Error flipHorizontal(bool on);             ///< SEG remap A0h/A1h (ส่ง buffer ใหม่ให้)
  Massmore_OLED_Error flipVertical(bool on);               ///< COM scan C0h/C8h
  Massmore_OLED_Error sleep();                             ///< จอดับ + ปิด charge pump / DC-DC
  Massmore_OLED_Error wake();
  bool     isSleeping() const { return _sleeping; }
  Massmore_OLED_Error allPixelsOn(bool on);                ///< A5h: ทุกพิกเซลติด (ทดสอบจอ), false = กลับมาแสดง RAM
  Massmore_OLED_Error setDisplayOffset(uint8_t rows);      ///< D3h xx
  Massmore_OLED_Error setStartLine(uint8_t line);          ///< 40h|line หรือ DCh xx (SH1107)

  /* ---------------- Scroll ---------------- */
  // speed 0 (ช้าสุด) … 7 (เร็วสุด) · page = แถวละ 8 px (0 … getPages()-1)
  // SH1106 / SH1107 ไม่มี HW scroll → คืน MASSMORE_OLED_ERR_NOT_SUPPORTED (ใช้ scrollBuffer() แทน)
  Massmore_OLED_Error scrollRight(uint8_t startPage, uint8_t endPage, uint8_t speed = 4);
  Massmore_OLED_Error scrollLeft(uint8_t startPage, uint8_t endPage, uint8_t speed = 4);
  Massmore_OLED_Error scrollDiagRight(uint8_t startPage, uint8_t endPage, uint8_t speed = 4, uint8_t vOffset = 1);
  Massmore_OLED_Error scrollDiagLeft(uint8_t startPage, uint8_t endPage, uint8_t speed = 4, uint8_t vOffset = 1);
  Massmore_OLED_Error scrollStop();                        ///< หยุด + ส่ง buffer ใหม่ (RAM เพี้ยนระหว่าง scroll)
  /** SSD1309 content scroll: เลื่อน RAM 1 คอลัมน์ต่อการเรียก (dir > 0 = ขวา, < 0 = ซ้าย) */
  Massmore_OLED_Error scrollContent(int8_t dir, uint8_t startPage = 0, uint8_t endPage = 0xFF);
  Massmore_OLED_Error setVerticalScrollArea(uint8_t topFixedRows, uint8_t scrollRows);
  bool     isScrolling() const { return _scrolling; }

  /* ---------------- Low level ---------------- */
  Massmore_OLED_Error command(uint8_t c);
  Massmore_OLED_Error command(uint8_t c, uint8_t a);
  Massmore_OLED_Error commandList(const uint8_t *cmds, uint8_t len);     ///< RAM
  Massmore_OLED_Error data(const uint8_t *buf, size_t len);              ///< GDDRAM ที่ตำแหน่งปัจจุบัน
  /** อ่าน status byte (control byte 00h แล้วอ่าน 1 byte) · -1 = อ่านไม่ได้ (SSD1306 มักไม่รองรับใน serial mode) */
  int16_t  readStatus();

  /* ---------------- Controller Identity (§8) ---------------- */
  /** probe ชิปจากพฤติกรรม (ใช้ใน AUTO) — ไม่เปลี่ยน profile ปัจจุบัน */
  Massmore_OLED_Controller detectController();
  /** SH1107: ID 6 บิต (07h) · ชิปอื่น: status & 3Fh · -1 = อ่านไม่ได้ */
  int16_t  readChipID();
  /** ตรวจว่าชิปทำงานตรงกับ controller ของ SKU ที่เลือก — ไม่ใช่การพิสูจน์ของแท้เชิง cryptographic */
  Massmore_OLED_Identity verifyController();
  Massmore_OLED_Identity lastIdentity() const { return _identity; }
  void     getIdentityReport(Print &out);   ///< พิมพ์รายละเอียดผลตรวจล่าสุด
  int16_t  identityStatusByte() const { return _idStatus; }     ///< status ที่อ่านได้ (-1 = ไม่ได้)
  bool     identityLoopback() const { return _idLoopback; }     ///< ON/OFF bit กลับค่าตาม AEh/AFh
  int8_t   identityLock() const { return _idLock; }             ///< command lock: 1 = holds, 0 = ไม่, -1 = ไม่ได้ทดสอบ
  /** ชิปที่ตรวจพบจริงจากการตรวจล่าสุด (เช่น SSD1315 บนจอที่เลือก profile SSD1306) */
  Massmore_OLED_Controller getDetectedController() const { return _idDetected; }

  /* ---------------- Errors / helpers ---------------- */
  Massmore_OLED_Error lastError() const { return _err; }
  static Massmore_Str errorString(Massmore_OLED_Error e);
  static Massmore_Str controllerName(Massmore_OLED_Controller c);
  static Massmore_Str identityString(Massmore_OLED_Identity id);
  /** จอ Blue-Yellow (SKU-0102-5): แถว y อยู่ในโซนสีเหลือง (0–15, rotation 0) หรือไม่ */
  static bool isYellowZone(int16_t y) { return y >= 0 && y < MASSMORE_OLED_BY_YELLOW_H; }

protected:
  void _onTextWritten() override;

private:
  uint8_t  _buffer[MASSMORE_OLED_BUFFER_SIZE];
  MassmoreOLED_Profile _p;
  Massmore_OLED_Model  _model;
  TwoWire *_wire;
  uint8_t  _addr;
  int8_t   _rst;
  bool     _started, _sleeping, _scrolling, _autoDisplay, _displayOn;
  bool     _flipH, _flipV;
  Massmore_OLED_Error    _err;
  Massmore_OLED_Identity _identity;
  int16_t  _idStatus;
  bool     _idLoopback;
  int8_t   _idLock;
  int16_t  _idLockStatus;
  Massmore_OLED_Controller _idDetected;

  bool     _loadProfile(uint8_t profileId);
  uint8_t  _profileForModel(Massmore_OLED_Model m) const;
  Massmore_OLED_Error _send(uint8_t ctrl, const uint8_t *buf, size_t len, bool progmem);
  Massmore_OLED_Error _sendInitSeq(const uint8_t *seq);
  Massmore_OLED_Error _sendPage(uint8_t page);
  Massmore_OLED_Error _setErr(Massmore_OLED_Error e) { _err = e; return e; }
  Massmore_OLED_Error _hwScroll(uint8_t cmd, uint8_t startPage, uint8_t endPage, uint8_t speed, uint8_t vOffset);
  bool     _loopback(int16_t *sOn, int16_t *sOff);
  int8_t   _lockTest();
  uint8_t  _speedCode(uint8_t speed) const;
};

/* ========================================================================== */
/*  Alias classes (header เดียวกัน)                                            */
/* ========================================================================== */
/** SSD1306: height 32 (0.91") หรือ 64 (0.96") */
class Massmore_SSD1306 : public Massmore_OLED {
public:
  explicit Massmore_SSD1306(uint8_t height = 64)
    : Massmore_OLED(height == 32 ? MASSMORE_OLED_SSD1306_128X32 : MASSMORE_OLED_SSD1306_128X64) {}
};
/** SH1106 128x64 (1.3") */
class Massmore_SH1106 : public Massmore_OLED {
public:
  Massmore_SH1106() : Massmore_OLED(MASSMORE_OLED_SH1106_128X64) {}
};
/** SH1107 128x128 (1.5") — ต้องต่อขา RES */
class Massmore_SH1107 : public Massmore_OLED {
public:
  Massmore_SH1107() : Massmore_OLED(MASSMORE_OLED_SH1107_128X128) {}
};
/** SSD1309 128x64: false = ค่า 1.54", true = ค่า 2.42" */
class Massmore_SSD1309 : public Massmore_OLED {
public:
  explicit Massmore_SSD1309(bool panel242 = false)
    : Massmore_OLED(panel242 ? MASSMORE_OLED_SSD1309_128X64_242 : MASSMORE_OLED_SSD1309_128X64) {}
};

/* ========================================================================== */
/*  UI widgets + Thai fonts                                                   */
/* ========================================================================== */
#include "Massmore_OLED_UI.h"

#if !defined(MASSMORE_OLED_NO_THAI_FONTS)
  #if !defined(__AVR__) || defined(MASSMORE_THAI_FONT_ALL)
    #define MASSMORE_THAI_FONT__ALL_
  #endif
  #if defined(MASSMORE_THAI_FONT__ALL_) || defined(MASSMORE_THAI_FONT_12)
    #include "fonts/Sarabun12.h"
  #endif
  #if defined(MASSMORE_THAI_FONT__ALL_) || defined(MASSMORE_THAI_FONT_16) || \
      !(defined(MASSMORE_THAI_FONT_12) || defined(MASSMORE_THAI_FONT_20) || \
        defined(MASSMORE_THAI_FONT_24) || defined(MASSMORE_THAI_FONT_32))
    #include "fonts/Sarabun16.h"
  #endif
  #if defined(MASSMORE_THAI_FONT__ALL_) || defined(MASSMORE_THAI_FONT_20)
    #include "fonts/Sarabun20.h"
  #endif
  #if defined(MASSMORE_THAI_FONT__ALL_) || defined(MASSMORE_THAI_FONT_24)
    #include "fonts/Sarabun24.h"
  #endif
  #if defined(MASSMORE_THAI_FONT__ALL_) || defined(MASSMORE_THAI_FONT_32)
    #include "fonts/Sarabun32.h"
  #endif
#endif
