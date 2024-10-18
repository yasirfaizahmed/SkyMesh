#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display width and height
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for SSD1306 display connected using I2C
#define OLED_RESET     -1 // Reset pin
#define SCREEN_ADDRESS 0x3C

// Ra-02 pinout
#define SS 15
#define RST 16

// OLED pinout
#define OLED_SDA 4  // GPIO4 (D2)
#define OLED_SCL 5  // GPIO5 (D1)

// Rotary Encoder pinout
#define CLK 0
#define DT 2


// Create an SSD1306 display object connected to I2C (SDA, SCL)
Adafruit_SSD1306 OLED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
String data = "bismillah!";

// Rotary encoder data init
int counter = 0;
int currentStateCLK;
int lastStateCLK;
String currentDir ="";
unsigned long lastButtonPress = 0;


bool initilialize_OLED(){
  // Start I2C communication with defined pins
  Wire.begin(OLED_SDA, OLED_SCL);

  // Initialize the display
  if (!OLED.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  // Clear the buffer
  OLED.clearDisplay();
  // Set the text color to white
  OLED.setTextColor(WHITE);
  OLED.setTextSize(1); // 2x scale

  return true;
}

void print_OLED(String message, int16_t x ,int16_t y){
  OLED.clearDisplay();
  OLED.setCursor(x, y);
  OLED.println(message);
  OLED.display();
}

void setup(){
  // Initialize serial
  Serial.begin(9600);
  while (!Serial);
  Serial.println("Sender Host");

  // Initialize OLED
  if(initilialize_OLED() == false){
    Serial.println("OLED initialization failed");
  }

  // Onboard LED
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize LoRa
  LoRa.setPins(SS, RST, -1);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Error");
    delay(100);
    while (1);
  }

  // Rotrary encoder
	pinMode(CLK,INPUT);
	pinMode(DT,INPUT);
  lastStateCLK = digitalRead(CLK);
}


void loop(){
  // Read the current state of CLK
	currentStateCLK = digitalRead(CLK);

  if (currentStateCLK != lastStateCLK){

		// If the DT state is different than the CLK state then
		// the encoder is rotating CCW so decrement
		if (digitalRead(DT) != currentStateCLK) {
			counter --;
			currentDir ="CCW";
		} else {
			// Encoder is rotating CW so increment
			counter ++;
			currentDir ="CW";
		}

    Serial.print("Sending Data: ");
    Serial.println(data);
    LoRa.beginPacket();
    LoRa.print(data + " " + String(counter));
    LoRa.endPacket();
    print_OLED(data + " " + String(counter), 0, 40);

		Serial.print("Direction: ");
		Serial.print(currentDir);
		Serial.print(" | Counter: ");
		Serial.println(counter);
	}

	// Remember last CLK state
	lastStateCLK = currentStateCLK;

	// Put in a slight delay to help debounce the reading
	delay(1);

}
