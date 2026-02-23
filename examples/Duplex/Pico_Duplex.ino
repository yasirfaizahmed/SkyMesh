#include <LoRa.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display width and height
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Declaration for SSD1306 display connected using I2C
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

// Raspberry Pi Pico pin mapping
// LoRa RA-02 via SPI0:
// SPI0 SCK  -> GP18
// SPI0 MOSI -> GP19
// SPI0 MISO -> GP16
#define SS   17   // LoRa NSS/CS
#define RST  20   // LoRa RST
#define DIO0 21   // LoRa DIO0

// OLED pinout (I2C0 on Pico)
#define OLED_SDA 4   // GP4
#define OLED_SCL 5   // GP5

// 4x4 Matrix Keypad pinout (Pico GPIO)
// Button numbering (top-left = 1):
//  1  2  3  4
//  5  6  7  8
//  9 10 11 12
// 13 14 15 16
// Key chars used here (layout rotated 90° clockwise):
//  * 7 4 1
//  0 8 5 2
//  # 9 6 3
//  D C B A
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;
byte rowPins[KEYPAD_ROWS] = {0, 1, 2, 3};
byte colPins[KEYPAD_COLS] = {6, 7, 8, 9};

// Easy-to-edit keypad layout by physical button number (1..16):
//  1  2  3  4
//  5  6  7  8
//  9 10 11 12
// 13 14 15 16
// Change only this array to remap any button quickly.
const uint8_t KEYPAD_BUTTON_COUNT = 16;
char keypadLayoutByButton[KEYPAD_BUTTON_COUNT] = {
  '*', '7', '4', '1',
  '0', '8', '5', '2',
  '#', '9', '6', '3',
  'D', 'C', 'B', 'A'
};

// Nokia-style multi-tap sets per physical button (same 1..16 order as above).
// You can fully customize letters/symbols per button here.
// Examples:
//  - button 2 uses "abc2"  (press repeatedly: a->b->c->2)
//  - button 7 uses "pqrs7" (includes 4 letters)
const char* keypadMultiTapByButton[KEYPAD_BUTTON_COUNT] = {
  // Rotated 90° clockwise from canonical 4x4 mapping
  // 1,2,3,A
  "*",     "pqrs7", "ghi4", ".,!?1",
  // 4,5,6,B
  " 0",    "tuv8",  "jkl5", "abc2",
  // 7,8,9,C
  "#",     "wxyz9", "mno6", "def3",
  // *,0,#,D
  "",      "",      "",     ""
};

char keymap[KEYPAD_ROWS][KEYPAD_COLS];

// Modes
#define BEACON "BEACON"

// One-to-one node addressing
const int DEFAULT_MY_NODE_ID = 1;
const int NODE_ID_MIN = 1;
const int NODE_ID_MAX = 9;

// Menu
String menu[] = {"Save", "Saved", "SelfID", "Target", "SEND"};
const int menu_size = sizeof(menu) / sizeof(menu[0]);

// Character table
const String alphanum[] = {
  " ", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
  "k", "l", "m", "n", "o", "p", "q", "r", "s", "t",
  "u", "v", "w", "x", "y", "z",
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};
const int alphanum_size = sizeof(alphanum) / sizeof(alphanum[0]);

// UI States
enum UiState {
  UI_HOME,
  UI_MENU,
  UI_BEACON,
  UI_SAVED,
  UI_TARGET,
  UI_SELF_ID
};

Adafruit_SSD1306 OLED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// App state
String payload = "";
String current_mode = BEACON;
int menu_index = 0;
int myNodeId = DEFAULT_MY_NODE_ID;
int targetNodeId = 2;
UiState uiState = UI_HOME;
bool loraReady = false;

// Multi-tap (2010 phone style) text input state
char lastMultiTapKey = '\0';
int multiTapIndex = 0;
bool multiTapActive = false;
unsigned long multiTapLastPressMs = 0;
const unsigned long multiTapTimeoutMs = 700;
String composePreview = "_";

// Beacon runtime (non-blocking)
const unsigned long beaconIntervalMs = 2000;
const unsigned long beaconAnimStepMs = 250;
unsigned long lastBeaconTxMs = 0;
unsigned long lastBeaconAnimMs = 0;
int beaconDotCount = 0;

// EEPROM saved message settings
const uint16_t EEPROM_SIZE = 256;
const uint8_t EEPROM_MAGIC = 0x42;
const uint8_t MAX_SAVED_MESSAGES = 5;
const uint8_t MAX_MESSAGE_LEN = 24;
const uint8_t SLOT_SIZE = MAX_MESSAGE_LEN + 1;

const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_COUNT = 1;
const int EEPROM_ADDR_HEAD = 2;
const int EEPROM_ADDR_DATA = 3;
const int EEPROM_ADDR_SELF_ID = EEPROM_ADDR_DATA + (MAX_SAVED_MESSAGES * SLOT_SIZE);

int savedCount = 0;
int savedHead = -1;
int savedBrowseIndex = 0; // 0 = latest message

// Status / UX extras
bool uiDirty = true;
String toastMessage = "";
unsigned long toastUntilMs = 0;
unsigned long rxIndicatorUntilMs = 0;

