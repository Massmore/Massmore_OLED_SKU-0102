/**
 * @file    Massmore_OLED_Profiles.h
 * @brief   SKU enum + hardware profile table + PROGMEM init sequences (single source of truth, §2)
 *
 * แนวคิด: driver = ข้อมูล ไม่ใช่ class แยกต่อชิป → ไม่มี virtual, โค้ดเล็ก, AVR ไม่บวม
 * ทุก controller ใช้ page addressing (B0h+page, 10h|colHi, colLo) เหมือนกัน ต่างกันแค่ init / offset / ฟีเจอร์
 *
 * Init sequence format (PROGMEM): [len, cmd, args...] ... [0x00] — แต่ละกลุ่มส่งใน 1 I2C transaction
 * ไม่มี AFh (display ON) ใน sequence — begin() เคลียร์ RAM ก่อนแล้วค่อยเปิดจอ (ไม่มีขยะกระพริบ)
 */
#pragma once
#include "Massmore_OLED_Defs.h"

/* ========================================================================== */
/*  Model selector — เลือกตาม SKU (แนะนำ) / ตามชิป / หรือ AUTO                  */
/* ========================================================================== */
enum Massmore_OLED_Model : uint8_t {
  MASSMORE_OLED_AUTO = 0,            // probe controller (§8) — SSD1306 จะถือเป็น 128x64

  /* ---- ตาม SKU (ดูตารางใน README) ---- */
  MASSMORE_OLED_SKU_0102_1 = 1,      // 0.91"  White        SSD1306 128x32
  MASSMORE_OLED_SKU_0102_2,          // 0.91"  Blue         SSD1306 128x32
  MASSMORE_OLED_SKU_0102_3,          // 0.96"  White        SSD1306 128x64
  MASSMORE_OLED_SKU_0102_4,          // 0.96"  Blue         SSD1306 128x64
  MASSMORE_OLED_SKU_0102_5,          // 0.96"  Blue-Yellow  SSD1306 128x64 (แถว 0–15 เหลือง)
  MASSMORE_OLED_SKU_0102_6,          // 1.3"   White        SH1106  128x64
  MASSMORE_OLED_SKU_0102_7,          // 1.3"   Blue         SH1106  128x64
  MASSMORE_OLED_SKU_0102_8,          // 1.5"   White        SH1107  128x128  (5 ขา มี RES)
  MASSMORE_OLED_SKU_0102_9,          // 1.54"  White        SSD1309 128x64
  MASSMORE_OLED_SKU_0102_10,         // 1.54"  Blue         SSD1309 128x64
  MASSMORE_OLED_SKU_0102_11,         // 2.42"  White        SSD1309 128x64   (5 ขา มี RES)
  MASSMORE_OLED_SKU_0102_12,         // 2.42"  Blue         SSD1309 128x64   (5 ขา มี RES)

  /* ---- ชื่ออ่านง่าย (แนะนำ): OLED<ขนาด>_<สี>_<ชิป> · ขนาด = นิ้ว x 100 (096 = 0.96") ----
     ค่าเดียวกับ MASSMORE_OLED_SKU_0102_N ใช้แทนกันได้ */
  OLED091_White_SSD1306      = MASSMORE_OLED_SKU_0102_1,   // 0.91"  128x32
  OLED091_Blue_SSD1306       = MASSMORE_OLED_SKU_0102_2,   // 0.91"  128x32
  OLED096_White_SSD1306      = MASSMORE_OLED_SKU_0102_3,   // 0.96"  128x64
  OLED096_Blue_SSD1306       = MASSMORE_OLED_SKU_0102_4,   // 0.96"  128x64
  OLED096_BlueYellow_SSD1306 = MASSMORE_OLED_SKU_0102_5,   // 0.96"  128x64 (แถว 0–15 เหลือง)
  OLED130_White_SH1106       = MASSMORE_OLED_SKU_0102_6,   // 1.3"   128x64
  OLED130_Blue_SH1106        = MASSMORE_OLED_SKU_0102_7,   // 1.3"   128x64
  OLED150_White_SH1107       = MASSMORE_OLED_SKU_0102_8,   // 1.5"   128x128 (5 ขา ต่อ RES)
  OLED154_White_SSD1309      = MASSMORE_OLED_SKU_0102_9,   // 1.54"  128x64
  OLED154_Blue_SSD1309       = MASSMORE_OLED_SKU_0102_10,  // 1.54"  128x64
  OLED242_White_SSD1309      = MASSMORE_OLED_SKU_0102_11,  // 2.42"  128x64  (5 ขา ต่อ RES)
  OLED242_Blue_SSD1309       = MASSMORE_OLED_SKU_0102_12,  // 2.42"  128x64  (5 ขา ต่อ RES)

