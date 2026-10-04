# Massmore_OLED Library — Structure & Feasibility Plan (SKU-0102)

> Planning document for Claude Code to implement. **No code yet.**
> เอกสารวางแผนโครงสร้างไลบรารี ให้ Claude Code นำไปสร้างโค้ดจริงต่อ
>
> Repo folder: `Massmore_OLED_SKU-0102` · Library name: `Massmore_OLED` · Prepared 2026-10-02

---

## 0. TL;DR — Answers to the feasibility questions

| Question | Answer |
|---|---|
| Can all 12 SKUs live in **one** library? | **Yes.** All 4 controllers (SSD1306, SSD1309, SH1106, SH1107) use the same I2C protocol (addr `0x3C/0x3D`, control byte `0x00` = command / `0x40` = data), the same 1-bit page-organised RAM (8 vertical pixels per byte) and the same page-addressing commands (`B0h+page`, `00h–0Fh`, `10h–1Fh`). Differences are only init tables, column offset, height and a few optional features → solved with a **data-driven profile table**, not separate libraries. |
| Can the user just `#include` one header and pick the driver? | **Yes.** `#include <Massmore_OLED.h>` then `Massmore_OLED oled(MASSMORE_OLED_SKU_0102_3);` (or by controller, or `AUTO`). Optional alias classes (`Massmore_SSD1306`, `Massmore_SH1106`, …) live in the same header — no extra files. |
| Need Adafruit_SSD1306 / Adafruit_GFX as dependency? | **No — recommended zero dependency** (only `Wire`). Write a lean `Massmore_GFX` with an **Adafruit-GFX-compatible API** and **GFXfont-compatible font format**, so existing GFX code and fonts still work. Adafruit_SH110x also pulls GrayOLED + BusIO — too heavy for Nano and against "vendor everything / no install". |
| Thai TH Sarabun with adjustable size? | **Yes**, with a custom UTF-8 + Thai shaping renderer (Adafruit GFX cannot do Thai: `drawChar()` takes `unsigned char`, no combining-mark placement). Pre-rendered bitmap sizes + integer scale. **License note:** use Google Fonts **Sarabun (SIL OFL 1.1)** instead of TH Sarabun New (GPL 2.0 + font exception) — see §7.4. |
| Genuine chip / ID verification? | **Partially, honestly labelled.** None of these controllers has a serial number or OTP ID. What is datasheet-backed: **SH1107 "Read ID" = `000111b` (0x07)**, SH1106 status low bits `000`, ON/OFF bit loop-back on all, and **SSD1309 command-lock (`FDh`) behaviour** that SSD1306 does not have. Combined = **"Controller Identity Verification"**, not a cryptographic authenticity proof. See §8. |
| Factory Test + `.bin`? | Yes — last example `10_Factory_Test`, ESP32 Classic, Qwiic SDA21/SCL22, `#RESULT/#VERDICT` lines, merged `.bin` + `manifest.json` in `firmware/`. |

---

## 1. Source material reviewed

### 1.1 Datasheets in `docs/datasheet/` (primary reference)

| File | What it gave us |
|---|---|
| `OLED_SSD1306/SSD1306.pdf` (Solomon Rev 1.1, 2008) | Command set, horizontal/vertical HW scroll `26h/27h/29h/2Ah/2Eh/2Fh/A3h`, charge pump `8Dh 14h`, COM pins `DAh`, addr `0111 10 SA0`. §10.1.21: *"No status read is provided for serial mode"* → status read over I2C is **not guaranteed** by datasheet (bench-verify). No fade/zoom commands in this revision → do **not** implement `23h`/`D6h`. |
| `OLED_SSD1309/SSD1309.pdf` (Solomon Rev 1.1, 2011) | Same core as SSD1306 **+ Command Lock `FDh`** (unlock `FD 12h`), **Content Scroll `2Ch/2Dh`**, GPIO `DCh`. **No internal charge pump** (module has external boost). Status register: `D6` = display OFF flag. |
| `OLED_SH1106_Manual/SH1106.pdf` | 132×64 RAM → **column offset 2** on 128-px glass. DC-DC `ADh 8Bh`, pump voltage `30h–33h`, Read-Modify-Write `E0h/EEh`. **No horizontal addressing mode, no HW scroll.** Read Status = `BUSY, ON/OFF, *, *, *, 0, 0, 0`. |
| `OLED_SH1106_Manual/ZJY130I0400WG01.pdf`, `1.3-ZJY130-2864KSWLG01.pdf` | 1.3" glass 128×64, I2C module, active area 29.42×14.7 mm. |
| `OLED_SH1107V2_Manual/SH1107V2.1.pdf` | 128×128, page addr `B0h–BFh` (16 pages), addressing mode `20h/21h`, display start line is a **2-byte command `DCh, xx`**, **Read ID: low 6 bits = `000111` (07h)**. |
| `OLED_SH1107V2_Manual/SPEC ZJY150-2828KSWKG01.pdf`, `ZJY150I0400WG02.pdf` | 1.5" 128×128, **default SPI, converted to I2C**, has RES#. Vendor init: `AE, D5 80, A8 7F, D3 60, DC 00, A1, C0, 81 6F, D9 1D, DB 35, A4, AF`. |