// RX marquee state
String lastRxMessage = "";
int lastRxFrom = 0;
bool rxScrollActive = false;
int rxScrollX = 0;
uint16_t rxTextWidth = 0;
unsigned long rxNextStepMs = 0;
unsigned long rxPauseUntilMs = 0;
const unsigned long rxScrollStepMs = 70;
const unsigned long rxScrollPauseMs = 700;
const unsigned long keypadDebounceMs = 25;
const unsigned long aLongPressMs = 800;

// Key mapping adjusters (for keypad modules where line groups are swapped/reversed)
// Your wiring uses: L1..L4 -> GP0..GP3, R1..R4 -> GP6..GP9
// For your current keypad board, L/R groups behave swapped logically.
// If you mount keypad with pins on TOP edge, rotate logical keys by 90.
const bool KEYPAD_SWAP_LR_GROUPS = true;
const bool KEYPAD_REVERSE_L_ORDER = false;
const bool KEYPAD_REVERSE_R_ORDER = false;
const int KEYPAD_ROTATION_DEG = 270;  // 90° anti-clockwise
const bool KEYPAD_MIRROR_HORIZONTAL = true; // flip left-to-right
const bool KEYPAD_MIRROR_VERTICAL = false;

// Custom keypad scan state
char lastRawMatrixKey = '\0';
char lastStableMatrixKey = '\0';
bool matrixKeyHeld = false;
unsigned long matrixLastChangeMs = 0;

// A-key press handling (short: delete last char, long: clear payload)
bool aPressTracking = false;
bool aLongPressHandled = false;
unsigned long aPressStartMs = 0;

enum APressMode {
  A_PRESS_NONE,
  A_PRESS_HOME_EDIT,
  A_PRESS_SAVED_SELECT
};
APressMode aPressMode = A_PRESS_NONE;


bool initilialize_OLED() {
  Wire.setSDA(OLED_SDA);
  Wire.setSCL(OLED_SCL);
  Wire.begin();

  if (!OLED.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  OLED.clearDisplay();
  OLED.setTextColor(WHITE);
  OLED.setTextSize(1);
  return true;
}


char layoutCharForButton(uint8_t buttonOneBased) {
  if (buttonOneBased < 1 || buttonOneBased > KEYPAD_BUTTON_COUNT) {
    return '\0';
  }
  return keypadLayoutByButton[buttonOneBased - 1];
}


void rebuildKeymapFromLayout() {
  for (uint8_t r = 0; r < KEYPAD_ROWS; r++) {
    for (uint8_t c = 0; c < KEYPAD_COLS; c++) {
      uint8_t buttonNum = (r * KEYPAD_COLS) + c + 1; // 1..16
      keymap[r][c] = layoutCharForButton(buttonNum);
    }
  }
}


void initMatrixKeypad() {
  // Rows are actively driven; keep idle HIGH.
  for (int r = 0; r < KEYPAD_ROWS; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], HIGH);
  }

  // Columns are read with pull-ups.
  for (int c = 0; c < KEYPAD_COLS; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }
}


char mapKeyFromScan(int lIndex, int rIndex) {
  int row = lIndex;
  int col = rIndex;

  // Optionally reverse physical line orders.
  if (KEYPAD_REVERSE_L_ORDER) row = (KEYPAD_ROWS - 1) - row;
  if (KEYPAD_REVERSE_R_ORDER) col = (KEYPAD_COLS - 1) - col;

  // If L/R groups are logically swapped on a module, transpose lookup.
  if (KEYPAD_SWAP_LR_GROUPS) {
    int t = row;
    row = col;
    col = t;
  }

  // Apply visual keypad orientation.
  int mappedRow = row;
  int mappedCol = col;

  if (KEYPAD_ROTATION_DEG == 90) {
    mappedRow = col;
    mappedCol = (KEYPAD_COLS - 1) - row;
  } else if (KEYPAD_ROTATION_DEG == 180) {
    mappedRow = (KEYPAD_ROWS - 1) - row;
    mappedCol = (KEYPAD_COLS - 1) - col;
  } else if (KEYPAD_ROTATION_DEG == 270) {
    mappedRow = (KEYPAD_ROWS - 1) - col;
    mappedCol = row;
  }

  if (KEYPAD_MIRROR_HORIZONTAL) {
    mappedCol = (KEYPAD_COLS - 1) - mappedCol;
  }
  if (KEYPAD_MIRROR_VERTICAL) {
    mappedRow = (KEYPAD_ROWS - 1) - mappedRow;
  }

  return keymap[mappedRow][mappedCol];
}


char readRawMatrixKey() {
  for (int r = 0; r < KEYPAD_ROWS; r++) {
    // Set all rows HIGH, then pull one LOW to scan it.
    for (int i = 0; i < KEYPAD_ROWS; i++) {
      digitalWrite(rowPins[i], HIGH);
    }
    digitalWrite(rowPins[r], LOW);
    delayMicroseconds(30);

    for (int c = 0; c < KEYPAD_COLS; c++) {
      if (digitalRead(colPins[c]) == LOW) {
        // Restore idle state before returning.
        digitalWrite(rowPins[r], HIGH);
        return mapKeyFromScan(r, c);
      }
    }

    digitalWrite(rowPins[r], HIGH);
  }

  return '\0';
}


