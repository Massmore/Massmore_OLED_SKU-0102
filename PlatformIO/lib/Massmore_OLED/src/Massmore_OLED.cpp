/**
 * @file    Massmore_OLED.cpp
 * @brief   Bus layer, init, page flush, display control, scroll, controller identity
 */
#define MASSMORE_OLED_NO_THAI_FONTS     // ไฟล์นี้ไม่ใช้ฟอนต์ไทย (ลดเวลา compile)
#include "Massmore_OLED.h"

// ความเร็ว HW scroll: index 0 (ช้า) … 7 (เร็ว) → รหัส 3 บิตของแต่ละชิป (ตาราง 26h/27h ของ datasheet)
static const uint8_t SSD1306_SCROLL_SPEED[8] PROGMEM = { 3, 2, 1, 6, 0, 5, 4, 7 };  // 256,128,64,25,5,4,3,2 frames
static const uint8_t SSD1309_SCROLL_SPEED[8] PROGMEM = { 3, 2, 1, 0, 6, 5, 4, 7 };  // 256,128,64,5,4,3,2,1 frames

/* ========================================================================== */
/*  Construction / profile                                                    */
/* ========================================================================== */
Massmore_OLED::Massmore_OLED(Massmore_OLED_Model model)
  : _model(model), _wire(nullptr), _addr(MASSMORE_OLED_ADDR_DEFAULT), _rst(-1),
    _started(false), _sleeping(false), _scrolling(false), _autoDisplay(false), _displayOn(false),
    _flipH(false), _flipV(false),
    _err(MASSMORE_OLED_OK), _identity(MASSMORE_IDENTITY_UNKNOWN),
    _idStatus(-1), _idLoopback(false), _idLock(-1), _idLockStatus(-1), _idDetected(MASSMORE_CTRL_UNKNOWN) {
  memset(&_p, 0, sizeof(_p));
  uint8_t pid = _profileForModel(model);
  _loadProfile(pid == MASSMORE_P_INVALID ? (uint8_t)MASSMORE_P_SSD1306_128X64 : pid);
}

bool Massmore_OLED::setModel(Massmore_OLED_Model model) {
  uint8_t pid = _profileForModel(model);
  if (pid == MASSMORE_P_INVALID) return false;
  _started = false;
  _model = model;
  _loadProfile(pid);
  return true;
}

uint8_t Massmore_OLED::_profileForModel(Massmore_OLED_Model m) const {
  if (m >= MASSMORE_OLED_SKU_0102_1 && m <= MASSMORE_OLED_SKU_0102_12) {
    return pgm_read_byte(&MASSMORE_OLED_SKUS[m - MASSMORE_OLED_SKU_0102_1].profile);
  }
  // model ตามชิป (32…37) เรียงลำดับเดียวกับ profile ID → คำนวณตรง ๆ (ไม่มี jump table ใน RAM ของ AVR)
  if (m >= MASSMORE_OLED_SSD1306_128X32 && m <= MASSMORE_OLED_SSD1309_128X64_242) {
    return (uint8_t)(m - MASSMORE_OLED_SSD1306_128X32);
  }
  if (m == MASSMORE_OLED_AUTO) return MASSMORE_P_SSD1306_128X64;
  return MASSMORE_P_INVALID;
}

bool Massmore_OLED::_loadProfile(uint8_t id) {
  if (id >= MASSMORE_P_COUNT) return false;
  memcpy_P(&_p, &MASSMORE_OLED_PROFILES[id], sizeof(_p));
  if ((uint32_t)_p.width * _p.pages > MASSMORE_OLED_BUFFER_SIZE) {
    _setupBuffer(_buffer, 0, 0);       // ยังไม่ให้วาด — begin() จะคืน ERR_NO_BUFFER
    return false;
  }
  _setupBuffer(_buffer, _p.width, _p.height);
  return true;
}

const char *Massmore_OLED::getModelName() const {
  static char b[18];
  if (_model >= MASSMORE_OLED_SKU_0102_1 && _model <= MASSMORE_OLED_SKU_0102_12) {
    snprintf(b, sizeof(b), "SKU-0102-%u", (unsigned)_model);
  } else {
    strcpy_P(b, (PGM_P)controllerName(getController()));
    size_t n = strlen(b);
    snprintf(b + n, sizeof(b) - n, " %ux%u", (unsigned)_p.width, (unsigned)_p.height);
  }
  return b;
}

