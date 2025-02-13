#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "MAX30105.h"
#include <LittleFS.h>
#include <DS3231.h>
#include <RTClib.h>
#include "heartRate.h"

// Sensor and RTC Objects
MAX30105 particleSensor;
DS3231 rtc;

// LCD Configuration
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Button Pins and Debouncing
#define PUSH_BUTTON_1 18
#define PUSH_BUTTON_3 5
#define DEBOUNCE_DELAY 50

// LittleFS Configuration
#define FORMAT_LITTLEFS_IF_FAILED true
const char* var2Path = "/data/value.txt";

// Button Structure
struct Button {
  int pin;
  int state;
  int lastState;
  unsigned long lastDebounceTime;
  bool isPressed;
};

// Age Constants
const uint8_t AGE_MIN = 20;
const uint8_t AGE_MAX = 52;

// Menu Variables
uint8_t menuIndex = 0;
const String menu[] = {"Mulai Ukur", "Upload Data", "Setting WiFi"};
const int MENU_SIZE = sizeof(menu) / sizeof(menu[0]);

// State Variables
bool inSubMenu = false;
bool isSettingAge = false;
bool shouldRunBacaSensor = false;
bool selesai_baca = false;
bool initialized = false;
unsigned long lastButtonPressTime = 0;
const unsigned long BUTTON_DELAY = 200;
uint8_t umur = AGE_MIN;

// Button Instances
Button button1 = {PUSH_BUTTON_1, HIGH, HIGH, 0, false};
Button button3 = {PUSH_BUTTON_3, HIGH, HIGH, 0, false};

// Measurement Constants
const uint8_t GLU_MIN = 82;
const uint8_t GLU_MAX = 396;
const uint8_t CHOL_MIN = 139;
const uint8_t CHOL_MAX = 223;
const float ACD_MIN = 3.8;
const float ACD_MAX = 7.0;

// Age-Related Constants (Refactor to use a struct or array if possible)
const uint8_t AGE_MIN_1 = 20;
const uint8_t AGE_MAX_1 = 52;
const uint8_t AGE_MIN_2 = 20;
const uint8_t AGE_MAX_2 = 52;
const uint8_t AGE_MIN_3 = 20;
const uint8_t AGE_MAX_3 = 52;

// Heart Rate Constants
const uint8_t HR_MIN_1 = 66;
const uint8_t HR_MAX_1 = 112;
const uint8_t HR_MIN_2 = 66;
const uint8_t HR_MAX_2 = 112;
const uint8_t HR_MIN_3 = 66;
const uint8_t HR_MAX_3 = 112;

// IR Constants
const uint32_t IR_MIN_1 = 86765;
const uint32_t IR_MAX_1 = 107756;
const uint32_t IR_MIN_2 = 86765;
const uint32_t IR_MAX_2 = 107756;
const uint32_t IR_MIN_3 = 86765;
const uint32_t IR_MAX_3 = 107756;

// Regression Coefficients (Refactor to use a struct or array if possible)
// GLU
const float AGE_OMEGA_1 = -0.04673855434301688;
const float IR_OMEGA_1 = -0.4197309962951855;
const float HR_OMEGA_1 = -0.2835012140361256;
const float BIAS_1 = -0.8315069733269372;

// CHOL
const float AGE_OMEGA_2 = 0.00850885812967044;
const float IR_OMEGA_2 = -0.09736722986940606;
const float HR_OMEGA_2 = -0.012682509022442733;
const float BIAS_2 = -0.176179293760974;

// ACD
const float AGE_OMEGA_3 = 0.31296509375281173;
const float IR_OMEGA_3 = -0.34460762103122644;
const float HR_OMEGA_3 = -0.18902828792276205;
const float BIAS_3 = -0.42445516595368277;

// Scaled Values
double scaledHR_1, scaledIR_1, scaledUmur_1;
double scaledHR_2, scaledIR_2, scaledUmur_2;
double scaledHR_3, scaledIR_3, scaledUmur_3;

// Intermediate Calculation Variables
double findValue_1, findValue_2, findValue_3;
double sigmoid_1, sigmoid_2, sigmoid_3;

// Measurement Results
uint8_t GLU, CHOL;
float ACD;