char getMatrixKeyDebounced() {
  unsigned long now = millis();
  char raw = readRawMatrixKey();

  if (raw != lastRawMatrixKey) {
    lastRawMatrixKey = raw;
    matrixLastChangeMs = now;
  }

  if (now - matrixLastChangeMs < keypadDebounceMs) {
    return '\0';
  }

  if (lastStableMatrixKey != lastRawMatrixKey) {
    lastStableMatrixKey = lastRawMatrixKey;

    // Key released: re-arm for next press.
    if (lastStableMatrixKey == '\0') {
      matrixKeyHeld = false;
      return '\0';
    }

    // New stable press: emit once.
    if (!matrixKeyHeld) {
      matrixKeyHeld = true;
      return lastStableMatrixKey;
    }
  }

  return '\0';
}


void powerOnAnimation() {
  const String brand = "SkyMesh";
  const String subtitle = "Pico Duplex";
  int16_t x1, y1;
  uint16_t w, h;

  // Phase 1: vertical logo slide-in
  OLED.setTextColor(WHITE);
  OLED.setTextSize(2);
  OLED.getTextBounds(brand, 0, 0, &x1, &y1, &w, &h);
  int logoX = (SCREEN_WIDTH - w) / 2;
  int logoTargetY = 12;

  for (int y = SCREEN_HEIGHT + 20; y >= logoTargetY; y -= 2) {
    OLED.clearDisplay();
    OLED.setCursor(logoX, y);
    OLED.print(brand);
    OLED.display();
    delay(12);
  }

  // Phase 2: subtitle fade-in effect (line by line reveal)
  OLED.setTextSize(1);
  OLED.getTextBounds(subtitle, 0, 0, &x1, &y1, &w, &h);
  int subX = (SCREEN_WIDTH - w) / 2;
  int subY = 36;

  for (int i = 1; i <= (int)subtitle.length(); i++) {
    OLED.clearDisplay();
    OLED.setTextSize(2);
    OLED.setCursor(logoX, logoTargetY);
    OLED.print(brand);
    OLED.setTextSize(1);
    OLED.setCursor(subX, subY);
    OLED.print(subtitle.substring(0, i));
    OLED.display();
    delay(45);
  }

  // Phase 3: loading bar fill
  int barX = 14;
  int barY = 52;
  int barW = SCREEN_WIDTH - 28;
  int barH = 8;

  for (int fill = 0; fill <= barW - 2; fill += 3) {
    OLED.clearDisplay();
    OLED.setTextSize(2);
    OLED.setCursor(logoX, logoTargetY);
    OLED.print(brand);
    OLED.setTextSize(1);
    OLED.setCursor(subX, subY);
    OLED.print(subtitle);

    OLED.drawRoundRect(barX, barY, barW, barH, 2, WHITE);
    OLED.fillRect(barX + 1, barY + 1, fill, barH - 2, WHITE);
    OLED.display();
    delay(20);
  }

  delay(350);
  OLED.clearDisplay();
  OLED.display();
}


int wrapIndex(int value, int maxExclusive) {
  if (value < 0) return maxExclusive - 1;
  if (value >= maxExclusive) return 0;
  return value;
}


String trimForWidth(const String &text, int maxChars) {
  if ((int)text.length() <= maxChars) return text;
  return "..." + text.substring(text.length() - (maxChars - 3));
}


String trimHeadForWidth(const String &text, int maxChars) {
  if ((int)text.length() <= maxChars) return text;
  return text.substring(0, maxChars - 3) + "...";
}


String getTailWindow(const String &text, int maxChars) {
  if ((int)text.length() <= maxChars) return text;
  return text.substring(text.length() - maxChars);
}


int getSlotAddress(int slot) {
  return EEPROM_ADDR_DATA + (slot * SLOT_SIZE);
}


void loadSavedMetadata() {
  savedCount = EEPROM.read(EEPROM_ADDR_COUNT);
  int rawHead = EEPROM.read(EEPROM_ADDR_HEAD);

  if (savedCount < 0 || savedCount > MAX_SAVED_MESSAGES) {
    savedCount = 0;
  }

  if (rawHead == 255) {
    savedHead = -1;
  } else if (rawHead >= 0 && rawHead < MAX_SAVED_MESSAGES) {
    savedHead = rawHead;
  } else {
    savedHead = -1;
  }

  if (savedCount == 0) {
    savedHead = -1;
  }
}


void sanitizeNodeIds() {
  if (myNodeId < NODE_ID_MIN || myNodeId > NODE_ID_MAX) {
    myNodeId = DEFAULT_MY_NODE_ID;
  }

  if (targetNodeId < NODE_ID_MIN || targetNodeId > NODE_ID_MAX || targetNodeId == myNodeId) {
    targetNodeId = (myNodeId < NODE_ID_MAX) ? myNodeId + 1 : NODE_ID_MIN;
    if (targetNodeId == myNodeId) {
      targetNodeId = (myNodeId == NODE_ID_MIN) ? NODE_ID_MIN + 1 : NODE_ID_MIN;
    }
  }
}


void loadMyNodeIdFromEEPROM() {
  int stored = EEPROM.read(EEPROM_ADDR_SELF_ID);
  if (stored >= NODE_ID_MIN && stored <= NODE_ID_MAX) {
    myNodeId = stored;
  } else {
    myNodeId = DEFAULT_MY_NODE_ID;
  }
  sanitizeNodeIds();
}


void saveMyNodeIdToEEPROM() {
  EEPROM.write(EEPROM_ADDR_SELF_ID, myNodeId);
  EEPROM.commit();
}


void clearMessageSlot(int slot) {
  int addr = getSlotAddress(slot);
  for (int i = 0; i < SLOT_SIZE; i++) {
    EEPROM.write(addr + i, 0);
  }
}


