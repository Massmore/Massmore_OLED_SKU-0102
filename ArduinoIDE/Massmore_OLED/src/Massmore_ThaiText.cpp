/**
 * @file    Massmore_ThaiText.cpp
 * @brief   ตัวจัดรูปอักษรไทย (shaper) ของ Massmore_GFX สำหรับฟอนต์ MassmoreFont
 *
 * หลักการ: "cluster" = พยัญชนะฐาน 1 ตัว + เครื่องหมายที่ตามมา (สระบน/ล่าง/วรรณยุกต์ สูงสุด 4)
 *  - เมื่อได้ฐาน: เลื่อน cursor ทันที แต่ "ยังไม่วาด" (รอดูว่ามีสระล่างที่ต้องตัดเชิง ญ ฐ หรือไม่)
 *  - เมื่อได้ฐานตัวถัดไป / ขึ้นบรรทัด / จบ print() / setCursor() / display() → วาดทั้ง cluster
 *  - ตำแหน่งเครื่องหมายคำนวณจากกรอบหมึกจริงของ glyph (ไม่ต้องมี glyph แปรรูปในฟอนต์):
 *      ชั้นบน: ซ้อนขึ้นจากหัวพยัญชนะ (หรือเส้น xTop) ทีละชั้น ห่างกัน markGap
 *              ลำดับ: สระบน/นิคหิต ก่อน → วรรณยุกต์/ทัณฑฆาต ทีหลัง (วรรณยุกต์จึง "ลอยต่ำ" เมื่อไม่มีสระบน)
 *              ชิดขวาของพยัญชนะ · ป ฝ ฟ ฬ เลื่อนซ้าย ascShift ให้พ้นหาง
 *      ชั้นล่าง: ใต้ขอบล่างของฐาน (ฎ ฏ มีหาง → สระลงต่ำใต้หางเอง) · ญ ฐ ตัดเชิงที่ baseline
 */
#include "Massmore_GFX.h"
#include "Massmore_ThaiText.h"

bool Massmore_GFX::_mmIndex(uint16_t cp, int16_t *idx) const {
  if (cp >= 0x20 && cp <= 0x7E) {
    *idx = (int16_t)(cp - 0x20);
    return true;
  }
  if (cp >= MASSMORE_FONT_THAI_FIRST && cp <= MASSMORE_FONT_THAI_LAST) {
    *idx = (int16_t)(MASSMORE_FONT_ASCII_COUNT + (cp - MASSMORE_FONT_THAI_FIRST));
    GFXglyph g;
    _mmGlyph(*idx, &g);
    return g.xAdvance || g.width;          // code point ที่ไม่มี glyph (U+0E3B..0E3E) = ไม่รองรับ
  }
  return false;
}

void Massmore_GFX::_mmGlyph(int16_t idx, GFXglyph *g) const {
  memcpy_P(g, &_mm.glyph[idx], sizeof(GFXglyph));
}

// วาด glyph ให้มุมซ้ายบนของหมึกอยู่ที่ (x, y) บนจอ · rows = จำนวนแถวที่วาด (ใช้ตัดเชิง ญ ฐ)
void Massmore_GFX::_mmBlit(const GFXglyph &g, int16_t x, int16_t y, int16_t rows) {
  const uint8_t *bmp = _mm.bitmap + g.bitmapOffset;
  uint8_t bits = 0;
  uint16_t bit = 0;
  for (int16_t yy = 0; yy < g.height && yy < rows; yy++) {
    for (int16_t xx = 0; xx < g.width; xx++) {
      if (!(bit++ & 7)) bits = pgm_read_byte(bmp++);
      if (bits & 0x80) _block(x + xx * _sx, y + yy * _sy, _sx, _sy, _fg);
      bits <<= 1;
    }
  }
}

void Massmore_GFX::_flushCluster() {
  if (!_clPending) return;
  _clPending = false;
  if (_fontType != 2) return;

  const int16_t xTop = _mm.xTop, gap = _mm.markGap;
  bool hasLower = false;
  for (uint8_t i = 0; i < _clNum; i++) {
    if (massmore_th_isLowerVowel(_clMarks[i])) hasLower = true;
  }

  /* ---- 1) ฐาน: วาด + หากรอบหมึก (หน่วย = พิกเซลฟอนต์, เทียบ origin/baseline) ---- */
  int16_t br, bt = -xTop, bb = 0;
  uint16_t baseCp = 0;
  if (_clBase >= 0) {
    GFXglyph b;
    _mmGlyph(_clBase, &b);
    baseCp = _clBaseCp;
    bool clipTail = hasLower && massmore_th_isRemovableTail(baseCp);
    int16_t rows = clipTail ? (int16_t)(-b.yOffset) : (int16_t)b.height;   // แถวเหนือ baseline เท่านั้น
    if (b.width && b.height) {
      _mmBlit(b, _clX + b.xOffset * _sx, _clY + b.yOffset * _sy, rows);
      br = b.xOffset + b.width;
      bt = b.yOffset;
      bb = b.yOffset + rows;
    } else {
      br = b.xAdvance;                     // ช่องว่าง: วางเครื่องหมายภายในช่อง
    }
  } else {
    br = (int16_t)_clBaseCp;               // ไม่มีฐาน: _clBaseCp เก็บความกว้างช่องที่จองไว้
  }

  /* ---- 2) ชั้นบน: สระบน/นิคหิต (pass 0) แล้ววรรณยุกต์/ทัณฑฆาต (pass 1) ---- */
  bool asc = massmore_th_isAscender(baseCp);
  int16_t top    = asc ? -xTop : (bt < -xTop ? bt : -xTop);
  int16_t rightU = br - (asc ? (int16_t)_mm.ascShift : 0);
  for (uint8_t pass = 0; pass < 2; pass++) {
    for (uint8_t i = 0; i < _clNum; i++) {
      uint16_t m = _clMarks[i];
      bool want = pass ? massmore_th_isTone(m) : massmore_th_isUpperVowel(m);
      if (!want) continue;
      int16_t idx;
      if (!_mmIndex(m, &idx)) continue;
      GFXglyph g;
      _mmGlyph(idx, &g);
      int16_t my = top - gap - g.height;
      _mmBlit(g, _clX + (rightU - g.width) * _sx, _clY + my * _sy, g.height);
      top = my;
    }
  }

  /* ---- 3) ชั้นล่าง: สระ ุ ู และพินทุ ฺ ---- */
  int16_t low = (bb > 0 ? bb : 0) + gap;
  for (uint8_t i = 0; i < _clNum; i++) {
    uint16_t m = _clMarks[i];
    if (!massmore_th_isLowerVowel(m)) continue;
    int16_t idx;
    if (!_mmIndex(m, &idx)) continue;
    GFXglyph g;
    _mmGlyph(idx, &g);
    _mmBlit(g, _clX + (br - g.width) * _sx, _clY + low * _sy, g.height);
    low += g.height + gap;
  }
  _clNum = 0;
}
