#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <BleKeyboard.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

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
// WIFI CREDENTIALS FOR OTA
// ==========================================
const char* ssid = "Airtel_Ethria2.4";
const char* password = "PalmDale007";
bool otaInitialized = false;

// ==========================================
// SYSTEM VARIABLES
// ==========================================
BleKeyboard bleKeyboard("S.H.I.E.L.D. Terminal", "Stark Ind.", 100);

enum ScreenState { HOME, MENU, SENSORS, MUSIC, TIMER_ALARM, TERMINAL, OTA_UPDATE, SCREENSAVER };
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
const int NUM_MENU_ITEMS = 5;
String menuItems[NUM_MENU_ITEMS] = {"1. Sensors", "2. Music Ctrl", "3. Start Timer", "4. Terminal", "5. OTA Update"};
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
  if (millis() - lastFrameTime > 80) { 
    lastFrameTime = millis();
    for (int i = 0; i < 14; i++) {
      int tailY = (kreeDrops[i] - 4) * 8;
      if (tailY >= 0) display.fillRect(i * 6, tailY, 6, 8, WHITE); 
      
      int headY = kreeDrops[i] * 8;
      if (headY >= 0 && headY < 48) {
        display.setCursor(i * 6, headY);
        char c = random(33, 90); 
        display.print(c);
      }
      kreeDrops[i]++;
      if (kreeDrops[i] * 8 > 48 + random(0, 50)) {
        kreeDrops[i] = random(-10, 0);
        display.fillRect(i * 6, 0, 6, 48, WHITE);
      }
    }
    display.display();
  }

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
      currentState != TIMER_ALARM &&
      currentState != OTA_UPDATE) { // Never trigger screensaver during OTA!
    
    currentState = SCREENSAVER;
    display.clearDisplay();
    for(int i = 0; i < 14; i++) kreeDrops[i] = random(-15, 0); 
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

      if (encoderDelta != 0) {
        currentMenuIndex = encoderCount % NUM_MENU_ITEMS;
        if (currentMenuIndex < 0) currentMenuIndex += NUM_MENU_ITEMS;
      }

      display.setCursor(0, 20);
      display.print(">");
      display.println(menuItems[currentMenuIndex]);

      if (registeredTaps == 1) {
        if (currentMenuIndex == 0) currentState = SENSORS;
        if (currentMenuIndex == 1) currentState = MUSIC;
        if (currentMenuIndex == 2) {
          timerRunning = true;
          timerEndTime = millis() + 60000; 
          currentState = HOME; 
        }
        if (currentMenuIndex == 3) {
          currentState = TERMINAL;
          currentInput = "";
          encoderCount = 0; 
        }
        if (currentMenuIndex == 4) {
          currentState = OTA_UPDATE;
          otaInitialized = false; // Reset the flag so WiFi boots up
        }
      }
      
      if (longPress) currentState = HOME;
      display.display();
      break;

    case TERMINAL:
      { 
        display.clearDisplay();
        predictedWord = "";
        if (currentInput.length() > 0) {
          for (int i = 0; i < DICT_SIZE; i++) {
            if (String(dictionary[i]).startsWith(currentInput)) {
              predictedWord = String(dictionary[i]);
              break;
            }
          }
        }

        int charIndex = encoderCount % 30;
        if (charIndex < 0) charIndex += 30;
        
        char selectedChar;
        if (charIndex < 26) selectedChar = 'A' + charIndex; 
        else if (charIndex == 26) selectedChar = '_';       
        else if (charIndex == 27) selectedChar = '<';       
        else if (charIndex == 28) selectedChar = '*';       
        else selectedChar = '>';                            

        display.setCursor(0, 0);
        display.print("CMD: ");
        display.setCursor(0, 10);
        display.print(currentInput);
        
        display.setCursor(0, 20);
        if (predictedWord != "") {
          display.print("~");
          display.print(predictedWord);
        }

        display.setCursor(0, 35);
        display.print("Char: [ ");
        display.print(selectedChar);
        display.print(" ]");

        if (registeredTaps == 1) {
          if (selectedChar >= 'A' && selectedChar <= 'Z' && currentInput.length() < 13) currentInput += selectedChar; 
          else if (selectedChar == '_' && currentInput.length() < 13) currentInput += ' ';          
          else if (selectedChar == '<' && currentInput.length() > 0) currentInput.remove(currentInput.length() - 1); 
          else if (selectedChar == '*' && predictedWord != "") currentInput = predictedWord; 
          else if (selectedChar == '>') {
            currentInput = "";
            currentState = HOME;
          }
        }

        if (longPress) currentState = MENU;
        display.display();
      }
      break;

    case OTA_UPDATE:
      {
        if (!otaInitialized) {
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println("SECURE UPLINK");
          display.println("Connecting...");
          display.display();

          WiFi.mode(WIFI_STA);
          WiFi.begin(ssid, password);
          
          // Wait up to 10 seconds for connection
          unsigned long startAttempt = millis();
          while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
            delay(500);
            display.print(".");
            display.display();
          }

          if (WiFi.status() == WL_CONNECTED) {
            ArduinoOTA.setHostname("SHIELD-Terminal");
            ArduinoOTA.begin();
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("UPLINK ACTIVE");
            display.println("IP Address:");
            display.println(WiFi.localIP());
            display.setCursor(0, 40);
            display.print("[Hold to Exit]");
          } else {
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("UPLINK FAILED");
            display.println("Check Network");
            display.setCursor(0, 40);
            display.print("[Hold to Exit]");
          }
          display.display();
          otaInitialized = true;
        }

        // Listen for new code over the air
        if (WiFi.status() == WL_CONNECTED) {
          ArduinoOTA.handle();
        }

        // Shut down WiFi and leave mode to save memory
        if (longPress) {
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          currentState = MENU;
        }
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
      
      if (bleKeyboard.isConnected()) display.println("BLE: Linked");
      else display.println("BLE: Offline");

      if (encoderDelta > 0) bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
      if (encoderDelta < 0) bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
      if (registeredTaps == 1) bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
      
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