void writeMessageSlot(int slot, const String &message) {
  int addr = getSlotAddress(slot);
  String msg = message;
  if ((int)msg.length() > MAX_MESSAGE_LEN) {
    msg = msg.substring(0, MAX_MESSAGE_LEN);
  }

  for (int i = 0; i < SLOT_SIZE; i++) {
    if (i < (int)msg.length()) {
      EEPROM.write(addr + i, msg[i]);
    } else {
      EEPROM.write(addr + i, 0);
    }
  }
}


String readMessageSlot(int slot) {
  int addr = getSlotAddress(slot);
  char buf[SLOT_SIZE];

  for (int i = 0; i < SLOT_SIZE; i++) {
    uint8_t b = EEPROM.read(addr + i);
    if (b == 0 || b == 0xFF) {
      buf[i] = '\0';
      break;
    }

    buf[i] = (char)b;

    if (i == SLOT_SIZE - 1) {
      buf[i] = '\0';
    }
  }

  return String(buf);
}


void initSavedStorage() {
  EEPROM.begin(EEPROM_SIZE);

  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.write(EEPROM_ADDR_COUNT, 0);
    EEPROM.write(EEPROM_ADDR_HEAD, 255);
    EEPROM.write(EEPROM_ADDR_SELF_ID, DEFAULT_MY_NODE_ID);

    for (int i = 0; i < MAX_SAVED_MESSAGES; i++) {
      clearMessageSlot(i);
    }

    EEPROM.commit();
  }

  loadSavedMetadata();
  loadMyNodeIdFromEEPROM();
}


bool savePayloadToEEPROM() {
  if (payload.length() == 0) {
    return false;
  }

  int newHead = (savedHead + 1 + MAX_SAVED_MESSAGES) % MAX_SAVED_MESSAGES;
  writeMessageSlot(newHead, payload);

  savedHead = newHead;
  if (savedCount < MAX_SAVED_MESSAGES) {
    savedCount++;
  }

  EEPROM.write(EEPROM_ADDR_COUNT, savedCount);
  EEPROM.write(EEPROM_ADDR_HEAD, savedHead);
  EEPROM.commit();
  return true;
}


bool deleteSavedMessageByDisplayIndex(int displayIndex) {
  if (savedCount <= 0 || displayIndex < 0 || displayIndex >= savedCount) {
    return false;
  }

  String kept[MAX_SAVED_MESSAGES];
  int keepCount = 0;

  for (int i = 0; i < savedCount; i++) {
    if (i == displayIndex) continue;
    kept[keepCount++] = getSavedMessageByIndex(i);
  }

  // Clear all slots first.
  for (int s = 0; s < MAX_SAVED_MESSAGES; s++) {
    clearMessageSlot(s);
  }

  if (keepCount == 0) {
    savedCount = 0;
    savedHead = -1;
    EEPROM.write(EEPROM_ADDR_COUNT, 0);
    EEPROM.write(EEPROM_ADDR_HEAD, 255);
    EEPROM.commit();
    return true;
  }

  // Rebuild as linear oldest->latest in slots [0..keepCount-1]
  // while kept[] is latest->oldest.
  for (int slot = 0; slot < keepCount; slot++) {
    int srcIndex = (keepCount - 1) - slot;
    writeMessageSlot(slot, kept[srcIndex]);
  }

  savedCount = keepCount;
  savedHead = keepCount - 1;
  EEPROM.write(EEPROM_ADDR_COUNT, savedCount);
  EEPROM.write(EEPROM_ADDR_HEAD, savedHead);
  EEPROM.commit();
  return true;
}


int getSlotByDisplayIndex(int displayIndex) {
  if (savedCount == 0 || displayIndex < 0 || displayIndex >= savedCount || savedHead < 0) {
    return -1;
  }

  int slot = savedHead - displayIndex;
  if (slot < 0) slot += MAX_SAVED_MESSAGES;
  return slot;
}


String getSavedMessageByIndex(int displayIndex) {
  int slot = getSlotByDisplayIndex(displayIndex);
  if (slot < 0) return "";
  return readMessageSlot(slot);
}


void showToast(const String &msg, unsigned long durationMs = 900) {
  toastMessage = msg;
  toastUntilMs = millis() + durationMs;
  uiDirty = true;
}


void drawHeader() {
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);
  OLED.setCursor(0, 0);

  String rxFlag = (millis() < rxIndicatorUntilMs) ? "RX*" : "RX-";
  OLED.print("N" + String(myNodeId) + "->" + String(targetNodeId));
  OLED.setCursor(86, 0);
  OLED.print(rxFlag);

  OLED.drawLine(0, 10, SCREEN_WIDTH - 1, 10, WHITE);
}


void drawFooterHints(const String &hint) {
  OLED.drawLine(0, 54, SCREEN_WIDTH - 1, 54, WHITE);
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);
  OLED.setCursor(0, 56);
  OLED.print(hint);
}


void resetRxMarquee(const String &msg, int fromNode) {
  lastRxMessage = msg;
  lastRxFrom = fromNode;

  int16_t x1, y1;
  uint16_t h;
  OLED.setTextSize(1);
  OLED.getTextBounds(lastRxMessage, 0, 0, &x1, &y1, &rxTextWidth, &h);

  int marqueeWidth = SCREEN_WIDTH - 24;
  if ((int)rxTextWidth <= marqueeWidth) {
    rxScrollActive = false;
    rxScrollX = 0;
  } else {
    rxScrollActive = true;
    rxScrollX = marqueeWidth;
    rxNextStepMs = millis() + rxScrollStepMs;
    rxPauseUntilMs = 0;
  }

  uiDirty = true;
}


