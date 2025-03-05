#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h> EthernetClient client = server.available();
    if (client) {
        Serial.println("new client");
        // an http request ends with a blank line
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c);
                // if you've gotten to the end of the line (received a newline
                // character) and the line is blank, the http request has ended,
                // so you can send a reply
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: close");  // the connection will be closed after completion of the response
                    client.println("Refresh: 5");  // refresh the page automatically every 5 sec
                    client.println();
                    client.println("<!DOCTYPE HTML>");
                    client.println("<html>");
                    
                    // Tampilkan data debu
                    client.println("<h1>Dust Sensor Monitor</h1>");
                    client.print("<p>Dust Density: ");
                    client.print(latestDustDensity);
                    client.println(" ug/m3</p>");
                    
                    // Tampilkan status alarm
                    client.print("<p>Alarm Status: ");
                    client.print(alarmDisabled ? "Disabled" : "Active");
                    client.println("</p>");
                    
                    // Tambahkan raw analog reading jika diperlukan
                    client.print("<p>Raw Analog Reading: ");
                    client.print(analogRead(sharpVoPin));
                    client.println("</p>");
                    
                    client.println("</html>");
                    break;
                }
                if (c == '\n') {
                    // you're starting a new line
                    currentLineIsBlank = true;
                }
                else if (c != '\r') {
                    // you've gotten a character on the current line
                    currentLineIsBlank = false;
                }
            }
        }
        // give the web browser time to receive the data
        delay(1);
        // close the connection:
        client.stop();
        Serial.println("client disconnected");
    }
    
    delay(1000);
}
#include <Wire.h>
#include <FS.h>
#include <TimeLib.h>

// Pin untuk sensor debu
const int sharpLEDPin = D5;
const int sharpVoPin = A0;
const int alarmPin = D6;
const int buttonPin = D3;

// Serial komunikasi ke CH9121
SoftwareSerial ch9121Serial(D7, D8);

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Konstanta dan variabel untuk sensor
const float Voc = 0.6;
const float K = 0.5;
const int dustThreshold = 50;
bool alarmDisabled = false;
unsigned long alarmDisableTime = 0;
unsigned long lastButtonPress = 0;
const int debounceDelay = 200;

// Struktur untuk menyimpan data historis
struct DustRecord {
    unsigned long timestamp;
    float dustDensity;
};

// Array untuk menyimpan data sementara
const int MAX_HISTORY = 144; // Menyimpan data 24 jam (dengan interval 10 menit)
DustRecord dustHistory[MAX_HISTORY];
int historyIndex = 0;

// Fungsi untuk menyimpan data ke SPIFFS dengan timestamp
void saveToSPIFFS(float dustDensity) {
    char filename[32];
    sprintf(filename, "/dust_%lu.txt", now());
    
    File file = SPIFFS.open(filename, "a");
    if (!file) {
        Serial.println("Gagal membuka file");
        return;
    }
    
    // Format: timestamp,dust_density
    file.print(now());
    file.print(",");
    file.println(dustDensity);
    file.close();
    
    // Simpan ke array circular buffer
    dustHistory[historyIndex].timestamp = now();
    dustHistory[historyIndex].dustDensity = dustDensity;
    historyIndex = (historyIndex + 1) % MAX_HISTORY;
}

// Fungsi untuk membaca data historis
String readHistoricalData(unsigned long startTime, unsigned long endTime) {
    String data = "";
    
    // Baca dari array circular buffer terlebih dahulu (data terbaru)
    for (int i = 0; i < MAX_HISTORY; i++) {
        if (dustHistory[i].timestamp >= startTime && 
            dustHistory[i].timestamp <= endTime) {
            data += String(dustHistory[i].timestamp);
            data += ",";
            data += String(dustHistory[i].dustDensity);
            data += ";";
        }
    }
    
    return data;
}

// Fungsi untuk memproses perintah dari Virtuino
void processVirtuinoCommand(String command) {
    // Format perintah: GET_HISTORY:starttime:endtime
    if (command.startsWith("GET_HISTORY")) {
        int firstColon = command.indexOf(':');
        int secondColon = command.indexOf(':', firstColon + 1);
        
        if (firstColon > 0 && secondColon > 0) {
            unsigned long startTime = command.substring(firstColon + 1, secondColon).toInt();
            unsigned long endTime = command.substring(secondColon + 1).toInt();
            
            String historicalData = readHistoricalData(startTime, endTime);
            
            // Kirim data historis ke Virtuino
            ch9121Serial.print("H0=");
            ch9121Serial.print(historicalData);
            ch9121Serial.print(";");
        }
    }
}

void setup() {
    pinMode(sharpLEDPin, OUTPUT);
    pinMode(alarmPin, OUTPUT);
    pinMode(buttonPin, INPUT_PULLUP);
    
    Serial.begin(115200);
    ch9121Serial.begin(9600);
    
    lcd.init();
    lcd.backlight();
    
    if (!SPIFFS.begin()) {
        Serial.println("Gagal mount SPIFFS");
        return;
    }
    
    // Set waktu awal (ganti sesuai kebutuhan)
    setTime(0);
    
    Serial.println("ESP8266 GP2Y1014AU dengan Virtuino dan History via CH9121");
}

void loop() {
    // Baca data dari Virtuino jika tersedia
    if (ch9121Serial.available()) {
        String command = ch9121Serial.readStringUntil('\n');
        processVirtuinoCommand(command);
    }
    
    // Nyalakan LED sensor
    digitalWrite(sharpLEDPin, LOW);
    delayMicroseconds(280);
    
    // Baca nilai analog
    int VoRaw = analogRead(sharpVoPin);
    digitalWrite(sharpLEDPin, HIGH);
    delayMicroseconds(9620);
    
    // Konversi nilai ke voltase
    float Vo = VoRaw / 1024.0 * 3.3;
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
    
    // Simpan data
    saveToSPIFFS(dustDensity);
    
    // Cek tombol alarm
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
    
    // Kontrol alarm
    if (dustDensity > dustThreshold && !alarmDisabled) {
        digitalWrite(alarmPin, HIGH);
    } else {
        digitalWrite(alarmPin, LOW);
    }
    
    // Kirim data real-time ke Virtuino
    ch9121Serial.print("V0=");
    ch9121Serial.print(dustDensity);
    ch9121Serial.print(";");
    
    delay(1000);
}
