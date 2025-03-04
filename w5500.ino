#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetServer.h>
#include <FS.h>
#include <TimeLib.h>

// Pin untuk sensor debu
const int sharpLEDPin = D0;
const int sharpVoPin = A0;
const int alarmPin = D3;
const int buttonPin = D4;

// Konfigurasi Ethernet W5500
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 177);
EthernetServer server(80);

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

// Variabel untuk menyimpan data sensor terbaru
float currentDustDensity = 0;

// Struktur untuk menyimpan data historis
struct DustRecord {
    unsigned long timestamp;
    float dustDensity;
};

// Array untuk menyimpan data sementara
const int MAX_HISTORY = 144; // Menyimpan data 24 jam (dengan interval 10 menit)
DustRecord dustHistory[MAX_HISTORY];
int historyIndex = 0;

// Variabel untuk interval penyimpanan data
const unsigned long SAVE_INTERVAL = 600000; // 10 menit
unsigned long lastSaveTime = 0;

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
String getHistoricalDataJSON(unsigned long startTime, unsigned long endTime) {
    String json = "[";
    bool firstEntry = true;
    
    // Baca dari array circular buffer
    for (int i = 0; i < MAX_HISTORY; i++) {
        if (dustHistory[i].timestamp >= startTime && 
            dustHistory[i].timestamp <= endTime &&
            dustHistory[i].timestamp > 0) {
            
            if (!firstEntry) {
                json += ",";
            }
            
            json += "{\"time\":";
            json += dustHistory[i].timestamp;
            json += ",\"value\":";
            json += dustHistory[i].dustDensity;
            json += "}";
            
            firstEntry = false;
        }
    }
    
    json += "]";
    return json;
}

