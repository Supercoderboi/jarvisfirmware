#include <WiFi.h>
#include <HTTPClient.h> 
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

// Power management headers to stop the 38W cable restart loop
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

const long GMT_OFFSET_SEC = 19800; // IST Timezone
const int DAYLIGHT_OFFSET_SEC = 0;

// ==========================================
// STATE MACHINE & UI VARIABLES
// ==========================================
enum ScreenState { HOME, MENU, JARVIS, SENSORS, TIMER, MUSIC, JARVIS_RESPONSE, OTA_UPDATE };
ScreenState currentState = HOME;

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

// Menu Variables (Added System Update)
const char* menuItems[] = {"Jarvis", "Sensors", "Timer", "Music", "System Update"};
const int numMenuItems = 5;
int menuIndex = 0;

// Jarvis Variables (Added '<' for Backspace)
const char charset[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,!?<";
int charIndex = 0;
String jarvisMessage = "";
String jarvisReply = "";
int jarvisScrollY = 0; 

// Timer Variables
int timerMinutes = 1;
unsigned long timerEndTime = 0;
bool timerRunning = false;

// OTA Variables
WebServer server(80);
bool otaStarted = false;

const char* serverIndex = 
  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
  "<h2 style='font-family:sans-serif;'>Jarvis System Update</h2>"
  "<p style='font-family:sans-serif;'>Select the new .bin file from your iPhone to flash.</p>"
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
  } else if (encval < -3) {
    encoderCount--;
    encval = 0;
  }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  // DISABLE BROWNOUT PANIC (Protects against 38W chargers causing restart loops)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT), readEncoder, CHANGE);

  display.begin();
  display.setContrast(55);
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
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  handleButton();

  switch (currentState) {
    case HOME:            runHome(); break;
    case MENU:            runMenu(); break;
    case JARVIS:          runJarvis(); break;
    case JARVIS_RESPONSE: runJarvisResponse(); break;
    case SENSORS:         runSensors(); break;
    case TIMER:           runTimer(); break;
    case MUSIC:           runMusic(); break;
    case OTA_UPDATE:      runOtaMode(); break;
  }

  registeredTaps = 0;
  longPress = false;
  
  delay(30); 
}

// ==========================================
// INPUT HANDLING
// ==========================================
void handleButton() {
  btnState = !digitalRead(ENC_SW); 
  unsigned long now = millis();
  
  if (btnState && !lastBtnState) {
    btnPressTime = now;
  }
  
  if (!btnState && lastBtnState) {
    unsigned long duration = now - btnPressTime;
    if (duration > 30 && duration < 600) { 
      tapCount++;
      btnReleaseTime = now;
    } else if (duration >= 600) {
      longPress = true;
    }
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
    char dateStr[15];
    
    strftime(timeStr, sizeof(timeStr), "%I:%M %p", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%d %b %Y", &timeinfo);
    
    display.setCursor(12, 10);
    display.print(timeStr);
    display.setCursor(10, 25);
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
  
  for (int i = 0; i < numMenuItems; i++) {
    if (i == menuIndex) display.print(">");
    else display.print(" ");
    
    // Scrolling logic for long menu names like "System Update"
    if (i == menuIndex && strlen(menuItems[i]) > 10) {
      display.println(String(menuItems[i]).substring(0, 10));
    } else {
      display.println(menuItems[i]);
    }
  }
  display.display();

  if (registeredTaps == 1) {
    encoderCount = 0;
    lastEncoderCount = 0;
    if (menuIndex == 0) currentState = JARVIS;
    else if (menuIndex == 1) currentState = SENSORS;
    else if (menuIndex == 2) currentState = TIMER;
    else if (menuIndex == 3) currentState = MUSIC;
    else if (menuIndex == 4) currentState = OTA_UPDATE;
  }
  
  if (longPress) {
    encoderCount = 0;
    lastEncoderCount = 0;
    currentState = HOME;
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
  display.println("Ask Jarvis:");
  
  if (jarvisMessage.length() > 14) {
    display.println(jarvisMessage.substring(jarvisMessage.length() - 14));
  } else {
    display.println(jarvisMessage);
  }
  
  display.println();
  display.print("Char: [");
  display.print(charset[charIndex]);
  display.println("]");
  display.display();

  if (registeredTaps == 1) {
    char selectedChar = charset[charIndex];
    if (selectedChar == '<') {
      // BACKSPACE LOGIC
      if (jarvisMessage.length() > 0) {
        jarvisMessage.remove(jarvisMessage.length() - 1);
      }
    } else {
      jarvisMessage += selectedChar;
    }
  }
  
  if (longPress) {
    sendToJarvis(jarvisMessage);
    jarvisMessage = ""; 
    currentState = JARVIS_RESPONSE;
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
    if (delta > 0) timerMinutes++;
    if (delta < 0 && timerMinutes > 1) timerMinutes--;

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Set Timer:");
    display.print(timerMinutes);
    display.println(" min");
    display.println("(Tap to start)");
    display.display();

    if (registeredTaps == 1) {
      timerEndTime = millis() + (timerMinutes * 60000UL);
      timerRunning = true;
    }
    if (longPress) {
      currentState = MENU;
    }
  } else {
    unsigned long now = millis();
    display.clearDisplay();
    display.setCursor(0, 0);
    
    if (now >= timerEndTime) {
      display.println("TIME'S UP!");
      if (registeredTaps > 0 || longPress) {
        timerRunning = false;
        currentState = MENU;
      }
    } else {
      unsigned long timeLeft = timerEndTime - now;
      int m = (timeLeft / 60000);
      int s = ((timeLeft % 60000) / 1000);
      display.println("Time Left:");
      display.print(m);
      display.print("m ");
      display.print(s);
      display.println("s");
      
      if (registeredTaps == 1 || longPress) {
        timerRunning = false; 
      }
    }
    display.display();
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

// ==========================================
// OTA WEBSERVER MODE
// ==========================================
void runOtaMode() {
  if (!otaStarted) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("OTA MODE ON");
    display.println("Open Safari:");
    display.println(WiFi.localIP().toString());
    display.display();

    // Serve the HTML file upload page
    server.on("/", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", serverIndex);
    });

    // Handle the actual file upload
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

  // Actively listen for the iPhone browser
  server.handleClient();

  // Exit OTA mode with a long press
  if (longPress) {
    server.stop();
    otaStarted = false;
    currentState = MENU;
  }
}
