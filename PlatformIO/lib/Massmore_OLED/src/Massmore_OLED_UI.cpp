/**
 * @file    Massmore_OLED_UI.cpp
 * @brief   UI widgets ของ Massmore_OLED
 */
#include "Massmore_OLED_UI.h"
#include <math.h>
#include <stdio.h>

void Massmore_OLED_UI::progressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, bool showText) {
  if (percent > 100) percent = 100;
  _g.fillRect(x, y, w, h, MASSMORE_BLACK);
  _g.drawRect(x, y, w, h, MASSMORE_WHITE);
  int16_t fw = (int16_t)((int32_t)(w - 4) * percent / 100);
  if (fw > 0) _g.fillRect(x + 2, y + 2, fw, h - 4, MASSMORE_WHITE);
  if (showText && h >= 9) {
    char t[6];
    snprintf(t, sizeof(t), "%u%%", (unsigned)percent);
    Massmore_TextState st;
    _g.saveTextState(st);
    _g.setFont();
    _g.setTextSize(1);
    _g.setTextColor(MASSMORE_INVERSE);          // ตัวอักษรกลับสีตามพื้นหลัง → อ่านได้ทั้งบนแถบและพื้นดำ
    _g.drawText(x + w / 2, y + (h - 7) / 2, t, ALIGN_CENTER);
    _g.restoreTextState(st);
  }
}

void Massmore_OLED_UI::gauge(int16_t cx, int16_t cy, int16_t r, float value, float minV, float maxV, const char *label) {
  if (maxV <= minV) maxV = minV + 1;
  float t = (value - minV) / (maxV - minV);
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  _g.drawArc(cx, cy, r, 180, 360, MASSMORE_WHITE, 2);
  // ขีดสเกล 5 ขีด (0, 25, 50, 75, 100 %)
  for (uint8_t i = 0; i <= 4; i++) {
    float a = (float)PI * (1.0f + i / 4.0f);
    int16_t x0 = cx + (int16_t)lroundf(cosf(a) * (r - 3)), y0 = cy + (int16_t)lroundf(sinf(a) * (r - 3));
    int16_t x1 = cx + (int16_t)lroundf(cosf(a) * (r - 6)), y1 = cy + (int16_t)lroundf(sinf(a) * (r - 6));
    _g.drawLine(x0, y0, x1, y1, MASSMORE_WHITE);
  }
  float a = (float)PI * (1.0f + t);
  int16_t nx = cx + (int16_t)lroundf(cosf(a) * (r - 5));
  int16_t ny = cy + (int16_t)lroundf(sinf(a) * (r - 5));
  _g.drawLine(cx, cy, nx, ny, MASSMORE_WHITE);
  _g.fillCircle(cx, cy, 2, MASSMORE_WHITE);
  if (label) {
    Massmore_TextState st;
    _g.saveTextState(st);
    _g.setFont();
    _g.setTextSize(1);
    _g.setTextColor(MASSMORE_WHITE);
    _g.drawText(cx, cy + 4, label, ALIGN_CENTER);
    _g.restoreTextState(st);
  }
}

void Massmore_OLED_UI::battery(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, bool charging) {
  if (percent > 100) percent = 100;
  int16_t tipW = (w >= 12) ? 2 : 1;
  int16_t bw = w - tipW;
  _g.fillRect(x, y, w, h, MASSMORE_BLACK);
  _g.drawRect(x, y, bw, h, MASSMORE_WHITE);
  _g.fillRect(x + bw, y + h / 4, tipW, h - 2 * (h / 4), MASSMORE_WHITE);
  int16_t fw = (int16_t)((int32_t)(bw - 4) * percent / 100);
  if (fw > 0) _g.fillRect(x + 2, y + 2, fw, h - 4, MASSMORE_WHITE);
  if (charging) {                               // สายฟ้า (กลับสี)
    int16_t mx = x + bw / 2, my = y + h / 2;
    _g.drawLine(mx + 2, y + 1, mx - 1, my, MASSMORE_INVERSE);
    _g.drawLine(mx + 1, my, mx - 2, y + h - 2, MASSMORE_INVERSE);
  }
}

void Massmore_OLED_UI::signalBars(int16_t x, int16_t y, uint8_t level, uint8_t bars, uint8_t barW, uint8_t maxH) {
  if (bars == 0) return;
  if (level > bars) level = bars;
  for (uint8_t i = 0; i < bars; i++) {
    int16_t bh = (int16_t)maxH * (i + 1) / bars;
    int16_t bx = x + i * (barW + 1);
    int16_t by = y + maxH - bh;
    if (i < level) _g.fillRect(bx, by, barW, bh, MASSMORE_WHITE);
    else { _g.fillRect(bx, by, barW, bh, MASSMORE_BLACK); _g.drawRect(bx, by, barW, bh, MASSMORE_WHITE); }
  }
}

