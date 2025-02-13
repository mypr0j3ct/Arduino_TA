#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <LittleFS.h>
// ------------------------------------------------------------------//
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

AsyncWebServer server(80);

const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

String ssid;
String pass;
String ip;
String gateway;

const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";

IPAddress localIP;
IPAddress localGateway;
IPAddress subnet(255, 255, 0, 0);

unsigned long previousMillis = 0;
const long interval = 10000;

void startWiFiSetup();
void initWiFi();

// ------------------------------------------------------------------//
#define PUSH_BUTTON_1 18
#define PUSH_BUTTON_3 5
#define DEBOUNCE_DELAY 50
#define FORMAT_LITTLEFS_IF_FAILED true

LiquidCrystal_I2C lcd(0x27, 16, 2); 

struct Button {
    int pin;
    int state;
    int lastState;
    unsigned long lastDebounceTime;
    bool isPressed;
};

unsigned long startTime;
uint8_t menuIndex = 0;
uint8_t cursorRow = 1;
bool inSubMenu = false;
unsigned long lastButtonPressTime = 0;
const unsigned long BUTTON_DELAY = 200;
const String menu[] = {"Mulai Ukur", "Upload Data", "Setting WiFi"};
const int menuSize = sizeof(menu) / sizeof(menu[0]);
const char* var1Path = "/read/data.txt";

Button button1 = {PUSH_BUTTON_1, HIGH, HIGH, 0, false}; 
Button button3 = {PUSH_BUTTON_3, HIGH, HIGH, 0, false}; 

void initLittleFS() {
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("Failed to mount LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");
}

void createDir(fs::FS &fs, const char * path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void appendToFile(fs::FS &fs, const char * path, const char * message) {
    Serial.printf("Appending to file: %s\n", path);
    File file = fs.open(path, FILE_APPEND); 
    if (!file) {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.println(message)) { 
        Serial.println("File appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

String readFile2(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\n", path);
  File file = fs.open(path, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent += file.readString();
  }
  file.close();
  return fileContent;
}

void showMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("#==== MENU ===#");
  lcd.setCursor(0, 1);
  lcd.print(menu[menuIndex]);
}

void handleMainMenu() {
  if (millis() - lastButtonPressTime >= BUTTON_DELAY) {
      menuIndex = (menuIndex + 1) % menuSize;
      lcd.setCursor(0, 1);
      lcd.print(menu[menuIndex]);
      lcd.print("                "); 
      lastButtonPressTime = millis();
  }
}

void handleSubMenu() {
  lcd.clear();
  if (menu[menuIndex] == "Mulai Ukur") {
    lcd.print("submenu_1");
    String storedData = readFile2(LittleFS, var1Path);
    Serial.println();
    Serial.println(storedData);
//--------------------------------//
  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  ip = readFile(LittleFS, ipPath);
  gateway = readFile (LittleFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);
//--------------------------------//    
   } else if (menu[menuIndex] == "Upload Data") {
      lcd.print("submenu_2");
      initWiFi();
  } else if (menu[menuIndex] == "Setting WiFi") {
      lcd.print("Mengatur WiFi...");
      startWiFiSetup(); // Memanggil fungsi pengaturan WiFi
  }
  inSubMenu = true;
}

bool debounceButton(Button& button) {
  int reading = digitalRead(button.pin);
  bool buttonPressed = false;

  if (reading != button.lastState) {
      button.lastDebounceTime = millis();
  }

  if ((millis() - button.lastDebounceTime) > DEBOUNCE_DELAY) {
      if (reading != button.state) {
          button.state = reading;
          if (button.state == LOW && !button.isPressed) { 
              button.isPressed = true;
              buttonPressed = true;
          }
          else if (button.state == HIGH) { 
              button.isPressed = false;
          }
      }
  }

  button.lastState = reading;
  return buttonPressed;
}

void returnToMainMenu() {
  inSubMenu = false;
  showMainMenu();
}

// ------------------------------------------------------------------//

// ------------------------------------------------------------------//

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");
  lcd.init(); 
  lcd.backlight();
  pinMode(PUSH_BUTTON_1, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_3, INPUT_PULLUP);
  initLittleFS();
  createDir(LittleFS, "/confg");
  String storedData = readFile2(LittleFS, var1Path);
  showMainMenu();
  startTime = millis();
}

void loop() {
    bool button1Pressed = debounceButton(button1);
    bool button3Pressed = debounceButton(button3);
    if (!inSubMenu && button1Pressed) {
        handleMainMenu();
    }
    
    if (button3Pressed) {
        if (!inSubMenu && cursorRow == 1) {
            handleSubMenu();
        } else if (inSubMenu) {
            returnToMainMenu();
        }
    }
}

void startWiFiSetup() {
    Serial.println("Setting AP (Access Point)");
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/wifimanager.html", "text/html");
    });

    server.serveStatic("/", LittleFS, "/");
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
        int params = request->params();
        for(int i=0;i<params;i++){
            const AsyncWebParameter* p = request->getParam(i);
            if(p->isPost()){
                if (p->name() == PARAM_INPUT_1) {
                    ssid = p->value().c_str();
                    Serial.print("SSID set to: ");
                    Serial.println(ssid);
                    writeFile(LittleFS, ssidPath, ssid.c_str());
                }
                if (p->name() == PARAM_INPUT_2) {
                    pass = p->value().c_str();
                    Serial.print("Password set to: ");
                    Serial.println(pass);
                    writeFile(LittleFS, passPath, pass.c_str());
                }
                if (p->name() == PARAM_INPUT_3) {
                    ip = p->value().c_str();
                    Serial.print("IP Address set to: ");
                    Serial.println(ip);
                    writeFile(LittleFS, ipPath, ip.c_str());
                }
                if (p->name() == PARAM_INPUT_4) {
                    gateway = p->value().c_str();
                    Serial.print("Gateway set to: ");
                    Serial.println(gateway);
                    writeFile(LittleFS, gatewayPath, gateway.c_str());
                }
            }
        }
        request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
        delay(3000);
        ESP.restart();
    });
    server.begin();
}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

void initWiFi() {
  if(ssid==""){
    Serial.println("Undefined SSID.");
  }
  WiFi.mode(WIFI_STA);
  if (ip != "") {
    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    if (!WiFi.config(localIP, localGateway, subnet)){
        Serial.println("STA Failed to configure");
    }
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");
  unsigned long currentMillis = millis();
  previousMillis = currentMillis;
  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
    }
  }
  Serial.println(WiFi.localIP());
  Serial.println("berhasil terhubung !");
  delay(3000);
  ESP.restart();
}
