#include <WiFi.h>
#include <HTTPClient.h> 
#include <WiFiClientSecure.h> // Required for HTTPS API calls
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "DHT.h"
#include <BleKeyboard.h>
#include <time.h>

// Web OTA Headers
#include <WebServer.h>
#include <Update.h>

// Power management headers
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define NOKIA_CLK  18
#define NOKIA_DIN  19
#define NOKIA_DC   21
#define NOKIA_CE   5
#define NOKIA_RST  15

#define ENC_CLK    32
#define ENC_DT     33
#define ENC_SW     25

#define DHTPIN     26
#define DHTTYPE    DHT11

// ==========================================
// GLOBAL OBJECTS & CONSTANTS
// ==========================================
Adafruit_PCD8544 display = Adafruit_PCD8544(NOKIA_CLK, NOKIA_DIN, NOKIA_DC, NOKIA_CE, NOKIA_RST);
DHT dht(DHTPIN, DHTTYPE);
BleKeyboard bleKeyboard("Jarvis Remote", "ESP32", 100);

const char* WIFI_SSID = "Airtel_Ethria2.4";
const char* WIFI_PASS = "PalmDale007";
const char* JARVIS_URL = "http://jarvisep.pythonanywhere.com/command";

const long GMT_OFFSET_SEC = 19800; 
const int DAYLIGHT_OFFSET_SEC = 0;

