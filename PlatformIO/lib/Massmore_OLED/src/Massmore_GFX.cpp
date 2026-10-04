/**
 * @file    Massmore_GFX.cpp
 * @brief   Drawing primitives + text engine (5x7 / GFXfont / MassmoreFont) ของ Massmore_GFX
 *
 * อัลกอริทึมเขียนใหม่ทั้งหมด (Bresenham line, scanline fill, วงกลม/วงรีแบบ half-width ต่อแถว)
 * ทุกรูปทรงแบบเติมสีวาด "แถวละครั้งเดียว" → ใช้สี INVERSE ได้ถูกต้อง ไม่มีพิกเซลกลับสีซ้ำ
 */
#include "Massmore_GFX.h"
#include "Massmore_ThaiText.h"
#include "fonts/Massmore_Font5x7.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========================================================================== */
/*  Helpers                                                                   */
/* ========================================================================== */
static inline void mm_swap(int16_t &a, int16_t &b) { int16_t t = a; a = b; b = t; }

// ใช้กับค่าสีทุกแบบ: 0 = ดำ, 2 = INVERSE, ค่าอื่น = ขาว (รองรับ SSD1306_WHITE ฯลฯ)
static inline void mm_apply(uint8_t *p, uint8_t mask, uint16_t color) {
  if (color == MASSMORE_INVERSE)    *p ^= mask;
  else if (color == MASSMORE_BLACK) *p &= (uint8_t)~mask;
  else                              *p |= mask;
}

// floor(sqrt(v)) แบบจำนวนเต็ม
static uint16_t mm_isqrt(uint32_t v) {
  uint32_t r = 0, bit = 1UL << 30;
  while (bit > v) bit >>= 2;
  while (bit) {
    if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
    else              { r >>= 1; }
    bit >>= 2;
  }
  return (uint16_t)r;
}

// ครึ่งความกว้างของวงกลมรัศมี r ที่ระยะ dy จากจุดศูนย์กลาง (ขอบเขต r+0.5 → รูปเหมือน midpoint circle)
static inline int16_t mm_circleHW(int16_t r, int16_t dy) {
  int32_t v = (int32_t)r * r + r - (int32_t)dy * dy;
  return v < 0 ? -1 : (int16_t)mm_isqrt((uint32_t)v);
}

/* ========================================================================== */
/*  Construction / buffer                                                     */
/* ========================================================================== */
Massmore_GFX::Massmore_GFX()
  : _buf(nullptr), _rawW(0), _rawH(0), _bufLen(0), _dirty(0),
    _w(0), _h(0), _rotation(0),
    _cx(0), _cy(0), _fg(MASSMORE_WHITE), _bg(MASSMORE_WHITE), _sx(1), _sy(1),
    _wrap(true), _utf8(true), _fontType(0), _gfxFont(nullptr), _mmFont(nullptr),
    _utfCp(0), _utfNeed(0),
    _clPending(false), _clX(0), _clY(0), _clBase(-1), _clBaseCp(0), _clNum(0),
    _measure(false), _mx0(0), _my0(0), _mx1(0), _my1(0), _mAdvMax(0) {
  memset(&_mm, 0, sizeof(_mm));
  memset(&_gf, 0, sizeof(_gf));
  memset(_clMarks, 0, sizeof(_clMarks));
}

void Massmore_GFX::_setupBuffer(uint8_t *buf, int16_t w, int16_t h) {
  _buf    = buf;
  _rawW   = w;
  _rawH   = h;
  _bufLen = (uint16_t)(w * (h >> 3));
  setRotation(_rotation);
}

void Massmore_GFX::setRotation(uint8_t r) {
  _flushCluster();
  _rotation = r & 3;
  _w = (_rotation & 1) ? _rawH : _rawW;
  _h = (_rotation & 1) ? _rawW : _rawH;
}

void Massmore_GFX::_markDirty(int16_t y0, int16_t y1) {
  if (y0 < 0) y0 = 0;
  if (y1 >= _rawH) y1 = _rawH - 1;
  if (y1 < y0) return;
  uint8_t p0 = (uint8_t)(y0 >> 3), p1 = (uint8_t)(y1 >> 3);
  uint32_t m = ((1UL << (p1 + 1)) - 1) & ~((1UL << p0) - 1);
  _dirty |= (uint16_t)m;
}

void Massmore_GFX::fillScreen(uint16_t color) {
  if (!_buf) return;
  if (color == MASSMORE_INVERSE) {
    for (uint16_t i = 0; i < _bufLen; i++) _buf[i] ^= 0xFF;
  } else {
    memset(_buf, color == MASSMORE_BLACK ? 0x00 : 0xFF, _bufLen);
  }
  _markAllDirty();
}