  /* ---- ตามชิป (ใช้กับจอยี่ห้ออื่นได้) — ลำดับต้องตรงกับ profile ID ด้านล่าง ---- */
  MASSMORE_OLED_SSD1306_128X32 = 32,
  MASSMORE_OLED_SSD1306_128X64,
  MASSMORE_OLED_SH1106_128X64,
  MASSMORE_OLED_SH1107_128X128,
  MASSMORE_OLED_SSD1309_128X64,      // ค่า init ของ 1.54"
  MASSMORE_OLED_SSD1309_128X64_242   // ค่า init ของ 2.42" (contrast / precharge ต่างกัน)
};

/* Profile IDs (index ใน MASSMORE_OLED_PROFILES[]) */
enum : uint8_t {
  MASSMORE_P_SSD1306_128X32 = 0,
  MASSMORE_P_SSD1306_128X64,
  MASSMORE_P_SH1106_128X64,
  MASSMORE_P_SH1107_128X128,
  MASSMORE_P_SSD1309_154,
  MASSMORE_P_SSD1309_242,
  MASSMORE_P_COUNT,
  MASSMORE_P_INVALID = 0xFF
};

static_assert(MASSMORE_OLED_SSD1309_128X64_242 - MASSMORE_OLED_SSD1306_128X32 == MASSMORE_P_SSD1309_242,
              "chip-model enum must follow profile ID order");

/** Hardware profile (§2.1) — อ่านจาก PROGMEM ด้วย memcpy_P */
struct MassmoreOLED_Profile {
  uint8_t        controller;          // Massmore_OLED_Controller
  uint8_t        width, height;       // พิกเซลจริงบนกระจก
  uint8_t        colOffset;           // SH1106 = 2 (RAM 132 คอลัมน์), อื่น ๆ = 0
  uint8_t        pages;               // height / 8
  uint16_t       flags;               // MASSMORE_F_*
  const uint8_t *initSeq;             // PROGMEM
  uint8_t        defaultContrast;
  uint8_t        segRemap;            // ค่า A0h/A1h ที่อยู่ใน init (ใช้กับ flipHorizontal)
  uint8_t        comScan;             // ค่า C0h/C8h ที่อยู่ใน init (ใช้กับ flipVertical)
  uint8_t        expectStatusMask;    // fingerprint สำหรับ §8 (0 = ไม่มี ID ให้ตรวจ)
  uint8_t        expectStatusValue;
};

/* ========================================================================== */
/*  Init sequences                                                            */
/* ========================================================================== */

// SSD1306 128x32 — 0.91" (SKU-0102-1/2): mux 1Fh, COM pins 02h, contrast 8Fh, internal charge pump
static const uint8_t MASSMORE_INIT_SSD1306_128X32[] PROGMEM = {
  1, 0xAE,              // display off
  2, 0xD5, 0x80,        // clock divide / osc freq (POR)
  2, 0xA8, 0x1F,        // multiplex = 32
  2, 0xD3, 0x00,        // display offset 0
  1, 0x40,              // start line 0
  2, 0x8D, 0x14,        // charge pump ON
  2, 0x20, 0x02,        // page addressing mode
  1, 0xA1,              // segment remap
  1, 0xC8,              // COM scan decrement
  2, 0xDA, 0x02,        // COM pins: sequential, no remap (128x32)
  2, 0x81, 0x8F,        // contrast
  2, 0xD9, 0xF1,        // pre-charge (internal VCC)
  2, 0xDB, 0x40,        // VCOMH deselect
  1, 0xA4,              // display follows RAM
  1, 0xA6,              // normal (not inverted)
  1, 0x2E,              // scroll off
  0
};

