#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 particleSensor;
LiquidCrystal_I2C lcd(0x27, 16, 2); 

const byte RATE_SIZE = 4; 
byte rates[RATE_SIZE];
byte irValues[RATE_SIZE]; 
byte rateSpot = 0;
long lastBeat = 0;

float beatsPerMinute;
int beatAvg;
uint64_t irAvg;
bool fingerDetected = false;

unsigned long interval;
int avgir, avgbpm;
bool tampilkan_hasil;

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");
  lcd.init(); 
  lcd.backlight();
  if (!particleSensor.begin(Wire)) { 
    Serial.println("MAX30105 tidak terdeteksi. Mohon periksa wiring/power.");
    while (1);
  }
  
  Serial.println("Letakkan jari di sensor dengan tekanan stabil.");
  
  byte ledBrightness = 45;
  particleSensor.setup(ledBrightness);
  particleSensor.setPulseAmplitudeRed(0x96); 
  particleSensor.setPulseAmplitudeGreen(0xff); 
}

void loop() {
  long irValue = particleSensor.getIR();

  if (irValue > 50000 && !fingerDetected) { 
    delay(500);
    particleSensor.setPulseAmplitudeRed(64);
    fingerDetected = true;
    beatsPerMinute = 0;
    beatAvg = 0;
    irAvg = 0;
    interval = millis();
  }
  else if(irValue <= 50000 && fingerDetected){
    delay(500);
    particleSensor.setPulseAmplitudeRed(0x06);
    fingerDetected = false;
  }

  if (fingerDetected) {
    if(millis() - interval <= 30000) { 
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
    }
    else {
      tampilkan_hasil = true;
      avgbpm = beatAvg;
      avgir = irAvg;
    }
  }

  if(tampilkan_hasil){
    Serial.print("IR: ");
    Serial.println(irAvg);
    Serial.print("BPM: ");
    Serial.println(beatAvg);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IR: ");
    lcd.print(irAvg); 
    lcd.setCursor(0, 1);
    lcd.print("HR: ");
    lcd.print(beatAvg);
    lcd.print(" bpm");
    delay(7000);
  }
  else {
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
}