/* ========================================================================== */
/*  Pixel + raw lines                                                         */
/* ========================================================================== */
void Massmore_GFX::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (!_buf || x < 0 || y < 0 || x >= _w || y >= _h) return;
  int16_t rx, ry;
  switch (_rotation) {
    case 1:  rx = _rawW - 1 - y; ry = x;              break;
    case 2:  rx = _rawW - 1 - x; ry = _rawH - 1 - y;  break;
    case 3:  rx = y;             ry = _rawH - 1 - x;  break;
    default: rx = x;             ry = y;              break;
  }
  mm_apply(&_buf[(ry >> 3) * _rawW + rx], (uint8_t)(1 << (ry & 7)), color);
  _dirty |= (uint16_t)(1U << (ry >> 3));
}

bool Massmore_GFX::getPixel(int16_t x, int16_t y) const {
  if (!_buf || x < 0 || y < 0 || x >= _w || y >= _h) return false;
  int16_t rx, ry;
  switch (_rotation) {
    case 1:  rx = _rawW - 1 - y; ry = x;              break;
    case 2:  rx = _rawW - 1 - x; ry = _rawH - 1 - y;  break;
    case 3:  rx = y;             ry = _rawH - 1 - x;  break;
    default: rx = x;             ry = y;              break;
  }
  return (_buf[(ry >> 3) * _rawW + rx] >> (ry & 7)) & 1;
}

void Massmore_GFX::_rawHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (!_buf || y < 0 || y >= _rawH) return;
  if (x < 0) { w += x; x = 0; }
  if (x + w > _rawW) w = _rawW - x;
  if (w <= 0) return;
  uint8_t *p = &_buf[(y >> 3) * _rawW + x];
  uint8_t mask = (uint8_t)(1 << (y & 7));
  while (w--) mm_apply(p++, mask, color);
  _dirty |= (uint16_t)(1U << (y >> 3));
}

void Massmore_GFX::_rawVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (!_buf || x < 0 || x >= _rawW) return;
  if (y < 0) { h += y; y = 0; }
  if (y + h > _rawH) h = _rawH - y;
  if (h <= 0) return;
  int16_t y1 = y + h - 1;
  for (int16_t page = y >> 3; page <= (y1 >> 3); page++) {
    uint8_t lo = (page == (y >> 3))  ? (y & 7)  : 0;
    uint8_t hi = (page == (y1 >> 3)) ? (y1 & 7) : 7;
    uint8_t mask = (uint8_t)((0xFF << lo) & (0xFF >> (7 - hi)));
    mm_apply(&_buf[page * _rawW + x], mask, color);
  }
  _markDirty(y, y1);
}

void Massmore_GFX::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (w < 0) { x += w + 1; w = -w; }
  if (w == 0 || y < 0 || y >= _h) return;
  if (x < 0) { w += x; x = 0; }
  if (x + w > _w) w = _w - x;
  if (w <= 0) return;
  switch (_rotation) {
    case 1:  _rawVLine(_rawW - 1 - y, x, w, color);              break;
    case 2:  _rawHLine(_rawW - x - w, _rawH - 1 - y, w, color);  break;
    case 3:  _rawVLine(y, _rawH - x - w, w, color);              break;
    default: _rawHLine(x, y, w, color);                          break;
  }
}

void Massmore_GFX::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (h < 0) { y += h + 1; h = -h; }
  if (h == 0 || x < 0 || x >= _w) return;
  if (y < 0) { h += y; y = 0; }
  if (y + h > _h) h = _h - y;
  if (h <= 0) return;
  switch (_rotation) {
    case 1:  _rawHLine(_rawW - y - h, x, h, color);              break;
    case 2:  _rawVLine(_rawW - 1 - x, _rawH - y - h, h, color);  break;
    case 3:  _rawHLine(y, _rawH - 1 - x, h, color);              break;
    default: _rawVLine(x, y, h, color);                          break;
  }
}

/* ========================================================================== */
/*  Lines / rectangles                                                        */
/* ========================================================================== */
void Massmore_GFX::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  if (x0 == x1) { drawFastVLine(x0, y0 < y1 ? y0 : y1, abs(y1 - y0) + 1, color); return; }
  if (y0 == y1) { drawFastHLine(x0 < x1 ? x0 : x1, y0, abs(x1 - x0) + 1, color); return; }
  // Bresenham (integer error term, all octants)
  int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  for (;;) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void Massmore_GFX::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w < 0) { x += w + 1; w = -w; }
  if (h < 0) { y += h + 1; h = -h; }
  if (w == 0 || h == 0) return;
  drawFastHLine(x, y, w, color);
  if (h > 1) drawFastHLine(x, y + h - 1, w, color);
  if (h > 2) {
    drawFastVLine(x, y + 1, h - 2, color);
    if (w > 1) drawFastVLine(x + w - 1, y + 1, h - 2, color);
  }
}