// ==========================================
// S.H.I.E.L.D. LOGO BITMAP (INVERTED & STATIC)
// ==========================================
const uint8_t shield_width    = 48;
const uint8_t shield_height   = 48;
const uint8_t PROGMEM shield_bitmap[] = { 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x3f, 
  0xff, 0xff, 0xff, 0xff, 0x80, 0x01, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xf8, 
  0x3f, 0xfc, 0x1f, 0xff, 0xff, 0xe1, 0xff, 0xff, 0x87, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 
  0xff, 0x8f, 0xff, 0xff, 0xf1, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xf8, 0xff, 0xfe, 0x3f, 0xff, 0x1f, 
  0xfc, 0x7f, 0xfc, 0x4f, 0xfc, 0x0f, 0xf2, 0x3f, 0xfc, 0x83, 0xfc, 0x3f, 0xc1, 0x3f, 0xf9, 0x81, 
  0xf8, 0x3f, 0x81, 0x1f, 0xf9, 0x80, 0xf8, 0x1f, 0x01, 0x9f, 0xf2, 0x80, 0x78, 0x1e, 0x03, 0xcf, 
  0xf2, 0x40, 0x10, 0x08, 0x02, 0x4f, 0xe6, 0x20, 0x00, 0x00, 0x04, 0x47, 0xe4, 0x10, 0x00, 0x00, 
  0x08, 0x27, 0xe4, 0x18, 0x00, 0x00, 0x18, 0x27, 0xe4, 0x2c, 0x00, 0x00, 0x34, 0x27, 0xe4, 0x46, 
  0x00, 0x00, 0x62, 0x27, 0xc8, 0x83, 0x00, 0x00, 0xc1, 0x03, 0xc8, 0x01, 0x00, 0x01, 0x80, 0x13, 
  0xcc, 0x00, 0x80, 0x01, 0x00, 0x23, 0xcc, 0x00, 0xc0, 0x03, 0x00, 0x23, 0xe0, 0x00, 0x60, 0x06, 
  0x00, 0x07, 0xe4, 0x00, 0x30, 0x0c, 0x00, 0x27, 0xe4, 0x00, 0x38, 0x1c, 0x00, 0x27, 0xe4, 0x00, 
  0x74, 0x2e, 0x00, 0x27, 0xe4, 0x00, 0xe2, 0x47, 0x00, 0x07, 0xf2, 0x01, 0xc1, 0x83, 0x80, 0x4f, 
  0xf3, 0x03, 0xc0, 0x03, 0xc0, 0xcf, 0xf9, 0x07, 0x80, 0x01, 0xc0, 0x9f, 0xf9, 0x87, 0x00, 0x00, 
  0xe0, 0x1f, 0xfc, 0x8f, 0x00, 0x00, 0xf1, 0x3f, 0xfc, 0x5e, 0x00, 0x00, 0x7a, 0x3f, 0xfe, 0x3c, 
  0x00, 0x00, 0x3c, 0x7f, 0xff, 0x1c, 0x00, 0x00, 0x38, 0xff, 0xff, 0x8c, 0x00, 0x00, 0x11, 0xff, 
  0xff, 0xc6, 0x00, 0x00, 0x43, 0xff, 0xff, 0xe1, 0x80, 0x01, 0x87, 0xff, 0xff, 0xf8, 0x38, 0x1c, 
  0x1f, 0xff, 0xff, 0xfe, 0x01, 0x80, 0x7f, 0xff, 0xff, 0xff, 0x80, 0x01, 0xff, 0xff, 0xff, 0xff, 
  0xfc, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

// ==========================================
// STATE MACHINE & UI VARIABLES
// ==========================================
enum ScreenState { HOME, MENU, JARVIS, SENSORS, TIMER, MUSIC, SETTINGS, JARVIS_RESPONSE, OTA_UPDATE, TIMER_ALARM, SCREENSAVER };
ScreenState currentState = HOME;

int displayContrast = 55;

// Encoder Variables
volatile int encoderCount = 0;
int lastEncoderCount = 0;

// Button Multi-Tap Variables
bool btnState = false;
bool lastBtnState = false;
unsigned long btnPressTime = 0;
unsigned long btnReleaseTime = 0;
int tapCount = 0;              
int registeredTaps = 0;        
bool longPress = false;        
const unsigned long TAP_TIMEOUT = 350; 

// Menu Variables
const char* menuItems[] = {"Jarvis", "Sensors", "Timer", "Music", "Settings", "System Update"};
const int numMenuItems = 6;
int menuIndex = 0;

// Screensaver Variables
unsigned long lastActivityTime = 0;
const unsigned long SCREENSAVER_TIMEOUT = 60000; 

// ==========================================
// J.A.R.V.I.S. AUTOFILL VARIABLES
// ==========================================
const char charset[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,!?<*>";
int charIndex = 0;
String jarvisMessage = "";
String predictedWord = "";
String jarvisReply = "";
int jarvisScrollY = 0; 

// Timer Variables
int timerHours = 0;
int timerMinutes = 0;
int timerSeconds = 0;
int timerSetupStage = 0; 
unsigned long timerEndTime = 0;
bool timerRunning = false;

// OTA Variables
WebServer server(80);
bool otaStarted = false;

const char* serverIndex = 
  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
  "<h2 style='font-family:sans-serif;'>Jarvis System Update</h2>"
  "<p style='font-family:sans-serif;'>Select the new .bin file from your phone to flash.</p>"
  "<form method='POST' action='/update' enctype='multipart/form-data'>"
  "<input type='file' name='update' accept='.bin' style='margin-bottom:20px;'><br>"
  "<input type='submit' value='Update Firmware' style='padding:10px 20px; background:#007BFF; color:white; border:none; border-radius:5px;'>"
  "</form>";

// ==========================================
// INTERRUPT SERVICE ROUTINE (ENCODER)
// ==========================================
void IRAM_ATTR readEncoder() {
  static uint8_t old_AB = 3;
  static int8_t encval = 0;
  static const int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  
  old_AB <<= 2;
  old_AB |= ((digitalRead(ENC_DT) << 1) | digitalRead(ENC_CLK));
  encval += enc_states[(old_AB & 0x0f)];
  
  if (encval > 3) {
    encoderCount++;
    encval = 0;
    lastActivityTime = millis(); 
  } else if (encval < -3) {
    encoderCount--;
    encval = 0;
    lastActivityTime = millis(); 
  }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT), readEncoder, CHANGE);

  display.begin();
  display.setContrast(displayContrast);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);

  dht.begin();
  bleKeyboard.begin();

  display.setCursor(0, 0);
  display.println("Booting...");
  display.println("Connecting WiFi");
  display.display();
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    display.print(".");
    display.display();
  }

  display.clearDisplay();
  display.println("Syncing Time...");
  display.display();
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org");
  
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
  }

  encoderCount = 0; 
  lastActivityTime = millis();
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  handleButton();

  if (timerRunning && millis() >= timerEndTime) {
    timerRunning = false;          
    currentState = TIMER_ALARM;    
  }

  if (millis() - lastActivityTime > SCREENSAVER_TIMEOUT && 
      currentState != SCREENSAVER && 
      currentState != TIMER_ALARM &&
      currentState != OTA_UPDATE) { 
    
    currentState = SCREENSAVER;
    display.clearDisplay();
  }

  switch (currentState) {
    case HOME:            runHome(); break;
    case MENU:            runMenu(); break;
    case JARVIS:          runJarvis(); break;
    case JARVIS_RESPONSE: runJarvisResponse(); break;
    case SENSORS:         runSensors(); break;
    case TIMER:           runTimer(); break;
    case MUSIC:           runMusic(); break;
    case SETTINGS:        runSettings(); break;
    case OTA_UPDATE:      runOtaMode(); break;
    case TIMER_ALARM:     runTimerAlarm(); break;
    case SCREENSAVER:     runScreensaver(); break;
  }

  registeredTaps = 0;
  longPress = false;
  
  if (currentState != OTA_UPDATE) {
    delay(30); 
  }
}