const char *Massmore_OLED::getPanelSize() const {
  static char s[6];
  if (_model < MASSMORE_OLED_SKU_0102_1 || _model > MASSMORE_OLED_SKU_0102_12) return "";
  strncpy_P(s, MASSMORE_OLED_SKUS[_model - 1].size, sizeof(s));
  return s;
}

const char *Massmore_OLED::getPanelColour() const {
  static char s[12];
  if (_model < MASSMORE_OLED_SKU_0102_1 || _model > MASSMORE_OLED_SKU_0102_12) return "";
  strncpy_P(s, MASSMORE_OLED_SKUS[_model - 1].colour, sizeof(s));
  return s;
}

/* ========================================================================== */
/*  Bus layer                                                                 */
/* ========================================================================== */
// ส่ง control byte + payload แบ่งเป็นก้อนตามขนาด Wire buffer
Massmore_OLED_Error Massmore_OLED::_send(uint8_t ctrl, const uint8_t *buf, size_t len, bool progmem) {
  if (!_wire) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  while (len) {
    size_t n = len > MASSMORE_OLED_I2C_CHUNK ? MASSMORE_OLED_I2C_CHUNK : len;
    _wire->beginTransmission(_addr);
    _wire->write(ctrl);
    for (size_t i = 0; i < n; i++) _wire->write(progmem ? pgm_read_byte(buf + i) : buf[i]);
    uint8_t r = _wire->endTransmission();
    if (r != 0) return _setErr(r == 2 ? MASSMORE_OLED_ERR_NO_ACK : MASSMORE_OLED_ERR_BUS);
    buf += n;
    len -= n;
  }
  return _setErr(MASSMORE_OLED_OK);
}

Massmore_OLED_Error Massmore_OLED::command(uint8_t c) {
  return _send(MASSMORE_OLED_CTRL_CMD, &c, 1, false);
}

Massmore_OLED_Error Massmore_OLED::command(uint8_t c, uint8_t a) {
  uint8_t b[2] = { c, a };
  return _send(MASSMORE_OLED_CTRL_CMD, b, 2, false);
}

Massmore_OLED_Error Massmore_OLED::commandList(const uint8_t *cmds, uint8_t len) {
  return _send(MASSMORE_OLED_CTRL_CMD, cmds, len, false);
}

Massmore_OLED_Error Massmore_OLED::data(const uint8_t *buf, size_t len) {
  return _send(MASSMORE_OLED_CTRL_DATA, buf, len, false);
}

Massmore_OLED_Error Massmore_OLED::_sendInitSeq(const uint8_t *seq) {
  for (;;) {
    uint8_t n = pgm_read_byte(seq++);
    if (n == 0) return _setErr(MASSMORE_OLED_OK);
    Massmore_OLED_Error e = _send(MASSMORE_OLED_CTRL_CMD, seq, n, true);
    if (e != MASSMORE_OLED_OK) return e;
    seq += n;
  }
}

int16_t Massmore_OLED::readStatus() {
  if (!_wire) return -1;
  _wire->beginTransmission(_addr);
  _wire->write(MASSMORE_OLED_CTRL_CMD);          // D/C# = 0 → การอ่านถัดไป = status
  if (_wire->endTransmission() != 0) return -1;
  if (_wire->requestFrom(_addr, (uint8_t)1) != 1) return -1;
  if (!_wire->available()) return -1;
  return (int16_t)(_wire->read() & 0xFF);
}

bool Massmore_OLED::isConnected() {
  if (!_wire) return false;
  _wire->beginTransmission(_addr);
  return _wire->endTransmission() == 0;
}

void Massmore_OLED::setI2CClock(uint32_t hz) {
  if (_wire) _wire->setClock(hz);
}

/* ========================================================================== */
/*  Start-up                                                                  */
/* ========================================================================== */
bool Massmore_OLED::reset() {
  if (_rst < 0) return false;
  pinMode(_rst, OUTPUT);
  digitalWrite(_rst, HIGH);
  delay(1);
  digitalWrite(_rst, LOW);          // RES# active low (datasheet ≥ 3–10 us — ให้ยาวไว้สำหรับวงจร RC บนโมดูล)
  delay(10);
  digitalWrite(_rst, HIGH);
  delay(10);
  return true;
}