void Massmore_GFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w < 0) { x += w + 1; w = -w; }
  if (h < 0) { y += h + 1; h = -h; }
  // คอลัมน์ละเส้น — บน rotation 0/2 เขียนทีละ byte (เร็ว)
  for (int16_t i = 0; i < w; i++) drawFastVLine(x + i, y, h, color);
}

/* ========================================================================== */
/*  Circles / round rects (half-width ต่อแถว — เส้นขอบและพื้นใช้สูตรเดียวกัน)    */
/* ========================================================================== */
void Massmore_GFX::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  if (r < 0) return;
  if (r == 0) { drawPixel(x0, y0, color); return; }
  for (int16_t dy = 0; dy <= r; dy++) {
    int16_t hw    = mm_circleHW(r, dy);
    int16_t inner = (dy < r) ? mm_circleHW(r, dy + 1) : -1;
    int16_t xs    = inner + 1;
    if (xs > hw) xs = hw;
    // span [xs..hw] ทั้ง 4 ควอดแรนต์ (ไม่วาดซ้ำที่แกน)
    for (int16_t x = xs; x <= hw; x++) {
      drawPixel(x0 + x, y0 + dy, color);
      if (x) drawPixel(x0 - x, y0 + dy, color);
      if (dy) {
        drawPixel(x0 + x, y0 - dy, color);
        if (x) drawPixel(x0 - x, y0 - dy, color);
      }
    }
  }
}

void Massmore_GFX::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  if (r < 0) return;
  for (int16_t dy = -r; dy <= r; dy++) {
    int16_t hw = mm_circleHW(r, dy);
    drawFastHLine(x0 - hw, y0 + dy, 2 * hw + 1, color);
  }
}

void Massmore_GFX::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  if (w < 0) { x += w + 1; w = -w; }
  if (h < 0) { y += h + 1; h = -h; }
  int16_t maxR = ((w < h) ? w : h) / 2;
  if (r > maxR) r = maxR;
  if (r <= 0) { drawRect(x, y, w, h, color); return; }
  // เส้นตรง 4 ด้าน
  drawFastHLine(x + r, y, w - 2 * r, color);
  drawFastHLine(x + r, y + h - 1, w - 2 * r, color);
  drawFastVLine(x, y + r, h - 2 * r, color);
  drawFastVLine(x + w - 1, y + r, h - 2 * r, color);
  // มุม 4 มุม: จุดศูนย์กลางมุม = (x+r, y+r) ฯลฯ
  int16_t cxl = x + r, cxr = x + w - 1 - r, cyt = y + r, cyb = y + h - 1 - r;
  for (int16_t dy = 0; dy <= r; dy++) {
    int16_t hw    = mm_circleHW(r, dy);
    int16_t inner = (dy < r) ? mm_circleHW(r, dy + 1) : -1;
    int16_t xs    = inner + 1;
    if (xs > hw) xs = hw;
    for (int16_t dx = xs; dx <= hw; dx++) {
      if (dx == 0 || dy == 0) continue;   // อยู่บนเส้นตรงแล้ว
      drawPixel(cxr + dx, cyt - dy, color);
      drawPixel(cxl - dx, cyt - dy, color);
      drawPixel(cxr + dx, cyb + dy, color);
      drawPixel(cxl - dx, cyb + dy, color);
    }
  }
}

void Massmore_GFX::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  if (w < 0) { x += w + 1; w = -w; }
  if (h < 0) { y += h + 1; h = -h; }
  int16_t maxR = ((w < h) ? w : h) / 2;
  if (r > maxR) r = maxR;
  if (r < 0) r = 0;
  for (int16_t i = 0; i < h; i++) {
    int16_t dy = 0;
    if (i < r)                dy = r - i;
    else if (i >= h - r)      dy = i - (h - 1 - r);
    int16_t inset = dy ? (r - mm_circleHW(r, dy)) : 0;
    drawFastHLine(x + inset, y + i, w - 2 * inset, color);
  }
}

/* ========================================================================== */
/*  Ellipse                                                                   */
/* ========================================================================== */
static int16_t mm_ellipseHW(int16_t rx, int16_t ry, int16_t dy) {
  float a = rx + 0.5f, b = ry + 0.5f;
  float t = 1.0f - ((float)dy * dy) / (b * b);
  if (t <= 0) return -1;
  return (int16_t)floorf(a * sqrtf(t));
}

void Massmore_GFX::drawEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t color) {
  if (rx < 0 || ry < 0) return;
  if (rx == 0) { drawFastVLine(x0, y0 - ry, 2 * ry + 1, color); return; }
  if (ry == 0) { drawFastHLine(x0 - rx, y0, 2 * rx + 1, color); return; }
  for (int16_t dy = 0; dy <= ry; dy++) {
    int16_t hw    = mm_ellipseHW(rx, ry, dy);
    int16_t inner = (dy < ry) ? mm_ellipseHW(rx, ry, dy + 1) : -1;
    int16_t xs    = inner + 1;
    if (xs > hw) xs = hw;
    for (int16_t x = xs; x <= hw; x++) {
      drawPixel(x0 + x, y0 + dy, color);
      if (x) drawPixel(x0 - x, y0 + dy, color);
      if (dy) {
        drawPixel(x0 + x, y0 - dy, color);
        if (x) drawPixel(x0 - x, y0 - dy, color);
      }
    }
  }
}