void tickRxMarquee() {
  if (!rxScrollActive) return;

  unsigned long now = millis();
  int marqueeWidth = SCREEN_WIDTH - 24;

  if (rxPauseUntilMs != 0) {
    if (now < rxPauseUntilMs) return;
    rxPauseUntilMs = 0;
    rxScrollX = marqueeWidth;
    rxNextStepMs = now + rxScrollStepMs;
    uiDirty = true;
    return;
  }

  if (now < rxNextStepMs) return;

  rxNextStepMs = now + rxScrollStepMs;
  rxScrollX -= 1;
  uiDirty = true;

  if (rxScrollX < -((int)rxTextWidth)) {
    rxPauseUntilMs = now + rxScrollPauseMs;
  }
}


void drawRxMarqueeArea() {
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);

  OLED.setCursor(0, 40);
  if (lastRxFrom > 0) {
    OLED.print("RX" + String(lastRxFrom) + ":");
  } else {
    OLED.print("RX:");
  }

  if (lastRxMessage.length() == 0) {
    OLED.setCursor(24, 40);
    OLED.print("-");
    return;
  }

  if (!rxScrollActive) {
    OLED.setCursor(24, 40);
    OLED.print(trimForWidth(lastRxMessage, 17));
  } else {
    OLED.setCursor(24 + rxScrollX, 40);
    OLED.print(lastRxMessage);
  }
}


void drawHomeScreen() {
  drawHeader();

  // Payload text (2 rows). Show active multi-tap preview inline at insertion point,
  // like classic keypad phones.
  String inlinePayload = payload;
  if (multiTapActive) {
    inlinePayload += composePreview;
  }

  // Wider rows now that preview is inlined (no separate preview area).
  const int perRowChars = 21;
  const int totalVisibleChars = perRowChars * 2;
  String visiblePayload = getTailWindow(inlinePayload, totalVisibleChars);

  String row1 = "";
  String row2 = "";
  if ((int)visiblePayload.length() <= perRowChars) {
    row1 = visiblePayload;
  } else {
    row1 = visiblePayload.substring(0, perRowChars);
    row2 = visiblePayload.substring(perRowChars);
  }

  OLED.setTextSize(1);
  OLED.setCursor(0, 22);
  OLED.print(row1);
  OLED.setCursor(0, 32);
  OLED.print(row2);

  // RX message area (marquee when long)
  drawRxMarqueeArea();
}


void drawMenuScreen() {
  drawHeader();

  int prevIndex = wrapIndex(menu_index - 1, menu_size);
  int nextIndex = wrapIndex(menu_index + 1, menu_size);

  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);
  OLED.setCursor(-8, 31);
  OLED.print(trimHeadForWidth(menu[prevIndex], 8));

  OLED.setCursor(103, 31);
  OLED.print(trimHeadForWidth(menu[nextIndex], 8));

  OLED.fillRoundRect(22, 24, 84, 18, 4, WHITE);
  OLED.drawRoundRect(21, 23, 86, 20, 4, WHITE);
  OLED.setTextColor(BLACK, WHITE);
  OLED.setTextSize(2);
  OLED.setCursor(26, 28);
  OLED.print(trimHeadForWidth(menu[menu_index], 7));
  OLED.setTextColor(WHITE, BLACK);
}


void drawBeaconScreen() {
  drawHeader();

  OLED.setCursor(0, 16);
  OLED.print("Unicast beacon");

  OLED.setCursor(0, 28);
  OLED.print("To " + String(targetNodeId) + ": ");
  OLED.print(trimForWidth(payload, 17));

  OLED.setCursor(0, 40);
  OLED.print("TX");
  for (int i = 0; i < beaconDotCount; i++) {
    OLED.print(".");
  }
}


void drawTargetScreen() {
  drawHeader();
  OLED.setTextColor(WHITE);
  OLED.setTextSize(1);
  OLED.setCursor(0, 16);
  OLED.print("Set Target Node");

  OLED.drawRoundRect(34, 24, 60, 20, 4, WHITE);
  OLED.setTextSize(2);
  OLED.setCursor(52, 28);
  OLED.print(targetNodeId);

  OLED.setTextSize(1);
  OLED.setCursor(0, 46);
  OLED.print("This node: ");
  OLED.print(myNodeId);
}


void drawSelfIdScreen() {
  drawHeader();
  OLED.setTextColor(WHITE);
  OLED.setTextSize(1);
  OLED.setCursor(0, 16);
  OLED.print("Set Self Node ID");

  OLED.drawRoundRect(34, 24, 60, 20, 4, WHITE);
  OLED.setTextSize(2);
  OLED.setCursor(52, 28);
  OLED.print(myNodeId);

  OLED.setTextSize(1);
  OLED.setCursor(0, 46);
  OLED.print("Target: ");
  OLED.print(targetNodeId);
}


