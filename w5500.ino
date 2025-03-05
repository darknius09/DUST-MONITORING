#include <SPI.h>
#include <Ethernet2.h>
#include <Wire.h>
#include <TimeLib.h>

// Struktur untuk menyimpan data debu
struct DustRecord {
  time_t timestamp;
  float dustDensity;
  float voltage;
};

// Ethernet Configuration
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 177);
EthernetServer server(80);

// Dust Sensor Pins
const int sharpLEDPin = D0;   // ESP8266 pin untuk LED sensor
const int sharpVoPin = A0;    // Pin analog untuk output sensor
const int buzzerPin = D3;     // Pin buzzer untuk alarm

// Konfigurasi Sensor Debu
const float Voc = 0.6;        // Tegangan keluaran tipikal saat tidak ada debu
const float K = 0.5;          // Sensitivitas dalam V per 100ug/m3
const float DUST_THRESHOLD = 50.0; // Ambang batas densitas debu dalam µg/m³

// Array untuk menyimpan data sementara
const int MAX_HISTORY = 144; // Menyimpan data 24 jam (dengan interval 10 menit)
DustRecord dustHistory[MAX_HISTORY];
int historyIndex = 0;

// Variabel Pengukuran Sensor Debu
float dustDensity[6] = {0.0}; // Menyimpan pembacaan untuk hingga 6 sensor/saluran
float voltage[6] = {0.0};     // Menyimpan tegangan untuk hingga 6 sensor/saluran

void setup() {
  // Inisialisasi Komunikasi Serial
  Serial.begin(9600);
  while (!Serial) {
    ; // tunggu port serial terhubung
  }

  // Set waktu awal (Anda bisa mengganti dengan sinkronisasi NTP nanti)
  setTime(0, 0, 0, 1, 1, 2024);  // Set waktu awal ke 1 Jan 2024 00:00:00

  // Inisialisasi Pin Alarm
  pinMode(buzzerPin, OUTPUT);
  pinMode(sharpLEDPin, OUTPUT);

  // Mulai koneksi Ethernet dan server
  Ethernet.init(D1);  // Gunakan pin 10 untuk Ethernet SS
  Ethernet.begin(mac, ip);
  server.begin();
  
  Serial.print("Server berada di ");
  Serial.println(Ethernet.localIP());
}

void simpanRiwayatDebu(float density, float volt) {
  // Simpan data ke array siklis
  dustHistory[historyIndex] = {
    now(),  // timestamp saat ini
    density,
    volt
  };

  // Perbarui indeks dengan mode siklis
  historyIndex = (historyIndex + 1) % MAX_HISTORY;
}

void measureDustDensity() {
  for (int analogChannel = 0; analogChannel < 1; analogChannel++) {
    // Nyalakan LED sensor debu
    digitalWrite(sharpLEDPin, LOW);
    delayMicroseconds(280);
    
    // Baca tegangan sensor
    int voRaw = analogRead(sharpVoPin);
    
    // Matikan LED sensor debu
    digitalWrite(sharpLEDPin, HIGH);
    delayMicroseconds(9620);
    
    // Konversi ke tegangan
    float vo = voRaw / 1024.0 * 5.0;
    
    // Hitung densitas debu
    float dV = vo - Voc;
    if (dV < 0) {
      dV = 0;
    }
    
    // Simpan pengukuran
    voltage[analogChannel] = vo * 1000.0;  // Konversi ke mV
    dustDensity[analogChannel] = dV / K * 100.0;

    // Simpan ke riwayat debu
    simpanRiwayatDebu(dustDensity[analogChannel], voltage[analogChannel]);

    // Periksa dan aktifkan alarm jika densitas debu tinggi
    checkDustAlarm(dustDensity[analogChannel]);
  }
}

void checkDustAlarm(float density) {
  if (density > DUST_THRESHOLD) {
    // Aktifkan Alarm
    tone(buzzerPin, 1000, 500);  // nada 1kHz selama 500ms
    delay(600);
    noTone(buzzerPin);
  }
}

