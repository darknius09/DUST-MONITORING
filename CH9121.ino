#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <FS.h>

// Pin untuk sensor debu
const int sharpLEDPin = D5; // LED kontrol sensor di GPIO14 (D5 pada NodeMCU)
const int sharpVoPin = A0; // Output analog ke A0 ESP8266
const int alarmPin = D6; // Pin untuk alarm
const int buttonPin = D3; // Tombol untuk mematikan alarm sementara

// Serial komunikasi ke CH9121
SoftwareSerial ch9121Serial(D7, D8); // D7 = RX, D8 = TX (sesuaikan dengan wiring)

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variabel untuk perhitungan
const float Voc = 0.6;  // Tegangan saat tidak ada debu (perlu kalibrasi)
const float K = 0.5;    // Sensitivitas sensor (V per 100 ug/m3)
const int dustThreshold = 50; // Ambang batas debu untuk menyalakan alarm
bool alarmDisabled = false;
unsigned long alarmDisableTime = 0;
unsigned long lastButtonPress = 0;
const int debounceDelay = 200; // Waktu debounce untuk tombol

void saveToSPIFFS(float dustDensity) {
    File file = SPIFFS.open("/dust_data.txt", "a");
    if (!file) {
        Serial.println("Gagal membuka file");
        return;
    }
    file.print("Dust Density: ");
    file.print(dustDensity);
    file.println(" ug/m3");
    file.close();
    Serial.println("Data disimpan ke SPIFFS");
}

void setup() {
    pinMode(sharpLEDPin, OUTPUT);
    pinMode(alarmPin, OUTPUT);
    pinMode(buttonPin, INPUT_PULLUP);
    Serial.begin(115200); // Debugging ke Serial Monitor
    ch9121Serial.begin(9600); // Baudrate komunikasi ke CH9121
    lcd.init();
    lcd.backlight();
    SPIFFS.begin(); // Inisialisasi SPIFFS
    delay(2000);
    Serial.println("ESP8266 GP2Y1014AU dengan Virtuino via CH9121");
}

void loop() {
    // Nyalakan LED sensor
    digitalWrite(sharpLEDPin, LOW);
    delayMicroseconds(280);
    
    // Baca nilai analog
    int VoRaw = analogRead(sharpVoPin);
    digitalWrite(sharpLEDPin, HIGH);
    delayMicroseconds(9620);
    
    // Konversi nilai ke voltase
    float Vo = VoRaw / 1024.0 * 3.3; // ESP8266 A0 hanya sampai 1V, jadi perlu pembagi tegangan jika output sensor 5V
    float dV = Vo - Voc;
    if (dV < 0) dV = 0;
    float dustDensity = (dV / K) * 100.0;
    
    // Cetak ke Serial Monitor
    Serial.print("Dust Density: ");
    Serial.print(dustDensity);
    Serial.println(" ug/m3");
    
    // Tampilkan di LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Dust: ");
    lcd.print(dustDensity);
    lcd.print(" ug/m3");
    
    // Simpan data ke SPIFFS
    saveToSPIFFS(dustDensity);
    
    // Cek tombol untuk mematikan alarm sementara dengan debounce
    if (digitalRead(buttonPin) == LOW) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastButtonPress > debounceDelay) {
            alarmDisabled = true;
            alarmDisableTime = millis();
            Serial.println("Alarm dimatikan sementara");
        }
        lastButtonPress = currentMillis;
    }
    
    // Reset alarm setelah 10 menit
    if (alarmDisabled && millis() - alarmDisableTime > 600000) {
        alarmDisabled = false;
        Serial.println("Alarm diaktifkan kembali");
    }

    // Cek batas debu dan kontrol alarm
    if (dustDensity > dustThreshold && !alarmDisabled) {
        digitalWrite(alarmPin, HIGH);
    } else {
        digitalWrite(alarmPin, LOW);
    }
    
    // Kirim data ke Virtuino melalui CH9121
    ch9121Serial.print("V0=");
    ch9121Serial.print(dustDensity);
    ch9121Serial.print(";");
    
    delay(1000); // Baca data setiap 1 detik
}