// ==========================================
// INPUT HANDLING
// ==========================================
void handleButton() {
  btnState = !digitalRead(ENC_SW); 
  unsigned long now = millis();
  
  if (btnState && !lastBtnState) {
    btnPressTime = now;
    lastActivityTime = now;
  }
  
  if (!btnState && lastBtnState) {
    unsigned long duration = now - btnPressTime;
    if (duration > 30 && duration < 600) { 
      tapCount++;
      btnReleaseTime = now;
    } else if (duration >= 600) {
      longPress = true;
    }
    lastActivityTime = now; 
  }

  if (tapCount > 0 && (now - btnReleaseTime) > TAP_TIMEOUT) {
    registeredTaps = tapCount;
    tapCount = 0; 
  }

  lastBtnState = btnState;
}

int getEncoderDelta() {
  int delta = encoderCount - lastEncoderCount;
  lastEncoderCount = encoderCount;
  return delta;
}

// ==========================================
// STATE FUNCTIONS
// ==========================================
void runHome() {
  display.clearDisplay();
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[10]; 
    char dayStr[15];
    char dateStr[15];
    
    strftime(timeStr, sizeof(timeStr), "%I:%M %p", &timeinfo);
    strftime(dayStr, sizeof(dayStr), "%A", &timeinfo); 
    strftime(dateStr, sizeof(dateStr), "%d %b %Y", &timeinfo);
    
    int timeX = (84 - (strlen(timeStr) * 6)) / 2;
    int dayX = (84 - (strlen(dayStr) * 6)) / 2;
    int dateX = (84 - (strlen(dateStr) * 6)) / 2;

    if (timeX < 0) timeX = 0;
    if (dayX < 0) dayX = 0;
    if (dateX < 0) dateX = 0;

    display.setCursor(timeX, 5);
    display.print(timeStr);
    
    display.setCursor(dayX, 18);
    display.print(dayStr);
    
    display.setCursor(dateX, 31);
    display.print(dateStr);
  }
  display.display();

  if (registeredTaps == 1) {
    encoderCount = 0;
    lastEncoderCount = 0;
    currentState = MENU;
  }
}

void runMenu() {
  int delta = getEncoderDelta();
  if (delta > 0) menuIndex = (menuIndex + 1) % numMenuItems;
  if (delta < 0) {
    menuIndex = (menuIndex - 1);
    if (menuIndex < 0) menuIndex = numMenuItems - 1;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("--- MENU ---");
  
  int maxVisible = 4;
  int startIndex = menuIndex - 1; 
  if (startIndex < 0) startIndex = 0;
  if (startIndex > numMenuItems - maxVisible) startIndex = numMenuItems - maxVisible;

  for (int i = 0; i < maxVisible; i++) {
    int currentI = startIndex + i;
    if (currentI >= numMenuItems) break;
    
    display.setCursor(0, 10 + (i * 9)); 
    if (currentI == menuIndex) display.print(">");
    else display.print(" ");
    
    if (currentI == menuIndex && strlen(menuItems[currentI]) > 12) {
      display.print(String(menuItems[currentI]).substring(0, 12));
    } else {
      display.print(menuItems[currentI]);
    }
  }

  display.drawLine(82, 10, 82, 48, BLACK); 
  int barY = map(menuIndex, 0, numMenuItems - 1, 10, 40); 
  display.fillRect(80, barY, 4, 8, BLACK); 

  display.display();

  if (registeredTaps == 1) {
    encoderCount = 0;
    lastEncoderCount = 0;
    if (menuIndex == 0) currentState = JARVIS;
    else if (menuIndex == 1) currentState = SENSORS;
    else if (menuIndex == 2) currentState = TIMER;
    else if (menuIndex == 3) currentState = MUSIC;
    else if (menuIndex == 4) currentState = SETTINGS;
    else if (menuIndex == 5) currentState = OTA_UPDATE;
  }
  
  if (longPress) {
    encoderCount = 0;
    lastEncoderCount = 0;
    currentState = HOME;
  }
}

void runSettings() {
  int delta = getEncoderDelta();
  
  if (delta != 0) {
    displayContrast += (delta * 2); 
    if (displayContrast < 0) displayContrast = 0;
    if (displayContrast > 100) displayContrast = 100;
    display.setContrast(displayContrast);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("- SETTINGS -");
  display.setCursor(0, 15);
  display.println("Contrast:");
  
  display.setCursor(15, 25);
  display.print("< ");
  display.print(displayContrast);
  display.println(" >");

  display.setCursor(0, 40);
  display.println("(Hold to Exit)");
  display.display();

  if (registeredTaps > 0 || longPress) {
    currentState = MENU;
  }
}

// ==========================================
// LIVE API AUTOCOMPLETE FUNCTION
// ==========================================
void fetchPrediction(String input) {
  if (input.length() == 0) {
    predictedWord = "";
    return;
  }

  display.setCursor(75, 20);
  display.print("...");
  display.display();

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); 
    HTTPClient http;
    
    String url = "https://api.datamuse.com/sug?s=" + input + "&max=1";
    http.begin(client, url);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error && doc.size() > 0) {
        predictedWord = doc[0]["word"].as<String>();
        predictedWord.toUpperCase(); 
      } else {
        predictedWord = "";
      }
    }
    http.end();
  }
}

