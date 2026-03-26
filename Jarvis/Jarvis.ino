#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <BleKeyboard.h>

// ==========================================
// PIN DEFINITIONS (Update to match your wiring!)
// ==========================================
// Nokia 5110 Pins: (CLK, DIN, DC, CE, RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(18, 23, 4, 15, 2);

// Rotary Encoder Pins
#define ENCODER_CLK 32
#define ENCODER_DT  33
#define ENCODER_SW  25

// ==========================================
// SYSTEM VARIABLES
// ==========================================
BleKeyboard bleKeyboard("S.H.I.E.L.D. Terminal", "Stark Ind.", 100);

enum ScreenState { HOME, MENU, SENSORS, MUSIC, TIMER_ALARM, TERMINAL, SCREENSAVER };
ScreenState currentState = HOME;

// --- Encoder & Button Tracking ---
int encoderCount = 0;
int lastEncoderCount = 0;
int encoderDelta = 0;

bool btnState = HIGH;
bool lastBtnState = HIGH;
unsigned long btnPressTime = 0;
bool longPress = false;
int registeredTaps = 0;

// --- Menu System Variables ---
const int NUM_MENU_ITEMS = 4;
String menuItems[NUM_MENU_ITEMS] = {"1. Sensors", "2. Music Ctrl", "3. Start Timer", "4. Terminal"};
int currentMenuIndex = 0;

// --- Terminal & Autocomplete Variables ---
String currentInput = "";
String predictedWord = "";
const int DICT_SIZE = 14;
const char* dictionary[DICT_SIZE] = {
  "WEATHER", "STATUS", "TIMER", "MUSIC", "LIGHTS", 
  "LOCK", "OVERRIDE", "TAHITI", "CLEARANCE", "SHIELD", 
  "COULSON", "HELP", "OFF", "ON"
};

// --- Background Timer Variables ---
bool timerRunning = false;
unsigned long timerEndTime = 0;

// --- Screensaver Variables ---
unsigned long lastActivityTime = 0;
const unsigned long SCREENSAVER_TIMEOUT = 60000; // 60 seconds (60000 ms)
int kreeDrops[14]; 
unsigned long lastFrameTime = 0;

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // Initialize Screen
  display.begin();
  display.setContrast(50); // Adjust between 40-60 if screen is too dark/light
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK); // Nokia 5110 uses BLACK text on WHITE background
  display.display();

  // Initialize Bluetooth
  bleKeyboard.begin();

  // Initialize Encoder Pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  // Initialize Activity Timer
  lastActivityTime = millis();
}

// ==========================================
// INPUT HANDLING
// ==========================================
void handleEncoder() {
  static int lastClk = HIGH;
  static unsigned long lastEncoderJitter = 0; // Jitter debounce
  int newClk = digitalRead(ENCODER_CLK);
  
  if (newClk != lastClk && newClk == LOW && (millis() - lastEncoderJitter > 5)) {
    if (digitalRead(ENCODER_DT) == LOW) {
      encoderCount--;
    } else {
      encoderCount++;
    }
    lastActivityTime = millis(); // Reset screensaver timer
    lastEncoderJitter = millis();
  }
  lastClk = newClk;
  
  encoderDelta = encoderCount - lastEncoderCount;
  lastEncoderCount = encoderCount;
}

void handleButton() {
  btnState = digitalRead(ENCODER_SW);
  registeredTaps = 0;
  longPress = false;

  // Button Pressed Down
  if (btnState == LOW && lastBtnState == HIGH) {
    btnPressTime = millis();
    lastActivityTime = millis(); // Reset screensaver timer
    delay(50); // Debounce
  }

  // Button Released
  if (btnState == HIGH && lastBtnState == LOW) {
    unsigned long pressDuration = millis() - btnPressTime;
    if (pressDuration > 600) {
      longPress = true; // Registered a Long Press
    } else if (pressDuration > 50) {
      registeredTaps = 1; // Registered a Single Tap
    }
    lastActivityTime = millis(); // Reset screensaver timer
    delay(50); // Debounce
  }
  lastBtnState = btnState;
}