Massmore_OLED_Error Massmore_OLED::begin(TwoWire &wire, uint8_t addr, int8_t rstPin) {
  _wire = &wire;
  _addr = addr;
  _rst = rstPin;
  _started = false;
  _scrolling = false;
  _sleeping = false;
  _flipH = _flipV = false;

  uint8_t pid = _profileForModel(_model);
  if (pid == MASSMORE_P_INVALID) return _setErr(MASSMORE_OLED_ERR_BAD_MODEL);

  reset();
  if (!isConnected()) {
    delay(50);                                   // บางโมดูลต้องรอไฟเลี้ยงนิ่ง
    if (!isConnected()) return _setErr(MASSMORE_OLED_ERR_NO_ACK);
  }

  if (_model == MASSMORE_OLED_AUTO) {
    switch (detectController()) {
      case MASSMORE_CTRL_SH1107:  pid = MASSMORE_P_SH1107_128X128; break;
      case MASSMORE_CTRL_SH1106:  pid = MASSMORE_P_SH1106_128X64;  break;
      case MASSMORE_CTRL_SSD1309: pid = MASSMORE_P_SSD1309_154;    break;
      default:                    pid = MASSMORE_P_SSD1306_128X64; break;  // 128x32 แยกทางไฟฟ้าไม่ได้
    }
  }
  if (!_loadProfile(pid)) return _setErr(MASSMORE_OLED_ERR_NO_BUFFER);

  Massmore_OLED_Error e = _sendInitSeq(_p.initSeq);
  if (e != MASSMORE_OLED_OK) return e;

  _started = true;
  clear();
  e = displayAll();                              // ล้าง GDDRAM ก่อนเปิดจอ → ไม่มีขยะกระพริบ
  if (e != MASSMORE_OLED_OK) { _started = false; return e; }
  e = command(OLED_CMD_DISPLAY_ON);
  if (e != MASSMORE_OLED_OK) { _started = false; return e; }
  _displayOn = true;
  return _setErr(MASSMORE_OLED_OK);
}

void Massmore_OLED::end() {
  if (_started) sleep();
  _started = false;
}

/* ========================================================================== */
/*  Buffer → GDDRAM                                                           */
/* ========================================================================== */
// page addressing ใช้ได้กับทั้ง 4 ชิป: B0h+page, 10h|colHi, colLo แล้วตามด้วย data
Massmore_OLED_Error Massmore_OLED::_sendPage(uint8_t page) {
  uint8_t col = _p.colOffset;
  uint8_t hdr[3] = { (uint8_t)(OLED_CMD_SET_PAGE + page),
                     (uint8_t)(OLED_CMD_SET_HIGH_COLUMN | (col >> 4)),
                     (uint8_t)(OLED_CMD_SET_LOW_COLUMN | (col & 0x0F)) };
  Massmore_OLED_Error e = _send(MASSMORE_OLED_CTRL_CMD, hdr, 3, false);
  if (e != MASSMORE_OLED_OK) return e;
  return _send(MASSMORE_OLED_CTRL_DATA, &_buffer[(uint16_t)page * _p.width], _p.width, false);
}

Massmore_OLED_Error Massmore_OLED::display() {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  finishText();
  for (uint8_t p = 0; p < _p.pages; p++) {
    if (!(_dirty & (1U << p))) continue;
    _dirty &= (uint16_t)~(1U << p);
    Massmore_OLED_Error e = _sendPage(p);
    if (e != MASSMORE_OLED_OK) { _dirty |= (uint16_t)(1U << p); return e; }
  }
  return _setErr(MASSMORE_OLED_OK);
}

Massmore_OLED_Error Massmore_OLED::displayAll() {
  _markAllDirty();
  return display();
}

bool Massmore_OLED::displayStep() {
  if (!_started) return false;
  finishText();
  for (uint8_t p = 0; p < _p.pages; p++) {
    if (!(_dirty & (1U << p))) continue;
    _dirty &= (uint16_t)~(1U << p);
    if (_sendPage(p) != MASSMORE_OLED_OK) _dirty |= (uint16_t)(1U << p);
    break;
  }
  return _dirty != 0;
}

