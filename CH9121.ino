#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <FS.h> // SPIFFS untuk penyimpanan data
#include "VirtuinoESP.h"

// Konfigurasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Konfigurasi CH9121 (Menggunakan SoftwareSerial)
#define CH9121_TX D7
#define CH9121_RX D6
SoftwareSerial ethSerial(CH9121_RX, CH9121_TX); // RX, TX

// Pin Konfigurasi Sensor Debu GP2Y1014AU
#define DUST_SENSOR A0
#define LED_DUST D1  // LED pemancar GP2Y1014AU
#define BUZZER D3
#define RESET_BUTTON D4

// Variabel Global
float dustDensity = 0.0;
bool alarmActive = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long alarmMuteStart = 0;
bool alarmMuted = false;

// Konfigurasi Virtuino
VirtuinoESP virtuino;
#define VIRTUINO_PASSWORD "1234"

void setup() {
    Serial.begin(115200);
    ethSerial.begin(19200); // Sesuaikan baud rate dengan CH9121

    Wire.begin();
    pinMode(DUST_SENSOR, INPUT);
    pinMode(LED_DUST, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    pinMode(RESET_BUTTON, INPUT_PULLUP);
    
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Monitoring...");

    if (!SPIFFS.begin()) {
        Serial.println("Gagal mount SPIFFS");
    }

    Serial.println("Ketik 'reset' untuk mematikan alarm.");

    // Inisialisasi Virtuino
    virtuino.begin(ethSerial);
}

void loop() {
    // Menyalakan LED GP2Y1014AU sebelum pembacaan sensor
    digitalWrite(LED_DUST, LOW);
    delayMicroseconds(280);  // Sesuai dengan datasheet

    // Membaca data dari sensor
    int rawValue = analogRead(DUST_SENSOR);
    
    // Mematikan LED setelah pembacaan
    digitalWrite(LED_DUST, HIGH);
    delayMicroseconds(9620);  // Sesuai datasheet
    
    // Konversi data sensor menjadi konsentrasi debu (ug/m3)
    dustDensity = map(rawValue, 0, 1023, 0, 500);

    // Simpan data ke SPIFFS
    saveDataToSPIFFS(dustDensity);
    
    // Tampilkan di LCD
    lcd.setCursor(0, 1);
    lcd.print("Debu: ");
    lcd.print(dustDensity);
    lcd.print(" ug/m3 ");

    // Tampilkan di Serial Monitor
    Serial.print("Debu: ");
    Serial.print(dustDensity);
    Serial.println(" ug/m3");

    // Update nilai sensor ke Virtuino (V0)
    virtuino.vMemoryWrite(0, dustDensity);

    // Logika alarm jika debu melebihi batas
    if (dustDensity > 50 && !alarmMuted) {
        digitalWrite(BUZZER, HIGH);
        alarmActive = true;
    } else if (dustDensity < 50 && alarmActive) {
        digitalWrite(BUZZER, LOW);
        alarmActive = false;
    }

    // Deteksi tombol reset untuk mute alarm
    if (digitalRead(RESET_BUTTON) == LOW && (millis() - lastDebounceTime) > debounceDelay) {
        lastDebounceTime = millis();
        muteAlarm();
    }

    // Reset alarm via Serial
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.equalsIgnoreCase("reset")) {
            muteAlarm();
            while (Serial.available() > 0) Serial.read();
        }
    }

    // Aktifkan kembali alarm setelah 10 menit jika sebelumnya dimatikan
    if (alarmMuted && millis() - alarmMuteStart >= 600000) {
        alarmMuted = false;
        Serial.println("Alarm kembali aktif.");
    }

    // Membaca kontrol Virtuino (V1) untuk menyalakan/mematikan alarm
    if (virtuino.vMemoryRead(1) == 1) {
        muteAlarm();
        virtuino.vMemoryWrite(1, 0);  // Reset nilai setelah digunakan
    }

    // Jalankan komunikasi Virtuino
    virtuino.run();
    delay(2000);
}

void saveDataToSPIFFS(float value) {
    File file = SPIFFS.open("/dust_log.txt", "a");
    if (!file) {
        Serial.println("Gagal membuka file");
        return;
    }
    file.println(value);
    file.close();
}

void muteAlarm() {
    digitalWrite(BUZZER, LOW);
    alarmActive = false;
    alarmMuted = true;
    alarmMuteStart = millis();
    Serial.println("Alarm dimatikan sementara selama 10 menit.");
}