void runJarvis() {
  int delta = getEncoderDelta();
  int charsetLen = strlen(charset);

  if (delta > 0) charIndex = (charIndex + 1) % charsetLen;
  if (delta < 0) {
    charIndex--;
    if (charIndex < 0) charIndex = charsetLen - 1;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("JARVIS>");
  
  if (jarvisMessage.length() > 14) {
    display.println(jarvisMessage.substring(jarvisMessage.length() - 14));
  } else {
    display.println(jarvisMessage);
  }

  display.setCursor(0, 20);
  if (predictedWord != "") {
    display.print("~");
    display.print(predictedWord);
  }
  
  char selectedChar = charset[charIndex];
  display.setCursor(0, 35);
  display.print("Char: [ ");
  display.print(selectedChar);
  display.println(" ]");
  display.display();

  if (registeredTaps == 1) {
    if (selectedChar == '<') {
      if (jarvisMessage.length() > 0) {
        jarvisMessage.remove(jarvisMessage.length() - 1); 
        fetchPrediction(jarvisMessage); 
      }
    } else if (selectedChar == '*') {
      if (predictedWord != "") {
        jarvisMessage = predictedWord; 
        predictedWord = ""; 
      }
    } else if (selectedChar == '>') {
       sendToJarvis(jarvisMessage); 
       jarvisMessage = ""; 
       predictedWord = "";
       currentState = JARVIS_RESPONSE;
    } else {
      jarvisMessage += selectedChar; 
      fetchPrediction(jarvisMessage); 
    }
  }
  
  if (longPress) {
    currentState = MENU; 
  }
}

void sendToJarvis(String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting...");
  display.display();

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client; 
    HTTPClient http;
    http.setTimeout(20000); 
    
    http.begin(client, JARVIS_URL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["text"] = msg;
    String requestBody;
    serializeJson(doc, requestBody);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sending...");
    display.display();

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      String responseStr = http.getString();
      StaticJsonDocument<500> respDoc; 
      DeserializationError error = deserializeJson(respDoc, responseStr);
      
      if (!error) {
        jarvisReply = respDoc["response"].as<String>();
        jarvisScrollY = 0; 
      } else {
        jarvisReply = "JSON Error";
      }
    } else {
      jarvisReply = "Err: " + http.errorToString(httpResponseCode); 
    }
    
    http.end();
  } else {
    jarvisReply = "No WiFi!";
  }
}

void runJarvisResponse() {
  int delta = getEncoderDelta();

  int totalLines = (jarvisReply.length() / 12) + 2; 
  int maxScroll = (totalLines * 8) - 48;
  if (maxScroll < 0) maxScroll = 0; 

  if (delta > 0) jarvisScrollY += 8; 
  if (delta < 0) jarvisScrollY -= 8; 

  if (jarvisScrollY < 0) jarvisScrollY = 0;
  if (jarvisScrollY > maxScroll) jarvisScrollY = maxScroll;

  display.clearDisplay();
  display.setCursor(0, -jarvisScrollY); 
  display.print(jarvisReply); 
  display.display();

  if (registeredTaps > 0 || longPress) {
    currentState = MENU;
  }
}

void runSensors() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("- SENSORS -");
  
  if (isnan(h) || isnan(t)) {
    display.println("Failed read");
  } else {
    display.print("Temp: ");
    display.print(t, 1);
    display.println("C");
    display.print("Hum:  ");
    display.print(h, 1);
    display.println("%");
  }
  display.display();

  if (registeredTaps > 0 || longPress) {
    currentState = MENU;
  }
}