### 1.2 Vendor example code in `docs/Example/` (secondary)

| Panel | Key init facts taken |
|---|---|
| 1.3" SH1106 Arduino | `0x3C`, col start `02h`, `81 CF`, `AD 8B` (DC-DC on), `33h` (VPP 9 V), `D9 1F`, `DB 40`. RES pin used ("needed when SPI module converted to IIC"). |
| 1.54" SSD1309 Arduino | `FD 12` unlock, `D5 A0`, `A8 3F`, `DA 12`, `81 BF`, `D9 25`, `DB 34`. RES pin used. |
| 2.42" SSD1309 STM32 | Same as 1.54" but `81 7F`, `D9 82`. Also sends `8D 14` (ignored by SSD1309 — harmless, not needed). RES pin used. |

### 1.3 Product page (massmore.shop) pinout

| Size | Pins |
|---|---|
| 0.91" / 0.96" (SSD1306) | 4-pin `GND VCC SCL SDA` |
| 1.3" SH1106 / 1.54" SSD1309 | 4-pin `GND VCC SCL SDA` |
| 1.5" SH1107 / 2.42" SSD1309 | **5-pin `GND VCC SCL SDA RES`** → RES must be driven by MCU |

Supply 3.3–5 V, −40…85 °C.

### 1.4 Reference libraries (patterns only, not dependencies)

- **Adafruit_SSD1306** (BSD): 128×32 → `DA 02`, contrast `8F`; 128×64 → `DA 12`, contrast `CF`; precharge `F1` (internal VCC). Uses `malloc` for the buffer, I2C chunking `WIRE_MAX`.
- **Adafruit-GFX-Library** (BSD): drawing API + `GFXfont` struct (`first/last` are `uint16_t` → a Thai range `0x0E01–0x0E5B` *fits* the struct, but `drawChar(unsigned char)` cannot address it).
- **Adafruit_SH110x** (BSD): SH1106G offset 2; SH1107 uses `AD 8A`, `DC 00`, `D3 60` (64×128) / `D3 00` + `A8 7F` (128×128). **Conflicts with ZJY vendor spec `D3 60` for 128×128 → bench-verify, expose `setDisplayOffset()`.**
- **Meshtastic firmware** `probeOLED()`: reads 1 byte at the OLED address; `(r & 0x0F)` = `0x00/0x08` → SH1106, `0x03–0x07` → SSD1306. Community-observed, not datasheet. Used only as a hint in §8.
- **Massmore_BNO08x_SKU-1010** (reference only): folder layout, `platformio.ini` style, `#RESULT/#VERDICT` format, `firmware/` contents.

---

## 2. SKU → hardware profile table (single source of truth)

> Colour (White / Blue / Blue-Yellow) is a **glass property only** — no software difference, except the B-Y layout helper.

