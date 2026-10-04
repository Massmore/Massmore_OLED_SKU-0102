/**
 * @file    Massmore_ThaiText.h
 * @brief   การจัดประเภทอักษรไทย (Unicode U+0E00 block) สำหรับตัวจัดรูป (shaper) ของ Massmore_GFX
 *
 * กฎการวาง (WTT-style ขั้นต่ำ, §5.3) — ทำแบบ "ตำแหน่งคำนวณจากขนาด glyph จริง" ไม่ต้องมี glyph แปรรูป:
 *  1. สระบน  ั ิ ี ึ ื ็ ํ ๎ และวรรณยุกต์/เครื่องหมาย  ่ ้ ๊ ๋ ์  ไม่มีความกว้าง วาดซ้อนบนพยัญชนะตัวก่อนหน้า
 *  2. วรรณยุกต์ไม่มีสระบน → วางต่ำ (ชิดหัวพยัญชนะ) / มีสระบน → ซ้อนเหนือสระ
 *  3. พยัญชนะหางบน ป ฝ ฟ ฬ → เลื่อนเครื่องหมายบนไปทางซ้ายพ้นหาง
 *  4. สระล่าง  ุ ู ฺ  วางใต้ฐาน · ฎ ฏ (หางล่าง) → สระลงต่ำใต้หาง · ญ ฐ → ตัดเชิงออกเมื่อมีสระล่าง
 *  5. สระอำ ำ = นิคหิต ํ บนพยัญชนะ (วรรณยุกต์ซ้อนเหนือ) + สระอา า
 *  6. ตัดบรรทัดเฉพาะที่ช่องว่าง หรือ ZWSP (U+200B) — ภาษาไทยไม่มีช่องว่างระหว่างคำ
 */
#pragma once
#include <Arduino.h>

#define MASSMORE_TH_SARA_AM      0x0E33
#define MASSMORE_TH_SARA_AA      0x0E32
#define MASSMORE_TH_NIKHAHIT     0x0E4D
#define MASSMORE_ZWSP            0x200B

/** สระบน + นิคหิต + ยามักการ (ชั้นที่ 1 เหนือพยัญชนะ) */
static inline bool massmore_th_isUpperVowel(uint16_t cp) {
  return cp == 0x0E31 || (cp >= 0x0E34 && cp <= 0x0E37) || cp == 0x0E47 || cp == 0x0E4D || cp == 0x0E4E;
}
/** วรรณยุกต์ ่ ้ ๊ ๋ และทัณฑฆาต ์ (ชั้นบนสุด) */
static inline bool massmore_th_isTone(uint16_t cp) {
  return cp >= 0x0E48 && cp <= 0x0E4C;
}
/** สระล่าง ุ ู และพินทุ ฺ */
static inline bool massmore_th_isLowerVowel(uint16_t cp) {
  return cp >= 0x0E38 && cp <= 0x0E3A;
}
/** เครื่องหมายไม่มีความกว้าง (combining) */
static inline bool massmore_th_isMark(uint16_t cp) {
  return massmore_th_isUpperVowel(cp) || massmore_th_isTone(cp) || massmore_th_isLowerVowel(cp);
}
/** พยัญชนะหางบน: ป ฝ ฟ ฬ */
static inline bool massmore_th_isAscender(uint16_t cp) {
  return cp == 0x0E1B || cp == 0x0E1D || cp == 0x0E1F || cp == 0x0E2C;
}
/** พยัญชนะที่ต้องตัดเชิงเมื่อมีสระล่าง: ญ ฐ */
static inline bool massmore_th_isRemovableTail(uint16_t cp) {
  return cp == 0x0E0D || cp == 0x0E10;
}
