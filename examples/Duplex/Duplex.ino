 #include <vector>
#include <variant>

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

// OLED pinout
#define OLED_SDA 4  // GPIO4 (D2)
#define OLED_SCL 5  // GPIO5 (D1)

// Rotary Encoder pinout
#define CLK 0
#define DT 2
#define SW 16

// Option
#define BEACON "BEACON"
#define SENT "SENT"
#define INBOX "INBOX"

// Buttons
String BUTTONS[] = {"texting", "send", "clear"};

// OLED cordinates
#define alphanum_y_cor 18
#define alphanum_x_cor 5

// Alphanumeric
std::vector<String> alphanum = {
        " ", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
        "k", "l", "m", "n", "o", "p", "q", "r", "s", "t",
        "u", "v", "w", "x", "y", "z",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "Send", "Clear"
};
int alphanum_size = alphanum.size();

// Create an SSD1306 display object connected to I2C (SDA, SCL)
Adafruit_SSD1306 OLED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
String payload = "";

// Rotary encoder data init
int currentStateCLK;
int lastStateCLK;
int debounceState = 1;

// current option
String current_option = BEACON;
int button_index = 0;
String current_button = BUTTONS[button_index];
int16_t alphanum_index = 0;
std::variant<String, char> selected_object = alphanum[alphanum_index];


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


void powerOnAnimation() {
  String message = "SkyMesh";
  int16_t x1, y1;
  uint16_t width, height;
  
  OLED.setTextSize(2);
  OLED.setTextColor(WHITE);
  OLED.getTextBounds(message, 0, 0, &x1, &y1, &width, &height);
  
  int offset = 30;
  int start_y = SCREEN_HEIGHT + offset;
  for(int y = start_y; y > (SCREEN_HEIGHT/2) - (height/2); y--){
    OLED.clearDisplay();
    OLED.setCursor((SCREEN_WIDTH - width)/2, y);
    OLED.print(message);
    OLED.display();
    delay(10);
  }

  delay(1000);
  OLED.clearDisplay();
  OLED.display();
}


void add_OLED(String message,
                int16_t x,
                int16_t y,
                uint16_t* width_ptr,
                uint16_t* height_ptr,
                int16_t font_size=1,
                bool center_x=false,
                bool center_y=false,
                bool draw_rect=false){
  uint16_t width, height;
  int16_t x1, y1;
  OLED.setTextSize(font_size);
  OLED.getTextBounds(message, 0, 0, &x1, &y1, width_ptr, height_ptr);
  if (center_x == true) x = (SCREEN_WIDTH/2) - (*width_ptr/2);
  if (center_y == true) y = (SCREEN_HEIGHT/2) - (*height_ptr/2);
  if (draw_rect == true) OLED.drawRect(x - 2, y - 2, *width_ptr + 3, *height_ptr + 3, WHITE);
  OLED.setCursor(x, y);
  OLED.println(message);
}


void updateOLED(){
  uint16_t width, height;
  bool draw_rect = false;

  // clear
  OLED.clearDisplay();

  // mode
  add_OLED(current_option, 5, 2, &width, &height, 1, false, false, false);
  // current character
  if (current_button == BUTTONS[0]) draw_rect = true;
  else draw_rect = false;
  add_OLED(alphanum[alphanum_index], alphanum_x_cor, alphanum_y_cor, &width, &height, 1, false, false, draw_rect);
  // payload
  add_OLED(payload, alphanum_x_cor + 35, alphanum_y_cor, &width, &height, 1, false, false, false);

  //display
  OLED.display();
}


void setup(){
  // Initialize serial
  Serial.begin(9600);
  while (!Serial);

  // Initialize OLED
  if(initilialize_OLED() == false){
    Serial.println("OLED initialization failed");
  }

  // Initialize LoRa
  LoRa.setPins(SS, -1, -1);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Error");
    delay(100);
    for (;;);
  }

  // Rotrary encoder
	pinMode(CLK,INPUT);
	pinMode(DT,INPUT);
  pinMode(SW, INPUT);
  lastStateCLK = digitalRead(CLK);

  // Run the animation
  // powerOnAnimation();

  // Go to default mode "BEACON"
  current_option = BEACON;
  updateOLED();
}


void loop(){
  // handle if any message recievied
  if (LoRa.parsePacket()) {
    // TODO:store the recieved message in permanent memory
    Serial.print("Receiving Data: ");
    while (LoRa.available()) {
      String data = LoRa.readString();
      Serial.println(data);
      // print_OLED("rx:" + data, 0, 50);
    }
  }

  // Read the current state of CLK
	currentStateCLK = digitalRead(CLK);
  if (currentStateCLK != lastStateCLK){

		// If the DT state is different than the CLK state then
		// the encoder is rotating CCW so decrement
		if (digitalRead(DT) != currentStateCLK) {
      if (--alphanum_index < 0) alphanum_index = alphanum_size - 1;
		} else {
      if (++alphanum_index > alphanum_size - 1 ) alphanum_index = 0;
		}

    // update OLED
    updateOLED();
	}
	// Remember last CLK state
	lastStateCLK = currentStateCLK;

  // deBouncer
  if (digitalRead(SW) == 0 && debounceState == 1){
    debounceState = 0;

    if (alphanum[alphanum_index] == "Clear"){
      payload = "";
    }
    else if (alphanum[alphanum_index] == "Send"){
      Serial.println("Sending payload");
    }
    else if (alphanum[alphanum_index] != "Text") payload += alphanum[alphanum_index];

    updateOLED();
  }
  if (digitalRead(SW) == 1) debounceState = 1;

	// Put in a slight delay to help debounce the reading
	delay(1);

}