void Massmore_OLED::_onTextWritten() {
  if (_autoDisplay && _started) display();
}

/* ========================================================================== */
/*  Display control                                                           */
/* ========================================================================== */
Massmore_OLED_Error Massmore_OLED::setContrast(uint8_t value) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  return command(OLED_CMD_SET_CONTRAST, value);
}

Massmore_OLED_Error Massmore_OLED::setBrightness(uint8_t percent) {
  if (percent > 100) percent = 100;
  return setContrast((uint8_t)(((uint16_t)percent * 255 + 50) / 100));
}

Massmore_OLED_Error Massmore_OLED::invert(bool on) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  return command(on ? OLED_CMD_INVERT_DISPLAY : OLED_CMD_NORMAL_DISPLAY);
}

Massmore_OLED_Error Massmore_OLED::flipHorizontal(bool on) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  _flipH = on;
  uint8_t seg = on ? (uint8_t)(_p.segRemap ^ 0x01) : _p.segRemap;
  Massmore_OLED_Error e = command(seg);
  if (e != MASSMORE_OLED_OK) return e;
  return displayAll();                 // SEG remap มีผลกับข้อมูลที่เขียนหลังจากนี้ → เขียนใหม่ทั้งจอ
}

Massmore_OLED_Error Massmore_OLED::flipVertical(bool on) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  _flipV = on;
  uint8_t com = on ? (uint8_t)(_p.comScan ^ 0x08) : _p.comScan;
  return command(com);
}

Massmore_OLED_Error Massmore_OLED::sleep() {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  Massmore_OLED_Error e = command(OLED_CMD_DISPLAY_OFF);
  if (e != MASSMORE_OLED_OK) return e;
  if (_p.flags & MASSMORE_F_CHARGE_PUMP)           e = command(OLED_CMD_CHARGE_PUMP, 0x10);
  else if (_p.controller == MASSMORE_CTRL_SH1106)  e = command(OLED_CMD_DCDC_CONTROL, 0x8A);
  _sleeping = true;
  _displayOn = false;
  return e;
}

Massmore_OLED_Error Massmore_OLED::wake() {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  Massmore_OLED_Error e = MASSMORE_OLED_OK;
  if (_p.flags & MASSMORE_F_CHARGE_PUMP)           e = command(OLED_CMD_CHARGE_PUMP, 0x14);
  else if (_p.controller == MASSMORE_CTRL_SH1106)  e = command(OLED_CMD_DCDC_CONTROL, 0x8B);
  if (e != MASSMORE_OLED_OK) return e;
  e = command(OLED_CMD_DISPLAY_ON);
  if (e == MASSMORE_OLED_OK) { _sleeping = false; _displayOn = true; }
  return e;
}

Massmore_OLED_Error Massmore_OLED::allPixelsOn(bool on) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  return command(on ? OLED_CMD_DISPLAY_ALL_ON : OLED_CMD_DISPLAY_RAM);
}

Massmore_OLED_Error Massmore_OLED::setDisplayOffset(uint8_t rows) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  return command(OLED_CMD_SET_DISPLAY_OFFSET, rows & 0x7F);
}

Massmore_OLED_Error Massmore_OLED::setStartLine(uint8_t line) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  if (_p.flags & MASSMORE_F_START_LINE_2BYTE) return command(OLED_CMD_SH1107_START_LINE, line & 0x7F);
  return command((uint8_t)(OLED_CMD_SET_START_LINE | (line & 0x3F)));
}

/* ========================================================================== */
/*  Scroll                                                                    */
/* ========================================================================== */
uint8_t Massmore_OLED::_speedCode(uint8_t speed) const {
  if (speed > 7) speed = 7;
  return pgm_read_byte(_p.controller == MASSMORE_CTRL_SSD1309 ? &SSD1309_SCROLL_SPEED[speed]
                                                               : &SSD1306_SCROLL_SPEED[speed]);
}

