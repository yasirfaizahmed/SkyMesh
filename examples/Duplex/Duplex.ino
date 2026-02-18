#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display width and height
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Declaration for SSD1306 display connected using I2C
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

// Ra-02 pinout (UNCHANGED)
#define SS 15

// OLED pinout (UNCHANGED)
#define OLED_SDA 4  // GPIO4 (D2)
#define OLED_SCL 5  // GPIO5 (D1)

// Rotary Encoder pinout (UNCHANGED)
#define CLK 0
#define DT 2
#define SW 16

// Modes
#define BEACON "BEACON"

// Menu
String menu[] = {"Delete", "Clear", "Save", "SEND"};
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
  UI_BEACON
};

Adafruit_SSD1306 OLED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// App state
String payload = "";
String current_mode = BEACON;
int menu_index = 0;
int16_t alphanum_index = 0;
UiState uiState = UI_HOME;

// Encoder state
int currentStateCLK;
int lastStateCLK;

// Button / click handling
int debounceState = 1;
unsigned long lastPressTime = 0;
const unsigned long doubleClickThreshold = 300;
bool waitingForDoubleClick = false;

// Beacon runtime (non-blocking)
const unsigned long beaconIntervalMs = 2000;
const unsigned long beaconAnimStepMs = 250;
unsigned long lastBeaconTxMs = 0;
unsigned long lastBeaconAnimMs = 0;
int beaconDotCount = 0;

// Status / UX extras
bool uiDirty = true;
String toastMessage = "";
unsigned long toastUntilMs = 0;
unsigned long rxIndicatorUntilMs = 0;


bool initilialize_OLED() {
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!OLED.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  OLED.clearDisplay();
  OLED.setTextColor(WHITE);
  OLED.setTextSize(1);
  return true;
}


void powerOnAnimation() {
  String message = "SkyMesh";
  int16_t x1, y1;
  uint16_t width, height;

  OLED.setTextSize(2);
  OLED.setTextColor(WHITE);
  OLED.getTextBounds(message, 0, 0, &x1, &y1, &width, &height);

  int offset = 30;
  int start_y = SCREEN_HEIGHT + offset;
  for (int y = start_y; y > (SCREEN_HEIGHT / 2) - (height / 2); y--) {
    OLED.clearDisplay();
    OLED.setCursor((SCREEN_WIDTH - width) / 2, y);
    OLED.print(message);
    OLED.display();
    delay(10);
  }

  delay(500);
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
  OLED.print(current_mode);
  OLED.setCursor(82, 0);
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


void drawHomeScreen() {
  drawHeader();

  // Selected char block
  OLED.setTextSize(1);
  OLED.setCursor(2, 14);
  OLED.print("Char");
  OLED.drawRect(2, 24, 18, 14, WHITE);
  OLED.setCursor(8, 28);
  OLED.print(alphanum[alphanum_index]);

  // Payload block
  OLED.setCursor(24, 14);
  OLED.print("Payload");
  OLED.drawRect(24, 24, 102, 14, WHITE);
  OLED.setCursor(27, 28);
  OLED.print(trimForWidth(payload, 16));

  if (millis() < toastUntilMs) {
    OLED.setCursor(2, 42);
    OLED.print(trimForWidth(toastMessage, 21));
  }

  drawFooterHints("1C:Add  2C:Menu");
}


void drawMenuScreen() {
  drawHeader();

  OLED.setCursor(0, 14);
  OLED.print("Menu");

  for (int i = 0; i < menu_size; i++) {
    int y = 24 + (i * 8);
    bool selected = (i == menu_index);

    if (selected) {
      OLED.fillRect(2, y - 1, 124, 8, WHITE);
      OLED.setTextColor(BLACK, WHITE);
    } else {
      OLED.setTextColor(WHITE, BLACK);
    }

    OLED.setCursor(6, y);
    OLED.print(menu[i]);
  }

  OLED.setTextColor(WHITE, BLACK);
  drawFooterHints("Turn:Nav  Click:OK");
}


void drawBeaconScreen() {
  drawHeader();

  OLED.setCursor(0, 16);
  OLED.print("Sending beacon");

  OLED.setCursor(0, 28);
  OLED.print("Msg: ");
  OLED.print(trimForWidth(payload, 17));

  OLED.setCursor(0, 40);
  OLED.print("TX");
  for (int i = 0; i < beaconDotCount; i++) {
    OLED.print(".");
  }

  drawFooterHints("Click/Dbl:Stop");
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
  }
  uiDirty = true;
}