// Fungsi untuk mengirim halaman web
void sendWebPage(EthernetClient client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    
    // HTML head and styling
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
    client.println("<head>");
    client.println("<title>Dust Sensor Monitoring</title>");
    client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    client.println("<style>");
    client.println("body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }");
    client.println(".container { max-width: 800px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }");
    client.println(".card { margin-bottom: 20px; padding: 15px; border-radius: 5px; background-color: #fff; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }");
    client.println(".value { font-size: 36px; font-weight: bold; text-align: center; margin: 10px 0; }");
    client.println(".chart-container { height: 300px; margin-top: 20px; }");
    client.println(".status { padding: 5px 10px; border-radius: 15px; display: inline-block; }");
    client.println(".good { background-color: #d4edda; color: #155724; }");
    client.println(".warning { background-color: #fff3cd; color: #856404; }");
    client.println(".danger { background-color: #f8d7da; color: #721c24; }");
    client.println("button { background-color: #4CAF50; color: white; border: none; padding: 10px 15px; text-align: center; border-radius: 5px; cursor: pointer; }");
    client.println("</style>");
    
    // Add JavaScript for chart
    client.println("<script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.9.4/Chart.min.js'></script>");
    client.println("</head>");
    client.println("<body>");
    
    client.println("<div class='container'>");
    client.println("<h1>Dust Sensor Monitoring</h1>");
    
    // Current readings section
    client.println("<div class='card'>");
    client.println("<h2>Current Dust Density</h2>");
    
    // Status indicator
    client.println("<div class='value'>");
    client.print(currentDustDensity, 2);
    client.println(" µg/m³</div>");
    
    client.print("<div style='text-align: center;'><span class='status ");
    if (currentDustDensity <= 30) {
        client.print("good'>Good");
    } else if (currentDustDensity <= 50) {
        client.print("warning'>Moderate");
    } else {
        client.print("danger'>Unhealthy");
    }
    client.println("</span></div>");
    client.println("</div>");
    
    // Historical data chart
    client.println("<div class='card'>");
    client.println("<h2>Historical Data</h2>");
    client.println("<div>");
    client.println("<button onclick='fetchLastHour()'>Last Hour</button>");
    client.println("<button onclick='fetchLast24Hours()'>Last 24 Hours</button>");
    client.println("</div>");
    client.println("<div class='chart-container'>");
    client.println("<canvas id='dustChart'></canvas>");
    client.println("</div>");
    client.println("</div>");
    
    // JavaScript for fetching and displaying data
    client.println("<script>");
    client.println("let chart;");
    client.println("function initChart(labels, data) {");
    client.println("  const ctx = document.getElementById('dustChart').getContext('2d');");
    client.println("  if (chart) chart.destroy();");
    client.println("  chart = new Chart(ctx, {");
    client.println("    type: 'line',");
    client.println("    data: {");
    client.println("      labels: labels,");
    client.println("      datasets: [{");
    client.println("        label: 'Dust Density (µg/m³)',");
    client.println("        data: data,");
    client.println("        borderColor: 'rgba(75, 192, 192, 1)',");
    client.println("        backgroundColor: 'rgba(75, 192, 192, 0.2)',");
    client.println("        tension: 0.1");
    client.println("      }]");
    client.println("    },");
    client.println("    options: {");
    client.println("      responsive: true,");
    client.println("      maintainAspectRatio: false,");
    client.println("      scales: {");
    client.println("        y: {");
    client.println("          beginAtZero: true");
    client.println("        }");
    client.println("      }");
    client.println("    }");
    client.println("  });");
    client.println("}");
    
    client.println("function formatTime(timestamp) {");
    client.println("  const date = new Date(timestamp * 1000);");
    client.println("  return date.getHours() + ':' + (date.getMinutes() < 10 ? '0' : '') + date.getMinutes();");
    client.println("}");
    
    client.println("function fetchLastHour() {");
    client.println("  const endTime = Math.floor(Date.now() / 1000);");
    client.println("  const startTime = endTime - 3600;");
    client.println("  fetchData(startTime, endTime);");
    client.println("}");
    
    client.println("function fetchLast24Hours() {");
    client.println("  const endTime = Math.floor(Date.now() / 1000);");
    client.println("  const startTime = endTime - 86400;");
    client.println("  fetchData(startTime, endTime);");
    client.println("}");
    
    client.println("function fetchData(startTime, endTime) {");
    client.println("  fetch(`/data?start=${startTime}&end=${endTime}`)");
    client.println("    .then(response => response.json())");
    client.println("    .then(data => {");
    client.println("      const labels = data.map(item => formatTime(item.time));");
    client.println("      const values = data.map(item => item.value);");
    client.println("      initChart(labels, values);");
    client.println("    });");
    client.println("}");
    
    client.println("// Auto-refresh current data every 10 seconds");
    client.println("setInterval(() => {");
    client.println("  fetch('/current')");
    client.println("    .then(response => response.text())");
    client.println("    .then(data => {");
    client.println("      document.querySelector('.value').innerText = data + ' µg/m³';");
    client.println("      // Update status class");
    client.println("      const value = parseFloat(data);");
    client.println("      const statusElem = document.querySelector('.status');");
    client.println("      statusElem.classList.remove('good', 'warning', 'danger');");
    client.println("      if (value <= 30) {");
    client.println("        statusElem.classList.add('good');");
    client.println("        statusElem.innerText = 'Good';");
    client.println("      } else if (value <= 50) {");
    client.println("        statusElem.classList.add('warning');");
    client.println("        statusElem.innerText = 'Moderate';");
    client.println("      } else {");
    client.println("        statusElem.classList.add('danger');");
    client.println("        statusElem.innerText = 'Unhealthy';");
    client.println("      }");
    client.println("    });");
    client.println("}, 10000);");
    
    client.println("// Initialize with last hour data");
    client.println("document.addEventListener('DOMContentLoaded', fetchLastHour);");
    client.println("</script>");
    
    client.println("</div>");
    client.println("</body>");
    client.println("</html>");
}