Massmore_OLED_Error Massmore_OLED::_hwScroll(uint8_t cmd, uint8_t sp, uint8_t ep, uint8_t speed, uint8_t vOff) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  if (!(_p.flags & MASSMORE_F_HW_SCROLL)) return _setErr(MASSMORE_OLED_ERR_NOT_SUPPORTED);
  uint8_t last = _p.pages - 1;
  if (ep > last) ep = last;
  if (sp > ep) sp = ep;
  finishText();
  command(OLED_CMD_SCROLL_STOP);             // datasheet: ต้องหยุดก่อนตั้งค่าใหม่
  display();                                 // ให้ RAM ตรงกับ buffer ก่อนเริ่มเลื่อน
  bool ssd1309 = (_p.controller == MASSMORE_CTRL_SSD1309);
  bool diag = (cmd == OLED_CMD_SCROLL_DIAG_RIGHT || cmd == OLED_CMD_SCROLL_DIAG_LEFT);
  uint8_t b[8];
  uint8_t n = 0;
  if (diag) {
    uint8_t area[3] = { OLED_CMD_VSCROLL_AREA, 0, _p.height };   // ทั้งจอเลื่อนแนวตั้ง
    Massmore_OLED_Error e = commandList(area, 3);
    if (e != MASSMORE_OLED_OK) return e;
  }
  b[n++] = cmd;
  b[n++] = (diag && ssd1309) ? 0x01 : 0x00;  // SSD1309 29h/2Ah: A[0]=1 → เลื่อนแนวนอนด้วย 1 คอลัมน์
  b[n++] = sp;
  b[n++] = _speedCode(speed);
  b[n++] = ep;
  if (diag) {
    b[n++] = vOff & 0x3F;
    if (ssd1309) { b[n++] = 0x00; b[n++] = 0x7F; }               // start / end column
  } else if (ssd1309) {
    b[n++] = 0x00; b[n++] = 0x00; b[n++] = 0x7F;                 // dummy, start col, end col
  } else {
    b[n++] = 0x00; b[n++] = 0xFF;                                // SSD1306 dummy bytes
  }
  Massmore_OLED_Error e = commandList(b, n);
  if (e != MASSMORE_OLED_OK) return e;
  e = command(OLED_CMD_SCROLL_START);
  if (e == MASSMORE_OLED_OK) _scrolling = true;
  return e;
}

Massmore_OLED_Error Massmore_OLED::scrollRight(uint8_t s, uint8_t e, uint8_t speed) {
  return _hwScroll(OLED_CMD_SCROLL_RIGHT, s, e, speed, 0);
}
Massmore_OLED_Error Massmore_OLED::scrollLeft(uint8_t s, uint8_t e, uint8_t speed) {
  return _hwScroll(OLED_CMD_SCROLL_LEFT, s, e, speed, 0);
}
Massmore_OLED_Error Massmore_OLED::scrollDiagRight(uint8_t s, uint8_t e, uint8_t speed, uint8_t v) {
  return _hwScroll(OLED_CMD_SCROLL_DIAG_RIGHT, s, e, speed, v);
}
Massmore_OLED_Error Massmore_OLED::scrollDiagLeft(uint8_t s, uint8_t e, uint8_t speed, uint8_t v) {
  return _hwScroll(OLED_CMD_SCROLL_DIAG_LEFT, s, e, speed, v);
}

Massmore_OLED_Error Massmore_OLED::scrollStop() {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  if (!(_p.flags & MASSMORE_F_HW_SCROLL)) return _setErr(MASSMORE_OLED_ERR_NOT_SUPPORTED);
  Massmore_OLED_Error e = command(OLED_CMD_SCROLL_STOP);
  if (e != MASSMORE_OLED_OK) return e;
  _scrolling = false;
  return displayAll();                       // RAM ถูกเลื่อนไปแล้ว → เขียน buffer กลับ
}

Massmore_OLED_Error Massmore_OLED::scrollContent(int8_t dir, uint8_t sp, uint8_t ep) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  if (!(_p.flags & MASSMORE_F_CONTENT_SCROLL)) return _setErr(MASSMORE_OLED_ERR_NOT_SUPPORTED);
  uint8_t last = _p.pages - 1;
  if (ep > last) ep = last;
  if (sp > ep) sp = ep;
  uint8_t b[8] = { (uint8_t)(dir < 0 ? OLED_CMD_CONTENT_SCROLL_L : OLED_CMD_CONTENT_SCROLL_R),
                   0x00, sp, 0x01, ep, 0x00, 0x00, 0x7F };
  return commandList(b, sizeof(b));
}