void Massmore_GFX::fillEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t color) {
  if (rx < 0 || ry < 0) return;
  for (int16_t dy = -ry; dy <= ry; dy++) {
    int16_t hw = (ry == 0) ? rx : mm_ellipseHW(rx, ry, dy < 0 ? -dy : dy);
    if (hw >= 0) drawFastHLine(x0 - hw, y0 + dy, 2 * hw + 1, color);
  }
}

/* ========================================================================== */
/*  Triangles                                                                 */
/* ========================================================================== */
void Massmore_GFX::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

void Massmore_GFX::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  // เรียงตาม y: y0 <= y1 <= y2
  if (y0 > y1) { mm_swap(y0, y1); mm_swap(x0, x1); }
  if (y1 > y2) { mm_swap(y1, y2); mm_swap(x1, x2); }
  if (y0 > y1) { mm_swap(y0, y1); mm_swap(x0, x1); }
  if (y0 == y2) {   // สามเหลี่ยมแบน 1 แถว
    int16_t a = x0, b = x0;
    if (x1 < a) a = x1;
    if (x1 > b) b = x1;
    if (x2 < a) a = x2;
    if (x2 > b) b = x2;
    drawFastHLine(a, y0, b - a + 1, color);
    return;
  }
  // scanline: ขอบยาว (0→2) กับขอบสั้น (0→1 แล้ว 1→2) — แถวละครั้ง
  for (int16_t y = y0; y <= y2; y++) {
    int16_t xa = x0 + (int16_t)((int32_t)(x2 - x0) * (y - y0) / (y2 - y0));
    int16_t xb;
    if (y < y1) xb = x0 + (int16_t)((int32_t)(x1 - x0) * (y - y0) / (y1 - y0));
    else        xb = (y2 == y1) ? x1 : x1 + (int16_t)((int32_t)(x2 - x1) * (y - y1) / (y2 - y1));
    if (xa > xb) mm_swap(xa, xb);
    drawFastHLine(xa, y, xb - xa + 1, color);
  }
}

/* ========================================================================== */
/*  Arc                                                                       */
/* ========================================================================== */
void Massmore_GFX::drawArc(int16_t x0, int16_t y0, int16_t r, int16_t startDeg, int16_t endDeg,
                           uint16_t color, uint8_t thickness) {
  if (r <= 0) return;
  while (endDeg < startDeg) endDeg += 360;
  if (thickness == 0) thickness = 1;
  for (uint8_t t = 0; t < thickness && r - t > 0; t++) {
    int16_t rr = r - t;
    float step = 40.0f / rr;                    // < 1 px ต่อก้าว → ไม่มีช่องว่าง
    int16_t px = INT16_MIN, py = INT16_MIN;
    for (float a = startDeg; ; a += step) {
      if (a > endDeg) a = endDeg;
      float rad = a * (float)(PI / 180.0);
      int16_t x = x0 + (int16_t)lroundf(cosf(rad) * rr);
      int16_t y = y0 + (int16_t)lroundf(sinf(rad) * rr);
      if (x != px || y != py) { drawPixel(x, y, color); px = x; py = y; }
      if (a >= endDeg) break;
    }
  }
}

/* ========================================================================== */
/*  Bitmaps                                                                   */
/* ========================================================================== */
void Massmore_GFX::_drawBitmapAny(int16_t x, int16_t y, const uint8_t *bmp, int16_t w, int16_t h,
                                  uint16_t color, uint16_t bg, bool hasBg, bool progmem, bool xbm) {
  if (!bmp) return;
  int16_t byteWidth = (w + 7) / 8;
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      const uint8_t *p = bmp + j * byteWidth + (i >> 3);
      uint8_t b = progmem ? pgm_read_byte(p) : *p;
      bool on = xbm ? ((b >> (i & 7)) & 1) : ((b << (i & 7)) & 0x80);
      if (on)         drawPixel(x + i, y + j, color);
      else if (hasBg) drawPixel(x + i, y + j, bg);
    }
  }
}

void Massmore_GFX::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color) {
  _drawBitmapAny(x, y, bitmap, w, h, color, 0, false, true, false);
}
void Massmore_GFX::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color, uint16_t bg) {
  _drawBitmapAny(x, y, bitmap, w, h, color, bg, true, true, false);
}
void Massmore_GFX::drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
  _drawBitmapAny(x, y, bitmap, w, h, color, 0, false, false, false);
}
void Massmore_GFX::drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg) {
  _drawBitmapAny(x, y, bitmap, w, h, color, bg, true, false, false);
}
void Massmore_GFX::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color) {
  _drawBitmapAny(x, y, bitmap, w, h, color, 0, false, true, true);
}