void runTimer() {
  int delta = getEncoderDelta();
  
  if (!timerRunning) {
    if (timerSetupStage == 0) {
      if (delta > 0) timerHours++;
      if (delta < 0 && timerHours > 0) timerHours--;
    } else if (timerSetupStage == 1) {
      if (delta > 0) timerMinutes++;
      if (delta < 0 && timerMinutes > 0) timerMinutes--;
      if (timerMinutes > 59) timerMinutes = 0; 
    } else if (timerSetupStage == 2) {
      if (delta > 0) timerSeconds++;
      if (delta < 0 && timerSeconds > 0) timerSeconds--;
      if (timerSeconds > 59) timerSeconds = 0;
    }

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Set Timer:");
    
    display.setCursor(0, 15);
    if (timerSetupStage == 0) display.print("["); else display.print(" ");
    display.print(timerHours);
    if (timerSetupStage == 0) display.print("]h "); else display.print(" h ");
    
    if (timerSetupStage == 1) display.print("["); else display.print(" ");
    display.print(timerMinutes);
    if (timerSetupStage == 1) display.print("]m "); else display.print(" m ");
    
    display.setCursor(0, 25);
    if (timerSetupStage == 2) display.print("["); else display.print(" ");
    display.print(timerSeconds);
    if (timerSetupStage == 2) display.println("]s"); else display.println(" s");

    display.setCursor(0, 38);
    display.println("(Tap to next)");
    display.display();

    if (registeredTaps == 1) {
      timerSetupStage++;
      if (timerSetupStage > 2) { 
        unsigned long totalMs = (timerHours * 3600000UL) + (timerMinutes * 60000UL) + (timerSeconds * 1000UL);
        if (totalMs > 0) {
          timerEndTime = millis() + totalMs;
          timerRunning = true;
        }
        timerSetupStage = 0; 
        currentState = HOME; 
      }
    }
    
    if (longPress) {
      timerSetupStage = 0;
      currentState = MENU;
    }
    
  } else {
    unsigned long now = millis();
    unsigned long timeLeft = timerEndTime - now;
    
    int h = (timeLeft / 3600000UL);
    int m = ((timeLeft % 3600000UL) / 60000UL);
    int s = ((timeLeft % 60000UL) / 1000UL);
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("- RUNNING -");
    display.print(h); display.print("h ");
    display.print(m); display.print("m ");
    display.print(s); display.println("s");
    display.println();
    display.println("(Hold=Stop)");
    display.display();
    
    if (longPress) {
      timerRunning = false;
      timerSetupStage = 0;
      currentState = MENU;
    }
  }
}

void runTimerAlarm() {
  if ((millis() / 500) % 2 == 0) {
    display.setTextColor(WHITE, BLACK); 
  } else {
    display.setTextColor(BLACK, WHITE);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 15);
  display.println("TIME");
  display.setCursor(25, 30);
  display.println("UP!");
  display.display();

  if (registeredTaps > 0 || longPress) {
    display.setTextColor(BLACK); 
    display.setTextSize(1);
    currentState = HOME;
  }
}

void runScreensaver() {
  display.clearDisplay();
  // Draw the static emblem exactly in the center
  display.drawBitmap(18, 0, shield_bitmap, shield_width, shield_height, BLACK);
  display.display();

  // Wake up immediately if the encoder is turned, clicked, or long pressed
  if (encoderCount != lastEncoderCount || registeredTaps > 0 || longPress) {
    lastActivityTime = millis();
    encoderCount = 0;
    lastEncoderCount = 0;
    currentState = HOME;
    display.clearDisplay();
  }
}

void runOtaMode() {
  if (!otaStarted) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("OTA MODE ON");
    display.println("Open Safari:");
    display.println(WiFi.localIP().toString());
    display.display();

    server.on("/", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", serverIndex);
    });

    server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "UPDATE FAILED! Rebooting..." : "SUCCESS! Restarting Jarvis...");
      delay(2000);
      ESP.restart();
    }, []() {
      HTTPUpload& upload = server.upload();
      
      if (upload.status == UPLOAD_FILE_START) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Receiving...");
        display.display();
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
          Update.printError(Serial); 
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println("DONE!");
          display.println("Rebooting...");
          display.display();
        }
      }
    });

    server.begin();
    otaStarted = true;
  }

  server.handleClient();

  if (longPress) {
    server.stop();
    otaStarted = false;
    currentState = MENU;
  }
}

void runMusic() {
  int delta = getEncoderDelta();

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(12, 16); 
  display.print("MUSIC");
  display.setTextSize(1); 
  display.display();

  if (delta > 0) bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
  if (delta < 0) bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
  
  if (registeredTaps == 1) bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
  if (registeredTaps == 2) bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
  if (registeredTaps == 3) bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
  
  if (longPress) {
    currentState = MENU;
  }
}