Massmore_OLED_Error Massmore_OLED::setVerticalScrollArea(uint8_t top, uint8_t rows) {
  if (!_started) return _setErr(MASSMORE_OLED_ERR_NOT_STARTED);
  if (!(_p.flags & MASSMORE_F_HW_SCROLL)) return _setErr(MASSMORE_OLED_ERR_NOT_SUPPORTED);
  if (top + rows > _p.height) rows = _p.height - top;
  uint8_t b[3] = { OLED_CMD_VSCROLL_AREA, (uint8_t)(top & 0x3F), (uint8_t)(rows & 0x7F) };
  return commandList(b, 3);
}

/* ========================================================================== */
/*  Controller Identity (§8)                                                  */
/* ========================================================================== */
// ON/OFF loop-back: AFh → bit6 = 0, AEh → bit6 = 1 (ทุกชิปนิยาม D6 เหมือนกัน)
bool Massmore_OLED::_loopback(int16_t *sOn, int16_t *sOff) {
  command(OLED_CMD_DISPLAY_ON);
  delay(2);
  *sOn = readStatus();
  command(OLED_CMD_DISPLAY_OFF);
  delay(2);
  *sOff = readStatus();
  return *sOn >= 0 && *sOff >= 0 && !(*sOn & OLED_STATUS_DISPLAY_OFF) && (*sOff & OLED_STATUS_DISPLAY_OFF);
}

// Command lock (FDh 16h) — มีใน SSD1309 และ SSD1315 (ไม่มีใน SSD1306 Rev 1.1)
// ถ้าล็อกได้ AEh จะถูกเมิน → จอยังเปิด (bit6 = 0) · เก็บ status ขณะล็อกไว้ใน _idLockStatus
// เรียกเฉพาะเมื่อ loop-back ผ่านแล้ว · ใช้กับทุกชิป: SH110x ไม่รู้จัก FDh (16h/12h = ตั้ง column ไม่มีผลเสีย)
int8_t Massmore_OLED::_lockTest() {
  command(OLED_CMD_DISPLAY_ON);
  delay(2);
  int16_t s1 = readStatus();
  command(OLED_CMD_COMMAND_LOCK, 0x16);       // SSD1306: FDh ไม่รู้จัก, 16h = ตั้ง column (ไม่มีผลเสีย)
  command(OLED_CMD_DISPLAY_OFF);
  delay(2);
  int16_t s2 = readStatus();
  command(OLED_CMD_COMMAND_LOCK, 0x12);       // unlock เสมอ
  _idLockStatus = s2;
  if (s1 < 0 || s2 < 0 || (s1 & OLED_STATUS_DISPLAY_OFF)) return -1;
  return (s2 & OLED_STATUS_DISPLAY_OFF) ? 0 : 1;
}

Massmore_OLED_Controller Massmore_OLED::detectController() {
  _idLock = -1;
  _idLockStatus = -1;
  _idStatus = readStatus();
  int16_t sOn, sOff;
  _idLoopback = (_idStatus >= 0) && _loopback(&sOn, &sOff);
  Massmore_OLED_Controller c = MASSMORE_CTRL_SSD1306;      // อ่าน status ไม่ได้ → SSD1306 (พบบ่อยสุด)
  if (_idLoopback) {
    _idStatus = sOff;
    // ทดสอบ lock ก่อนเสมอ: SSD1315 บางล็อตให้ status = 07h ซ้ำกับ ID ของ SH1107 (วัดจริงบน 0.96")
    // FDh ไม่มีในตาราง SH110x แต่ไม่มีผลเสีย (16h/12h = ตั้ง column) และ begin() init ใหม่ทั้งหมดหลังตรวจ
    _idLock = _lockTest();
    // ล็อกได้ = SSD1309 หรือ SSD1315 — แยกกันไม่ได้ (D7 / บิตล่างของ status ไม่ต่างกันอย่างเชื่อถือได้)
    if (_idLock == 1)                                                    c = MASSMORE_CTRL_SSD1309_1315;
    else if ((sOff & OLED_SH1107_ID_MASK) == OLED_SH1107_ID_VALUE)       c = MASSMORE_CTRL_SH1107;
    else if ((sOff & OLED_SH1106_ID_MASK) == OLED_SH1106_ID_VALUE)       c = MASSMORE_CTRL_SH1106;
  }
  _idDetected = c;
  command(OLED_CMD_DISPLAY_OFF);
  if (_started && _displayOn && !_sleeping) command(OLED_CMD_DISPLAY_ON);
  return c;
}

