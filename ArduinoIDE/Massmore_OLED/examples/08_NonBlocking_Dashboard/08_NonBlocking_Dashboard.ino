/*
  08_NonBlocking_Dashboard — Massmore_OLED (Advance)
  ---------------------------------------------------------------------------
  Dashboard ที่ไม่บล็อก loop():
    - ตัวจับเวลา millis() แยกงาน: อ่าน "เซนเซอร์" ทุก 100 ms, วาดใหม่ทุก 250 ms, LED กระพริบ 500 ms
    - displayStep(): ส่งจอทีละ 1 page ต่อรอบ loop (~3 ms ที่ 400 kHz) แทน display() ที่ใช้ ~24 ms
      → งานอื่น (ปุ่ม, UART, มอเตอร์) ไม่ถูกหน่วง
    - Dirty pages: วาดทับเฉพาะส่วนที่เปลี่ยน → ส่งเฉพาะ page นั้น (ดู getDirtyMask())

  ตัวเลือก (ESP32 เท่านั้น):
    USE_FREERTOS_TASK 1 → ย้ายการส่งจอไปไว้ใน task แยก (core 0) ป้องกัน buffer ด้วย mutex
    SECOND_OLED       1 → จอที่สองบน address 0x3D (Wire เดียวกัน) หรือย้ายไป Wire1 ได้
                          (Nano RAM ไม่พอสำหรับ 2 จอ)

  Wiring: ดู 01_HelloWorld · LED_BUILTIN (ถ้ามี) กระพริบให้เห็นว่า loop ไม่ค้าง
  Designed and Manufactured by Massmore — https://www.massmore.shop
*/

#include <Wire.h>
#include <Massmore_OLED.h>

#define OLED_SKU    OLED096_White_SSD1306      // ← เปลี่ยนเป็นรุ่นของคุณ
#define OLED_ADDR   0x3C
#define OLED_RST    (-1)

#define USE_FREERTOS_TASK  0                   // 1 = ESP32: ส่งจอใน FreeRTOS task
#define SECOND_OLED        0                   // 1 = มีจอที่สองที่ 0x3D
#define OLED2_SKU          OLED091_White_SSD1306   // รุ่นของจอที่สอง

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define I2C_SDA 14
  #define I2C_SCL 15
#elif defined(ARDUINO_ARCH_ESP32)
  #define I2C_SDA 21
  #define I2C_SCL 22
#endif

#if (USE_FREERTOS_TASK || SECOND_OLED) && defined(__AVR__)
  #error "USE_FREERTOS_TASK / SECOND_OLED ใช้ได้เฉพาะ ESP32"
#endif

Massmore_OLED    oled(OLED_SKU);
Massmore_OLED_UI ui(oled);
#if SECOND_OLED
Massmore_OLED    oled2(OLED2_SKU);
#endif

static int16_t history[64];
Massmore_OLED_Chart chart(history, 64);

/* ---------------- "เซนเซอร์" จำลอง ---------------- */
float    temperature = 25.0f;
uint8_t  load = 0;
uint32_t loops = 0, loopsPerSec = 0;

/* ---------------- FreeRTOS ---------------- */
#if USE_FREERTOS_TASK
SemaphoreHandle_t oledMutex;
void displayTask(void *) {
  for (;;) {
    if (xSemaphoreTake(oledMutex, portMAX_DELAY) == pdTRUE) {
      oled.display();                           // ส่งเฉพาะ dirty pages
      xSemaphoreGive(oledMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(20));              // ~50 Hz
  }
}
  #define LOCK()   xSemaphoreTake(oledMutex, portMAX_DELAY)
  #define UNLOCK() xSemaphoreGive(oledMutex)
#else
  #define LOCK()
  #define UNLOCK()
#endif

void drawStatic() {
  oled.clear();
  ui.header(nullptr);
  oled.setCursor(0, 4);
  oled.print(F("Dashboard"));
  oled.display();
}

void drawDynamic() {
  int16_t W = oled.width(), H = oled.height();
  // ตัวเลขอุณหภูมิ (มุมขวาบน) — ลบพื้นที่เดิมก่อน
  oled.fillRect(W - 48, 4, 48, 8, BLACK);
  oled.setCursor(W - 48, 4);
  oled.print(temperature, 1);
  oled.print(F(" C"));
  if (H >= 64) {
    ui.progressBar(0, 18, W, 9, load, true);
    chart.draw(oled, 0, 29, W, H - 29 - 9);
    oled.fillRect(0, H - 8, W, 8, BLACK);
    oled.setCursor(0, H - 8);
    oled.print(F("loop/s "));
    oled.print(loopsPerSec);
  } else {
    ui.progressBar(0, 18, W, 12, load, true);
  }
}

void setup() {
  Serial.begin(115200);
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
#endif
#if defined(I2C_SDA)
  Wire.begin(I2C_SDA, I2C_SCL);
#else
  Wire.begin();
#endif
  Wire.setClock(400000);
  if (oled.begin(Wire, OLED_ADDR, OLED_RST) != MASSMORE_OLED_OK) {
    Serial.println(Massmore_OLED::errorString(oled.lastError()));
    while (true) delay(100);
  }
#if SECOND_OLED
  if (oled2.begin(Wire, 0x3D) != MASSMORE_OLED_OK) Serial.println(F("2nd OLED not found"));
#endif
  drawStatic();
#if USE_FREERTOS_TASK
  oledMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(displayTask, "oled", 4096, nullptr, 1, nullptr, 0);
#endif
}

void loop() {
  static uint32_t tSensor = 0, tDraw = 0, tLed = 0, tRate = 0;
  uint32_t now = millis();
  loops++;

  if (now - tSensor >= 100) {                   // งาน 1: อ่านเซนเซอร์
    tSensor = now;
    temperature += (random(-10, 11)) / 100.0f;
    load = (uint8_t)(50 + 45 * sin(now / 1500.0));
    chart.push((int16_t)(temperature * 10));
  }

  if (now - tDraw >= 250) {                     // งาน 2: วาดลง buffer (เร็ว — ยังไม่ส่ง I2C)
    tDraw = now;
    LOCK();
    drawDynamic();
    UNLOCK();
#if SECOND_OLED
    oled2.clear();
    oled2.setCursor(0, 0);
    oled2.print(temperature, 2);
    oled2.display();
#endif
  }

  if (now - tLed >= 500) {                      // งาน 3: LED heartbeat
    tLed = now;
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
#endif
  }

  if (now - tRate >= 1000) {
    tRate = now;
    loopsPerSec = loops;
    loops = 0;
    Serial.print(F("loop/s="));
    Serial.print(loopsPerSec);
    Serial.print(F(" dirty=0x"));
    Serial.println(oled.getDirtyMask(), HEX);
  }

#if !USE_FREERTOS_TASK
  oled.displayStep();                           // ส่ง 1 page แล้วกลับทันที
#endif
}