// SSD1306 128x64 — 0.96" (SKU-0102-3/4/5)
static const uint8_t MASSMORE_INIT_SSD1306_128X64[] PROGMEM = {
  1, 0xAE,
  2, 0xD5, 0x80,
  2, 0xA8, 0x3F,        // multiplex = 64
  2, 0xD3, 0x00,
  1, 0x40,
  2, 0x8D, 0x14,
  2, 0x20, 0x02,
  1, 0xA1,
  1, 0xC8,
  2, 0xDA, 0x12,        // COM pins: alternative (128x64)
  2, 0x81, 0xCF,
  2, 0xD9, 0xF1,
  2, 0xDB, 0x40,
  1, 0xA4,
  1, 0xA6,
  1, 0x2E,
  0
};

// SH1106 128x64 — 1.3" (SKU-0102-6/7): ตามโค้ดตัวอย่างผู้ผลิตกระจก (docs/Example/1.3OLED_Example)
static const uint8_t MASSMORE_INIT_SH1106_128X64[] PROGMEM = {
  1, 0xAE,
  1, 0x02,              // lower column = 2 (RAM 132 → glass 128)
  1, 0x10,
  1, 0x40,
  1, 0xB0,
  2, 0x81, 0xCF,
  1, 0xA1,
  1, 0xA6,
  2, 0xA8, 0x3F,
  2, 0xAD, 0x8B,        // DC-DC ON (internal VPP)
  1, 0x33,              // pump voltage 9.0 V
  1, 0xC8,
  2, 0xD3, 0x00,
  2, 0xD5, 0x80,
  2, 0xD9, 0x1F,
  2, 0xDA, 0x12,
  2, 0xDB, 0x40,
  1, 0xA4,
  0
};

// SH1107 128x128 — 1.5" (SKU-0102-8): ตาม spec ZJY150-2828KSWKG01 (docs/datasheet/OLED_SH1107V2_Manual)
// D3h 60h = ค่าของผู้ผลิต (Adafruit 128x128 ใช้ 00h) → bench-verify, ปรับได้ด้วย setDisplayOffset()
static const uint8_t MASSMORE_INIT_SH1107_128X128[] PROGMEM = {
  1, 0xAE,
  1, 0x00,
  1, 0x10,
  1, 0xB0,
  2, 0xDC, 0x00,        // display start line (2-byte command)
  2, 0x81, 0x6F,
  1, 0x20,              // page addressing mode (single-byte on SH1107)
  1, 0xA1,
  1, 0xC0,
  1, 0xA4,
  1, 0xA6,
  2, 0xA8, 0x7F,        // multiplex = 128
  2, 0xD3, 0x60,        // display offset (vendor)
  2, 0xD5, 0x80,
  2, 0xD9, 0x1D,
  2, 0xDB, 0x35,
  2, 0xAD, 0x80,        // DC-DC off (โมดูลมี boost ภายนอก)
  0
};

// SSD1309 128x64 — 1.54" (SKU-0102-9/10): ตามโค้ดตัวอย่างผู้ผลิต (docs/Example/1.54OLED_Example)
// SSD1309 ไม่มี charge pump ภายใน (โมดูลมี boost ภายนอก) → ไม่ส่ง 8Dh
static const uint8_t MASSMORE_INIT_SSD1309_154[] PROGMEM = {
  1, 0xAE,
  2, 0xFD, 0x12,        // command unlock
  2, 0xD5, 0xA0,
  2, 0xA8, 0x3F,
  2, 0xD3, 0x00,
  1, 0x40,
  2, 0x20, 0x02,        // page addressing mode
  1, 0xA1,
  1, 0xC8,
  2, 0xDA, 0x12,
  2, 0x81, 0xBF,
  2, 0xD9, 0x25,
  2, 0xDB, 0x34,
  1, 0xA4,
  1, 0xA6,
  1, 0x2E,
  0
};

// SSD1309 128x64 — 2.42" (SKU-0102-11/12): ตาม docs/Example/2.42OLED_Example
static const uint8_t MASSMORE_INIT_SSD1309_242[] PROGMEM = {
  1, 0xAE,
  2, 0xFD, 0x12,
  2, 0xD5, 0xA0,
  2, 0xA8, 0x3F,
  2, 0xD3, 0x00,
  1, 0x40,
  2, 0x20, 0x02,
  1, 0xA1,
  1, 0xC8,
  2, 0xDA, 0x12,
  2, 0x81, 0x7F,
  2, 0xD9, 0x82,
  2, 0xDB, 0x34,
  1, 0xA4,
  1, 0xA6,
  1, 0x2E,
  0
};