uint8_t Massmore_OLED_UI::listMenu(int16_t x, int16_t y, int16_t w, const char *const items[], uint8_t count,
                                   uint8_t selected, uint8_t visibleRows, uint8_t rowH) {
  if (!items || count == 0 || visibleRows == 0) return 0;
  if (selected >= count) selected = count - 1;
  if (rowH == 0) rowH = (uint8_t)_g.fontHeight();
  uint8_t first = 0;
  if (selected >= visibleRows) first = selected - visibleRows + 1;
  bool bar = count > visibleRows;
  int16_t tw = bar ? w - 4 : w;
  int16_t asc = _g.fontAscent();
  Massmore_TextState st;
  _g.saveTextState(st);
  _g.fillRect(x, y, w, (int16_t)rowH * visibleRows, MASSMORE_BLACK);
  for (uint8_t i = 0; i < visibleRows && first + i < count; i++) {
    uint8_t idx = first + i;
    int16_t ry = y + (int16_t)i * rowH;
    bool sel = (idx == selected);
    if (sel) _g.fillRect(x, ry, tw, rowH, MASSMORE_WHITE);
    _g.setTextColor(sel ? MASSMORE_BLACK : MASSMORE_WHITE);
    _g.setTextWrap(false);
    // ฟอนต์ 5x7: cursor = ขอบบน · ฟอนต์อื่น: cursor = baseline → ตัวอักษรหลักอยู่กลางแถว
    int16_t ty = asc ? ry + (rowH + _g.fontXHeight()) / 2 : ry + (rowH - 7) / 2;
    _g.drawText(x + 2, ty, items[idx], ALIGN_LEFT);
  }
  _g.restoreTextState(st);
  if (bar) {
    int16_t th = (int16_t)rowH * visibleRows;
    int16_t kh = th * visibleRows / count;
    if (kh < 3) kh = 3;
    int16_t ky = y + (int16_t)((int32_t)(th - kh) * first / (count - visibleRows));
    _g.drawFastVLine(x + w - 2, y, th, MASSMORE_WHITE);
    _g.fillRect(x + w - 3, ky, 3, kh, MASSMORE_WHITE);
  }
  return first;
}

void Massmore_OLED_UI::header(const char *title, int16_t h, bool inverted) {
  _g.fillRect(0, 0, _g.width(), h, inverted ? MASSMORE_WHITE : MASSMORE_BLACK);
  if (!inverted) _g.drawFastHLine(0, h - 1, _g.width(), MASSMORE_WHITE);
  if (title) {
    Massmore_TextState st;
    _g.saveTextState(st);
    _g.setTextColor(inverted ? MASSMORE_BLACK : MASSMORE_WHITE);
    _g.setTextWrap(false);
    int16_t asc = _g.fontAscent();
    int16_t ty = asc ? (h - 1 + _g.fontXHeight()) / 2 : (h - 1 - 7) / 2;
    _g.drawText(_g.width() / 2, ty, title, ALIGN_CENTER);
    _g.restoreTextState(st);
  }
}

void Massmore_OLED_UI::toggle(int16_t x, int16_t y, bool on, int16_t w, int16_t h) {
  int16_t r = h / 2;
  _g.fillRoundRect(x, y, w, h, r, on ? MASSMORE_WHITE : MASSMORE_BLACK);
  _g.drawRoundRect(x, y, w, h, r, MASSMORE_WHITE);
  int16_t kx = on ? x + w - r - 1 : x + r;
  _g.fillCircle(kx, y + r, r - 2, on ? MASSMORE_BLACK : MASSMORE_WHITE);
}

void Massmore_OLED_UI::spinner(int16_t cx, int16_t cy, int16_t r, uint8_t phase) {
  _g.fillCircle(cx, cy, r + 1, MASSMORE_BLACK);
  for (uint8_t i = 0; i < 8; i++) {
    float a = (float)PI * 2.0f * i / 8.0f;
    int16_t px = cx + (int16_t)lroundf(cosf(a) * r);
    int16_t py = cy + (int16_t)lroundf(sinf(a) * r);
    uint8_t d = (uint8_t)((i - phase) & 7);          // จุดนำหน้าใหญ่สุด ไล่หางเล็กลง
    _g.fillCircle(px, py, d == 0 ? 2 : (d < 3 ? 1 : 0), MASSMORE_WHITE);
  }
}

/* ========================================================================== */
/*  Chart                                                                     */
/* ========================================================================== */
void Massmore_OLED_Chart::push(int16_t v) {
  if (!_d || !_cap) return;
  _d[_head] = v;
  _head = (_head + 1) % _cap;
  if (_n < _cap) _n++;
}

void Massmore_OLED_Chart::draw(Massmore_GFX &g, int16_t x, int16_t y, int16_t w, int16_t h, bool frame) {
  g.fillRect(x, y, w, h, MASSMORE_BLACK);
  if (frame) g.drawRect(x, y, w, h, MASSMORE_WHITE);
  if (_n < 2 || w < 4 || h < 4) return;
  int16_t ix = frame ? x + 1 : x, iy = frame ? y + 1 : y;
  int16_t iw = frame ? w - 2 : w, ih = frame ? h - 2 : h;
  int16_t lo = _lo, hi = _hi;
  uint16_t n = _n < (uint16_t)iw ? _n : (uint16_t)iw;      // จุดละ 1 คอลัมน์
  if (_auto) {
    lo = INT16_MAX; hi = INT16_MIN;
    for (uint16_t i = 0; i < n; i++) {
      int16_t v = _d[(_head + _cap - n + i) % _cap];
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
  }
  if (hi <= lo) hi = lo + 1;
  int16_t px = 0, py = 0;
  for (uint16_t i = 0; i < n; i++) {
    int16_t v = _d[(_head + _cap - n + i) % _cap];
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    int16_t cx = ix + iw - (int16_t)n + i;
    int16_t cy = iy + ih - 1 - (int16_t)((int32_t)(v - lo) * (ih - 1) / (hi - lo));
    if (i) g.drawLine(px, py, cx, cy, MASSMORE_WHITE);
    px = cx; py = cy;
  }
}
