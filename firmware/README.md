# Massmore_OLED — Factory Test Firmware

เฟิร์มแวร์ที่ compile แล้วของตัวอย่าง [`10_Factory_Test`](../ArduinoIDE/Massmore_OLED/examples/10_Factory_Test/)
สำหรับ **ESP32 (Classic) DevKit — Primary Factory Test MCU** ใช้ตรวจจอ Massmore OLED ทั้ง 12 รุ่น (SKU-0102-1 … 12)
ก่อนส่งลูกค้า (Outgoing QA/QC) และให้ **Massmore Web Serial Monitor** อ่านผลอัตโนมัติ
**Firmware ตัวเดียวใช้ได้ทุกขนาด** — ตรวจชิปให้ก่อน แล้วถามขนาดจอทาง Serial

## Files

| File | Flash offset | Size | SHA-256 |
|---|---|---|---|
| `bin/Massmore_OLED_FactoryTest_ESP32.bin` | `0x0` | 404,560 B | `0d3fe0526fcceda609906e031c9db6224a489b86a659b6f7480e58e8a11154af` |
| `bin/Massmore_OLED_FactoryTest_ESP32_app.bin` | `0x10000` | 339,024 B | `65d07e43be424322b9bc45ce9873fa504aa9a479a6cd853de454cbb1c15fdc72` |
| `manifest.json` | — | — | Manifest สำหรับ ESP Web Tools |

- **Merged image** (`…_ESP32.bin`) = bootloader `0x1000` + partitions `0x8000` + boot_app0 `0xE000` + app `0x10000` — ไฟล์เดียวจบ ใช้กับ web flasher / esptool
- **App only** (`…_app.bin`) ใช้เมื่อไม่ต้องการเขียนทับ bootloader / partition table

Build (v1.0.0): PlatformIO env `esp32dev`, pioarduino `55.03.311` (Arduino-ESP32 Core 3.3.11), partition `default.csv`, 4 MB flash, DIO
**Rebuild + อัปเดต SHA-256 ในตารางนี้ทุกครั้งที่แก้ `10_Factory_Test.ino` หรือไลบรารี**

## Wiring (Primary MCU = ESP32 Classic, I2C / Qwiic)

