/**
 * @file    Massmore_GFX.h
 * @brief   Drawing engine ของ Massmore_OLED — API เข้ากันได้กับ Adafruit-GFX + ฟอนต์ GFXfont
 *          + ตัวเรนเดอร์ภาษาไทย (UTF-8 + จัดตำแหน่งสระ/วรรณยุกต์) สำหรับฟอนต์ MassmoreFont
 *
 * - เขียนลง frame buffer แบบ page (1 byte = 8 พิกเซลแนวตั้ง) ตรงกับ GDDRAM ของทั้ง 4 controller
 * - ไม่มี virtual ในเส้นทางวาด, ไม่มี malloc, ไม่มี String
 * - ทุกฟังก์ชันวาดจะ mark "dirty page" → display() ส่งเฉพาะ page ที่เปลี่ยน
 *
 * Coordinates: (0,0) = มุมซ้ายบน หลังหมุนด้วย setRotation()
 * Text cursor: ฟอนต์ 5x7 ในตัว → y = ขอบบนของตัวอักษร (เหมือน Adafruit classic font)
 *              GFXfont / MassmoreFont   → y = baseline (เหมือน Adafruit custom font)
 *
 * Designed and Manufactured by Massmore — https://www.massmore.shop
 */
#pragma once
#include <Arduino.h>
#include <Print.h>
#include "Massmore_OLED_Defs.h"

/* ========================================================================== */
/*  Font structures                                                           */
/* ========================================================================== */
// GFXfont / GFXglyph — layout เดียวกับ Adafruit gfxfont.h (ใช้ include guard เดียวกัน)
// → ฟอนต์ .h ที่สร้างด้วย fontconvert ของ Adafruit ใช้กับไลบรารีนี้ได้ทันที และไม่ชนกันถ้ามี Adafruit_GFX ด้วย
#ifndef _GFXFONT_H_
#define _GFXFONT_H_
typedef struct {
  uint16_t bitmapOffset;  ///< ตำแหน่งเริ่มใน bitmap array
  uint8_t  width;         ///< ความกว้าง bitmap (px)
  uint8_t  height;        ///< ความสูง bitmap (px)
  uint8_t  xAdvance;      ///< ระยะเลื่อน cursor
  int8_t   xOffset;       ///< offset จาก cursor ถึงมุมซ้ายบนของ bitmap
  int8_t   yOffset;
} GFXglyph;

typedef struct {
  uint8_t  *bitmap;       ///< glyph bitmaps (row-major, MSB first, ต่อเนื่องไม่ pad)
  GFXglyph *glyph;        ///< glyph array
  uint16_t  first;        ///< code point แรก
  uint16_t  last;         ///< code point สุดท้าย
  uint8_t   yAdvance;     ///< ระยะบรรทัด
} GFXfont;
#endif

/**
 * MassmoreFont — ฟอนต์ไทย + ละติน (สร้างจาก Sarabun ด้วย Massmore fontgen)
 * glyph[0..94]   = ASCII U+0020..U+007E
 * glyph[95..185] = Thai  U+0E01..U+0E5B (สระบน/ล่าง/วรรณยุกต์ xAdvance = 0)
 */
typedef struct {
  const uint8_t  *bitmap;
  const GFXglyph *glyph;
  uint8_t yAdvance;       ///< ระยะบรรทัด (รวมวรรณยุกต์ซ้อน 2 ชั้น)
  uint8_t ascent;         ///< baseline ห่างจากขอบบนบรรทัด
  uint8_t xTop;           ///< ความสูงตัวพยัญชนะไทย (ก) เหนือ baseline
  uint8_t markGap;        ///< ระยะห่างระหว่างเครื่องหมายที่ซ้อนกัน
  uint8_t ascShift;       ///< ระยะเลื่อนเครื่องหมายไปทางซ้ายบน ป ฝ ฟ ฬ
} MassmoreFont;

#define MASSMORE_FONT_ASCII_COUNT   95
#define MASSMORE_FONT_THAI_FIRST    0x0E01
#define MASSMORE_FONT_THAI_LAST     0x0E5B
#define MASSMORE_FONT_GLYPH_COUNT   (MASSMORE_FONT_ASCII_COUNT + (MASSMORE_FONT_THAI_LAST - MASSMORE_FONT_THAI_FIRST + 1))

/** สถานะข้อความ (ฟอนต์/ขนาด/สี/wrap) — ใช้ save/restore รอบโค้ดที่เปลี่ยนฟอนต์ชั่วคราว */
struct Massmore_TextState {
  uint8_t     fontType;
  const void *font;
  uint8_t     sx, sy;
  uint16_t    fg, bg;
  bool        wrap;
};

/* ========================================================================== */
/*  Massmore_GFX                                                              */
/* ========================================================================== */
class Massmore_GFX : public Print {
public:
  Massmore_GFX();