| SKU | Size | Colour | Controller | Res | Pins | RES | Profile ID | Init notes |
|---|---|---|---|---|---|---|---|---|
| SKU-0102-1 | 0.91" | White | SSD1306 | 128×32 | 4 | — | `P_SSD1306_128x32` | `A8 1F`, `DA 02`, `81 8F`, `8D 14` |
| SKU-0102-2 | 0.91" | Blue | SSD1306 | 128×32 | 4 | — | `P_SSD1306_128x32` | same |
| SKU-0102-3 | 0.96" | White | SSD1306 | 128×64 | 4 | — | `P_SSD1306_128x64` | `A8 3F`, `DA 12`, `81 CF`, `8D 14` |
| SKU-0102-4 | 0.96" | Blue | SSD1306 | 128×64 | 4 | — | `P_SSD1306_128x64` | same |
| SKU-0102-5 | 0.96" | Blue-Yellow | SSD1306 | 128×64 | 4 | — | `P_SSD1306_128x64` | + `ZONE_YELLOW = rows 0–15` |
| SKU-0102-6 | 1.3" | White | SH1106 | 128×64 (RAM 132) | 4 | — | `P_SH1106_128x64` | **col offset 2**, `AD 8B`, `33h`, `81 CF` |
| SKU-0102-7 | 1.3" | Blue | SH1106 | 128×64 | 4 | — | `P_SH1106_128x64` | same |
| SKU-0102-8 | 1.5" | White | SH1107 | 128×128 | 5 | **Yes** | `P_SH1107_128x128` | 16 pages, `A8 7F`, `DC 00`, `D3 xx` (verify), `81 6F` |
| SKU-0102-9 | 1.54" | White | SSD1309 | 128×64 | 4 | — | `P_SSD1309_154` | `FD 12`, `D5 A0`, `81 BF`, `D9 25`, `DB 34` |
| SKU-0102-10 | 1.54" | Blue | SSD1309 | 128×64 | 4 | — | `P_SSD1309_154` | same |
| SKU-0102-11 | 2.42" | White | SSD1309 | 128×64 | 5 | **Yes** | `P_SSD1309_242` | `FD 12`, `D5 A0`, `81 7F`, `D9 82`, `DB 34` |
| SKU-0102-12 | 2.42" | Blue | SSD1309 | 128×64 | 5 | **Yes** | `P_SSD1309_242` | same |

I2C address default `0x3C` (module silkscreen `0x78` = 8-bit form). `0x3D` selectable on modules that expose SA0 resistor.

### 2.1 Profile struct (data-driven driver)

```cpp
// แนวคิด: driver = ข้อมูล ไม่ใช่ class แยก → โค้ดเล็ก, ไม่มี virtual, AVR ไม่บวม
struct MassmoreOLED_Profile {
  uint8_t  controller;      // SSD1306 / SSD1309 / SH1106 / SH1107
  uint8_t  width, height;   // พิกเซลจริงบนกระจก
  uint8_t  colOffset;       // SH1106 = 2, อื่น ๆ = 0
  uint8_t  pages;           // height / 8
  uint16_t flags;           // HAS_HW_SCROLL | HAS_CONTENT_SCROLL | HAS_CHARGE_PUMP |
                            // HAS_DCDC | HAS_CMD_LOCK | HAS_READ_ID | NEEDS_RESET | START_LINE_2BYTE
  const uint8_t* initSeq;   // PROGMEM: [len, cmd, args...] ... 0x00 terminator
  uint8_t  defaultContrast;
  uint8_t  expectStatusMask, expectStatusValue;  // fingerprint สำหรับ §8
};
```

One generic `flush()` writes page-by-page (`B0h+p`, `10h|colHi`, `colLo`, then data) — valid on **all four controllers**. SSD1306 horizontal-addressing burst would save only 3 command bytes per page → not worth a second code path.

---

## 3. Repository layout (ready to push)

```
Massmore_OLED_SKU-0102/
├── README.md                         ← main README (EN + TH helpers, product images)
├── .gitignore
├── ArduinoIDE/
│   ├── Massmore_OLED.zip             ← Sketch ▸ Include Library ▸ Add .ZIP Library
│   └── Massmore_OLED/
│       ├── library.properties
│       ├── keywords.txt
│       ├── LICENSE                   ← MIT (code)
│       ├── src/                      ← ★ canonical source (see §3.1)
│       └── examples/01_… 10_Factory_Test/
├── PlatformIO/
│   ├── platformio.ini
│   ├── .gitignore
│   ├── include/.gitkeep
│   ├── src/main.cpp                  ← = example 01 (+ #include <Arduino.h>)
│   └── lib/Massmore_OLED/            ← vendored copy, byte-identical src/ + library.json
├── firmware/
│   ├── README.md
│   ├── manifest.json                 ← ESP Web Tools
│   └── bin/
│       ├── Massmore_OLED_FactoryTest_ESP32.bin       (merged, offset 0x0)
│       └── Massmore_OLED_FactoryTest_ESP32_app.bin   (app, offset 0x10000)
└── docs/                             ← do not touch (datasheet/, Example/, images/)
```