void processReceivedLoRa() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String data = "";
  while (LoRa.available()) {
    data += (char)LoRa.read();
  }

  Serial.print("Receiving Data: ");
  Serial.println(data);

  rxIndicatorUntilMs = millis() + 1200;
  showToast("RX: " + trimForWidth(data, 17), 1200);
}


int readEncoderDirection() {
  currentStateCLK = digitalRead(CLK);

  int direction = 0;
  if (currentStateCLK != lastStateCLK) {
    if (digitalRead(DT) != currentStateCLK) {
      direction = -1;  // CCW
    } else {
      direction = 1;   // CW
    }
  }

  lastStateCLK = currentStateCLK;
  return direction;
}


bool readButtonPressEdge() {
  if (digitalRead(SW) == 0 && debounceState == 1) {
    debounceState = 0;
    return true;
  }

  if (digitalRead(SW) == 1) {
    debounceState = 1;
  }

  return false;
}


void executeMenuAction() {
  String selected = menu[menu_index];

  if (selected == "Delete") {
    if (payload.length() > 0) {
      payload.remove(payload.length() - 1);
      showToast("Deleted last char");
    } else {
      showToast("Payload is empty");
    }
    enterState(UI_HOME);
  }
  else if (selected == "Clear") {
    payload = "";
    showToast("Payload cleared");
    enterState(UI_HOME);
  }
  else if (selected == "Save") {
    // Placeholder behavior kept simple for now
    showToast("Save not set yet");
    enterState(UI_HOME);
  }
  else if (selected == "SEND") {
    if (payload.length() == 0) {
      showToast("Payload empty");
      enterState(UI_HOME);
    } else {
      enterState(UI_BEACON);
      showToast("Beacon started", 700);
    }
  }
}


void handleSingleClick() {
  if (uiState == UI_HOME) {
    payload += alphanum[alphanum_index];
    uiDirty = true;
  } else if (uiState == UI_MENU) {
    executeMenuAction();
  } else if (uiState == UI_BEACON) {
    enterState(UI_HOME);
    showToast("Beacon stopped", 700);
  }
}


void handleDoubleClick() {
  if (uiState == UI_HOME) {
    enterState(UI_MENU);
  } else if (uiState == UI_MENU) {
    enterState(UI_HOME);
  } else if (uiState == UI_BEACON) {
    enterState(UI_HOME);
    showToast("Beacon stopped", 700);
  }
}


void handleEncoderRotate(int direction) {
  if (direction == 0) return;

  if (uiState == UI_HOME) {
    alphanum_index = wrapIndex(alphanum_index + direction, alphanum_size);
    uiDirty = true;
  }
  else if (uiState == UI_MENU) {
    menu_index = wrapIndex(menu_index + direction, menu_size);
    uiDirty = true;
  }
}


void handleButtonClicks() {
  if (readButtonPressEdge()) {
    unsigned long now = millis();
    if (waitingForDoubleClick && (now - lastPressTime <= doubleClickThreshold)) {
      waitingForDoubleClick = false;
      handleDoubleClick();
    } else {
      waitingForDoubleClick = true;
      lastPressTime = now;
    }
  }

  if (waitingForDoubleClick && (millis() - lastPressTime > doubleClickThreshold)) {
    waitingForDoubleClick = false;
    handleSingleClick();
  }
}


void runBeaconTick() {
  if (uiState != UI_BEACON) return;

  unsigned long now = millis();

  if (lastBeaconTxMs == 0 || (now - lastBeaconTxMs >= beaconIntervalMs)) {
    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();
    lastBeaconTxMs = now;
    Serial.print("Beacon sent: ");
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
  while (!Serial);

  if (initilialize_OLED() == false) {
    Serial.println("OLED initialization failed");
  }

  LoRa.setPins(SS, -1, -1);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Error");
    delay(100);
    for (;;);
  }

  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT);
  lastStateCLK = digitalRead(CLK);

  powerOnAnimation();

  current_mode = BEACON;
  enterState(UI_HOME);
}


void loop() {
  processReceivedLoRa();

  int rotation = readEncoderDirection();
  handleEncoderRotate(rotation);

  handleButtonClicks();

  runBeaconTick();

  updateOLED();

  delay(1);
}
