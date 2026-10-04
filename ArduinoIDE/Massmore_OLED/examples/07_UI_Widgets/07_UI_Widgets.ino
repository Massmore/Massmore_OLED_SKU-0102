/*
  07_UI_Widgets — Massmore_OLED (Advance)
  ---------------------------------------------------------------------------
  Widgets สำเร็จรูปจาก Massmore_OLED_UI (include มากับ Massmore_OLED.h แล้ว):
    header (แถบหัว 16 px = โซนสีเหลืองของจอ Blue-Yellow SKU-0102-5), battery, signalBars,
    progressBar, gauge, toggle, spinner, listMenu (เมนูไทยได้) และกราฟเส้น Massmore_OLED_Chart

  จอ Blue-Yellow: แถว 0–15 เป็นสีเหลือง → วางหัวเรื่อง/สถานะไว้ในโซนนี้
  (ค่าคงที่ MASSMORE_OLED_BY_YELLOW_H = 16, ฟังก์ชัน isYellowZone(y))

  แนะนำจอ 128x64 ขึ้นไป (จอ 128x32 จะแสดงเฉพาะส่วนบน)
  Wiring: ดู 01_HelloWorld
  Designed and Manufactured by Massmore — https://www.massmore.shop
*/

#include <Wire.h>
#include <Massmore_OLED.h>

#define OLED_SKU    OLED096_BlueYellow_SSD1306 // ← Blue-Yellow (เปลี่ยนเป็นรุ่นของคุณ)
#define OLED_ADDR   0x3C
#define OLED_RST    (-1)

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define I2C_SDA 14
  #define I2C_SCL 15
#elif defined(ARDUINO_ARCH_ESP32)
  #define I2C_SDA 21
  #define I2C_SCL 22
#endif

Massmore_OLED    oled(OLED_SKU);
Massmore_OLED_UI ui(oled);

#if defined(__AVR__)
  #define CHART_N 32                            // Nano: RAM 2 KB → เก็บจุดน้อยลง
#else
  #define CHART_N 96
#endif
static int16_t chartData[CHART_N];              // กราฟ: sketch จอง array เอง (ไม่มี malloc)
Massmore_OLED_Chart chart(chartData, CHART_N);

const char *const MENU[] = { "ตั้งค่า WiFi", "ความสว่าง", "Sensor", "Firmware", "เกี่ยวกับ", "รีสตาร์ท" };
const uint8_t MENU_N = sizeof(MENU) / sizeof(MENU[0]);

const int16_t Y0 = MASSMORE_OLED_BY_YELLOW_H;   // เนื้อหาเริ่มใต้โซนเหลือง

// แถบสถานะในโซนเหลือง: ชื่อหน้า + สัญญาณ + แบตเตอรี่
void statusBar(const char *title, uint8_t batt, uint8_t rssiBars) {
  oled.setFont();
  ui.header(nullptr);
  oled.setCursor(0, 4);
  oled.print(title);
  ui.signalBars(oled.width() - 42, 2, rssiBars, 4, 3, 11);
  ui.battery(oled.width() - 24, 3, 24, 10, batt, batt < 100 && (millis() / 500) % 2);
}

void setup() {
  Serial.begin(115200);
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
}

void loop() {
  int16_t W = oled.width(), H = oled.height();

  // ---- 1) progress bar + spinner ----
  for (uint8_t p = 0; p <= 100; p += 2) {
    oled.clear();
    statusBar("Update", p, p / 25);
    ui.progressBar(4, Y0 + 8, W - 8, 12, p, true);
    ui.spinner(W / 2, Y0 + 34, 7, p / 2);
    oled.display();
    delay(30);
  }
  delay(500);

  // ---- 2) gauge + toggles ----
  for (int v = 0; v <= 100; v += 4) {
    oled.clear();
    statusBar("Gauge", 80, 3);
    char lbl[8];
    snprintf(lbl, sizeof(lbl), "%d%%", v);
    ui.gauge(40, H - 10, 32, v, 0, 100, nullptr);
    oled.setCursor(32, H - 8);
    oled.print(lbl);
    oled.setCursor(84, Y0 + 6);  oled.print(F("Fan"));
    ui.toggle(104, Y0 + 5, v > 50);
    oled.setCursor(84, Y0 + 22); oled.print(F("LED"));
    ui.toggle(104, Y0 + 21, (v / 8) % 2);
    oled.display();
    delay(60);
  }
  delay(800);

  // ---- 3) list menu ภาษาไทย ----
  // แถวละ 24 px: Sarabun16 มีพยัญชนะสูง 10 px + สระ/วรรณยุกต์ซ้อนด้านบน
  uint8_t rowH = 24;
  uint8_t rows = (H - Y0) / rowH;
  if (rows == 0) rows = 1;
  for (uint8_t sel = 0; sel < MENU_N; sel++) {
    oled.clear();
    oled.setFont(&Sarabun16);
    ui.listMenu(0, Y0, W, MENU, MENU_N, sel, rows, rowH);
    statusBar("Menu", 60, 4);                       // วาดแถบหัวทีหลัง → ทับวรรณยุกต์ที่ล้นขึ้นไป
    oled.display();
    delay(700);
  }
  oled.setFont();

  // ---- 4) กราฟเส้น (live chart) ----
  for (int i = 0; i < 150; i++) {
    int16_t v = (int16_t)(50 + 40 * sin(i * 0.15) + random(-6, 7));
    chart.push(v);
    oled.clear();
    statusBar("Chart", 40, 2);
    chart.draw(oled, 0, Y0, W, H - Y0);
    oled.setCursor(2, Y0 + 2);
    oled.print(chart.latest());
    oled.display();
    delay(30);
  }
}