void kirimRiwayatDebu(EthernetClient& client) {
  client.println("<div class='history-container'>");
  client.println("<h2>Riwayat Densitas Debu</h2>");
  client.println("<table>");
  client.println("<tr><th>Waktu</th><th>Densitas Debu (µg/m³)</th><th>Tegangan (mV)</th></tr>");
  
  // Mulai dari indeks terbaru dan kembali ke belakang
  int startIndex = historyIndex;
  for (int i = 0; i < MAX_HISTORY; i++) {
    // Hitung indeks mundur
    int index = (startIndex - 1 - i + MAX_HISTORY) % MAX_HISTORY;
    
    // Lewati entri kosong
    if (dustHistory[index].timestamp == 0) continue;
    
    // Format waktu
    char waktuStr[20];
    snprintf(waktuStr, sizeof(waktuStr), 
             "%02d/%02d/%04d %02d:%02d:%02d", 
             day(dustHistory[index].timestamp),
             month(dustHistory[index].timestamp),
             year(dustHistory[index].timestamp),
             hour(dustHistory[index].timestamp),
             minute(dustHistory[index].timestamp),
             second(dustHistory[index].timestamp)
    );
    
    client.println("<tr>");
    client.print("<td>"); client.print(waktuStr); client.println("</td>");
    client.print("<td>"); client.print(dustHistory[index].dustDensity, 2); client.println("</td>");
    client.print("<td>"); client.print(dustHistory[index].voltage, 2); client.println("</td>");
    client.println("</tr>");
  }
  
  client.println("</table>");
  client.println("</div>");
}

void hapusRiwayatDebu() {
  // Atur semua timestamp ke 0
  for (int i = 0; i < MAX_HISTORY; i++) {
    dustHistory[i].timestamp = 0;
  }
  historyIndex = 0;
}

void kirimHalamanUtama(EthernetClient& client) {
  // Kirim halaman utama dengan HTML, CSS, dan JavaScript yang lengkap
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  
  client.println("<!DOCTYPE HTML>");
  client.println("<html lang='id'>");
  client.println("<head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<title>Monitor Sensor Debu</title>");
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; background-color: #f4f4f4; }");
  client.println(".container { background-color: white; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); padding: 20px; }");
  client.println(".sensor-data { display: flex; justify-content: space-between; margin-bottom: 20px; }");
  client.println(".sensor-card { background-color: #f9f9f9; border-left: 4px solid #3498db; padding: 15px; width: 45%; }");
  client.println(".dust-warning { color: red; font-weight: bold; }");
  client.println(".history-container { margin-top: 20px; }");
  client.println("table { width: 100%; border-collapse: collapse; }");
  client.println("table, th, td { border: 1px solid #ddd; }");
  client.println("th, td { padding: 8px; text-align: left; }");
  client.println("th { background-color: #f2f2f2; }");
  client.println("button { margin: 10px 5px; padding: 8px 15px; background-color: #3498db; color: white; border: none; border-radius: 4px; cursor: pointer; }");
  client.println("button:hover { background-color: #2980b9; }");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<div class='container'>");
  client.println("<h1>Monitor Sensor Debu</h1>");
  
  client.println("<div class='sensor-data'>");
  for (int analogChannel = 0; analogChannel < 1; analogChannel++) {
    client.println("<div class='sensor-card'>");
    client.print("<h3>Sensor Debu ");
    client.print(analogChannel);
    client.println("</h3>");
    
    client.print("<p>Densitas: <span id='density'>");
    client.print(dustDensity[analogChannel]);
    client.println(" µg/m³</span></p>");
    
    client.print("<p>Tegangan: <span id='voltage'>");
    client.print(voltage[analogChannel]);
    client.println(" mV</span></p>");
    
    if (dustDensity[analogChannel] > DUST_THRESHOLD) {
      client.println("<p class='dust-warning'>PERINGATAN: Debu Tinggi!</p>");
    }
    
    client.println("</div>");
  }
  client.println("</div>");
  
  client.println("<div class='actions'>");
  client.println("<button id='history-btn' onclick='fetchHistory()'>Lihat Riwayat</button>");
  client.println("<button onclick='clearHistory()'>Hapus Riwayat</button>");
  client.println("</div>");
  
  client.println("<div id='history-container'></div>");
  
  // Tambahkan JavaScript untuk AJAX
  client.println("<script>");
  client.println("let historyVisible = false;");
  
  client.println("function fetchHistory() {");
  client.println("  if (!historyVisible) {");
  client.println("    fetch('/history')");
  client.println("      .then(response => response.text())");
  client.println("      .then(html => {");
  client.println("        document.getElementById('history-container').innerHTML = html;");
  client.println("        document.getElementById('history-btn').textContent = 'Tutup Riwayat';");
  client.println("        historyVisible = true;");
  client.println("      });");
  client.println("  } else {");
  client.println("    document.getElementById('history-container').innerHTML = '';");
  client.println("    document.getElementById('history-btn').textContent = 'Lihat Riwayat';");
  client.println("    historyVisible = false;");
  client.println("  }");
  client.println("}");
  
  client.println("function clearHistory() {");
  client.println("  fetch('/clear')");
  client.println("    .then(() => {");
  client.println("      document.getElementById('history-container').innerHTML = '';");
  client.println("      document.getElementById('history-btn').textContent = 'Lihat Riwayat';");
  client.println("      historyVisible = false;");
  client.println("    });");
  client.println("}");
  
  client.println("function updateSensorData() {");
  client.println("  fetch('/')");
  client.println("    .then(response => response.text())");
  client.println("    .then(html => {");
  client.println("      const parser = new DOMParser();");
  client.println("      const doc = parser.parseFromString(html, 'text/html');");
  
  client.println("      const densitySpan = document.getElementById('density');");
  client.println("      const voltageSpan = document.getElementById('voltage');");
  client.println("      const warningContainer = document.querySelector('.sensor-card');");
  
  client.println("      const newDensity = doc.querySelector('#density').textContent;");
  client.println("      const newVoltage = doc.querySelector('#voltage').textContent;");
  client.println("      const newWarningHTML = doc.querySelector('.dust-warning') ? doc.querySelector('.dust-warning').outerHTML : '';");
  
  client.println("      densitySpan.textContent = newDensity;");
  client.println("      voltageSpan.textContent = newVoltage;");
  
  // Pembaruan status peringatan
  client.println("      const existingWarning = warningContainer.querySelector('.dust-warning');");
  client.println("      if (newWarningHTML && !existingWarning) {");
  // Tambahkan peringatan baru jika belum ada
  client.println("        const warningElement = document.createElement('div');");
  client.println("        warningElement.innerHTML = newWarningHTML;");
  client.println("        warningContainer.appendChild(warningElement.firstChild);");
  client.println("      } else if (!newWarningHTML && existingWarning) {");
  // Hapus peringatan jika kondisi bahaya sudah berlalu
  client.println("        existingWarning.remove();");
  client.println("      } else if (newWarningHTML && existingWarning) {");
  // Perbarui teks peringatan jika berubah
  client.println("        existingWarning.textContent = doc.querySelector('.dust-warning').textContent;");
  client.println("      }");
  
  client.println("    });");
  client.println("}");
  
  client.println("setInterval(updateSensorData, 5000);");
  client.println("setInterval(function() {");
  client.println("  if (historyVisible) {");
  client.println("    fetch('/history')");
  client.println("      .then(response => response.text())");
  client.println("      .then(html => {");
  client.println("        document.getElementById('history-container').innerHTML = html;");
  client.println("      });");
  client.println("  }");
  client.println("}, 10000);"); // Refresh riwayat setiap 10 detik jika terbuka
  client.println("</script>");
  
  client.println("</div>");
  client.println("</body>");
  client.println("</html>");
}