/* ========================================================================== */
/*  Software scroll                                                           */
/* ========================================================================== */
void Massmore_GFX::scrollBuffer(int16_t dx, int16_t dy) {
  if (!_buf || (dx == 0 && dy == 0)) return;
  // วนจากด้านที่ "ปลายทาง" อยู่ก่อน เพื่อไม่ให้ทับต้นทางที่ยังไม่ได้อ่าน
  for (int16_t j = 0; j < _h; j++) {
    int16_t y = (dy > 0) ? (_h - 1 - j) : j;
    for (int16_t i = 0; i < _w; i++) {
      int16_t x = (dx > 0) ? (_w - 1 - i) : i;
      int16_t sx = x - dx, sy = y - dy;
      bool v = (sx >= 0 && sy >= 0 && sx < _w && sy < _h) ? getPixel(sx, sy) : false;
      drawPixel(x, y, v ? MASSMORE_WHITE : MASSMORE_BLACK);
    }
  }
}

/* ========================================================================== */
/*  Text — settings                                                           */
/* ========================================================================== */
void Massmore_GFX::setCursor(int16_t x, int16_t y) {
  _flushCluster();
  _cx = x;
  _cy = y;
}

void Massmore_GFX::setTextSize(uint8_t sx, uint8_t sy) {
  _sx = sx ? sx : 1;
  _sy = sy ? sy : 1;
}

void Massmore_GFX::setFont() {
  _flushCluster();
  _fontType = 0;
  _gfxFont  = nullptr;
  _mmFont   = nullptr;
}

void Massmore_GFX::setFont(const GFXfont *f) {
  if (!f) { setFont(); return; }
  _flushCluster();
  memcpy_P(&_gf, f, sizeof(GFXfont));
  _gfxFont  = f;
  _mmFont   = nullptr;
  _fontType = 1;
}

void Massmore_GFX::setFont(const MassmoreFont *f) {
  if (!f) { setFont(); return; }
  _flushCluster();
  memcpy_P(&_mm, f, sizeof(MassmoreFont));
  _mmFont   = f;
  _gfxFont  = nullptr;
  _fontType = 2;
}

void Massmore_GFX::saveTextState(Massmore_TextState &st) const {
  st.fontType = _fontType;
  st.font     = (_fontType == 1) ? (const void *)_gfxFont : (_fontType == 2) ? (const void *)_mmFont : nullptr;
  st.sx = _sx; st.sy = _sy;
  st.fg = _fg; st.bg = _bg;
  st.wrap = _wrap;
}

void Massmore_GFX::restoreTextState(const Massmore_TextState &st) {
  if (st.fontType == 1)      setFont((const GFXfont *)st.font);
  else if (st.fontType == 2) setFont((const MassmoreFont *)st.font);
  else                       setFont();
  _sx = st.sx; _sy = st.sy;
  _fg = st.fg; _bg = st.bg;
  _wrap = st.wrap;
}

int16_t Massmore_GFX::fontHeight() const {
  switch (_fontType) {
    case 1:  return (int16_t)_gf.yAdvance * _sy;
    case 2:  return (int16_t)_mm.yAdvance * _sy;
    default: return 8 * _sy;
  }
}

int16_t Massmore_GFX::fontAscent() const {
  if (_fontType == 2) return (int16_t)_mm.ascent * _sy;
  if (_fontType == 1) {
    uint16_t cp = ('A' >= _gf.first && 'A' <= _gf.last) ? 'A' : _gf.first;
    GFXglyph g;
    memcpy_P(&g, &_gf.glyph[cp - _gf.first], sizeof(GFXglyph));
    return (int16_t)(-g.yOffset) * _sy;
  }
  return 0;
}

int16_t Massmore_GFX::fontXHeight() const {
  if (_fontType == 2) return (int16_t)_mm.xTop * _sy;
  if (_fontType == 1) return fontAscent();
  return 7 * _sy;
}

/* ========================================================================== */
/*  Text — Print interface                                                    */
/* ========================================================================== */
size_t Massmore_GFX::write(uint8_t c) {
  _putByte(c);
  if (!_measure && c == '\n') _onTextWritten();
  return 1;
}