int16_t Massmore_OLED::readChipID() {
  int16_t s = readStatus();
  return s < 0 ? -1 : (int16_t)(s & OLED_SH1107_ID_MASK);
}

Massmore_OLED_Identity Massmore_OLED::verifyController() {
  _idLock = -1;
  _idLockStatus = -1;
  _idLoopback = false;
  _idDetected = MASSMORE_CTRL_UNKNOWN;
  _idStatus = readStatus();
  if (_idStatus < 0) return _identity = (isConnected() ? MASSMORE_IDENTITY_CONSISTENT : MASSMORE_IDENTITY_UNKNOWN);

  int16_t sOn, sOff;
  _idLoopback = _loopback(&sOn, &sOff);
  if (_idLoopback) _idStatus = sOff;
  Massmore_OLED_Identity r = MASSMORE_IDENTITY_CONSISTENT;
  if (_idLoopback) {
    uint8_t s = (uint8_t)sOff;
    bool looksSH1107 = (s & OLED_SH1107_ID_MASK) == OLED_SH1107_ID_VALUE;
    _idLock = _lockTest();                       // ล็อกได้ = SSD1315 / SSD1309 (ไม่ใช่ SH110x)
    bool locked = (_idLock == 1);
    Massmore_OLED_Controller lockedChip = locked ? MASSMORE_CTRL_SSD1309_1315 : MASSMORE_CTRL_UNKNOWN;
    switch (_p.controller) {
      case MASSMORE_CTRL_SH1107:
        if (locked)                                                  _idDetected = lockedChip;
        else if (looksSH1107)                                        _idDetected = MASSMORE_CTRL_SH1107;
        else if ((s & OLED_SH1106_ID_MASK) == OLED_SH1106_ID_VALUE) _idDetected = MASSMORE_CTRL_SH1106;
        r = (_idDetected == MASSMORE_CTRL_SH1107) ? MASSMORE_IDENTITY_VERIFIED : MASSMORE_IDENTITY_MISMATCH;
        break;
      case MASSMORE_CTRL_SH1106:
        if (locked)                                                  _idDetected = lockedChip;
        else if (looksSH1107)                                        _idDetected = MASSMORE_CTRL_SH1107;
        else if ((s & OLED_SH1106_ID_MASK) == OLED_SH1106_ID_VALUE) _idDetected = MASSMORE_CTRL_SH1106;
        r = (_idDetected == MASSMORE_CTRL_SH1106) ? MASSMORE_IDENTITY_VERIFIED : MASSMORE_IDENTITY_MISMATCH;
        break;
      case MASSMORE_CTRL_SSD1309:
        // ล็อกได้ = ผ่าน (แยก SSD1309 / SSD1315 ทางไฟฟ้าไม่ได้ — ขนาดจอยืนยันแทน)
        if (locked) _idDetected = MASSMORE_CTRL_SSD1309;
        r = locked ? MASSMORE_IDENTITY_VERIFIED : MASSMORE_IDENTITY_MISMATCH;
        break;
      default:   // SSD1306 family: SSD1306 (ล็อกไม่ได้) หรือ SSD1315 (ล็อกได้ + D7) — SSD1309 / SH1106 = ไม่ผ่าน
        if (locked) {                            // SKU ตระกูล SSD1306 + ล็อกได้ → SSD1315 (ยอมรับ)
          _idDetected = MASSMORE_CTRL_SSD1315;
          r = MASSMORE_IDENTITY_VERIFIED;
        } else if ((s & 0x0F) == 0x00 || (s & 0x0F) == 0x08) {
          _idDetected = MASSMORE_CTRL_SH1106;
          r = MASSMORE_IDENTITY_MISMATCH;
        } else {
          _idDetected = MASSMORE_CTRL_SSD1306;
          r = MASSMORE_IDENTITY_VERIFIED;
        }
        break;
    }
  }
  // คืนสถานะจอเดิม
  if (_displayOn && !_sleeping) command(OLED_CMD_DISPLAY_ON);
  else                          command(OLED_CMD_DISPLAY_OFF);
  return _identity = r;
}