> `docs/images/` **does not exist yet** — README will reference the file names in §11; Wooddy adds the images.

### 3.1 `src/` contents

```
src/
├── Massmore_OLED.h            ← the only header users include
├── Massmore_OLED.cpp          ← bus, init, flush, control, identity
├── Massmore_OLED_Profiles.h   ← SKU enum, profile table, PROGMEM init sequences
├── Massmore_OLED_Defs.h       ← command constants (named per datasheet), status/error enums
├── Massmore_GFX.h / .cpp      ← drawing engine, Adafruit-GFX-compatible API, Print
├── Massmore_ThaiText.h / .cpp ← UTF-8 decoder + Thai shaping + glyph placement
├── Massmore_OLED_UI.h / .cpp  ← optional widgets (progress, gauge, battery, list menu, chart)
└── fonts/
    ├── Massmore_Font5x7.h     ← built-in classic font (own, ASCII 0x20–0x7E)
    ├── Sarabun12.h  Sarabun16.h  Sarabun20.h  Sarabun24.h  Sarabun32.h
    └── OFL.txt                ← font licence (required by OFL)
```

Rule: **ArduinoIDE/…/src and PlatformIO/lib/…/src must be identical** (Claude Code: generate once, copy, verify with `diff -r`).

---

## 4. Public API (proposal)

```cpp
#include <Wire.h>
#include <Massmore_OLED.h>

// เลือกตาม SKU (แนะนำ) / ตามชิป / หรือ AUTO
Massmore_OLED oled(MASSMORE_OLED_SKU_0102_3);            // 0.96" SSD1306 128x64
// Massmore_OLED oled(MASSMORE_OLED_SSD1309_128X64);
// Massmore_OLED oled(MASSMORE_OLED_AUTO);               // probe controller (§8)
// Massmore_SH1106 oled;                                 // alias class, same header

void setup() {
  // begin(wire, addr, sda, scl, rstPin, i2cHz) — ไม่มี hardcoded pin
  if (oled.begin(Wire, 0x3C, 21, 22, -1, 400000) != MASSMORE_OLED_OK) { /* error */ }
  oled.clear();
  oled.setFont(&Sarabun20);
  oled.setCursor(0, 20);
  oled.print("สวัสดี Massmore");
  oled.display();
}
```

### 4.1 Groups

| Group | Methods |
|---|---|
| Start-up | `begin()`, `end()`, `reset()` (RES pulse), `setI2CClock()`, `isConnected()`, `getModel()`, `getControllerName()`, `width()`, `height()` |
| Buffer | `display()` (dirty pages only), `displayAll()`, `displayStep()` (one page per call — cooperative non-blocking), `isBusy()`, `clear()`, `fill()`, `getBuffer()`, `setAutoDisplay()` |
| Drawing (GFX-compatible) | `drawPixel`, `getPixel`, `drawLine`, `drawFastH/VLine`, `drawRect/fillRect`, `drawRoundRect/fillRoundRect`, `drawCircle/fillCircle`, `drawEllipse/fillEllipse`, `drawTriangle/fillTriangle`, `drawArc`, `drawBitmap` (PROGMEM/RAM), `drawXBitmap` (XBM), colours `BLACK/WHITE/INVERSE` |
| Text | `print/println/printf` (UTF-8 aware), `setCursor`, `setTextSize(sx,sy)`, `setTextColor(fg,bg)`, `setTextWrap`, `setFont(GFXfont* or Massmore font)`, `getTextBounds`, `drawText(x,y,str,align)` with `ALIGN_LEFT/CENTER/RIGHT` |
| Thai | `setThaiFont(&SarabunNN)`, `drawThai()` (same as print with shaping), `thaiTextWidth()` |
| Display control | `setContrast(0–255)`, `setBrightness(%)`, `invert(bool)`, `setRotation(0–3)` (software), `flipHorizontal/Vertical` (HW `A0/A1`, `C0/C8`), `sleep()` / `wake()` (incl. charge-pump / DC-DC off), `allPixelsOn(bool)` (`A5h`), `setDisplayOffset()`, `setStartLine()` |
| Scroll | `scrollRight/Left(startPage,endPage,speed)`, `scrollDiagRight/Left`, `scrollStop()` → HW on SSD1306/1309, returns `ERR_NOT_SUPPORTED` on SH110x; `scrollContent(dir)` (SSD1309 `2Ch/2Dh`); `scrollBuffer(dx,dy)` software fallback for all; `setVerticalScrollArea()` |
| Low level | `command(byte)`, `commandList(buf,len)`, `data(buf,len)`, `readStatus()` |
| Identity (§8) | `detectController()`, `readChipID()`, `verifyController()`, `getIdentityReport()` |
| Errors | `lastError()`, `errorString(code)` — `OK, ERR_NO_ACK, ERR_BUS, ERR_NO_BUFFER, ERR_BAD_MODEL, ERR_NOT_SUPPORTED, ERR_ID_MISMATCH` |
| B-Y helper | `MASSMORE_OLED_BY_YELLOW_H = 16` constant + `isYellowZone(y)` |