void drawSavedScreen() {
  drawHeader();
  OLED.setCursor(0, 14);
  OLED.print("Saved Messages");

  if (savedCount == 0) {
    OLED.setCursor(0, 30);
    OLED.print("No saved messages");
    return;
  }

  int rows = 4;
  int start = 0;

  if (savedCount > rows) {
    start = savedBrowseIndex - 1;
    if (start < 0) start = 0;
    if (start > savedCount - rows) start = savedCount - rows;
  }

  int end = start + rows;
  if (end > savedCount) end = savedCount;

  for (int i = start; i < end; i++) {
    int y = 24 + ((i - start) * 8);
    bool selected = (i == savedBrowseIndex);
    String line = String(i + 1) + ":" + trimForWidth(getSavedMessageByIndex(i), 16);

    if (selected) {
      OLED.fillRect(0, y - 1, 128, 8, WHITE);
      OLED.setTextColor(BLACK, WHITE);
    } else {
      OLED.setTextColor(WHITE, BLACK);
    }

    OLED.setCursor(2, y);
    OLED.print(line);
  }

  OLED.setTextColor(WHITE, BLACK);
}


void updateOLED() {
  if (!uiDirty) return;

  OLED.clearDisplay();

  if (uiState == UI_HOME) {
    drawHomeScreen();
  } else if (uiState == UI_MENU) {
    drawMenuScreen();
  } else if (uiState == UI_BEACON) {
    drawBeaconScreen();
  } else if (uiState == UI_SAVED) {
    drawSavedScreen();
  } else if (uiState == UI_TARGET) {
    drawTargetScreen();
  } else if (uiState == UI_SELF_ID) {
    drawSelfIdScreen();
  }

  OLED.display();
  uiDirty = false;
}


void enterState(UiState nextState) {
  uiState = nextState;
  if (uiState == UI_BEACON) {
    lastBeaconTxMs = 0;  // send immediately on first tick
    lastBeaconAnimMs = millis();
    beaconDotCount = 0;
  } else if (uiState == UI_SAVED) {
    savedBrowseIndex = 0;
  }
  uiDirty = true;
}


String buildUnicastPacket(const String &message) {
  return "T:" + String(targetNodeId) + "|F:" + String(myNodeId) + "|M:" + message;
}


bool parseUnicastPacket(const String &packet, int &to, int &from, String &message) {
  if (!packet.startsWith("T:")) return false;

  int fPos = packet.indexOf("|F:");
  int mPos = packet.indexOf("|M:");
  if (fPos < 0 || mPos < 0 || mPos <= fPos) return false;

  String toPart = packet.substring(2, fPos);
  String fromPart = packet.substring(fPos + 3, mPos);
  message = packet.substring(mPos + 3);

  to = toPart.toInt();
  from = fromPart.toInt();

  if (to < NODE_ID_MIN || to > NODE_ID_MAX) return false;
  if (from < NODE_ID_MIN || from > NODE_ID_MAX) return false;
  return true;
}


void processReceivedLoRa() {
  if (!loraReady) return;

  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String data = "";
  while (LoRa.available()) {
    data += (char)LoRa.read();
  }

  int toNode = 0;
  int fromNode = 0;
  String msg = "";
  if (!parseUnicastPacket(data, toNode, fromNode, msg)) {
    return;
  }

  if (toNode != myNodeId) {
    return;
  }

  Serial.print("Receiving Data: ");
  Serial.println("from " + String(fromNode) + ": " + msg);

  rxIndicatorUntilMs = millis() + 1200;
  resetRxMarquee(msg, fromNode);
  showToast("RX " + String(fromNode) + ": " + trimForWidth(msg, 14), 1200);
}


const char* getMultiTapSet(char key) {
  for (uint8_t i = 0; i < KEYPAD_BUTTON_COUNT; i++) {
    if (keypadLayoutByButton[i] == key) {
      return keypadMultiTapByButton[i];
    }
  }

  return nullptr;
}


void clearMultiTapPreview() {
  multiTapActive = false;
  lastMultiTapKey = '\0';
  multiTapIndex = 0;
  composePreview = "_";
  uiDirty = true;
}


void commitPendingMultiTap() {
  if (!multiTapActive) return;

  const char* set = getMultiTapSet(lastMultiTapKey);
  if (set == nullptr) {
    clearMultiTapPreview();
    return;
  }

  int len = strlen(set);
  if (len <= 0) {
    clearMultiTapPreview();
    return;
  }

  char c = set[multiTapIndex % len];
  payload += String(c);
  clearMultiTapPreview();
}


void executeMenuAction() {
  String selected = menu[menu_index];

  if (selected == "Save") {
    if (savePayloadToEEPROM()) {
      showToast("Saved message");
    } else {
      showToast("Payload empty");
    }
    enterState(UI_HOME);
  }
  else if (selected == "Saved") {
    enterState(UI_SAVED);
  }
  else if (selected == "SelfID") {
    enterState(UI_SELF_ID);
  }
  else if (selected == "Target") {
    enterState(UI_TARGET);
  }
  else if (selected == "SEND") {
    if (payload.length() == 0) {
      showToast("Payload empty");
      enterState(UI_HOME);
    } else if (!loraReady) {
      showToast("LoRa not detected");
      enterState(UI_HOME);
    } else if (targetNodeId == myNodeId) {
      showToast("Target cannot be self");
      enterState(UI_HOME);
    } else {
      enterState(UI_BEACON);
      showToast("Beacon started", 700);
    }
  }
}


