#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 particleSensor;

// Konfigurasi Push Button
#define PUSH_BUTTON_1 18
#define PUSH_BUTTON_3 5
#define DEBOUNCE_DELAY 50

struct Button {
    int pin;
    int state;
    int lastState;
    unsigned long lastDebounceTime;
    bool isPressed;
};

// Timing Variables
unsigned long startTime;

// Button Initializations
Button button1 = {PUSH_BUTTON_1, HIGH, HIGH, 0, false}; 
Button button3 = {PUSH_BUTTON_3, HIGH, HIGH, 0, false}; 

unsigned long lastButtonPressTime = 0;
const unsigned long BUTTON_DELAY = 200;

// Variabel Pengukuran
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

// Fungsi Reset Measurement
void resetMeasurement() {
    fingerDetected = false;
    beatsPerMinute = 0;
    beatAvg = 0;
    irAvg = 0;
    interval = millis();
    Serial.println("Measurement reset.");
}

// Fungsi untuk Membaca Tombol dengan Debouncing
void readButton(Button &button) {
    int reading = digitalRead(button.pin);

    // Deteksi Perubahan Status Tombol
    if (reading != button.lastState) {
        button.lastDebounceTime = millis();
    }

    // Jika Sudah Melewati Waktu Debounce
    if ((millis() - button.lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != button.state) {
            button.state = reading;

            // Jika Tombol Ditekan (LOW)
            if (button.state == LOW) {
                button.isPressed = true;
            } else {
                button.isPressed = false;
            }
        }
    }

    button.lastState = reading;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Initializing...");

    // Inisialisasi Sensor MAX30105
    if (!particleSensor.begin(Wire)) { 
        Serial.println("MAX30105 tidak terdeteksi. Mohon periksa wiring/power.");
        while (1);
    }

    Serial.println("Letakkan jari di sensor dengan tekanan stabil.");

    byte ledBrightness = 45;
    particleSensor.setup(ledBrightness);
    particleSensor.setPulseAmplitudeRed(0x96); 
    particleSensor.setPulseAmplitudeGreen(0xff); 

    // Konfigurasi Pin Tombol
    pinMode(button1.pin, INPUT_PULLUP);
    pinMode(button3.pin, INPUT_PULLUP);
}

void loop() {
    // Baca Tombol
    readButton(button1);

    // Jika Tombol 1 Ditekan, Panggil resetMeasurement()
    if (button1.isPressed && (millis() - lastButtonPressTime > BUTTON_DELAY)) {
        resetMeasurement();
        lastButtonPressTime = millis(); // Update Waktu Terakhir Penekanan Tombol
    }

    // Logika Pengukuran IR dan HR
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
        delay(100);
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