void Massmore_GFX::_putByte(uint8_t c) {
  if (!_utf8 || c < 0x80) {
    _utfNeed = 0;
    _codepoint(c);
  } else if ((c & 0xC0) == 0x80) {           // continuation byte
    if (_utfNeed) {
      _utfCp = (_utfCp << 6) | (c & 0x3F);
      if (--_utfNeed == 0 && _utfCp <= 0xFFFF) _codepoint((uint16_t)_utfCp);
    }
  } else if ((c & 0xE0) == 0xC0) { _utfCp = c & 0x1F; _utfNeed = 1; }
  else if ((c & 0xF0) == 0xE0)   { _utfCp = c & 0x0F; _utfNeed = 2; }
  else if ((c & 0xF8) == 0xF0)   { _utfCp = c & 0x07; _utfNeed = 3; }  // > U+FFFF: ข้าม
  else                           { _utfNeed = 0; }
  if (_measure && _cx > _mAdvMax) _mAdvMax = _cx;
}

// จุดตัดคำ: ต้นข้อความ, หลังช่องว่าง/ขึ้นบรรทัด, หลัง ZWSP (E2 80 8B)
static bool mm_atWordStart(const uint8_t *buf, size_t i) {
  if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\r') return false;
  if ((buf[i] & 0xC0) == 0x80) return false;            // กลาง UTF-8 sequence
  if (i == 0) return true;
  uint8_t p = buf[i - 1];
  if (p == ' ' || p == '\n') return true;
  return (i >= 3 && buf[i - 3] == 0xE2 && buf[i - 2] == 0x80 && buf[i - 1] == 0x8B);
}

size_t Massmore_GFX::write(const uint8_t *buffer, size_t size) {
  if (!buffer) return 0;
  _writeRaw(buffer, size);
  _flushCluster();
  if (!_measure) _onTextWritten();
  return size;
}

void Massmore_GFX::_writeRaw(const uint8_t *buffer, size_t size) {
  for (size_t i = 0; i < size; i++) {
    // ตัดบรรทัดทั้งคำ: ถ้าคำถัดไปล้นขอบ → ขึ้นบรรทัดใหม่ก่อน (คำยาวกว่าบรรทัด → ตัดทีละตัวอักษร)
    if (_wrap && _utfNeed == 0 && mm_atWordStart(buffer, i)) {
      int16_t ww = _wordWidth(buffer + i, size - i);
      if (_cx > 0 && _cx + ww > _w && ww <= _w) {
        _flushCluster();
        _newline();
      }
    }
    _putByte(buffer[i]);
  }
}

size_t Massmore_GFX::printf(const char *fmt, ...) {
#if defined(__AVR__)
  char buf[48];                                  // AVR: stack เล็ก
#else
  char buf[96];
#endif
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0) return 0;
  return write((const uint8_t *)buf, strlen(buf));
}

/* ========================================================================== */
/*  Text — engine                                                             */
/* ========================================================================== */
void Massmore_GFX::_newline() {
  _cx = 0;
  _cy += fontHeight();
}

void Massmore_GFX::_block(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (_measure) {
    if (x < _mx0) _mx0 = x;
    if (y < _my0) _my0 = y;
    if (x + w - 1 > _mx1) _mx1 = x + w - 1;
    if (y + h - 1 > _my1) _my1 = y + h - 1;
    return;
  }
  if (w == 1 && h == 1) drawPixel(x, y, color);
  else                  fillRect(x, y, w, h, color);
}

int16_t Massmore_GFX::_advanceOf(uint16_t cp) {
  if (cp == MASSMORE_ZWSP || cp == '\n' || cp == '\r') return 0;
  if (_fontType == 0) return massmore_th_isMark(cp) ? 0 : 6 * _sx;
  if (_fontType == 1) {
    if (cp < _gf.first || cp > _gf.last) return 0;
    GFXglyph g;
    memcpy_P(&g, &_gf.glyph[cp - _gf.first], sizeof(GFXglyph));
    return (int16_t)g.xAdvance * _sx;
  }
  if (massmore_th_isMark(cp)) return 0;
  if (cp == MASSMORE_TH_SARA_AM) cp = MASSMORE_TH_SARA_AA;
  int16_t idx;
  if (!_mmIndex(cp, &idx)) return 0;
  GFXglyph g;
  _mmGlyph(idx, &g);
  return (int16_t)g.xAdvance * _sx;
}

int16_t Massmore_GFX::_wordWidth(const uint8_t *p, size_t n) {
  int16_t  w = 0;
  uint32_t cp = 0;
  uint8_t  need = 0;
  for (size_t i = 0; i < n; i++) {
    uint8_t c = p[i];
    if (!_utf8 || c < 0x80)       { cp = c; need = 0; }
    else if ((c & 0xC0) == 0x80)  { if (!need) continue; cp = (cp << 6) | (c & 0x3F); if (--need) continue; }
    else if ((c & 0xE0) == 0xC0)  { cp = c & 0x1F; need = 1; continue; }
    else if ((c & 0xF0) == 0xE0)  { cp = c & 0x0F; need = 2; continue; }
    else                          { need = 0; continue; }
    if (cp == ' ' || cp == '\n' || cp == '\r' || cp == MASSMORE_ZWSP) break;
    if (cp <= 0xFFFF) w += _advanceOf((uint16_t)cp);
  }
  return w;
}