// ==========================================
// KREE SCREENSAVER (Digital Rain)
// ==========================================
void runScreensaver() {
  // Update the animation frame every 80ms
  if (millis() - lastFrameTime > 80) { 
    lastFrameTime = millis();
    
    for (int i = 0; i < 14; i++) {
      // 1. Erase the "tail" (4 characters behind the head)
      int tailY = (kreeDrops[i] - 4) * 8;
      if (tailY >= 0) {
        display.fillRect(i * 6, tailY, 6, 8, WHITE); // WHITE erases pixels
      }
      
      // 2. Draw a new random cryptic character at the "head"
      int headY = kreeDrops[i] * 8;
      if (headY >= 0 && headY < 48) {
        display.setCursor(i * 6, headY);
        // ASCII 33 to 90 provides uppercase letters, numbers, and symbols
        char c = random(33, 90); 
        display.print(c);
      }
      
      // 3. Move the drop down one row
      kreeDrops[i]++;
      
      // 4. Reset the drop to the top if it falls off the bottom (with random delay)
      if (kreeDrops[i] * 8 > 48 + random(0, 50)) {
        kreeDrops[i] = random(-10, 0);
        // Wipe the rest of the column to prevent ghosting
        display.fillRect(i * 6, 0, 6, 48, WHITE);
      }
    }
    display.display();
  }

  // WAKE UP SEQUENCE: Any touch brings it back to reality
  if (encoderDelta != 0 || registeredTaps > 0 || longPress) {
    lastActivityTime = millis();
    encoderCount = 0;
    lastEncoderCount = 0;
    currentState = HOME;
    display.clearDisplay();
  }
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  handleEncoder();
  handleButton();

  // --- BACKGROUND TIMER CHECK ---
  if (timerRunning && millis() >= timerEndTime) {
    timerRunning = false;          
    currentState = TIMER_ALARM;    
  }

  // --- SCREENSAVER TRIGGER LOGIC ---
  if (millis() - lastActivityTime > SCREENSAVER_TIMEOUT && 
      currentState != SCREENSAVER && 
      currentState != TIMER_ALARM) {
    
    currentState = SCREENSAVER;
    display.clearDisplay();
    
    // Stagger the starting heights of the falling text
    for(int i = 0; i < 14; i++) {
      kreeDrops[i] = random(-15, 0); 
    }
  }

  // --- STATE MACHINE UI ---
  switch (currentState) {
    
    case HOME:
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("DIRECTOR:");
      display.println("COULSON");
      display.drawLine(0, 18, 84, 18, BLACK);
      
      display.setCursor(0, 24);
      display.println("Status: SECURE");
      
      if (timerRunning) {
        display.setCursor(0, 36);
        display.print("Timer: ");
        display.print((timerEndTime - millis()) / 1000);
        display.print("s");
      }
      
      if (registeredTaps == 1) {
        currentState = MENU;
        encoderCount = currentMenuIndex; // Start menu at last position
      }
      display.display();
      break;

    case MENU:
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(">> MAIN MENU");
      display.drawLine(0, 10, 84, 10, BLACK);

      // Scroll Logic
      if (encoderDelta != 0) {
        currentMenuIndex = encoderCount % NUM_MENU_ITEMS;
        if (currentMenuIndex < 0) currentMenuIndex += NUM_MENU_ITEMS;
      }

      // Display selected item
      display.setCursor(0, 20);
      display.print(">");
      display.println(menuItems[currentMenuIndex]);

      // Selection Logic
      if (registeredTaps == 1) {
        if (currentMenuIndex == 0) currentState = SENSORS;
        if (currentMenuIndex == 1) currentState = MUSIC;
        if (currentMenuIndex == 2) {
          // Start a 60-second background timer as an example
          timerRunning = true;
          timerEndTime = millis() + 60000; 
          currentState = HOME; 
        }
        if (currentMenuIndex == 3) {
          currentState = TERMINAL;
          currentInput = "";
          encoderCount = 0; // Reset dial to start at 'A'
        }
      }
      
      // Go back
      if (longPress) currentState = HOME;
      display.display();
      break;

    case TERMINAL:
      { // Brackets required here so variable declarations don't break the switch statement
        display.clearDisplay();
        
        // 1. Calculate the Predicted Word based on dictionary
        predictedWord = "";
        if (currentInput.length() > 0) {
          for (int i = 0; i < DICT_SIZE; i++) {
            if (String(dictionary[i]).startsWith(currentInput)) {
              predictedWord = String(dictionary[i]);
              break;
            }
          }
        }

        // 2. Wheel Logic (30 options: A-Z, Space, Backspace, Autocomplete, Send)
        int charIndex = encoderCount % 30;
        if (charIndex < 0) charIndex += 30;
        
        char selectedChar;
        if (charIndex < 26) selectedChar = 'A' + charIndex; // A-Z
        else if (charIndex == 26) selectedChar = '_';       // Space
        else if (charIndex == 27) selectedChar = '<';       // Backspace
        else if (charIndex == 28) selectedChar = '*';       // Autocomplete trigger
        else selectedChar = '>';                            // Send/Execute trigger

        // 3. Draw UI
        display.setCursor(0, 0);
        display.print("CMD: ");
        display.setCursor(0, 10);
        display.print(currentInput);
        
        // Draw suggestion faintly
        display.setCursor(0, 20);
        if (predictedWord != "") {
          display.print("~");
          display.print(predictedWord);
        }

        display.setCursor(0, 35);
        display.print("Char: [ ");
        display.print(selectedChar);
        display.print(" ]");

        // 4. Input Actions
        if (registeredTaps == 1) {
          if (selectedChar >= 'A' && selectedChar <= 'Z' && currentInput.length() < 13) {
            currentInput += selectedChar; // Add letter
          } else if (selectedChar == '_' && currentInput.length() < 13) {
            currentInput += ' ';          // Add space
          } else if (selectedChar == '<' && currentInput.length() > 0) {
            currentInput.remove(currentInput.length() - 1); // Delete last character
          } else if (selectedChar == '*' && predictedWord != "") {
            currentInput = predictedWord; // AUTOCOMPLETE IT!
          } else if (selectedChar == '>') {
            // EXECUTE COMMAND (For now, just wipes and goes home)
            currentInput = "";
            currentState = HOME;
          }
        }

        if (longPress) currentState = MENU;
        display.display();
      }
      break;

    case SENSORS:
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("LOCAL ENV:");
      display.println("Temp: 72F");
      display.println("Hum:  45%");
      display.setCursor(0, 35);
      display.println("[Hold to exit]");
      
      if (longPress) currentState = MENU;
      display.display();
      break;

    case MUSIC:
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("MEDIA CONTROL");
      display.println("Tap: Play/Pse");
      display.println("Dial: Volume");
      
      if (bleKeyboard.isConnected()) {
        display.println("BLE: Linked");
        
        // Volume Control
        if (encoderDelta > 0) bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
        if (encoderDelta < 0) bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
        // Play/Pause
        if (registeredTaps == 1) bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
        
      } else {
        display.println("BLE: Offline");
      }

      if (longPress) currentState = MENU;
      display.display();
      break;

    case TIMER_ALARM:
      display.clearDisplay();
      display.setCursor(10, 15);
      display.println("TIMER");
      display.setCursor(10, 25);
      display.println("COMPLETE!");
      display.setCursor(0, 40);
      display.println("[Tap to Ack]");
      
      if (registeredTaps > 0 || longPress) currentState = HOME;
      display.display();
      break;

    case SCREENSAVER:
      runScreensaver();
      break;
  }
}