// Heart Rate Measurement Variables
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte irValues[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;
uint32_t irAvg;
int HR = 0;
uint64_t IR = 0;
bool fingerDetected = false;
unsigned long interval;
int avgir, avgbpm;
bool tampilkan_hasil;

//---------------------------------------------------------------------
// Function Prototypes
//---------------------------------------------------------------------

// File System Functions
void initLittleFS();
void createDir(fs::FS &fs, const char *path);
void appendToFile(fs::FS &fs, const char *path, const char *message);
void deleteFile(fs::FS &fs, const char *path);
String getTimestamp();
String readFile2(fs::FS &fs, const char *path);

// Data Processing Functions
double minMaxScaling(double value, double min, double max);
uint8_t RE_minMaxScaling(double value, double min, double max);
float REE_minMaxScaling(double value, double min, double max);
double findValue(double value1, double value2, double value3, double omega1, double omega2, double omega3, double omega4);
double sigmoid(double value);

// Sensor Reading Function
void bacasensorStep();

// LCD and Menu Functions
void showMainMenu();
void returnToMainMenu();
void melihat();
void handleSubMenu();
void handleMainMenu();
void updateAgeDisplay();
bool debounceButton(Button &button);
void handleAgeInput(bool button1Pressed, bool button3Pressed);

//---------------------------------------------------------------------
// File System Functions
//---------------------------------------------------------------------

void initLittleFS() {
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("Failed to mount LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");
}

void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void appendToFile(fs::FS &fs, const char *path, const char *message) {
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

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\r\n", path);
  if (fs.remove(path)) {
    Serial.println("- file deleted");
  } else {
    Serial.println("- delete failed");
  }
}

String getTimestamp() {
  char dateBuffer[25];
  bool isCentury = false;
  bool is12HourFormat = false;
  bool isPM = false;

  snprintf(dateBuffer, sizeof(dateBuffer), "%04u-%02u-%02uT%02u:%02u:%02u+07:00",
           rtc.getYear() + 2000,
           rtc.getMonth(isCentury),
           rtc.getDate(),
           rtc.getHour(is12HourFormat, isPM),
           rtc.getMinute(),
           rtc.getSecond());
  return String(dateBuffer);
}

String readFile2(fs::FS &fs, const char *path) {
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

//---------------------------------------------------------------------
// Data Processing Functions
//---------------------------------------------------------------------

double minMaxScaling(double value, double min, double max) {
  return (value - min) / (max - min);
}

uint8_t RE_minMaxScaling(double value, double min, double max) {
  return static_cast<uint8_t>((value * (max - min)) + min);
}

float REE_minMaxScaling(double value, double min, double max) {
  return (value * (max - min)) + min;
}

double findValue(double value1, double value2, double value3, double omega1, double omega2, double omega3, double omega4) {
  return ((value1 * omega1) + (value2 * omega2) + (value3 * omega3) + omega4);
}

double sigmoid(double value) {
  return 1.0 / (1.0 + exp(-value));
}

//---------------------------------------------------------------------
// Sensor Reading Function
//---------------------------------------------------------------------

void bacasensorStep() {
  long irValue = particleSensor.getIR();

  if (irValue > 50000 && !fingerDetected) {
    delay(500);
    particleSensor.setPulseAmplitudeRed(64);
    fingerDetected = true;
    beatsPerMinute = 0;
    beatAvg = 0;
    irAvg = 0;
    interval = millis();
  } else if (irValue <= 50000 && fingerDetected) {
    delay(500);
    particleSensor.setPulseAmplitudeRed(0x06);
    fingerDetected = false;
  }

  if (fingerDetected) {
    if (millis() - interval <= 30000) {
      tampilkan_hasil = false;

      irValues[rateSpot] = irValue;

      if (checkForBeat(irValue) == true) {
        long delta = millis() - lastBeat;
        lastBeat = millis();

        beatsPerMinute = 60 / (delta / 1000.0);

        if (beatsPerMinute < 180 && beatsPerMinute > 50) {
          rates[rateSpot] = (byte)beatsPerMinute;
          rateSpot = (rateSpot + 1) % RATE_SIZE;

          beatAvg = 0;
          for (byte x = 0; x < RATE_SIZE; x++) {
            beatAvg += rates[x];
          }
          beatAvg /= RATE_SIZE;

          irAvg = 0;
          for (byte x = 0; x < RATE_SIZE; x++) {
            irAvg += irValue;
          }
          irAvg /= RATE_SIZE;
        }
      }
      int progress = map(millis() - interval, 0, 30000, 0, 100);
      lcd.setCursor(0, 0);
      lcd.print("Mengukur : ");
      lcd.setCursor(0, 1);
      lcd.print(progress);
      lcd.print("%");
    } else {
      avgbpm = beatAvg;
      avgir = irAvg;
      HR = avgbpm;
      IR = avgir;
      umur = umur; 
      delay(100);
      tampilkan_hasil = true;
    }
  }

  if (tampilkan_hasil) {
    Serial.print("IR: ");
    Serial.println(IR);
    Serial.print("BPM: ");
    Serial.println(HR);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IR: ");
    lcd.print(IR);
    lcd.setCursor(0, 1);
    lcd.print("HR: ");
    lcd.print(HR);
    lcd.print(" bpm");
    delay(2000);
    selesai_baca = true;
  } else {
    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.print(beatAvg);
    Serial.print(", Avg IR=");
    Serial.print(irAvg);

    if (!fingerDetected)
      Serial.print(" (No finger detected)");
    Serial.println();
  }

  if (selesai_baca) {
    scaledHR_1 = minMaxScaling(HR, HR_MIN_1, HR_MAX_1);
    scaledIR_1 = minMaxScaling(IR, IR_MIN_1, IR_MAX_1);
    scaledUmur_1 = minMaxScaling(umur, AGE_MIN_1, AGE_MAX_1);
    scaledHR_2 = minMaxScaling(HR, HR_MIN_2, HR_MAX_2);
    scaledIR_2 = minMaxScaling(IR, IR_MIN_2, IR_MAX_2);
    scaledUmur_2 = minMaxScaling(umur, AGE_MIN_2, AGE_MAX_2);
    scaledHR_3 = minMaxScaling(HR, HR_MIN_3, HR_MAX_3);
    scaledIR_3 = minMaxScaling(IR, IR_MIN_3, IR_MAX_3);
    scaledUmur_3 = minMaxScaling(umur, AGE_MIN_3, AGE_MAX_3);

    findValue_1 = findValue(scaledHR_1, scaledIR_1, scaledUmur_1, HR_OMEGA_1, IR_OMEGA_1, AGE_OMEGA_1, BIAS_1);
    findValue_2 = findValue(scaledHR_2, scaledIR_2, scaledUmur_2, HR_OMEGA_2, IR_OMEGA_2, AGE_OMEGA_2, BIAS_2);
    findValue_3 = findValue(scaledHR_3, scaledIR_3, scaledUmur_3, HR_OMEGA_3, IR_OMEGA_3, AGE_OMEGA_3, BIAS_3);

    sigmoid_1 = sigmoid(findValue_1);
    sigmoid_2 = sigmoid(findValue_2);
    sigmoid_3 = sigmoid(findValue_3);

    GLU = RE_minMaxScaling(sigmoid_1, GLU_MIN, GLU_MAX);
    CHOL = RE_minMaxScaling(sigmoid_2, CHOL_MIN, CHOL_MAX);
    ACD = REE_minMaxScaling(sigmoid_3, ACD_MIN, ACD_MAX);

    String dataToSave1 = "Gula Darah: " + String(GLU) + ", Timestamp: " + getTimestamp();
    appendToFile(LittleFS, var2Path, dataToSave1.c_str());
    String dataToSave2 = "Kolestrol: " + String(CHOL) + ", Timestamp: " + getTimestamp();
    appendToFile(LittleFS, var2Path, dataToSave2.c_str());
    String dataToSave3 = "Asam Urat: " + String(ACD) + ", Timestamp: " + getTimestamp();
    appendToFile(LittleFS, var2Path, dataToSave3.c_str());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Glu: ");
    lcd.print(GLU);
    lcd.print(" mg/dL");
    lcd.setCursor(0, 1);
    lcd.print("Acid: ");
    lcd.print(ACD, 1);
    lcd.print(" mg/dL");
    delay(3000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Chol: ");
    lcd.print(CHOL);
    lcd.print(" mg/dL");
    delay(3000);
    returnToMainMenu();
  }
}

//---------------------------------------------------------------------
// LCD and Menu Functions
//---------------------------------------------------------------------

void showMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("#==== MENU ====#");
  lcd.setCursor(0, 1);
  lcd.print(menu[menuIndex]);
}

void returnToMainMenu() {
  inSubMenu = false;
  isSettingAge = false;
  shouldRunBacaSensor = false;
  selesai_baca = false;
  showMainMenu();
  rateSpot = 0;
  lastBeat = 0;
  fingerDetected = false;
  beatsPerMinute = 0;
  beatAvg = 0;
  irAvg = 0;
}

void melihat() {
  Serial.println();
  Serial.println("Nilai Mentah");
  Serial.print("IR: ");
  Serial.println(IR);
  Serial.print("HR: ");
  Serial.println(HR);
  Serial.print("Umur: ");
  Serial.println(umur);
  Serial.println();
  Serial.println("Gula Darah");
  Serial.print("IR Min-Max Scaling: ");
  Serial.println(scaledHR_1);
  Serial.print("HR Min-Max Scaling: ");
  Serial.println(scaledIR_1);
  Serial.print("Umur Min-Max Scaling: ");
  Serial.println(scaledUmur_1);
  Serial.print("Nilai Y Gula Darah: ");
  Serial.println(sigmoid_1);
  Serial.print("Prediksi Gula Darah: ");
  Serial.println(GLU);
  Serial.println();
  Serial.println("Asam Urat");
  Serial.print("IR Min-Max Scaling: ");
  Serial.println(scaledHR_3);
  Serial.print("HR Min-Max Scaling: ");
  Serial.println(scaledIR_3);
  Serial.print("Umur Min-Max Scaling: ");
  Serial.println(scaledUmur_3);
  Serial.print("Nilai Y Asam urat: ");
  Serial.println(sigmoid_3);
  Serial.print("Prediksi Asam urat: ");
  Serial.println(ACD);
  Serial.println();
  Serial.println("Kolestrol");
  Serial.print("IR Min-Max Scaling: ");
  Serial.println(scaledHR_2);
  Serial.print("HR Min-Max Scaling: ");
  Serial.println(scaledIR_2);
  Serial.print("Umur Min-Max Scaling: ");
  Serial.println(scaledUmur_2);
  Serial.print("Nilai Y Kolestrol: ");
  Serial.println(sigmoid_2);
  Serial.print("Prediksi Kolestrol: ");
  Serial.println(CHOL);
}

void handleSubMenu() {
  lcd.clear();
  if (menu[menuIndex] == "Mulai Ukur") {
    isSettingAge = true;
    initialized = false;
    handleAgeInput(false, false);
  } else if (menu[menuIndex] == "Upload Data") {
    lcd.print("submenu_2");
    melihat();
  } else if (menu[menuIndex] == "Setting WiFi") {
    lcd.print("submenu_3");
    String storedData = readFile2(LittleFS, var2Path);
    Serial.println();
    Serial.println(storedData);
  }
  inSubMenu = true;
}

void handleMainMenu() {
  if (millis() - lastButtonPressTime >= BUTTON_DELAY) {
    menuIndex = (menuIndex + 1) % MENU_SIZE;
    lcd.setCursor(0, 1);
    lcd.print(menu[menuIndex]);
    lcd.print("                ");
    lastButtonPressTime = millis();
  }
}

void updateAgeDisplay() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(umur);
}

bool debounceButton(Button &button) {
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
      } else if (button.state == HIGH) {
        button.isPressed = false;
      }
    }
  }

  button.lastState = reading;
  return buttonPressed;
}