### 4.2 Memory policy

- **No `malloc`.** Static buffer sized at compile time: `MASSMORE_OLED_BUFFER_SIZE` default **2048** on ESP32/S3 (fits 128×128), **1024** on AVR. User may override via `-D`.
- AVR Nano (2 KB RAM): SSD1306 / SH1106 / SSD1309 OK (≤1 KB). **SH1107 128×128 on Nano → `begin()` returns `ERR_NO_BUFFER`** in v1.0. Optional v1.1: page-render mode (`firstPage()/nextPage()`, u8g2-style). Document in the MCU matrix.
- I2C chunk: 128 B on ESP32 core 3.x, 31 B on AVR (Wire buffer 32).
- Dirty-page bitmask (`uint16_t`) → only changed pages are sent.

### 4.3 Expected performance (400 kHz, theoretical)

| Panel | Bytes / frame | Full refresh | ~FPS |
|---|---|---|---|
| 128×32 | 512 | ~12 ms | ~80 |
| 128×64 | 1024 | ~24 ms | ~40 |
| 128×128 | 2048 | ~48 ms | ~20 |

Optional ESP32 "fast mode+" 800 kHz–1 MHz via `setI2CClock()` with warning (outside SSD1306/SH110x 400 kHz spec — bench test before claiming).

---

## 5. Thai font rendering (`Massmore_ThaiText`)

### 5.1 Why custom
GFX `drawChar(unsigned char)` handles only 8-bit codes; Thai needs UTF-8 decoding **and** zero-width combining marks positioned above/below the base consonant.

### 5.2 Font data format
- Keep the **`GFXfont` / `GFXglyph` structs** (compatible) and add a `MassmoreFont` wrapper:
  - block A: ASCII `0x20–0x7E`
  - block B: Thai `U+0E01–U+0E5B` (contiguous → fits `first/last uint16_t`)
  - block C: positional variants (PUA-style): tone marks **low**, marks **shifted left** (for ป ฝ ฟ ฬ), ญ/ฐ **without tail** (when followed by below-vowel)
- `print()` = `Print::write(byte)` → UTF-8 state machine → code point → shaper.

### 5.3 Shaping rules to implement (minimum WTT-style)
1. Above vowels `ั ิ ี ึ ื ็` and tone/marks `่ ้ ๊ ๋ ์ ํ` are zero-advance, drawn over previous base.
2. Tone mark **without** an above vowel → use the low variant; with above vowel → stack above it.
3. Base is ascender consonant (ป ฝ ฟ ฬ) → use left-shifted variants of upper marks.
4. Below vowels `ุ ู ฺ` zero-advance; base ฎ ฏ → shifted-down/left variant; base ญ ฐ → tail-less base glyph.
5. Sara Am `ำ` = draw nikhahit `ํ` over base (tone goes above it) + advance with sara aa `า`.
6. Line wrap only at spaces / ZWSP (`U+200B`) — Thai has no spaces between words; document that users insert ZWSP or spaces.

### 5.4 Sizes ("ปรับขนาดได้")
- Pre-rendered pixel heights: **12, 16, 20, 24, 32 px** (Sarabun renders small for its em-size; 16 px ≈ readable minimum on 128×64).
- Plus integer scaling `setTextSize(2)` (blocky but free).
- Compile-time selection to save flash: `#define MASSMORE_THAI_FONT_16` … ; AVR default = **16 px only** (~3–4 KB flash est.), ESP32 = all sizes (est. 30–40 KB total).