  /* ---------- Geometry ---------- */
  int16_t  width()  const { return _w; }       ///< ความกว้างหลังหมุน
  int16_t  height() const { return _h; }       ///< ความสูงหลังหมุน
  uint8_t  getRotation() const { return _rotation; }
  void     setRotation(uint8_t r);              ///< 0–3 (ซอฟต์แวร์, ทีละ 90°)

  /* ---------- Buffer ---------- */
  uint8_t *getBuffer() { return _buf; }
  void     clear() { fillScreen(MASSMORE_BLACK); }  ///< ล้าง buffer (เรียก display() เพื่อส่งขึ้นจอ)
  void     fill(uint16_t color) { fillScreen(color); }
  void     fillScreen(uint16_t color);

  /* ---------- Drawing (Adafruit-GFX compatible) ---------- */
  void     drawPixel(int16_t x, int16_t y, uint16_t color);
  bool     getPixel(int16_t x, int16_t y) const;
  void     drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void     drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void     drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void     drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void     fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void     drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void     fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void     drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
  void     fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
  void     drawEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t color);
  void     fillEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t color);
  void     drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
  void     fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
  /** ส่วนโค้ง: มุมเป็นองศา, 0° = ทิศ 3 นาฬิกา, เพิ่มตามเข็มนาฬิกา · thickness = ความหนาเข้าด้านใน */
  void     drawArc(int16_t x0, int16_t y0, int16_t r, int16_t startDeg, int16_t endDeg,
                   uint16_t color, uint8_t thickness = 1);

  /** Bitmap แบบ Adafruit (row-major, MSB = ซ้ายสุด, แต่ละแถว pad ให้ครบ byte) — const = PROGMEM */
  void     drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color);
  void     drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color, uint16_t bg);
  /** Bitmap ใน RAM (non-const) */
  void     drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
  void     drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg);
  /** XBM (GIMP export: LSB = ซ้ายสุด) ใน PROGMEM */
  void     drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color);

  /** เลื่อนภาพใน buffer (software scroll ใช้ได้ทุก controller) — พื้นที่ว่างเติมสีดำ */
  void     scrollBuffer(int16_t dx, int16_t dy);

  /* ---------- Text ---------- */
  void     setCursor(int16_t x, int16_t y);
  int16_t  getCursorX() const { return _cx; }
  int16_t  getCursorY() const { return _cy; }
  void     setTextSize(uint8_t s) { setTextSize(s, s); }
  void     setTextSize(uint8_t sx, uint8_t sy);
  void     setTextColor(uint16_t c) { _fg = _bg = c; }        ///< พื้นหลังโปร่งใส
  void     setTextColor(uint16_t fg, uint16_t bg) { _fg = fg; _bg = bg; }
  void     setTextWrap(bool w) { _wrap = w; }
  void     setUTF8(bool on) { _utf8 = on; }                    ///< false = 1 byte = 1 ตัวอักษร (เหมือน Adafruit)
  void     setFont();                                          ///< กลับไปใช้ฟอนต์ 5x7 ในตัว
  void     setFont(const GFXfont *f);                          ///< ฟอนต์ Adafruit GFX (PROGMEM)
  void     setFont(const MassmoreFont *f);                     ///< ฟอนต์ไทย Sarabun
  void     setThaiFont(const MassmoreFont *f) { setFont(f); }
  const GFXfont      *getGfxFont() const  { return _fontType == 1 ? _gfxFont : nullptr; }
  const MassmoreFont *getThaiFont() const { return _fontType == 2 ? _mmFont : nullptr; }
  void     saveTextState(Massmore_TextState &st) const;
  void     restoreTextState(const Massmore_TextState &st);

  /** ระยะบรรทัดของฟอนต์ปัจจุบัน (รวม text size) */
  int16_t  fontHeight() const;
  /** baseline ห่างจากขอบบนบรรทัด (ฟอนต์ 5x7 = 0 เพราะ cursor อยู่ขอบบนอยู่แล้ว) */
  int16_t  fontAscent() const;
  /** ความสูงตัวอักษรหลัก (ไทย = พยัญชนะ ก, GFXfont = 'A', 5x7 = 7) — ใช้จัดข้อความกึ่งกลางแนวตั้ง */
  int16_t  fontXHeight() const;

  /** กรอบหมึกจริงของข้อความ ถ้าพิมพ์ที่ (x,y) — รองรับ UTF-8 / ไทย */
  void     getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);
  void     getTextBounds(const __FlashStringHelper *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);
  /** ความกว้างตามระยะเลื่อน cursor (ใช้จัดกึ่งกลาง/ชิดขวา) */
  uint16_t textWidth(const char *str);
  uint16_t textWidth(const __FlashStringHelper *str);
  uint16_t thaiTextWidth(const char *str) { return textWidth(str); }
  /** วาดข้อความ: ALIGN_LEFT → x = ซ้าย, ALIGN_CENTER → x = กึ่งกลาง, ALIGN_RIGHT → x = ขอบขวา */
  void     drawText(int16_t x, int16_t y, const char *str, Massmore_TextAlign align = ALIGN_LEFT);
  void     drawText(int16_t x, int16_t y, const __FlashStringHelper *str, Massmore_TextAlign align = ALIGN_LEFT);
  /** พิมพ์ภาษาไทยที่ (x, baseline y) — เหมือน setCursor()+print() */
  void     drawThai(int16_t x, int16_t y, const char *str);

  /** printf (ESP32 และ AVR) — ยาวสุด 95 ตัวอักษร (AVR 47) · AVR: %f ไม่รองรับ ใช้ print(value, digits) แทน */
  size_t   printf(const char *fmt, ...);

  /* ---------- Print interface ---------- */
  size_t   write(uint8_t c) override;
  size_t   write(const uint8_t *buffer, size_t size) override;
  using Print::write;

  /** ฉบับที่ display() / setCursor() เรียกให้อัตโนมัติ: วาดกลุ่มอักษรไทยที่ค้างอยู่ (สระ/วรรณยุกต์) */
  void     finishText() { _flushCluster(); }