void Massmore_OLED::getIdentityReport(Print &out) {
  out.print(F("Controller (selected) : "));
  out.println(getControllerName());
  out.print(F("Status byte           : "));
  if (_idStatus < 0) out.println(F("unreadable"));
  else { out.print(F("0x")); if (_idStatus < 0x10) out.print('0'); out.println(_idStatus, HEX); }
  out.print(F("ON/OFF loop-back      : "));
  out.println(_idLoopback ? F("OK") : F("not available"));
  if (_idStatus >= 0) {
    out.print(F("ID bits (status&0x3F) : 0x"));
    uint8_t id = (uint8_t)(_idStatus & 0x3F);
    if (id < 0x10) out.print('0');
    out.print(id, HEX);
    out.println(id == OLED_SH1107_ID_VALUE ? F(" (SH1107 Read ID)") : F(""));
  }
  out.print(F("Command lock (FDh)    : "));
  if (_idLock < 0) out.println(F("not tested"));
  else if (!_idLock) out.println(F("no effect"));
  else {
    out.print(F("holds (status 0x"));
    if (_idLockStatus < 0x10) out.print('0');
    out.print(_idLockStatus, HEX);
    out.println(')');
  }
  out.print(F("Detected controller   : "));
  out.print(controllerName(_idDetected));
  out.println(_idDetected == MASSMORE_CTRL_SSD1315 ? F(" (SSD1306-compatible)") : F(""));
  // หมายเหตุ: SSD1309 กับ SSD1315 ตอบ command lock เหมือนกัน — ชื่อที่รายงานอิงจาก SKU ที่เลือก
  out.print(F("Controller Identity   : "));
  out.print(identityString(_identity));
  out.println(_identity == MASSMORE_IDENTITY_VERIFIED ? F(" by Massmore") : F(""));
}

/* ========================================================================== */
/*  Strings                                                                   */
/* ========================================================================== */
Massmore_Str Massmore_OLED::errorString(Massmore_OLED_Error e) {
  switch (e) {
    case MASSMORE_OLED_OK:                return MASSMORE_STR("OK");
    case MASSMORE_OLED_ERR_NO_ACK:        return MASSMORE_STR("No ACK (check wiring / address / power)");
    case MASSMORE_OLED_ERR_BUS:           return MASSMORE_STR("I2C bus error");
    case MASSMORE_OLED_ERR_NO_BUFFER:     return MASSMORE_STR("Buffer too small for this panel");
    case MASSMORE_OLED_ERR_BAD_MODEL:     return MASSMORE_STR("Unknown model");
    case MASSMORE_OLED_ERR_NOT_SUPPORTED: return MASSMORE_STR("Not supported by this controller");
    case MASSMORE_OLED_ERR_ID_MISMATCH:   return MASSMORE_STR("Controller ID mismatch");
    case MASSMORE_OLED_ERR_NOT_STARTED:   return MASSMORE_STR("begin() not called");
    default:                              return MASSMORE_STR("Unknown error");
  }
}

Massmore_Str Massmore_OLED::controllerName(Massmore_OLED_Controller c) {
  switch (c) {
    case MASSMORE_CTRL_SSD1306: return MASSMORE_STR("SSD1306");
    case MASSMORE_CTRL_SSD1309: return MASSMORE_STR("SSD1309");
    case MASSMORE_CTRL_SH1106:  return MASSMORE_STR("SH1106");
    case MASSMORE_CTRL_SH1107:  return MASSMORE_STR("SH1107");
    case MASSMORE_CTRL_SSD1315: return MASSMORE_STR("SSD1315");
    case MASSMORE_CTRL_SSD1309_1315: return MASSMORE_STR("SSD1309/SSD1315");
    default:                    return MASSMORE_STR("UNKNOWN");
  }
}

Massmore_Str Massmore_OLED::identityString(Massmore_OLED_Identity id) {
  switch (id) {
    case MASSMORE_IDENTITY_VERIFIED:   return MASSMORE_STR("VERIFIED");
    case MASSMORE_IDENTITY_CONSISTENT: return MASSMORE_STR("CONSISTENT");
    case MASSMORE_IDENTITY_MISMATCH:   return MASSMORE_STR("MISMATCH");
    default:                           return MASSMORE_STR("UNKNOWN");
  }
}