| OLED | ESP32 DevKit | Note |
|---|---|---|
| `VCC` | 3V3 | จอรองรับ 3.3–5 V |
| `GND` | GND | |
| `SDA` | GPIO 21 | |
| `SCL` | GPIO 22 | I2C 400 kHz |
| `RES` (เฉพาะ 1.5" SKU-0102-8, 2.42" SKU-0102-11/12) | GPIO 17 | แนะนำให้ต่อ (2.42" ทดสอบแล้วทำงานได้แม้ไม่ต่อ → `RESET_PIN WARN`) · จอ 4 ขาไม่ต้องต่อ |
| จอ 2.42" | — | กินกระแสสูงตอนติดเต็มจอ — ถ้า USB หลุด / บอร์ดรีบูต ให้ต่อ VCC จอเข้า 5V (VIN) แทน 3V3 |
| ปุ่ม BOOT บนบอร์ด (GPIO 0) | — | กด = ยืนยัน VISUAL ผ่าน |

## Flashing

**1. esptool (command line)** — ไฟล์เดียวจบ

```bash
esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 460800 \
  write_flash -z 0x0 bin/Massmore_OLED_FactoryTest_ESP32.bin
```

460800 ทดสอบผ่านกับ jig (ESP32 + CH340) — 512000 อัพโหลดไม่ผ่าน · ถ้ายังไม่นิ่งลดเป็น `115200` · Windows ใช้ `--port COM5`

**2. PlatformIO** — copy `ArduinoIDE/Massmore_OLED/examples/10_Factory_Test/10_Factory_Test.ino` ไปทับ `PlatformIO/src/main.cpp`
(เติม `#include <Arduino.h>` บรรทัดแรก) แล้ว `pio run -e esp32dev -t upload`

**3. Web flasher (ESP Web Tools)** — วางโฟลเดอร์ `firmware/` บนเว็บ HTTPS แล้วใช้

```html
<script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js"></script>
<esp-web-install-button manifest="firmware/manifest.json"></esp-web-install-button>
```

## Test procedure

1. เสียบจอเข้ากับ jig แล้วเปิด Serial Monitor 115200 (กด Enter เป็นการส่งบรรทัด)
2. บอร์ดสแกน I2C และ **ตรวจชิปให้อัตโนมัติ** → รายงาน `#DETECT <ชิป>` + ขนาดที่น่าจะเป็น
   (แสดงบนจอด้วย ยกเว้นตระกูล SSD1306 และ SSD1309/SSD1315 — จอว่างไว้จนกว่าจะพิมพ์ขนาด เพราะ 0.91" 128x32 กับ 0.96" 128x64 ใช้ชิปเดียวกัน วาดก่อนรู้ขนาดภาพจะเพี้ยน)
3. พิมพ์ **ขนาดจอ** แล้ว Enter: `0.91` `0.96` `1.3` `1.5` `1.54` `2.42` — ต่อท้าย `B` = สีฟ้า, `Y` = ฟ้า-เหลือง (เช่น `0.96B`)
   (Web Serial Monitor ส่ง `SKU 0102-N` แทนได้) — ถ้าขนาดไม่ตรงกับชิปที่ตรวจพบ `CONTROLLER_ID` จะ FAIL เช่น `EXPECT_SH1106_GOT_SSD1315`
4. ทดสอบทางไฟฟ้า ~2 วินาที แล้วดูภาพทดสอบที่วนบนจอ:
   all-on (หาจุดตาย) → checkerboard → กรอบ + ทแยง → inverse → contrast sweep → ภาษาไทย → โซน Blue-Yellow
5. ส่ง `P` = ผ่าน / `F` = ไม่ผ่าน (หรือกดปุ่ม BOOT = ผ่าน) ภายใน 30 วินาที — ไม่ตอบ = `SKIP`
6. **จอถัดไป:** ถอดจอ เสียบตัวใหม่ แล้วกด Enter (ตรวจชิป + ถามขนาดใหม่) · `r` = ทดสอบจอเดิมซ้ำ · พิมพ์ขนาด = ทดสอบจอเดิมด้วยขนาดนั้น

## Serial Output Format (115200 8N1, English only)

ทุกบรรทัดที่เว็บ parse ขึ้นต้นด้วย `#` · บรรทัดอื่นเป็น human-readable · parser ต้องข้ามบรรทัดที่ไม่รู้จักได้

```text
#MASSMORE_FACTORY_TEST v1.0
#PRODUCT Massmore_OLED
#DETECT <SSD1306 | SSD1309/SSD1315 | SH1106 | SH1107 | NONE> <addr> [ASSUMED]
#PROMPT SIZE                                     (รอขนาดจอ หรือ "SKU 0102-N")
#SKU SKU-0102-N
#MCU ESP32
#CHIP <ชิปของ SKU ที่เลือก>                       (ข้อมูลเท่านั้น ไม่ใช่ PASS/FAIL)
#RESULT <TEST_NAME> <PASS|FAIL|WARN|SKIP> <value>
#VERDICT <PASS|FAIL> [<REASON>]                  (ครั้งเดียว, บรรทัด # สุดท้าย)
[PASS] OLED QA PASSED - READY TO SHIP - <DRIVER>   |   [FAIL] QA CHECK FAILED: <REASON> - <DRIVER>
#DRIVER <SSD1306 | SSD1315 | SSD1309 | SH1106 | SH1107>   (driver ที่ตรวจพบจริง)
PASS - <DRIVER>   |   FAIL - <DRIVER>                      (บรรทัดสรุป — แสดงบนจอ OLED ด้วย)
```

`WARN` และ `SKIP` ไม่ทำให้ `#VERDICT` เป็น FAIL — มีเพียง `FAIL` เท่านั้น

| TEST_NAME | PASS criteria | On fail | Value |
|---|---|---|---|
| `BUS_SCAN` | ACK ที่ 0x3C หรือ 0x3D | FAIL | address |
| `RESET_PIN` | จอ 5 ขา: NACK ขณะ RES = LOW และ ACK หลังปล่อย | WARN (จอ 4 ขา = SKIP) | `NACK_IN_RESET` |
| `INIT_SEQ` | ทุก byte ของ init sequence ได้ ACK | FAIL | model |
| `CHIP_STATUS` | อ่าน status byte ได้ | WARN (`UNREADABLE` — ปกติของ SSD1306) | hex |
| `CONTROLLER_ID` | Controller Identity = `VERIFIED` / `CONSISTENT` (SKU ตระกูล SSD1306 รับ SSD1315) | FAIL เมื่อ `MISMATCH` | `<ชิปที่พบ>_VERIFIED` / `EXPECT_<A>_GOT_<B>` |
| `DISPLAY_ONOFF` | บิต D6 ของ status กลับค่าตาม AEh / AFh | FAIL (SKIP ถ้าอ่าน status ไม่ได้) | `D6_TOGGLES` |
| `FRAME_WRITE` | ส่งเต็มเฟรมได้ ACK ครบ | FAIL | ms |
| `FRAME_RATE` | ≥ 30 fps (128x32/128x64) · ≥ 15 fps (128x128) | WARN | fps |
| `I2C_400K` | 50 เฟรมติดกันที่ 400 kHz ไม่มี NACK | FAIL | `n/50` |
| `VISUAL` | พนักงานยืนยันภาพทดสอบ | FAIL (`F`) / SKIP (timeout 30 s) | `OPERATOR_OK` |

## Expected Report (จอจริง SKU-0102-1, 2026-10-05)

```text
Massmore_OLED Factory Test - one firmware for all SKU-0102 panels

#DETECT SSD1309/SSD1315 0x3C
Detected controller: SSD1309/SSD1315  ->  likely size: 0.91 / 0.96 / 1.54 / 2.42
#PROMPT SIZE
Enter panel size: 0.91 / 0.96 / 1.3 / 1.5 / 1.54 / 2.42  (add B = blue, Y = blue-yellow)

#MASSMORE_FACTORY_TEST v1.0
#PRODUCT Massmore_OLED
#SKU SKU-0102-1
#MCU ESP32
#RESULT BUS_SCAN PASS 0x3C
#RESULT RESET_PIN SKIP 4PIN_MODULE
#RESULT INIT_SEQ PASS SKU-0102-1
#CHIP SSD1306
#RESULT CHIP_STATUS PASS 0x02
#RESULT CONTROLLER_ID PASS SSD1315_VERIFIED
#RESULT DISPLAY_ONOFF PASS D6_TOGGLES
#RESULT FRAME_WRITE PASS 13.9ms
#RESULT FRAME_RATE PASS 71fps
#RESULT I2C_400K PASS 50/50
VISUAL: check the patterns, then send 'P' (pass) / 'F' (fail) or press BOOT
#RESULT VISUAL PASS OPERATOR_OK
#VERDICT PASS
[PASS] OLED QA PASSED - READY TO SHIP - SSD1315
#DRIVER SSD1315
PASS - SSD1315
Next: plug the next display and press Enter  |  'r' = repeat this display  |  or type a size
```

ผลวัดจริงที่ผ่านแล้ว: SKU-0102-6 (1.3" SH1106, status `0x28`, 35 fps) · SKU-0102-1 / SKU-0102-2 (0.91" ขาว / ฟ้า ชิป SSD1315, status `0x02`, 71 fps) · SKU-0102-4 / SKU-0102-5 (0.96" ฟ้า / ฟ้า-เหลือง ชิป SSD1315, 35 fps) · SKU-0102-11 (2.42" SSD1309 ไม่ต่อ RES, status `0x01`, 35 fps)

## Rebuild

**PlatformIO (Arduino-ESP32 Core 3.3.11):** วาง `10_Factory_Test.ino` เป็น `PlatformIO/src/main.cpp` (เติม `#include <Arduino.h>`) แล้ว

```bash
cd PlatformIO
pio run -e esp32dev
cp .pio/build/esp32dev/firmware.factory.bin ../firmware/bin/Massmore_OLED_FactoryTest_ESP32.bin
cp .pio/build/esp32dev/firmware.bin         ../firmware/bin/Massmore_OLED_FactoryTest_ESP32_app.bin
shasum -a 256 ../firmware/bin/*.bin          # อัปเดตตาราง Files ด้านบน
```

> macOS: ถ้า PlatformIO ขึ้น `Python's lzma module is unavailable` ให้รันด้วย `DYLD_FALLBACK_LIBRARY_PATH=/usr/lib pio run -e esp32dev`
> แนะนำตั้ง `PLATFORMIO_BUILD_DIR` ไว้นอก repo เพื่อไม่ให้ `.pio/` ปนในโฟลเดอร์

**Arduino IDE / arduino-cli (ทางเลือก):**

```bash
arduino-cli compile -b esp32:esp32:esp32 --library ArduinoIDE/Massmore_OLED \
  --build-path build/fw ArduinoIDE/Massmore_OLED/examples/10_Factory_Test
# build/fw/10_Factory_Test.ino.merged.bin = merged image สำหรับ offset 0x0
```