void loop() {
  // Tunggu klien masuk
  EthernetClient client = server.available();
  
  if (client) {
    Serial.println("Klien terhubung");
    
    // Permintaan HTTP berakhir dengan baris kosong
    boolean barisKosongSaatIni = true;
    String barisKini = "";
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        barisKini += c;
        Serial.write(c);
        
        // Periksa permintaan GET spesifik
        if (barisKini.endsWith("GET /history")) {
          // Kirim header respons HTTP standar
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          
          // Kirim riwayat debu
          kirimRiwayatDebu(client);
          break;
        }
        else if (barisKini.endsWith("GET /clear")) {
          hapusRiwayatDebu();
          
          // Alihkan kembali ke halaman utama
          client.println("HTTP/1.1 302 Found");
          client.println("Location: /");
          client.println();
          break;
        }
        
        // Jika telah mencapai akhir baris dan baris kosong, permintaan HTTP telah berakhir
        if (c == '\n' && barisKosongSaatIni) {
          // Ukur densitas debu sebelum mengirim respons
          measureDustDensity();
          
          // Kirim halaman utama
          kirimHalamanUtama(client);
          break;
        }
        
        if (c == '\n') {
          // Anda mulai baris baru
          barisKosongSaatIni = true;
          barisKini = "";
        }
        else if (c != '\r') {
          // Anda mendapatkan karakter pada baris saat ini
          barisKosongSaatIni = false;
        }
      }
    }
    
    // Beri waktu browser web menerima data
    delay(1);
    
    // Tutup koneksi
    client.stop();
    Serial.println("Klien terputus");
  }
}