void handleSingleClick() {
  if (uiState == UI_HOME) {
    commitPendingMultiTap();
  } else if (uiState == UI_MENU) {
    executeMenuAction();
  } else if (uiState == UI_BEACON) {
    enterState(UI_HOME);
    showToast("Beacon stopped", 700);
  } else if (uiState == UI_SAVED) {
    if (savedCount > 0) {
      payload = getSavedMessageByIndex(savedBrowseIndex);
      showToast("Loaded saved msg");
      enterState(UI_HOME);
    }
  } else if (uiState == UI_TARGET) {
    sanitizeNodeIds();
    showToast("Target: " + String(targetNodeId));
    enterState(UI_MENU);
  } else if (uiState == UI_SELF_ID) {
    sanitizeNodeIds();
    saveMyNodeIdToEEPROM();
    showToast("Self ID: " + String(myNodeId));
    enterState(UI_MENU);
  }
}


void handleDoubleClick() {
  commitPendingMultiTap();

  if (uiState == UI_HOME) {
    enterState(UI_MENU);
  } else if (uiState == UI_MENU) {
    enterState(UI_HOME);
  } else if (uiState == UI_BEACON) {
    enterState(UI_HOME);
    showToast("Beacon stopped", 700);
  } else if (uiState == UI_SAVED) {
    enterState(UI_MENU);
  } else if (uiState == UI_TARGET) {
    enterState(UI_MENU);
  } else if (uiState == UI_SELF_ID) {
    enterState(UI_MENU);
  }
}


void handleEncoderRotate(int direction) {
  if (direction == 0) return;

  if (uiState == UI_MENU) {
    menu_index = wrapIndex(menu_index + direction, menu_size);
    uiDirty = true;
  }
  else if (uiState == UI_SAVED) {
    if (savedCount > 0) {
      savedBrowseIndex = wrapIndex(savedBrowseIndex + direction, savedCount);
      uiDirty = true;
    }
  }
  else if (uiState == UI_TARGET) {
    targetNodeId += direction;
    if (targetNodeId < NODE_ID_MIN) targetNodeId = NODE_ID_MAX;
    if (targetNodeId > NODE_ID_MAX) targetNodeId = NODE_ID_MIN;
    uiDirty = true;
  }
  else if (uiState == UI_SELF_ID) {
    myNodeId += direction;
    if (myNodeId < NODE_ID_MIN) myNodeId = NODE_ID_MAX;
    if (myNodeId > NODE_ID_MAX) myNodeId = NODE_ID_MIN;
    sanitizeNodeIds();
    uiDirty = true;
  }
}


void handleMultiTapKey(char key) {
  const char* set = getMultiTapSet(key);
  if (set == nullptr) return;

  unsigned long now = millis();
  int len = strlen(set);
  if (len <= 0) return;

  if (multiTapActive && key == lastMultiTapKey && (now - multiTapLastPressMs <= multiTapTimeoutMs)) {
    multiTapIndex = (multiTapIndex + 1) % len;
  } else {
    commitPendingMultiTap();
    lastMultiTapKey = key;
    multiTapIndex = 0;
    multiTapActive = true;
  }

  composePreview = String(set[multiTapIndex]);
  multiTapLastPressMs = now;
  uiDirty = true;
}


void tickMultiTapTimeout() {
  if (!multiTapActive) return;
  if (millis() - multiTapLastPressMs > multiTapTimeoutMs) {
    commitPendingMultiTap();
  }
}


void beginAPressTracking(APressMode mode) {
  aPressTracking = true;
  aLongPressHandled = false;
  aPressStartMs = millis();
  aPressMode = mode;
}


void resetAPressTracking() {
  aPressTracking = false;
  aLongPressHandled = false;
  aPressStartMs = 0;
  aPressMode = A_PRESS_NONE;
}


void doDeleteLastChar() {
  commitPendingMultiTap();
  if (payload.length() > 0) {
    payload.remove(payload.length() - 1);
    showToast("Deleted last char", 700);
  } else {
    showToast("Payload is empty", 700);
  }
  uiDirty = true;
}


void doClearPayload() {
  commitPendingMultiTap();
  if (payload.length() > 0) {
    payload = "";
    showToast("Payload cleared", 700);
  } else {
    showToast("Payload is empty", 700);
  }
  uiDirty = true;
}


void doBackFromSaved() {
  enterState(UI_MENU);
  showToast("Back", 600);
}


void doDeleteSelectedSavedMessage() {
  if (savedCount <= 0) {
    showToast("No saved messages", 700);
    return;
  }

  if (deleteSavedMessageByDisplayIndex(savedBrowseIndex)) {
    if (savedCount == 0) {
      savedBrowseIndex = 0;
      showToast("Saved msg deleted", 700);
      enterState(UI_MENU);
    } else {
      if (savedBrowseIndex >= savedCount) {
        savedBrowseIndex = savedCount - 1;
      }
      showToast("Saved msg deleted", 700);
      uiDirty = true;
    }
  } else {
    showToast("Delete failed", 700);
  }
}


void tickAPressActions() {
  if (!aPressTracking) return;

  bool isAHeld = (lastStableMatrixKey == 'A' && matrixKeyHeld);
  unsigned long now = millis();

  if (isAHeld) {
    if (!aLongPressHandled && (now - aPressStartMs >= aLongPressMs)) {
      if (aPressMode == A_PRESS_HOME_EDIT) {
        doClearPayload();
      } else if (aPressMode == A_PRESS_SAVED_SELECT) {
        doDeleteSelectedSavedMessage();
      }
      aLongPressHandled = true;
    }
    return;
  }

  // A was released (or changed key).
  if (!aLongPressHandled) {
    if (aPressMode == A_PRESS_HOME_EDIT) {
      doDeleteLastChar();
    } else if (aPressMode == A_PRESS_SAVED_SELECT) {
      doBackFromSaved();
    }
  }

  resetAPressTracking();
}


