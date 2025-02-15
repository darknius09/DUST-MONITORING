#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <FS.h> // SPIFFS untuk penyimpanan data

// Konfigurasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Konfigurasi CH9121 (Menggunakan SoftwareSerial)
#define CH9121_TX D7
#define CH9121_RX D6
SoftwareSerial ethSerial(CH9121_RX, CH9121_TX); // RX, TX

// Pin Konfigurasi
#define DUST_SENSOR A0
#define BUZZER D3
#define RESET_BUTTON D4

// Variabel Global
float dustDensity = 0.0;
bool alarmActive = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long alarmMuteStart = 0;
bool alarmMuted = false;

void setup() {
    Serial.begin(115200);
    ethSerial.begin(19200); // Sesuaikan baud rate dengan CH9121

    Wire.begin();
    pinMode(DUST_SENSOR, INPUT);
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
}

void loop() {
    int rawValue = analogRead(DUST_SENSOR);
    dustDensity = map(rawValue, 0, 1023, 0, 500);

    saveDataToSPIFFS(dustDensity);
    
    lcd.setCursor(0, 1);
    lcd.print("Debu: ");
    lcd.print(dustDensity);
    lcd.print(" ug/m3 ");

    Serial.print("Debu: ");
    Serial.print(dustDensity);
    Serial.println(" ug/m3");

    if (dustDensity > 50 && !alarmMuted) {
        digitalWrite(BUZZER, HIGH);
        alarmActive = true;
    } else if (dustDensity < 50 && alarmActive) {
        digitalWrite(BUZZER, LOW);
        alarmActive = false;
    }

    if (digitalRead(RESET_BUTTON) == LOW && (millis() - lastDebounceTime) > debounceDelay) {
        lastDebounceTime = millis();
        muteAlarm();
    }

    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.equalsIgnoreCase("reset")) {
            muteAlarm();
            while (Serial.available() > 0) Serial.read();
        }
    }

    if (alarmMuted && millis() - alarmMuteStart >= 600000) {
        alarmMuted = false;
        Serial.println("Alarm kembali aktif.");
    }

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
