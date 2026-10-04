/**
 * @file    Massmore_OLED_Defs.h
 * @brief   Command constants (ชื่อตาม datasheet), enum สถานะ / error / identity ของ Massmore_OLED
 *
 * แหล่งอ้างอิง: docs/datasheet/ — SSD1306 Rev 1.1, SSD1309 Rev 1.1, SH1106 V0.2, SH1107 V2.1
 * Designed and Manufactured by Massmore — https://www.massmore.shop
 */
#pragma once
#include <Arduino.h>

/* ========================================================================== */
/*  Library version                                                           */
/* ========================================================================== */
#define MASSMORE_OLED_VERSION        "1.0.0"
#define MASSMORE_OLED_VERSION_MAJOR  1
#define MASSMORE_OLED_VERSION_MINOR  0
#define MASSMORE_OLED_VERSION_PATCH  0

/* ========================================================================== */
/*  Compile-time memory policy (§4.2) — override ได้ด้วย -D หรือ #define ก่อน include */
/* ========================================================================== */
#ifndef MASSMORE_OLED_BUFFER_SIZE
  #if defined(__AVR__)
    #define MASSMORE_OLED_BUFFER_SIZE  1024   // 128x64 พอดี — SH1107 128x128 ใช้ไม่ได้บน Nano (v1.0)
  #else
    #define MASSMORE_OLED_BUFFER_SIZE  2048   // ครอบคลุม 128x128
  #endif
#endif

// ขนาด payload ต่อ 1 I2C transaction (ไม่รวม control byte)
#ifndef MASSMORE_OLED_I2C_CHUNK
  #if defined(ARDUINO_ARCH_ESP32)
    #define MASSMORE_OLED_I2C_CHUNK    127    // Arduino-ESP32 3.x: Wire buffer 128 B
  #elif defined(BUFFER_LENGTH) && (BUFFER_LENGTH > 32)
    #define MASSMORE_OLED_I2C_CHUNK    (BUFFER_LENGTH - 1)
  #else
    #define MASSMORE_OLED_I2C_CHUNK    31     // AVR Wire buffer 32 B
  #endif
#endif

/* ========================================================================== */
/*  I2C protocol (common to all 4 controllers)                                */
/* ========================================================================== */
#define MASSMORE_OLED_ADDR_DEFAULT   0x3C   // silkscreen "0x78" = 8-bit form
#define MASSMORE_OLED_ADDR_ALT       0x3D   // SA0 = 1
#define MASSMORE_OLED_CTRL_CMD       0x00   // Co=0, D/C#=0 → command stream
#define MASSMORE_OLED_CTRL_DATA      0x40   // Co=0, D/C#=1 → GDDRAM data stream

/* ========================================================================== */
/*  Command set — shared core (SSD1306 / SSD1309 / SH1106 / SH1107)           */
/* ========================================================================== */
#define OLED_CMD_SET_LOW_COLUMN      0x00   // 00h–0Fh  lower nibble of column
#define OLED_CMD_SET_HIGH_COLUMN     0x10   // 10h–1Fh  upper nibble of column
#define OLED_CMD_SET_START_LINE      0x40   // 40h–7Fh  (SSD13xx / SH1106 only)
#define OLED_CMD_SET_CONTRAST        0x81   // + 1 byte
#define OLED_CMD_SEG_REMAP_OFF       0xA0   // column 0 → SEG0
#define OLED_CMD_SEG_REMAP_ON        0xA1   // column 0 → SEG127 (SH1106: SEG131)
#define OLED_CMD_DISPLAY_RAM         0xA4   // resume to RAM content
#define OLED_CMD_DISPLAY_ALL_ON      0xA5   // entire display ON (ignore RAM)
#define OLED_CMD_NORMAL_DISPLAY      0xA6
#define OLED_CMD_INVERT_DISPLAY      0xA7
#define OLED_CMD_SET_MULTIPLEX       0xA8   // + 1 byte (height-1)
#define OLED_CMD_DISPLAY_OFF         0xAE
#define OLED_CMD_DISPLAY_ON          0xAF
#define OLED_CMD_SET_PAGE            0xB0   // B0h + page (SH1107: B0h–BFh)
#define OLED_CMD_COM_SCAN_INC        0xC0
#define OLED_CMD_COM_SCAN_DEC        0xC8
#define OLED_CMD_SET_DISPLAY_OFFSET  0xD3   // + 1 byte
#define OLED_CMD_SET_CLOCK_DIV       0xD5   // + 1 byte
#define OLED_CMD_SET_PRECHARGE       0xD9   // + 1 byte
#define OLED_CMD_SET_COM_PINS        0xDA   // + 1 byte (SSD13xx / SH1106)
#define OLED_CMD_SET_VCOM_DETECT     0xDB   // + 1 byte
#define OLED_CMD_NOP                 0xE3

