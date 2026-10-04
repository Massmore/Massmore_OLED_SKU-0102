/**
 * @file    Massmore_OLED_UI.h
 * @brief   UI widgets สำเร็จรูป (optional): progress bar, gauge, battery, signal bars, list menu,
 *          header bar (โซนเหลืองของจอ Blue-Yellow), toggle, spinner และกราฟเส้นแบบ ring buffer
 *
 * ทุก widget วาดลง buffer เท่านั้น — เรียก oled.display() เองเมื่อพร้อม
 * ไม่มี malloc: กราฟ (Massmore_OLED_Chart) ใช้ array ที่ sketch จองให้
 *
 * @code
 *   Massmore_OLED_UI ui(oled);
 *   ui.progressBar(0, 20, 128, 10, 75, true);
 *   ui.battery(100, 0, 24, 10, 60);
 *   oled.display();
 * @endcode
 */
#pragma once
#include "Massmore_GFX.h"

class Massmore_OLED_UI {
public:
  explicit Massmore_OLED_UI(Massmore_GFX &gfx) : _g(gfx) {}

  /** แถบความคืบหน้า 0–100 % · showText = แสดง "NN%" ตรงกลาง (สีกลับอัตโนมัติ) */
  void progressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, bool showText = false);

  /** มาตรวัดครึ่งวงกลม (ด้านบน) พร้อมเข็ม + ขีดสเกล · label = ข้อความใต้จุดหมุน (ฟอนต์ 5x7) */
  void gauge(int16_t cx, int16_t cy, int16_t r, float value, float minV, float maxV, const char *label = nullptr);

  /** ไอคอนแบตเตอรี่ 0–100 % (ขั้วอยู่ขวา) · charging = วาดสายฟ้า */
  void battery(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, bool charging = false);

  /** แท่งสัญญาณ 0–bars (เช่น WiFi RSSI) — แท่งสูงขึ้นทีละขั้น จนถึง maxH */
  void signalBars(int16_t x, int16_t y, uint8_t level, uint8_t bars = 4, uint8_t barW = 3, uint8_t maxH = 12);

  /**
   * รายการเมนู: แถวที่เลือกแสดงแบบกลับสี + scrollbar ด้านขวาเมื่อรายการยาวกว่าที่แสดงได้
   * ใช้ฟอนต์ปัจจุบันของจอ (5x7 / GFXfont / ไทย) · rowH = 0 → ใช้ fontHeight()
   * @return แถวแรกที่แสดง (ใช้ต่อเนื่องเพื่อเลื่อนรายการ)
   */
  uint8_t listMenu(int16_t x, int16_t y, int16_t w, const char *const items[], uint8_t count,
                   uint8_t selected, uint8_t visibleRows, uint8_t rowH = 0);

  /** แถบหัวเรื่องสูง h px (ค่าเริ่มต้น 16 = โซนสีเหลืองของ SKU-0102-5) + เส้นคั่น */
  void header(const char *title, int16_t h = 16, bool inverted = false);

  /** สวิตช์เปิด/ปิด ขนาด w x h */
  void toggle(int16_t x, int16_t y, bool on, int16_t w = 20, int16_t h = 10);

  /** วงกลมหมุน (loading) — phase เพิ่มทีละ 1 ทุกเฟรม */
  void spinner(int16_t cx, int16_t cy, int16_t r, uint8_t phase);

private:
  Massmore_GFX &_g;
};

/**
 * กราฟเส้นแบบเลื่อน (live chart) — sketch จอง array เอง:
 * @code
 *   static int16_t samples[100];
 *   Massmore_OLED_Chart chart(samples, 100);
 *   chart.push(analogRead(A0));
 *   chart.draw(oled, 0, 16, 128, 48);
 * @endcode
 */
class Massmore_OLED_Chart {
public:
  Massmore_OLED_Chart(int16_t *storage, uint16_t capacity)
    : _d(storage), _cap(capacity), _n(0), _head(0), _auto(true), _lo(0), _hi(1) {}

  void     push(int16_t v);
  void     clear() { _n = 0; _head = 0; }
  void     setRange(int16_t minV, int16_t maxV) { _auto = false; _lo = minV; _hi = maxV; }
  void     setAutoRange() { _auto = true; }
  uint16_t size() const { return _n; }
  int16_t  latest() const { return _n ? _d[(_head + _cap - 1) % _cap] : 0; }
  /** วาดในกรอบ (x,y,w,h) — จุดล่าสุดอยู่ขวาสุด · frame = วาดกรอบ */
  void     draw(Massmore_GFX &g, int16_t x, int16_t y, int16_t w, int16_t h, bool frame = true);

private:
  int16_t *_d;
  uint16_t _cap, _n, _head;
  bool     _auto;
  int16_t  _lo, _hi;
};