void Massmore_GFX::_codepoint(uint16_t cp) {
  if (cp == '\n') { _flushCluster(); _newline(); return; }
  if (cp == '\r') return;
  if (cp == MASSMORE_ZWSP) { _flushCluster(); return; }

  if (_fontType == 0) {                         // ---- ฟอนต์ 5x7 ในตัว ----
    if (massmore_th_isMark(cp)) return;         // ไม่มี glyph ไทย
    uint8_t c = (cp >= 0x20 && cp <= 0x7E) ? (uint8_t)cp : '?';
    if (_wrap && _cx + 6 * _sx > _w && _cx > 0) _newline();
    _drawClassic(c);
    _cx += 6 * _sx;
    return;
  }
  if (_fontType == 1) {                         // ---- Adafruit GFXfont ----
    _drawGfx(cp);
    return;
  }

  // ---- MassmoreFont (ไทย + ละติน) ----
  if (massmore_th_isMark(cp)) {
    if (!_clPending) {                          // เครื่องหมายลอย ๆ ไม่มีฐาน → จองที่ว่างให้
      int16_t idx;
      GFXglyph g;
      if (!_mmIndex(cp, &idx)) return;
      _mmGlyph(idx, &g);
      _clPending = true; _clX = _cx; _clY = _cy; _clBase = -1; _clBaseCp = (uint16_t)(g.width + 1); _clNum = 0;
      _cx += (int16_t)(g.width + 1) * _sx;
    }
    if (_clNum < 4) _clMarks[_clNum++] = cp;
    return;
  }
  if (cp == MASSMORE_TH_SARA_AM) {              // ำ = ํ (บนฐาน) + า (ตัวถัดไป)
    if (!_clPending) { _clPending = true; _clX = _cx; _clY = _cy; _clBase = -1; _clBaseCp = 0; _clNum = 0; }
    if (_clNum < 4) _clMarks[_clNum++] = MASSMORE_TH_NIKHAHIT;
    int16_t idx;
    GFXglyph g;
    if (!_mmIndex(MASSMORE_TH_SARA_AA, &idx)) return;
    _mmGlyph(idx, &g);
    _mmBlit(g, _cx + g.xOffset * _sx, _cy + g.yOffset * _sy, g.height);
    _cx += (int16_t)g.xAdvance * _sx;
    return;
  }
  int16_t idx;
  if (!_mmIndex(cp, &idx)) return;              // ไม่มีในฟอนต์ → ข้าม
  _flushCluster();
  GFXglyph g;
  _mmGlyph(idx, &g);
  int16_t adv = (int16_t)g.xAdvance * _sx;
  if (_wrap && _cx > 0 && _cx + adv > _w) {
    _newline();
    if (cp == ' ') return;                      // ไม่ขึ้นต้นบรรทัดด้วยช่องว่าง
  }
  _clPending = true;
  _clX = _cx; _clY = _cy;
  _clBase = idx; _clBaseCp = cp; _clNum = 0;
  _cx += adv;
}

void Massmore_GFX::_drawClassic(uint8_t c) {
  const uint8_t *glyph = &Massmore_Font5x7[(c - 0x20) * 5];
  bool opaque = (_bg != _fg);
  for (int8_t col = 0; col < 6; col++) {
    uint8_t line = (col < 5) ? pgm_read_byte(glyph + col) : 0;
    for (int8_t row = 0; row < 8; row++, line >>= 1) {
      if (line & 1)    _block(_cx + col * _sx, _cy + row * _sy, _sx, _sy, _fg);
      else if (opaque) _block(_cx + col * _sx, _cy + row * _sy, _sx, _sy, _bg);
    }
  }
}

void Massmore_GFX::_drawGfx(uint16_t cp) {
  if (cp < _gf.first || cp > _gf.last) return;
  GFXglyph g;
  memcpy_P(&g, &_gf.glyph[cp - _gf.first], sizeof(GFXglyph));
  if (_wrap && _cx > 0 && _cx + ((int16_t)g.xOffset + g.width) * _sx > _w) _newline();
  const uint8_t *bmp = _gf.bitmap + g.bitmapOffset;
  uint8_t bits = 0, bit = 0;
  for (uint8_t yy = 0; yy < g.height; yy++) {
    for (uint8_t xx = 0; xx < g.width; xx++) {
      if (!(bit++ & 7)) bits = pgm_read_byte(bmp++);
      if (bits & 0x80) _block(_cx + ((int16_t)g.xOffset + xx) * _sx, _cy + ((int16_t)g.yOffset + yy) * _sy, _sx, _sy, _fg);
      bits <<= 1;
    }
  }
  _cx += (int16_t)g.xAdvance * _sx;
}