### 5.5 Generator (build tool, not shipped to users)
`fontgen` Python script (freetype-py or Pillow) → rasterise TTF at N px → emit `SarabunNN.h` incl. variant glyphs. **Decision D5:** keep in repo under `tools/fontgen/` or keep outside repo (Wooddy's "minimum files" preference). Recommendation: keep outside the repo; header files carry a comment of the command used.

---

## 6. Examples (10, Basic → Advance, last = Factory Test)

Each example: one `#define OLED_SKU` line at top to pick the panel, Thai comments, pins via defines (ESP32 21/22, ESP32-S3 MOMO 14/15, Nano A4/A5), no WiFi/SD/external libs.

| # | Folder | Level | Covers |
|---|---|---|---|
| 01 | `01_HelloWorld` | Basic | begin, clear, print, display, error handling |
| 02 | `02_Graphics_Primitives` | Basic | every draw* function, INVERSE colour |
| 03 | `03_Text_Fonts` | Basic | 5×7 font, size, wrap, alignment, numbers/float, `getTextBounds`, GFXfont use |
| 04 | `04_Thai_Sarabun` | Intermediate | all Thai sizes, mixed TH/EN, stacked marks test string (`ปู่ ญี่ปุ่น ฏุ ป้ำ ฟ้า`), centred text |
| 05 | `05_Bitmap_Animation` | Intermediate | PROGMEM bitmap, XBM icon, sprite frames, FPS counter |
| 06 | `06_Display_Control_Scroll` | Intermediate | contrast/brightness, invert, rotation 0–3, flip, sleep/wake, HW scroll (SSD13xx) vs software scroll (SH110x) — prints "not supported" cleanly |
| 07 | `07_UI_Widgets` | Advance | progress bar, gauge, battery, signal bars, list menu, live chart; B-Y layout (header 16 px) |
| 08 | `08_NonBlocking_Dashboard` | Advance | `millis()` scheduler, `displayStep()`, dirty pages; ESP32: optional FreeRTOS task; two displays (0x3C/0x3D or Wire/Wire1) |
| 09 | `09_AutoDetect_Identity` | Advance | I2C scan, `MASSMORE_OLED_AUTO`, status/ID read, identity report |
| 10 | `10_Factory_Test` | Factory | §9 |

> **Decision D2:** standing preference says ~5 examples, this request says "ครบทุกฟังก์ชัน". Plan uses **10**; can merge to 6 (01+02, 03+04, 05+06, 07+08, 09, 10) if preferred.

---

## 7. Build targets & configuration

### 7.1 `platformio.ini` (pattern from BNO08x)

```ini
[platformio]
default_envs = esp32dev
[env]
framework = arduino
monitor_speed = 115200
build_flags = -Wall -Wextra
[esp32_common]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
upload_speed = 512000          ; ค่าหลักตามที่ขอ (52x,xxx) สำหรับ Mac
; upload_speed = 921600        ; ทางเลือก เร็วกว่า ถ้าสายดี
; upload_speed = 460800        ; ทางเลือก ถ้าอัพโหลดไม่นิ่ง
monitor_filters = esp32_exception_decoder
[env:esp32dev]            ; Factory Test MCU, Qwiic SDA21/SCL22
[env:esp32-s3-devkitc-1]  ; MOMO ESP32-S3, -DARDUINO_USB_CDC_ON_BOOT=0, I2C 14/15
[env:nano]                ; atmelavr, nanoatmega328new
```

**Decision D1:** "Upload speed 52x,xxx" interpreted as **512000** primary. (Previous standard used 921600 primary.)

### 7.2 Arduino IDE
`library.properties`: `name=Massmore_OLED`, `version=1.0.0`, `architectures=esp32,avr`, `includes=Massmore_OLED.h`, `depends=` (empty), `category=Display`. Tested on Arduino-ESP32 core 3.x (latest 3.3.x) + Arduino AVR boards.

### 7.3 MCU support matrix (README)
| MCU | 128×32 / 128×64 | 128×128 | Thai sizes | Notes |
|---|---|---|---|---|
| ESP32 Classic | ✓ | ✓ | all | Factory Test MCU |
| ESP32-S3 | ✓ | ✓ | all | MOMO board |
| Nano (ATmega328P) | ✓ | ✗ (v1.0) | 16 px | 1 KB buffer of 2 KB RAM |

### 7.4 Licences
- Library code: **MIT** (same as BNO08x).
- Thai bitmaps: generate from **Google Fonts "Sarabun" (SIL OFL 1.1)** — compatible with an MIT library when `OFL.txt` is shipped with the font headers. TH Sarabun New is **GPL 2.0 + font exception**; bitmaps derived from it and compiled into firmware are a grey area for an MIT library → **avoid** (Decision D6).
- No Adafruit code copied verbatim → no BSD notice needed; if any algorithm is ported (e.g. circle/triangle routines), keep the BSD notice in that file.

---

## 8. Controller Identity Verification ("ของแท้จากโรงงาน")

**Honest scope:** these chips have no unique ID / serial / OTP. We can verify that the silicon *behaves like the datasheet controller* and that it matches the SKU selected — this catches wrong/substitute controllers (e.g. SSD1315 / CH1116 clones sold as SSD1306, SSD1306 sold as SSD1309) but is **not** a cryptographic authenticity proof. Name it **"Controller Identity: VERIFIED by Massmore"** in output, not "Genuine".

| Check | Source | SSD1306 | SSD1309 | SH1106 | SH1107 |
|---|---|---|---|---|---|
| ACK at 0x3C/0x3D | all datasheets | ✓ | ✓ | ✓ | ✓ |
| Status/ID byte read (`requestFrom(addr,1)` after control byte `0x00`) | SH1106 §23, SH1107 §22, SSD1309 Table 9-6 | low nibble 3–7 *(community)* | bench-verify | low 3 bits `000` *(datasheet)* | **low 6 bits = `000111` (07h)** *(datasheet)* |
| ON/OFF loop-back: `AE` → status D6 = 1, `AF` → D6 = 0 | all four define D6 | bench-verify (§10.1.21 caveat) | ✓ | ✓ | ✓ |
| **Command-lock test**: `FD 16` (lock) → `AE` → read D6 still 0 → `FD 12` (unlock) | SSD1309 §10.22 | unlock fails → display turns off → "not SSD1309" | **lock holds → SSD1309 confirmed** | skip | skip |
| BUSY bit = 0 after init | SH1106/SH1107 | — | — | ✓ | ✓ |

Result enum: `IDENTITY_VERIFIED`, `IDENTITY_CONSISTENT` (no contradiction but status read unsupported), `IDENTITY_MISMATCH` (e.g. SKU says SH1107 but ID ≠ 07h), `IDENTITY_UNKNOWN`.

`MASSMORE_OLED_AUTO` uses the same probe: ID 07h → SH1107; low bits `000` → SH1106; SSD13xx + lock test → SSD1309 else SSD1306. **SSD1306 128×32 vs 128×64 cannot be detected electrically** → AUTO defaults to 128×64 and the user/web chooses the SKU.

> All "bench-verify" values must be measured on real Massmore stock (all 12 SKUs) before release; put the measured table in README.

---

## 9. Factory Test (`10_Factory_Test`) — ESP32 Classic

### 9.1 Wiring
| OLED | ESP32 DevKit |
|---|---|
| VCC / GND | 3V3 / GND |
| SDA / SCL | **GPIO 21 / 22** (Qwiic) |
| RES (1.5", 2.42" only) | **GPIO 17** *(Decision D3 — same RST pin as BNO08x jig)* |

### 9.2 SKU selection
Web Serial Monitor sends `SKU 0102-N\n` within 3 s after boot; else AUTO (§8) with 128×64 default for SSD1306. Firmware prints the chosen SKU.

### 9.3 Test items (one `#RESULT` each)

| Item | PASS rule | On fail |
|---|---|---|
| `BUS_SCAN` | ACK at 0x3C or 0x3D | FAIL |
| `RESET_PIN` | (5-pin SKUs) device NACK/recovers around RES pulse | WARN |
| `INIT_SEQ` | every init byte ACKed | FAIL |
| `CHIP_STATUS` | status byte readable; value logged | WARN if unreadable (SSD1306) |
| `CONTROLLER_ID` | §8 result = VERIFIED/CONSISTENT and matches SKU | FAIL on MISMATCH |
| `DISPLAY_ONOFF` | D6 toggles with AE/AF | FAIL (if status readable) |
| `FRAME_WRITE` | full frame ACK, time logged (ms) | FAIL |
| `FRAME_RATE` | ≥ 30 fps (128×64) / ≥ 15 fps (128×128) @400 kHz | WARN |
| `I2C_400K` | 50 frames without NACK at 400 kHz | FAIL |
| `VISUAL` | operator confirms patterns (all-on `A5h`, checkerboard, border, inverse, contrast sweep, Thai string, B-Y zones) | FAIL / SKIP on timeout |

Visual confirmation (**Decision D4**): serial `P`/`F` from the web page **or** ESP32 BOOT button (GPIO0) = PASS, timeout 30 s = `SKIP`. Dead pixels cannot be detected electrically — only by the visual step.

### 9.4 Output (115200, English only)
```
#MASSMORE_FACTORY_TEST v1.0
#PRODUCT Massmore_OLED
#SKU SKU-0102-3
#MCU ESP32
#RESULT BUS_SCAN PASS 0x3C
#RESULT CONTROLLER_ID PASS SSD1306
...
#VERDICT PASS
```

### 9.5 Firmware deliverables
`firmware/bin/*.bin` (merged @0x0 + app @0x10000), `manifest.json`, `README.md` with size + SHA-256, flashing via esptool (`--baud 512000`, fallback 460800/115200), PlatformIO and ESP Web Tools. **Rebuild + update SHA whenever the test changes.**

---

## 10. Implementation phases for Claude Code

| Phase | Deliverable | Done when |
|---|---|---|
| 1 | `Defs.h`, `Profiles.h` (all 12 SKUs), bus layer, `begin/reset/flush/clear`, `displayStep` | 01_HelloWorld compiles for esp32dev / esp32-s3 / nano |
| 2 | `Massmore_GFX` (full GFX-compatible set) + 5×7 font | 02, 03 compile; pixel tests on 128×32/64/128 |
| 3 | Display control + scroll (HW + SW fallback) | 06 compiles; unsupported features return error, never hang |
| 4 | Thai renderer + Sarabun 12–32 px + OFL | 04 shows stacked-mark test string correctly |
| 5 | UI widgets, non-blocking dashboard, identity/AUTO | 07, 08, 09 compile |
| 6 | Factory Test + `.bin` + `manifest.json` + firmware README | merged bin flashes and prints `#VERDICT` |
| 7 | README, `keywords.txt`, `library.properties/json`, `.zip`, `diff -r` both src copies | zip imports in Arduino IDE; `pio run` builds all envs clean with `-Wall -Wextra` |
| 8 | Bench verification on all 12 SKUs | §8 table filled with measured values |

Build notes for the Cowork VM (from BNO08x): registry.platformio.org is blocked → install platformio via pip + manual `tool-scons`; set `PLATFORMIO_BUILD_DIR` outside the repo so `.pio/` never lands in the folder.

---

## 11. README.md outline (main)

1. Title + product cover `docs/images/oled-sku0102-cover.webp`
2. Product overview + **SKU table (§2)** + images `oled-sku0102-sizes.webp`, `oled-sku0102-colors.webp`
3. Pinout (4-pin / 5-pin) `oled-sku0102-pinout.webp`, ESP32 wiring `oled-sku0102-esp32-wiring.webp`
4. MCU compatibility matrix (§7.3)
5. Installation — Arduino IDE (.zip) / PlatformIO (open folder → build)
6. Quick start (choose SKU in one line)
7. API reference (§4.1 table)
8. Thai font usage + ZWSP wrapping note + font licence
9. Examples table (§6)
10. Factory Test & Web Serial Monitor (§9)
11. Controller Identity explanation (honest scope)
12. Troubleshooting (blank 2.42"/1.5" → RES not wired; 2-px garbage column → wrong SH1106 profile; 128×32 shows half → wrong SKU; upload unstable → lower baud)
13. Where to buy · License

> Image file names above are proposals — Wooddy supplies the images into `docs/images/`.

---

## 12. Open decisions (need Wooddy's answer before / during build)

| ID | Decision | Recommendation |
|---|---|---|
| D1 | Upload speed "52x,xxx" | 512000 primary, 921600 / 460800 as comments |
| D2 | Example count | 10 (as §6); fallback merge to 6 |
| D3 | RES pin on factory jig for 1.5"/2.42" | GPIO 17 |
| D4 | Visual check confirmation | Serial `P/F` from web + BOOT button, 30 s timeout → SKIP |
| D5 | Ship `tools/fontgen/` in repo? | No — keep repo minimal |
| D6 | Thai font source | Google Fonts Sarabun (OFL 1.1), not TH Sarabun New (GPL) |
| D7 | SH1107 on Nano | Not supported in v1.0; page mode in v1.1 if needed |
| D8 | SH1107 display offset `D3 60` (vendor) vs `D3 00` (Adafruit 128×128) | Bench-test on SKU-0102-8, keep `setDisplayOffset()` public |