/* ---- SSD1306 / SSD1309 ---- */
#define OLED_CMD_MEMORY_MODE         0x20   // + 1 byte (02h = page addressing)
#define OLED_CMD_SCROLL_RIGHT        0x26
#define OLED_CMD_SCROLL_LEFT         0x27
#define OLED_CMD_SCROLL_DIAG_RIGHT   0x29
#define OLED_CMD_SCROLL_DIAG_LEFT    0x2A
#define OLED_CMD_SCROLL_STOP         0x2E
#define OLED_CMD_SCROLL_START        0x2F
#define OLED_CMD_VSCROLL_AREA        0xA3   // + 2 bytes
#define OLED_CMD_CHARGE_PUMP         0x8D   // SSD1306 only: 14h = on, 10h = off

/* ---- SSD1309 only ---- */
#define OLED_CMD_CONTENT_SCROLL_R    0x2C   // one column per command, no 2Fh needed
#define OLED_CMD_CONTENT_SCROLL_L    0x2D
#define OLED_CMD_COMMAND_LOCK        0xFD   // FDh 16h = lock, FDh 12h = unlock

/* ---- SH1106 / SH1107 ---- */
#define OLED_CMD_DCDC_CONTROL        0xAD   // + 1 byte. SH1106: 8Bh on / 8Ah off. SH1107: 81h on / 80h off
#define OLED_CMD_PUMP_VOLTAGE        0x30   // SH1106: 30h–33h (6.4 / 7.4 / 8.0 / 9.0 V)
#define OLED_CMD_RMW_START           0xE0
#define OLED_CMD_RMW_END             0xEE

/* ---- SH1107 only ---- */
#define OLED_CMD_SH1107_PAGE_MODE    0x20   // single-byte: page addressing
#define OLED_CMD_SH1107_START_LINE   0xDC   // + 1 byte (2-byte start-line command)

/* ---- Status / ID byte (read after control byte 00h) ---- */
#define OLED_STATUS_BUSY             0x80   // SH1106 / SH1107
#define OLED_STATUS_DISPLAY_OFF      0x40   // all four: 1 = display OFF
#define OLED_STATUS_LOCKED           0x80   // วัดจริง: D7 = 1 ขณะ FDh 16h ล็อกอยู่ (ทั้ง SSD1315 และ SSD1309)
#define OLED_SH1107_ID_MASK          0x3F
#define OLED_SH1107_ID_VALUE         0x07   // SH1107 V2.1 §22 "Read ID" = 000111b
#define OLED_SH1106_ID_MASK          0x07
#define OLED_SH1106_ID_VALUE         0x00   // SH1106 "Read Status" low bits = 000

/* ========================================================================== */
/*  Colours (Adafruit-GFX compatible values)                                  */
/* ========================================================================== */
#ifndef BLACK
  #define BLACK    0
#endif
#ifndef WHITE
  #define WHITE    1
#endif
#ifndef INVERSE
  #define INVERSE  2
#endif
#define MASSMORE_BLACK    0
#define MASSMORE_WHITE    1
#define MASSMORE_INVERSE  2

/* Blue-Yellow glass (SKU-0102-5): แถว 0–15 เป็นสีเหลือง, 16–63 สีฟ้า */
#define MASSMORE_OLED_BY_YELLOW_H   16

/* ========================================================================== */
/*  Enums                                                                     */
/* ========================================================================== */