void handleAgeInput(bool button1Pressed, bool button3Pressed) {
  if (!initialized) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Isi Umur Anda");
    updateAgeDisplay();
    initialized = true;
    return;
  }

  if (shouldRunBacaSensor) {
    return;
  }

  if (button1Pressed && millis() - lastButtonPressTime >= BUTTON_DELAY) {
    umur = (umur >= AGE_MAX) ? AGE_MIN : umur + 1;
    updateAgeDisplay();
    lastButtonPressTime = millis();
  }

  if (button3Pressed) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Umur tersimpan:");
    lcd.setCursor(0, 1);
    lcd.print(umur);
    lcd.print(" tahun");
    delay(1000);
    shouldRunBacaSensor = true;
    lcd.clear();
  }
}

//---------------------------------------------------------------------
// Setup and Loop
//---------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");
  initLittleFS();
  createDir(LittleFS, "/data");
  
  // Initialize RTC
  Wire.begin();
  rtc.begin();

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 not detected. Please check wiring/power.");
    while (1);
  }
  Serial.println("Place your finger on the sensor with steady pressure.");
  lcd.begin(16, 2);
  lcd.backlight();
  pinMode(PUSH_BUTTON_1, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_3, INPUT_PULLUP);
  byte ledBrightness = 45;
  particleSensor.setup(ledBrightness);
  particleSensor.setPulseAmplitudeRed(0x96);
  particleSensor.setPulseAmplitudeGreen(0xff);
  showMainMenu();
}

void loop() {
  bool button1Pressed = debounceButton(button1);
  bool button3Pressed = debounceButton(button3);
  if (!inSubMenu && button1Pressed) {
    handleMainMenu();
  }

  if (isSettingAge) {
    handleAgeInput(button1Pressed, button3Pressed);
  }

  if (button3Pressed) {
    if (!inSubMenu) {
      handleSubMenu();
    } else if (inSubMenu && !isSettingAge) {
      returnToMainMenu();
    }
  }
  if (shouldRunBacaSensor) {
    bacasensorStep();
  }
}