void handleActionKey(char key) {
  commitPendingMultiTap();

  if (key == 'B') {
    // Requested: B not set for now.
    return;
  } else if (key == 'C') {
    // Requested: C not set for now.
    return;
  }

  if (key == 'A') {
    // Requested mapping:
    // - Home: A short-press deletes last char, long-press clears payload
    // - Menu / menu-related screens: A acts as back
    if (uiState == UI_HOME) {
      beginAPressTracking(A_PRESS_HOME_EDIT);
    } else if (uiState == UI_SAVED) {
      // Saved list: short A = back, long A = delete selected saved message.
      beginAPressTracking(A_PRESS_SAVED_SELECT);
    } else if (uiState == UI_MENU) {
      enterState(UI_HOME);
      showToast("Back", 600);
    } else {
      enterState(UI_MENU);
      showToast("Back", 600);
    }
  } else if (key == 'D') {
    // Requested mapping: D opens menu.
    enterState(UI_MENU);
    showToast("Menu", 600);
  }
}


void handleNavigationKey(char key) {
  if (uiState == UI_SAVED) {
    // Saved list is vertical: use only 2/8 for up/down.
    if (key == '2') {
      handleEncoderRotate(-1);
    } else if (key == '8') {
      handleEncoderRotate(1);
    }
    return;
  }

  // Menu navigation keys: only 4 (left/up) and 6 (right/down), plus g/m aliases.
  if (key == '4' || key == 'a' || key == 'g') {
    handleEncoderRotate(-1);
  } else if (key == '6' || key == 'm' || key == 't') {
    handleEncoderRotate(1);
  }
}


void handleKeypadInput(char key) {
  if (!key) return;

  if (uiState == UI_HOME) {
    // Dedicated action keys in HOME.
    if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
      handleActionKey(key);
      return;
    }

    // In current rotated layout, letters/punctuation can also be on A/B/C/D.
    // So for HOME typing, accept any key that has a non-empty multi-tap set.
    const char* set = getMultiTapSet(key);
    if (set != nullptr && strlen(set) > 0) {
      handleMultiTapKey(key);
      return;
    }

    // Optional direct delete/space helpers in compose screen
    if (key == '*') {
      commitPendingMultiTap();
      if (payload.length() > 0) {
        payload.remove(payload.length() - 1);
        showToast("Deleted last char", 700);
      } else {
        showToast("Payload is empty", 700);
      }
      uiDirty = true;
    } else if (key == '0') {
      commitPendingMultiTap();
      payload += " ";
      uiDirty = true;
    } else if (key == '#') {
      commitPendingMultiTap();
      handleSingleClick();
    }
    return;
  }

  // Non-home screens keep action-key handling.
  if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
    handleActionKey(key);
    return;
  }

  // Non-home screens: nav + OK (j / key 5).
  if (key == '5' || key == 'j') {
    handleSingleClick();
    return;
  }

  handleNavigationKey(key);
}


void runBeaconTick() {
  if (uiState != UI_BEACON || !loraReady) return;

  unsigned long now = millis();

  if (lastBeaconTxMs == 0 || (now - lastBeaconTxMs >= beaconIntervalMs)) {
    String packet = buildUnicastPacket(payload);
    LoRa.beginPacket();
    LoRa.print(packet);
    LoRa.endPacket();
    lastBeaconTxMs = now;
    Serial.print("Beacon sent to ");
    Serial.print(targetNodeId);
    Serial.print(": ");
    Serial.println(payload);
  }

  if (now - lastBeaconAnimMs >= beaconAnimStepMs) {
    beaconDotCount = (beaconDotCount + 1) % 4;
    lastBeaconAnimMs = now;
    uiDirty = true;
  }
}


void setup() {
  Serial.begin(9600);
  unsigned long serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart < 2000)) {
    delay(10);
  }

  if (initilialize_OLED() == false) {
    Serial.println("OLED initialization failed");
  }

  rebuildKeymapFromLayout();
  initMatrixKeypad();

  // Always show intro on OLED, even when other modules are not connected.
  powerOnAnimation();

  initSavedStorage();

  LoRa.setPins(SS, RST, DIO0);
  if (LoRa.begin(433E6)) {
    loraReady = true;
    Serial.println("LoRa OK");
  } else {
    loraReady = false;
    Serial.println("LoRa not detected - running OLED/UI only");
  }

  current_mode = BEACON;
  enterState(UI_HOME);
  sanitizeNodeIds();
  if (loraReady) {
    showToast("Me:" + String(myNodeId) + " T:" + String(targetNodeId), 1300);
  } else {
    showToast("OLED mode (LoRa off)", 1300);
  }
}


void loop() {
  processReceivedLoRa();
  tickRxMarquee();

  char key = getMatrixKeyDebounced();
  if (key) {
    handleKeypadInput(key);
  }
  tickAPressActions();
  tickMultiTapTimeout();

  runBeaconTick();

  updateOLED();

  delay(1);
}