/** ผลลัพธ์ของทุกฟังก์ชันที่อาจล้มเหลว — อ่านข้อความด้วย errorString() */
enum Massmore_OLED_Error : uint8_t {
  MASSMORE_OLED_OK = 0,
  MASSMORE_OLED_ERR_NO_ACK,          // ไม่มี ACK ที่ address (สาย / address / ไฟเลี้ยง)
  MASSMORE_OLED_ERR_BUS,             // I2C error อื่น (timeout, arbitration, data NACK)
  MASSMORE_OLED_ERR_NO_BUFFER,       // MASSMORE_OLED_BUFFER_SIZE เล็กกว่าจอ (เช่น SH1107 บน Nano)
  MASSMORE_OLED_ERR_BAD_MODEL,       // model ไม่รู้จัก
  MASSMORE_OLED_ERR_NOT_SUPPORTED,   // controller นี้ไม่มีฟีเจอร์นี้ (เช่น HW scroll บน SH110x)
  MASSMORE_OLED_ERR_ID_MISMATCH,     // controller ที่ตรวจพบไม่ตรงกับ SKU ที่เลือก
  MASSMORE_OLED_ERR_NOT_STARTED      // ยังไม่ได้เรียก begin()
};

/** ชิปควบคุม */
enum Massmore_OLED_Controller : uint8_t {
  MASSMORE_CTRL_UNKNOWN = 0,
  MASSMORE_CTRL_SSD1306,
  MASSMORE_CTRL_SSD1309,
  MASSMORE_CTRL_SH1106,
  MASSMORE_CTRL_SH1107,
  MASSMORE_CTRL_SSD1315,             // ตรวจพบเท่านั้น: สั่งงานแบบ SSD1306 + มีคำสั่ง lock (ใช้ profile SSD1306)
  MASSMORE_CTRL_SSD1309_1315         // AUTO: ล็อกได้ แต่แยก SSD1309 / SSD1315 ทางไฟฟ้าไม่ได้ (วัดจริง: D7 ติดทั้งคู่)
};

/** ผลการตรวจ Controller Identity (§8) — ไม่ใช่ cryptographic authenticity */
enum Massmore_OLED_Identity : uint8_t {
  MASSMORE_IDENTITY_UNKNOWN = 0,     // ยังไม่ได้ตรวจ / อ่านอะไรไม่ได้เลย
  MASSMORE_IDENTITY_VERIFIED,        // พฤติกรรมตรง datasheet ของ controller ที่เลือก
  MASSMORE_IDENTITY_CONSISTENT,      // ไม่ขัดแย้ง แต่ชิปไม่รองรับการอ่าน status → ยืนยันไม่ได้
  MASSMORE_IDENTITY_MISMATCH         // พฤติกรรมเป็น controller อื่น
};

/** ตำแหน่งจัดข้อความสำหรับ drawText() */
enum Massmore_TextAlign : uint8_t {
  ALIGN_LEFT = 0,
  ALIGN_CENTER,
  ALIGN_RIGHT
};

/* ข้อความคงที่ (ชื่อชิป / error): AVR เก็บใน flash (ประหยัด RAM), ESP32 เป็น const char* ปกติ
 * ใช้กับ Serial.print() ได้เหมือนกันทั้งสองแบบ */
#if defined(__AVR__)
  typedef const __FlashStringHelper *Massmore_Str;
  #define MASSMORE_STR(s) F(s)
#else
  typedef const char *Massmore_Str;
  #define MASSMORE_STR(s) (s)
#endif

/* Profile flags (§2.1) */
#define MASSMORE_F_HW_SCROLL        0x0001   // 26h/27h/29h/2Ah/2Eh/2Fh/A3h
#define MASSMORE_F_CONTENT_SCROLL   0x0002   // 2Ch/2Dh (SSD1309)
#define MASSMORE_F_CHARGE_PUMP      0x0004   // 8Dh (SSD1306)
#define MASSMORE_F_DCDC             0x0008   // ADh (SH1106 / SH1107)
#define MASSMORE_F_CMD_LOCK         0x0010   // FDh (SSD1309)
#define MASSMORE_F_READ_ID          0x0020   // ID bits ใน status byte (SH1107)
#define MASSMORE_F_NEEDS_RESET      0x0040   // โมดูล 5 ขา — ต้องต่อ RES
#define MASSMORE_F_START_LINE_2BYTE 0x0080   // DCh xx (SH1107)