/* ========================================================================== */
/*  Text — measuring / alignment                                              */
/* ========================================================================== */
void Massmore_GFX::_measureBegin() {
  _measure = true;
  _mx0 = _my0 = INT16_MAX;
  _mx1 = _my1 = INT16_MIN;
  _mAdvMax = _cx;
}

void Massmore_GFX::_measureText(const char *s, bool progmem) {
  if (!s) return;
  if (!progmem) { _writeRaw((const uint8_t *)s, strlen(s)); _flushCluster(); return; }
  // F("...") → คัดลอกมา RAM ทีละก้อน (ตัดที่ขอบ UTF-8 / ช่องว่าง เพื่อให้วัดคำได้ถูก)
#if defined(__AVR__)
  char tmp[48];
#else
  char tmp[128];
#endif
  size_t len = strlen_P(s), pos = 0;
  while (pos < len) {
    size_t n = len - pos;
    if (n > sizeof(tmp)) {
      n = sizeof(tmp);
      size_t sp = n;                                                         // ตัดหลังช่องว่างถ้ามี
      while (sp > 1 && pgm_read_byte(s + pos + sp - 1) != ' ') sp--;
      if (sp > 1) n = sp;
      else while (n > 1 && (pgm_read_byte(s + pos + n) & 0xC0) == 0x80) n--; // อย่าตัดกลางตัวอักษร
    }
    memcpy_P(tmp, s + pos, n);
    _writeRaw((const uint8_t *)tmp, n);                                     // cluster ไทยต่อเนื่องข้ามก้อนได้
    pos += n;
  }
  _flushCluster();
}

void Massmore_GFX::getTextBounds(const char *str, int16_t x, int16_t y,
                                 int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  _flushCluster();
  int16_t sx = _cx, sy = _cy;
  _cx = x; _cy = y;
  _measureBegin();
  _measureText(str, false);
  _measure = false;
  _cx = sx; _cy = sy;
  if (_mx1 < _mx0) { *x1 = x; *y1 = y; *w = 0; *h = 0; return; }
  *x1 = _mx0; *y1 = _my0;
  *w = (uint16_t)(_mx1 - _mx0 + 1);
  *h = (uint16_t)(_my1 - _my0 + 1);
}

void Massmore_GFX::getTextBounds(const __FlashStringHelper *str, int16_t x, int16_t y,
                                 int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  _flushCluster();
  int16_t sx = _cx, sy = _cy;
  _cx = x; _cy = y;
  _measureBegin();
  _measureText((const char *)str, true);
  _measure = false;
  _cx = sx; _cy = sy;
  if (_mx1 < _mx0) { *x1 = x; *y1 = y; *w = 0; *h = 0; return; }
  *x1 = _mx0; *y1 = _my0;
  *w = (uint16_t)(_mx1 - _mx0 + 1);
  *h = (uint16_t)(_my1 - _my0 + 1);
}

uint16_t Massmore_GFX::textWidth(const char *str) {
  _flushCluster();
  int16_t sx = _cx, sy = _cy;
  bool wrap = _wrap;
  _wrap = false;
  _cx = 0; _cy = 0;
  _measureBegin();
  _measureText(str, false);
  _measure = false;
  uint16_t w = (uint16_t)(_mAdvMax > 0 ? _mAdvMax : 0);
  _wrap = wrap;
  _cx = sx; _cy = sy;
  return w;
}

uint16_t Massmore_GFX::textWidth(const __FlashStringHelper *str) {
  _flushCluster();
  int16_t sx = _cx, sy = _cy;
  bool wrap = _wrap;
  _wrap = false;
  _cx = 0; _cy = 0;
  _measureBegin();
  _measureText((const char *)str, true);
  _measure = false;
  uint16_t w = (uint16_t)(_mAdvMax > 0 ? _mAdvMax : 0);
  _wrap = wrap;
  _cx = sx; _cy = sy;
  return w;
}

void Massmore_GFX::drawText(int16_t x, int16_t y, const __FlashStringHelper *str, Massmore_TextAlign align) {
  if (!str) return;
  if (align != ALIGN_LEFT) {
    int16_t w = (int16_t)textWidth(str);
    x -= (align == ALIGN_CENTER) ? w / 2 : w;
  }
  setCursor(x, y);
  _measureText((const char *)str, true);       // _measure = false → วาดจริง
  _onTextWritten();
}

void Massmore_GFX::drawText(int16_t x, int16_t y, const char *str, Massmore_TextAlign align) {
  if (!str) return;
  if (align != ALIGN_LEFT) {
    int16_t w = (int16_t)textWidth(str);
    x -= (align == ALIGN_CENTER) ? w / 2 : w;
  }
  setCursor(x, y);
  write((const uint8_t *)str, strlen(str));
}

void Massmore_GFX::drawThai(int16_t x, int16_t y, const char *str) {
  if (!str) return;
  setCursor(x, y);
  write((const uint8_t *)str, strlen(str));
}