/* ========================================================================== */
/*  Profile table                                                             */
/* ========================================================================== */
static const MassmoreOLED_Profile MASSMORE_OLED_PROFILES[MASSMORE_P_COUNT] PROGMEM = {
  // ctrl                 w    h   off pg  flags                                                      init                             contrast seg   com   mask  value
  { MASSMORE_CTRL_SSD1306, 128,  32, 0,  4, MASSMORE_F_HW_SCROLL | MASSMORE_F_CHARGE_PUMP,           MASSMORE_INIT_SSD1306_128X32,  0x8F, 0xA1, 0xC8, 0x00, 0x00 },
  { MASSMORE_CTRL_SSD1306, 128,  64, 0,  8, MASSMORE_F_HW_SCROLL | MASSMORE_F_CHARGE_PUMP,           MASSMORE_INIT_SSD1306_128X64,  0xCF, 0xA1, 0xC8, 0x00, 0x00 },
  { MASSMORE_CTRL_SH1106,  128,  64, 2,  8, MASSMORE_F_DCDC,                                         MASSMORE_INIT_SH1106_128X64,   0xCF, 0xA1, 0xC8, OLED_SH1106_ID_MASK, OLED_SH1106_ID_VALUE },
  { MASSMORE_CTRL_SH1107,  128, 128, 0, 16, MASSMORE_F_DCDC | MASSMORE_F_READ_ID | MASSMORE_F_NEEDS_RESET | MASSMORE_F_START_LINE_2BYTE,
                                                                                                     MASSMORE_INIT_SH1107_128X128,  0x6F, 0xA1, 0xC0, OLED_SH1107_ID_MASK, OLED_SH1107_ID_VALUE },
  { MASSMORE_CTRL_SSD1309, 128,  64, 0,  8, MASSMORE_F_HW_SCROLL | MASSMORE_F_CONTENT_SCROLL | MASSMORE_F_CMD_LOCK,
                                                                                                     MASSMORE_INIT_SSD1309_154,     0xBF, 0xA1, 0xC8, 0x00, 0x00 },
  { MASSMORE_CTRL_SSD1309, 128,  64, 0,  8, MASSMORE_F_HW_SCROLL | MASSMORE_F_CONTENT_SCROLL | MASSMORE_F_CMD_LOCK | MASSMORE_F_NEEDS_RESET,
                                                                                                     MASSMORE_INIT_SSD1309_242,     0x7F, 0xA1, 0xC8, 0x00, 0x00 },
};

/* SKU-0102-N → profile + ข้อมูลกระจก (index = N-1) */
struct MassmoreOLED_SkuInfo {
  uint8_t profile;
  char    size[6];      // "0.91" ...
  char    colour[12];   // "White" / "Blue" / "Blue-Yellow"
};

static const MassmoreOLED_SkuInfo MASSMORE_OLED_SKUS[12] PROGMEM = {
  { MASSMORE_P_SSD1306_128X32,  "0.91", "White"       },
  { MASSMORE_P_SSD1306_128X32,  "0.91", "Blue"        },
  { MASSMORE_P_SSD1306_128X64,  "0.96", "White"       },
  { MASSMORE_P_SSD1306_128X64,  "0.96", "Blue"        },
  { MASSMORE_P_SSD1306_128X64,  "0.96", "Blue-Yellow" },
  { MASSMORE_P_SH1106_128X64,   "1.3",  "White"       },
  { MASSMORE_P_SH1106_128X64,   "1.3",  "Blue"        },
  { MASSMORE_P_SH1107_128X128,  "1.5",  "White"       },
  { MASSMORE_P_SSD1309_154,     "1.54", "White"       },
  { MASSMORE_P_SSD1309_154,     "1.54", "Blue"        },
  { MASSMORE_P_SSD1309_242,     "2.42", "White"       },
  { MASSMORE_P_SSD1309_242,     "2.42", "Blue"        },
};