protected:
  /* frame buffer (ตั้งค่าโดย Massmore_OLED::begin) */
  uint8_t  *_buf;
  int16_t   _rawW, _rawH;         // ขนาดกระจกก่อนหมุน
  uint16_t  _bufLen;
  uint16_t  _dirty;               // bit ต่อ page (สูงสุด 16 page)
  void      _setupBuffer(uint8_t *buf, int16_t w, int16_t h);
  void      _markDirty(int16_t rawY0, int16_t rawY1);
  void      _markAllDirty() { _dirty = (_rawH >= 128) ? 0xFFFF : (uint16_t)((1UL << (_rawH >> 3)) - 1); }
  virtual void _onTextWritten() {}    // hook สำหรับ setAutoDisplay()

private:
  int16_t   _w, _h;
  uint8_t   _rotation;

  /* text state */
  int16_t   _cx, _cy;
  uint16_t  _fg, _bg;
  uint8_t   _sx, _sy;
  bool      _wrap, _utf8;
  uint8_t   _fontType;            // 0 = 5x7, 1 = GFXfont, 2 = MassmoreFont
  const GFXfont      *_gfxFont;
  const MassmoreFont *_mmFont;
  MassmoreFont        _mm;        // สำเนา header ของ MassmoreFont (ใน RAM)
  GFXfont             _gf;        // สำเนา header ของ GFXfont (ใน RAM)
  uint32_t  _utfCp;
  uint8_t   _utfNeed;

  /* Thai cluster: พยัญชนะฐาน + เครื่องหมายที่ค้างอยู่ */
  bool      _clPending;
  int16_t   _clX, _clY;           // origin (cursor) ของฐาน
  int16_t   _clBase;              // glyph index ของฐาน (-1 = ไม่มีฐาน)
  uint16_t  _clBaseCp;
  uint16_t  _clMarks[4];
  uint8_t   _clNum;

  /* measuring (getTextBounds / textWidth) */
  bool      _measure;
  int16_t   _mx0, _my0, _mx1, _my1, _mAdvMax;

  /* raw (unrotated) primitives */
  void      _rawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void      _rawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void      _circleQuarter(int16_t x0, int16_t y0, int16_t r, uint8_t corners, uint16_t color);
  void      _circleFillHelper(int16_t x0, int16_t y0, int16_t r, uint8_t sides, int16_t delta, uint16_t color);
  void      _drawBitmapAny(int16_t x, int16_t y, const uint8_t *bmp, int16_t w, int16_t h,
                           uint16_t color, uint16_t bg, bool hasBg, bool progmem, bool xbm);

  /* text engine */
  void      _putByte(uint8_t c);
  void      _writeRaw(const uint8_t *buf, size_t n);
  void      _codepoint(uint16_t cp);
  void      _newline();
  void      _block(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void      _drawClassic(uint8_t c);
  void      _drawGfx(uint16_t cp);
  bool      _mmIndex(uint16_t cp, int16_t *idx) const;
  void      _mmGlyph(int16_t idx, GFXglyph *g) const;
  void      _mmBlit(const GFXglyph &g, int16_t ox, int16_t oy, int16_t maxRow);
  void      _flushCluster();
  int16_t   _advanceOf(uint16_t cp);
  int16_t   _wordWidth(const uint8_t *p, size_t n);
  void      _measureBegin();
  void      _measureText(const char *s, bool progmem);
};
