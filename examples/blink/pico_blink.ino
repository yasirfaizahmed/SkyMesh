// Raspberry Pi Pico OLED + LED sanity check
// Uses the same OLED pins as examples/Duplex/Pico_Duplex.ino:
// SDA = GP4, SCL = GP5, I2C address = 0x3C

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// On most Pico cores, LED_BUILTIN is onboard LED (GP25)
const int LED_PIN = LED_BUILTIN;

// OLED settings (same as Pico_Duplex.ino)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
#define OLED_SDA 4
#define OLED_SCL 5

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastToggleMs = 0;
bool ledState = false;
int counter = 0;

void drawScreen() {
  oled.clearDisplay();
  oled.setTextColor(WHITE);

  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("Pico OLED Test");
  oled.println("SDA: GP4  SCL: GP5");
  oled.println("Addr: 0x3C");

  oled.setCursor(0, 30);
  oled.print("LED: ");
  oled.println(ledState ? "ON" : "OFF");

  oled.setCursor(0, 45);
  oled.print("Count: ");
  oled.println(counter);

  oled.display();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.setSDA(OLED_SDA);
  Wire.setSCL(OLED_SCL);
  Wire.begin();

  if (!oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    // If OLED init fails, fast-blink LED forever for visible fault indication.
    while (true) {
      digitalWrite(LED_PIN, HIGH);
      delay(120);
      digitalWrite(LED_PIN, LOW);
      delay(120);
    }
  }

  drawScreen();
}

void loop() {
  unsigned long now = millis();
  if (now - lastToggleMs >= 500) {
    lastToggleMs = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);

    counter++;
    drawScreen();
  }
}