// Fungsi untuk menghandle client request
void handleClient(EthernetClient client) {
    // Baca request pertama
    String request = client.readStringUntil('\r');
    client.readStringUntil('\n');
    
    // Parse request
    if (request.indexOf("GET /data") != -1) {
        // Handling untuk request data historis
        int startIdx = request.indexOf("start=") + 6;
        int endStartIdx = request.indexOf("&", startIdx);
        int endIdx = request.indexOf("end=") + 4;
        int endEndIdx = request.indexOf(" ", endIdx);
        
        unsigned long startTime = request.substring(startIdx, endStartIdx).toInt();
        unsigned long endTime = request.substring(endIdx, endEndIdx).toInt();
        
        String jsonData = getHistoricalDataJSON(startTime, endTime);
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println(jsonData);
    } 
    else if (request.indexOf("GET /current") != -1) {
        // Handling untuk request data saat ini
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/plain");
        client.println("Connection: close");
        client.println();
        client.println(currentDustDensity);
    }
    else {
        // Default - kirim halaman web
        sendWebPage(client);
    }
    
    delay(10);
    client.stop();
}

void setup() {
    pinMode(sharpLEDPin, OUTPUT);
    pinMode(alarmPin, OUTPUT);
    pinMode(buttonPin, INPUT_PULLUP);
    
    Serial.begin(115200);
    
    // Inisialisasi LCD
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Initializing...");
    
    if (!SPIFFS.begin()) {
        Serial.println("Gagal mount SPIFFS");
        lcd.setCursor(0, 1);
        lcd.print("SPIFFS Failed");
        delay(2000);
    }
    
    // Inisialisasi Ethernet W5500
    lcd.setCursor(0, 1);
    lcd.print("Ethernet Init...");
    
    Ethernet.init(D1);  // Sesuaikan pin CS untuk W5500 (ubah sesuai wiring Anda)
    Ethernet.begin(mac, ip);
    
    // Cek koneksi
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("Ethernet shield tidak ditemukan");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("No Ethernet");
        lcd.setCursor(0, 1);
        lcd.print("Shield");
        delay(2000);
    }
    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Kabel Ethernet tidak terhubung");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("No Ethernet");
        lcd.setCursor(0, 1);
        lcd.print("Cable");
        delay(2000);
    }
    
    // Start the server
    server.begin();
    
    // Set waktu awal (ganti sesuai kebutuhan)
    setTime(0);
    
    Serial.print("Server aktif di ");
    Serial.println(Ethernet.localIP());
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Server Ready");
    lcd.setCursor(0, 1);
    lcd.print(Ethernet.localIP());
    delay(2000);
}

void loop() {
    // Baca nilai sensor
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
    currentDustDensity = (dV / K) * 100.0;
    
    // Tampilkan di LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Dust: ");
    lcd.print(currentDustDensity);
    lcd.print(" ug/m3");
    lcd.setCursor(0, 1);
    
    // Display status on LCD
    if (currentDustDensity <= 30) {
        lcd.print("Status: Good");
    } else if (currentDustDensity <= 50) {
        lcd.print("Status: Moderate");
    } else {
        lcd.print("Status: Unhealthy");
    }
    
    // Check if it's time to save data
    unsigned long currentMillis = millis();
    if (currentMillis - lastSaveTime >= SAVE_INTERVAL) {
        saveToSPIFFS(currentDustDensity);
        lastSaveTime = currentMillis;
    }
    
    // Cek tombol alarm
    if (digitalRead(buttonPin) == LOW) {
        if (currentMillis - lastButtonPress > debounceDelay) {
            alarmDisabled = true;
            alarmDisableTime = currentMillis;
            Serial.println("Alarm dimatikan sementara");
            
            // Update LCD
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Alarm: Disabled");
            lcd.setCursor(0, 1);
            lcd.print("for 10 minutes");
            delay(1000);
        }
        lastButtonPress = currentMillis;
    }
    
    // Reset alarm setelah 10 menit
    if (alarmDisabled && currentMillis - alarmDisableTime > 600000) {
        alarmDisabled = false;
        Serial.println("Alarm diaktifkan kembali");
    }
    
    // Kontrol alarm
    if (currentDustDensity > dustThreshold && !alarmDisabled) {
        digitalWrite(alarmPin, HIGH);
    } else {
        digitalWrite(alarmPin, LOW);
    }
    
    // Cek Client Web
    EthernetClient client = server.available();
    if (client) {
        Serial.println("New client");
        handleClient(client);
    }
    
    delay(1000);
}
