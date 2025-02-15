#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <UIPEthernet.h>
#include <SPI.h>
#include <FS.h> // SPIFFS untuk penyimpanan data

// Konfigurasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Konfigurasi Ethernet ENC28J60
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 100);
EthernetServer server(80);

// Pin Konfigurasi
#define DUST_SENSOR A0
#define LED_SENSOR D0
#define BUZZER D3
#define RESET_BUTTON D4

// Variabel Global
float dustDensity = 0.0;
bool alarmActive = false;
unsigned long alarmResetTime = 0;
const unsigned long resetDuration = 600000; // 10 menit dalam milidetik
bool manualReset = false;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    pinMode(DUST_SENSOR, INPUT);
    pinMode(LED_SENSOR, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    pinMode(RESET_BUTTON, INPUT_PULLUP);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Monitoring...");

    Ethernet.begin(mac, ip);
    server.begin();
    Serial.println("Server Ethernet aktif");

    if (!SPIFFS.begin()) {
        Serial.println("Gagal mount SPIFFS");
    }

    Serial.println("Ketik 'reset' untuk mematikan alarm sementara.");
}

void loop() {
    // Aktifkan LED sensor sebelum membaca
    digitalWrite(LED_SENSOR, HIGH);
    delayMicroseconds(280); // Waktu sampling GP2Y1014AU
    int rawValue = analogRead(DUST_SENSOR);
    delayMicroseconds(40);
    digitalWrite(LED_SENSOR, LOW);

    // Konversi ke ug/m3
    dustDensity = map(rawValue, 0, 1023, 0, 500);

    saveDataToSPIFFS(dustDensity);

    lcd.setCursor(0, 1);
    lcd.print("Debu: ");
    lcd.print(dustDensity);
    lcd.print(" ug/m3 ");

    Serial.print("Debu: ");
    Serial.print(dustDensity);
    Serial.println(" ug/m3");

    // Cek kondisi alarm
    if (dustDensity > 50 && !manualReset) {
        alarmActive = true;
        alarmResetTime = 0; // Reset timer
    } 
    else if (dustDensity < 50 && alarmResetTime == 0) {
        alarmActive = false;
    }

    // Kontrol buzzer
    if (alarmActive) {
        digitalWrite(BUZZER, HIGH);
    } else {
        digitalWrite(BUZZER, LOW);
    }

    // Cek tombol reset
    if (digitalRead(RESET_BUTTON) == LOW) {
        resetAlarm();
    }

    // Cek input dari Serial Monitor
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.equalsIgnoreCase("reset")) {
            resetAlarm();
            while (Serial.available() > 0) Serial.read();
        }
    }

    // Timer untuk reset manual 10 menit
    if (manualReset && millis() - alarmResetTime >= resetDuration) {
        manualReset = false;
        Serial.println("Reset manual selesai, alarm aktif kembali.");
    }

    handleClient();
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

void handleClient() {
    EthernetClient client = server.available();
    if (client) {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<html><body>");
        client.print("<h1>Monitoring Debu</h1>");
        client.print("<p>Debu: ");
        client.print(dustDensity);
        client.println(" ug/m3</p>");
        client.println("</body></html>");
        client.stop();
    }
}

void resetAlarm() {
    alarmActive = false;
    digitalWrite(BUZZER, LOW);
    manualReset = true;
    alarmResetTime = millis();
    Serial.println("Alarm dimatikan sementara selama 10 menit.");
}
