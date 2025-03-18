#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <Ethernet2.h>
#include <RTClib.h>

// Konfigurasi Pin
#define SD_CS        D8  
#define W5500_CS     D0
#define BUZZER_PIN   D3
#define DUST_LED_PIN D4
#define DUST_A_PIN   A0

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);
EthernetServer server(80);

float lastDust = -1;
unsigned long lastSaveTime = 0;
const unsigned long saveInterval = 600000;
const unsigned long updateInterval = 2000; // 2 detik interval update

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  if (!rtc.begin()) {
    Serial.println("RTC tidak terdeteksi!");
    lcd.setCursor(0, 0);
    lcd.print("RTC Error!");
  } else {
    // Set RTC ke waktu sekarang (WIB - UTC+7) jika perlu
    if (rtc.lostPower()) {
      // Set RTC to compile time + 7 hours for WIB
      DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
      rtc.adjust(DateTime(compileTime.unixtime() + 7 * 3600));
      Serial.println("RTC diatur ke waktu kompilasi + 7 jam (WIB)");
    }
    printDateTime();
    Serial.println("RTC terdeteksi dan berfungsi");
  }

  // Inisialisasi SD Card dengan error handling yang lebih baik
  Serial.println("Inisialisasi SD Card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("Gagal inisialisasi SD Card dengan pin standar");
    // Coba dengan pin dari contoh
    if (!SD.begin(15)) {
      Serial.println("Gagal inisialisasi SD Card dengan pin 15!");
    } else {
      Serial.println("SD Card berhasil diinisialisasi dengan pin 15");
    }
  } else {
    Serial.println("SD Card berhasil diinisialisasi");
  }

  // Periksa file log
  if (!SD.exists("dust_log.csv")) {
    // Buat file baru jika tidak ada
    File logFile = SD.open("dust_log.csv", FILE_WRITE);
    if (logFile) {
      logFile.println("Timestamp,Dust_Level");
      logFile.close();
      Serial.println("File dust_log.csv dibuat");
    } else {
      Serial.println("Gagal membuat file dust_log.csv");
    }
  }

  Ethernet.init(W5500_CS);
  delay(5000);
  Ethernet.begin(mac, ip);
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());
  server.begin();

  pinMode(DUST_LED_PIN, OUTPUT);
  pinMode(DUST_A_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  float dustDensity = readDustSensor();
  DateTime now = rtc.now();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Debu:");
  lcd.setCursor(6, 0);
  lcd.print(dustDensity);
  lcd.print(" ug/m3");
  
  // Tampilkan waktu pada baris kedua LCD
  lcd.setCursor(0, 1);
  printDateTimeToLCD(now);

  if (dustDensity > 50) digitalWrite(BUZZER_PIN, HIGH);
  else digitalWrite(BUZZER_PIN, LOW);

  // Simpan data ke SD Card dengan penanganan error yang lebih baik
  if (dustDensity != lastDust || (millis() - lastSaveTime) > saveInterval) {
    File logFile = SD.open("dust_log.csv", FILE_WRITE);
    if (logFile) {
      logFile.print(formatDateTime(now));
      logFile.print(",");
      logFile.println(dustDensity);
      logFile.close();
      Serial.print("Data disimpan: ");
      Serial.print(formatDateTime(now));
      Serial.print(", ");
      Serial.println(dustDensity);
      lastDust = dustDensity;
      lastSaveTime = millis();
    } else {
      Serial.println("Gagal membuka file dust_log.csv untuk menulis");
    }
  }

  EthernetClient client = server.available();
  if (client) handleWebServer(client);

  delay(5000);
}

float readDustSensor() {
  digitalWrite(DUST_LED_PIN, LOW);  // Nyalakan LED sensor debu
  delayMicroseconds(280);           // Tunggu 280 mikrodetik
  int voRaw = analogRead(DUST_A_PIN);  // Baca nilai analog dari sensor
  digitalWrite(DUST_LED_PIN, HIGH); // Matikan LED sensor debu
  delayMicroseconds(9620);          // Tunggu 9620 mikrodetik

  // Konversi nilai analog ke tegangan (mV)
  float vo = voRaw * (5.0 / 1023.0) * 1000.0;

  // Hitung densitas debu (ug/m3)
  float dV = vo - 600.0;  // 600 mV adalah tegangan saat tidak ada debu
  if (dV < 0) {
    dV = 0;
  }
  float dustDensity = dV / 0.5;  // 0.5 adalah faktor kalibrasi (V per 100ug/m3)

  return dustDensity;
}

// Fungsi untuk memformat waktu dengan lebih baik
String formatDateTime(DateTime dt) {
  char buffer[30];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          dt.year(), dt.month(), dt.day(),
          dt.hour(), dt.minute(), dt.second());
  return String(buffer);
}

// Fungsi untuk menampilkan waktu di Serial Monitor
void printDateTime() {
  DateTime now = rtc.now();
  char buffer[50];
  sprintf(buffer, "Waktu RTC: %04d-%02d-%02d %02d:%02d:%02d (%.2f°C)",
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second(),
          rtc.getTemperature());
  Serial.println(buffer);
}

// Fungsi untuk menampilkan waktu di LCD
void printDateTimeToLCD(DateTime dt) {
  char buffer[17]; // 16 karakter + null terminator untuk LCD 16x2
  sprintf(buffer, "%02d/%02d %02d:%02d:%02d",
          dt.day(), dt.month(),
          dt.hour(), dt.minute(), dt.second());
  lcd.print(buffer);
}

// Web Server
void handleWebServer(EthernetClient client) {
  String request = client.readStringUntil('\r');
  client.flush();
  
  // Handle AJAX requests untuk data real-time
  if (request.indexOf("/getdata") != -1) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.print("{\"dustLevel\":");
    client.print(lastDust);
    client.print(",\"timestamp\":\"");
    client.print(formatDateTime(rtc.now()));
    client.print("\",\"temperature\":\"");
    client.print(rtc.getTemperature(), 2);
    client.println("\"}");
    client.stop();
    return;
  }
  
  // Handle set time request
  if (request.indexOf("/settime") != -1) {
    // Extract time parameters from URL
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    
    if (extractTimeParam(request, "year=", year) && 
        extractTimeParam(request, "month=", month) && 
        extractTimeParam(request, "day=", day) && 
        extractTimeParam(request, "hour=", hour) && 
        extractTimeParam(request, "minute=", minute) && 
        extractTimeParam(request, "second=", second)) {
      
      // Validasi parameter waktu
      if (isValidDateTime(year, month, day, hour, minute, second)) {
        // Set RTC
        rtc.adjust(DateTime(year, month, day, hour, minute, second));
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<html><body><h2>Waktu berhasil disetel</h2>");
        client.println("<p>Waktu baru: " + formatDateTime(rtc.now()) + "</p>");
        client.println("<meta http-equiv='refresh' content='2;url=/' />");
        client.println("<p>Mengarahkan kembali ke halaman utama...</p>");
        client.println("</body></html>");
      } else {
        // Parameter tidak valid
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<html><body><h2>Parameter waktu tidak valid</h2>");
        client.println("<meta http-equiv='refresh' content='2;url=/' />");
        client.println("<p>Mengarahkan kembali ke halaman utama...</p>");
        client.println("</body></html>");
      }
    } else {
      // Parameter tidak lengkap
      client.println("HTTP/1.1 400 Bad Request");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<html><body><h2>Parameter waktu tidak lengkap</h2>");
      client.println("<meta http-equiv='refresh' content='2;url=/' />");
      client.println("<p>Mengarahkan kembali ke halaman utama...</p>");
      client.println("</body></html>");
    }
    client.stop();
    return;
  }
  
  // Handle sync time with client
  if (request.indexOf("/synctime") != -1) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("<html><body>");
    client.println("<h2>Sinkronisasi Waktu dengan Browser</h2>");
    client.println("<p>Sedang mensinkronkan waktu...</p>");
    
    // JavaScript untuk mengirim waktu browser
    client.println("<script>");
    client.println("var now = new Date();");
    client.println("var year = now.getFullYear();");
    client.println("var month = now.getMonth() + 1;"); // JavaScript bulan dimulai dari 0
    client.println("var day = now.getDate();");
    client.println("var hour = now.getHours();");
    client.println("var minute = now.getMinutes();");
    client.println("var second = now.getSeconds();");
    client
      .println("window.location.href = '/settime?year=' + year + '&month=' + month + '&day=' + day + '&hour=' + hour + '&minute=' + minute + '&second=' + second;");
    client.println("</script>");
    client.println("</body></html>");
    client.stop();
    return;
  }

  // Handle download/export request
  if (request.indexOf("/export") != -1) {
    File dataFile = SD.open("dust_log.csv");
    if (dataFile) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/csv");
      client.println("Content-Disposition: attachment; filename=\"dust_log.csv\"");
      client.println("Connection: close");
      client.println();
      
      // Send file contents in chunks
      byte buffer[64];
      int bytesRead;
      while ((bytesRead = dataFile.read(buffer, sizeof(buffer))) > 0) {
        client.write(buffer, bytesRead);
        // Add small delay to avoid buffer overflows
        delay(1);
      }
      dataFile.close();
    } else {
      client.println("HTTP/1.1 404 Not Found");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<html><body><h2>File tidak ditemukan</h2>");
      client.println("<a href='/'>Kembali ke halaman utama</a>");
      client.println("</body></html>");
    }
    client.stop();
    return;
  }
  
  // Handle AJAX request untuk data historis - dioptimasi
  // Replace the existing gethistory handler in the handleWebServer function with this updated version
if (request.indexOf("/gethistory") != -1) {
  int page = 1;
  int pageSize = 30; // Default page size
  String sortOrder = "desc"; // Default sort: newest first
  
  // Get page number
  if (request.indexOf("page=") != -1) {
    int start = request.indexOf("page=") + 5;
    int end = request.indexOf("&", start);
    if (end == -1) end = request.length();
    page = request.substring(start, end).toInt();
  }
  
  // Check if sort parameter exists (but don't require it)
  if (request.indexOf("sort=") != -1) {
    int start = request.indexOf("sort=") + 5;
    int end = request.indexOf("&", start);
    if (end == -1) end = request.length();
    sortOrder = request.substring(start, end);
  }
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  client.print("{\"data\":[");
  
  // Count total lines in file - more efficient way
  File logFile = SD.open("dust_log.csv");
  int totalLines = 0;
  if (logFile) {
    char c;
    while (logFile.available()) {
      c = logFile.read();
      if (c == '\n') totalLines++;
    }
    logFile.close();
  }
  
  // Create temporary array to store entries
  struct LogEntry {
    String timestamp;
    float dustValue;
  };
  LogEntry *entries = new LogEntry[pageSize];
  int entryCount = 0;
  
  // Calculate boundaries based on page and sort order
  int startLine, endLine;
  
  if (sortOrder == "asc") { // Oldest first
    startLine = (page - 1) * pageSize;
    endLine = startLine + pageSize;
  } else { // Newest first (desc)
    startLine = totalLines - page * pageSize;
    if (startLine < 0) startLine = 0;
    endLine = startLine + pageSize;
    if (endLine > totalLines) endLine = totalLines;
  }
  
  // Read entries into array - optimized for performance
  logFile = SD.open("dust_log.csv");
  if (logFile) {
    char buffer[64]; // Buffer for reading lines
    int bufferPos = 0;
    int currentLine = 0;
    
    while (logFile.available() && entryCount < pageSize) {
      char c = logFile.read();
      
      if (c == '\n') {
        buffer[bufferPos] = 0; // Null terminate
        String line = String(buffer);
        
        if (currentLine >= startLine && currentLine < endLine) {
          int commaPos = line.indexOf(',');
          if (commaPos > 0) {
            int idx = (sortOrder == "desc") ? (endLine - startLine - 1) - (currentLine - startLine) : (currentLine - startLine);
            if (idx >= 0 && idx < pageSize) {
              entries[idx].timestamp = line.substring(0, commaPos);
              entries[idx].dustValue = line.substring(commaPos + 1).toFloat();
              entryCount++;
            }
          }
        }
        
        // Reset buffer for next line
        bufferPos = 0;
        currentLine++;
      } else if (bufferPos < 63) {
        buffer[bufferPos++] = c;
      }
    }
    logFile.close();
  }
  
  // Output sorted entries
  bool firstEntry = true;
  for (int i = 0; i < entryCount; i++) {
    if (!firstEntry) client.print(",");
    firstEntry = false;
    
    client.print("{\"time\":\"");
    client.print(entries[i].timestamp);
    client.print("\",\"dust\":");
    client.print(entries[i].dustValue);
    client.print("}");
  }
  
  // Free memory
  delete[] entries;
  
  client.print("],\"totalPages\":");
  client.print(ceil((float)totalLines / pageSize));
  client.print(",\"currentPage\":");
  client.print(page);
  client.print(",\"sortOrder\":\"");
  client.print(sortOrder);
  client.println("\"}");
  client.stop();
  return;
}
  
  // Handle clear history request - improved with error handling
  if (request.indexOf("/clear") != -1) {
    bool success = false;
    
    if (SD.exists("dust_log.csv")) {
      if (SD.remove("dust_log.csv")) {
        // Create new empty file with header
        File newFile = SD.open("dust_log.csv", FILE_WRITE);
        if (newFile) {
          newFile.println("Timestamp,Dust_Level");
          newFile.close();
          success = true;
        }
      }
    } else {
      // If file doesn't exist, create it with header
      File newFile = SD.open("dust_log.csv", FILE_WRITE);
      if (newFile) {
        newFile.println("Timestamp,Dust_Level");
        newFile.close();
        success = true;
      }
    }
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    
    if (success) {
      client.println("<html><body><h2>History berhasil dihapus</h2>");
    } else {
      client.println("<html><body><h2>Gagal menghapus history</h2>");
    }
    client.println("<meta http-equiv='refresh' content='2;url=/' />");
    client.println("<p>Mengarahkan kembali ke halaman utama...</p>");
    client.println("</body></html>");
    client.stop();
    return;
  }

  // Main HTML page dengan pengaturan waktu
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html><html><head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<title>Monitoring Debu</title>");
  client.println("<style>");
  client.println("body { font-family: 'Segoe UI', Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; color: #333; }");
  client.println("header { background-color: #4CAF50; color: white; padding: 15px; border-radius: 5px; text-align: center; margin-bottom: 20px; }");
  client.println(".container { max-width: 1000px; margin: 0 auto; background: white; border-radius: 5px; padding: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }");
  client.println(".dashboard { display: flex; justify-content: space-between; flex-wrap: wrap; margin-bottom: 20px; }");
  client.println(".card { background: white; border-radius: 5px; padding: 15px; margin: 10px 0; box-shadow: 0 2px 5px rgba(0,0,0,0.1); flex: 1; min-width: 200px; text-align: center; }");
  client.println(".dust-gauge { position: relative; height: 150px; margin: 10px auto; width: 80%; }");
  client.println(".gauge-bg { position: absolute; width: 100%; height: 20px; background: linear-gradient(to right, green, yellow, red); border-radius: 10px; }");
  client.println(".gauge-pointer { position: absolute; width: 12px; height: 30px; background: #333; border-radius: 3px; bottom: 0; transform: translateX(-50%); transition: left 0.5s; }");
  client.println(".btn { background-color: #4CAF50; color: white; border: none; padding: 10px 15px; text-align: center; text-decoration: none; display: inline-block; font-size: 14px; margin: 4px 2px; cursor: pointer; border-radius: 4px; }");
  client.println(".btn-warning { background-color: #f44336; }");
  client.println(".btn-info { background-color: #2196F3; }");
  client.println("table { width: 100%; border-collapse: collapse; margin-top: 20px; }");
  client.println("th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }");
  client.println("th { background-color: #f2f2f2; }");
  client.println("tr:hover { background-color: #f5f5f5; }");
  client.println(".pagination { display: flex; justify-content: center; margin-top: 20px; }");
  client.println(".page-btn { margin: 0 5px; padding: 8px 15px; border: 1px solid #ddd; background-color: white; cursor: pointer; border-radius: 3px; }");
  client.println(".page-btn.active { background-color: #4CAF50; color: white; border-color: #4CAF50; }");
  client.println(".hidden { display: none; }");
  client.println(".dustLevel { font-size: 24px; font-weight: bold; }");
  client.println(".status-indicator { width: 15px; height: 15px; border-radius: 50%; display: inline-block; margin-right: 10px; }");
  client.println(".status-good { background-color: green; }");
  client.println(".status-warning { background-color: orange; }");
  client.println(".status-danger { background-color: red; }");
  client.println(".footer { margin-top: 20px; text-align: center; font-size: 12px; color: #777; }");
  client.println(".controls { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; }");
  client.println(".sort-controls { display: flex; align-items: center; }");
  client.println(".sort-controls label { margin-right: 10px; }");
  client.println(".page-size { margin-left: 20px; }");
  client.println(".time-section { margin-top: 20px; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }");
  client.println(".time-form { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; }");
  client.println(".time-form input { padding: 8px; border: 1px solid #ddd; border-radius: 4px; }");
  client.println(".time-form label { display: block; margin-bottom: 5px; }");
  client.println(".time-actions { margin-top: 15px; display: flex; justify-content: space-between; }");
  client.println("@media (max-width: 600px) { .dashboard { flex-direction: column; } .controls { flex-direction: column; } .time-form { grid-template-columns: 1fr; } }");
  client.println("</style>");
  
  // JavaScript untuk RTC dan monitoring
  client.println("<script>");
  client.println("let currentPage = 1;");
  client.println("let totalPages = 1;");
  
  // Update data fungsi
  client.println("function updateDustData() {");
  client.println("  fetch('/getdata')");
  client.println("    .then(response => response.json())");
  client.println("    .then(data => {");
  client.println("      document.getElementById('currentDust').innerText = data.dustLevel.toFixed(2);");
  client.println("      document.getElementById('timestamp').innerText = data.timestamp;");
  client.println("      document.getElementById('temperature').innerText = data.temperature;");
  client.println("      ");
  client.println("      // Update gauge");
  client.println("      const gaugePercent = Math.min(data.dustLevel / 100 * 100, 100);");
  client.println("      document.getElementById('gaugePointer').style.left = gaugePercent + '%';");
  client.println("      ");
  client.println("      // Update status indicator");
  client.println("      const statusIndicator = document.getElementById('statusIndicator');");
  client.println("      statusIndicator.className = 'status-indicator';");
  client.println("      if (data.dustLevel < 25) {");
  client.println("        statusIndicator.classList.add('status-good');");
  client.println("        document.getElementById('statusText').innerText = 'Baik';");
  client.println("      } else if (data.dustLevel < 50) {");
  client.println("        statusIndicator.classList.add('status-warning');");
  client.println("        document.getElementById('statusText').innerText = 'Sedang';");
  client.println("      } else {");
  client.println("        statusIndicator.classList.add('status-danger');");
  client.println("        document.getElementById('statusText').innerText = 'Buruk';");
  client.println("      }");
  client.println("    });");
  client.println("}");
  
  // Fungsi untuk history
// Replace the existing loadHistoryData function with this updated version
client.println("function loadHistoryData(page = 1) {");
client.println("  currentPage = page;");
client.println("  console.log('Loading history data, page: ' + page);");
client.println("  ");
client.println("  // Remove the sort parameter since sorting functionality has been removed");
client.println("  fetch('/gethistory?page=' + page)");
client.println("    .then(response => {");
client.println("      console.log('Response status: ' + response.status);");
client.println("      return response.json();");
client.println("    })");
client.println("    .then(data => {");
client.println("      console.log('Data received:', data);");
client.println("      const tableBody = document.getElementById('historyTableBody');");
client.println("      tableBody.innerHTML = '';");
client.println("      totalPages = data.totalPages;");
client.println("      ");
client.println("      data.data.forEach(entry => {");
client.println("        const row = document.createElement('tr');");
client.println("        const timeCell = document.createElement('td');");
client.println("        const dustCell = document.createElement('td');");
client.println("        const statusCell = document.createElement('td');");
client.println("        ");
client.println("        timeCell.innerText = entry.time;");
client.println("        dustCell.innerText = parseFloat(entry.dust).toFixed(2) + ' ug/m3';");
client.println("        ");
client.println("        const dustValue = parseFloat(entry.dust);");
client.println("        if (dustValue < 25) {");
client.println("          statusCell.innerHTML = '<span class=\"status-indicator status-good\"></span>Baik';");
client.println("        } else if (dustValue < 50) {");
client.println("          statusCell.innerHTML = '<span class=\"status-indicator status-warning\"></span>Sedang';");
client.println("        } else {");
client.println("          statusCell.innerHTML = '<span class=\"status-indicator status-danger\"></span>Buruk';");
client.println("        }");
client.println("        ");
client.println("        row.appendChild(timeCell);");
client.println("        row.appendChild(dustCell);");
client.println("        row.appendChild(statusCell);");
client.println("        tableBody.appendChild(row);");
client.println("      });");
client.println("      ");
client.println("      // Update pagination");
client.println("      updatePagination();");
client.println("    })");
client.println("    .catch(error => {");
client.println("      console.error('Error fetching history data:', error);");
client.println("      alert('Terjadi kesalahan saat memuat data historis. Silakan coba lagi.');");
client.println("    });");
client.println("}");
  
  // Fungsi untuk pagination
  client.println("function updatePagination() {");
client.println("  const paginationDiv = document.getElementById('pagination');");
client.println("  paginationDiv.innerHTML = '';");
client.println("  ");
client.println("  // Previous button");
client.println("  if (currentPage > 1) {");
client.println("    const prevBtn = document.createElement('button');");
client.println("    prevBtn.className = 'page-btn';");
client.println("    prevBtn.innerText = 'Prev';");
client.println("    prevBtn.onclick = () => loadHistoryData(currentPage - 1);");
client.println("    paginationDiv.appendChild(prevBtn);");
client.println("  }");
client.println("  ");
client.println("  // Page numbers");
client.println("  const startPage = Math.max(1, currentPage - 2);");
client.println("  const endPage = Math.min(totalPages, startPage + 4);");
client.println("  ");
client.println("  for (let i = startPage; i <= endPage; i++) {");
client.println("    const pageBtn = document.createElement('button');");
client.println("    pageBtn.className = 'page-btn' + (i === currentPage ? ' active' : '');");
client.println("    pageBtn.innerText = i;");
client.println("    pageBtn.onclick = () => loadHistoryData(i);");
client.println("    paginationDiv.appendChild(pageBtn);");
client.println("  }");
client.println("  ");
client.println("  // Next button");
client.println("  if (currentPage < totalPages) {");
client.println("    const nextBtn = document.createElement('button');");
client.println("    nextBtn.className = 'page-btn';");
client.println("    nextBtn.innerText = 'Next';");
client.println("    nextBtn.onclick = () => loadHistoryData(currentPage + 1);");
client.println("    paginationDiv.appendChild(nextBtn);");
client.println("  }");
client.println("}");
  
  // Fungsi untuk toggle history
  client.println("function toggleHistory() {");
  client.println("  const historySection = document.getElementById('historySection');");
  client.println("  historySection.classList.toggle('hidden');");
  client.println("  if (!historySection.classList.contains('hidden')) {");
  client.println("    loadHistoryData(1);");
  client.println("  }");
  client.println("}");
  
  // Fungsi untuk toggle time settings
  client.println("function toggleTimeSettings() {");
  client.println("  const timeSection = document.getElementById('timeSection');");
  client.println("  timeSection.classList.toggle('hidden');");
  client.println("}");
  
  // Fungsi untuk konfirmasi clear history
  client.println("function confirmClear() {");
  client.println("  return confirm('Apakah Anda yakin ingin menghapus semua data historis?');");
  client.println("}");
  
  // Fungsi untuk validasi form waktu
  client.println("function validateTimeForm() {");
  client.println("  const year = parseInt(document.getElementById('year').value);");
  client.println("  const month = parseInt(document.getElementById('month').value);");
  client.println("  const day = parseInt(document.getElementById('day').value);");
  client.println("  const hour = parseInt(document.getElementById('hour').value);");
  client.println("  const minute = parseInt(document.getElementById('minute').value);");
  client.println("  const second = parseInt(document.getElementById('second').value);");
  client.println("  ");
  client.println("  if (isNaN(year) || year < 2000 || year > 2099) {");
  client.println("    alert('Tahun harus antara 2000-2099');");
  client.println("    return false;");
  client.println("  }");
  client.println("  if (isNaN(month) || month < 1 || month > 12) {");
  client.println("    alert('Bulan harus antara 1-12');");
  client.println("    return false;");
  client.println("  }");
  client.println("  if (isNaN(day) || day < 1 || day > 31) {");
  client.println("    alert('Tanggal harus antara 1-31');");
  client.println("    return false;");
  client.println("  }");
  client.println("  if (isNaN(hour) || hour < 0 || hour > 23) {");
  client.println("    alert('Jam harus antara 0-23');");
  client.println("    return false;");
  client.println("  }");
  client.println("  if (isNaN(minute) || minute < 0 || minute > 59) {");
  client.println("    alert('Menit harus antara 0-59');");
  client.println("    return false;");
  client.println("  }");
  client.println("  if (isNaN(second) || second < 0 || second > 59) {");
  client.println("    alert('Detik harus antara 0-59');");
  client.println("    return false;");
  client.println("  }");
  client.println("  ");
  client.println("  return true;");
  client.println("}");
  
  // Fungsi untuk mengisi form waktu dengan waktu sekarang
  client.println("function fillCurrentTime() {");
  client.println("  fetch('/getdata')");
  client.println("    .then(response => response.json())");
  client.println("    .then(data => {");
  client.println("      const timestamp = data.timestamp;");
  client.println("      const dateParts = timestamp.split(' ')[0].split('-');");
  client.println("      const timeParts = timestamp.split(' ')[1].split(':');");
  client.println("      ");
  client.println("      document.getElementById('year').value = dateParts[0];");
  client.println("      document.getElementById('month').value = dateParts[1];");
  client.println("      document.getElementById('day').value = dateParts[2];");
  client.println("      document.getElementById('hour').value = timeParts[0];");
  client.println("      document.getElementById('minute').value = timeParts[1];");
  client.println("      document.getElementById('second').value = timeParts[2];");
  client.println("    });");
  client.println("}");
  
  // Initialize
  client.println("document.addEventListener('DOMContentLoaded', function() {");
  client.println("  updateDustData();");
  client.println("  setInterval(updateDustData, 2000);");
  client.println("  fillCurrentTime();");
  client.println("});");
  client.println("</script>");
  client.println("</head>");
  client.println("<body>");
  
  client.println("<div class='container'>");
  client.println("<header>");
  client.println("<h1>Sistem Monitoring Kualitas Udara</h1>");
  client.println("</header>");
  
  client.println("<div class='dashboard'>");
  client.println("<div class='card'>");
  client.println("<h2>Tingkat Debu Saat Ini</h2>");
  client.println("<div class='dustLevel'><span id='currentDust'>" + String(lastDust) + "</span> ug/m³</div>");
  client.println("<div class='dust-gauge'>");
  client.println("<div class='gauge-bg'></div>");
  // Fix: Mengkonversi 100 ke float dan menggunakan std::min
  float dustPercentage = lastDust / 100.0f * 100.0f;
  float gaugePosition = (dustPercentage > 100.0f) ? 100.0f : dustPercentage;
  client.println("<div id='gaugePointer' class='gauge-pointer' style='left: " + String(gaugePosition) + "%;'></div>");
  client.println("</div>");
  
  // Fix: Membagi string menjadi beberapa bagian untuk menghindari operator + pada string literal
  client.println("<p>Status: <span id='statusIndicator' class='status-indicator ");
  if (lastDust < 25) {
    client.println("status-good");
  } else if (lastDust < 50) {
    client.println("status-warning");
  } else {
    client.println("status-danger");
  }
  client.println("'></span><span id='statusText'>");
  if (lastDust < 25) {
    client.println("Baik");
  } else if (lastDust < 50) {
    client.println("Sedang");
  } else {
    client.println("Buruk");
  }
  client.println("</span></p>");
  
  client.println("</div>");
  
  client.println("<div class='card'>");
  client.println("<h2>Informasi</h2>");
  client.println("<p>Waktu: <span id='timestamp'>" + formatDateTime(rtc.now()) + "</span></p>");
  client.println("<p>Suhu RTC: <span id='temperature'>" + String(rtc.getTemperature(), 2) + "</span> °C</p>");
  client.println("<p>Peringatan: Di atas 50 ug/m³ berbahaya</p>");
  client.println("<div>");
  client.println("<button class='btn' onclick='toggleHistory()'>Tampilkan/Tutup History</button>");
  client.println("<button class='btn btn-info' onclick='toggleTimeSettings()'>Pengaturan Waktu</button>");
  client.println("</div>");
  client.println("<div>");
  client.println("<a href='/clear' onclick='return confirmClear()'><button class='btn btn-warning'>Hapus History</button></a>");
  client.println("<a href='/export'><button class='btn'>Download Data CSV</button></a>");
  client.println("</div>");
  client.println("</div>");
  client.println("</div>");
  
  // Bagian pengaturan waktu (awalnya tersembunyi)
  client.println("<div id='timeSection' class='time-section hidden'>");
  client.println("<h2>Pengaturan Waktu RTC</h2>");
  client.println("<form action='/settime' method='get' onsubmit='return validateTimeForm()'>");
  client.println("<div class='time-form'>");
  
  client.println("<div>");
  client.println("<label for='year'>Tahun:</label>");
  client.println("<input type='number' id='year' name='year' min='2000' max='2099' required>");
  client.println("</div>");
  
  client.println("<div>");
  client.println("<label for='month'>Bulan:</label>");
  client.println("<input type='number' id='month' name='month' min='1' max='12' required>");
  client.println("</div>");
  
  client.println("<div>");
  client.println("<label for='day'>Tanggal:</label>");
  client.println("<input type='number' id='day' name='day' min='1' max='31' required>");
  client.println("</div>");
  
  client.println("<div>");
  client.println("<label for='hour'>Jam:</label>");
  client.println("<input type='number' id='hour' name='hour' min='0' max='23' required>");
  client.println("</div>");
  
  client.println("<div>");
  client.println("<label for='minute'>Menit:</label>");
  client.println("<input type='number' id='minute' name='minute' min='0' max='59' required>");
  client.println("</div>");
  
  client.println("<div>");
  client.println("<label for='second'>Detik:</label>");
  client.println("<input type='number' id='second' name='second' min='0' max='59' required>");
  client.println("</div>");
  
  client.println("</div>");
  
  client.println("<div class='time-actions'>");
  client.println("<button type='button' class='btn' onclick='fillCurrentTime()'>Isi dengan Waktu Sekarang</button>");
  client.println("<a href='/synctime'><button type='button' class='btn btn-info'>Sinkronkan dengan Browser</button></a>");
  client.println("<button type='submit' class='btn'>Simpan Pengaturan Waktu</button>");
  client.println("</div>");
  
  client.println("</form>");
  client.println("</div>");
  
  // Updated history section with sort controls
  client.println("<div id='historySection' class='hidden'>");
  client.println("<h2>Data Historis</h2>");
  
  // Added sort controls
  client.println("<div class='controls'>");
  client.println("<div class='page-info'>30 data per halaman</div>");
  client.println("</div>");
  
  client.println("<table>");
  client.println("<thead>");
  client.println("<tr>");
  client.println("<th>Waktu</th>");
  client.println("<th>Debu (ug/m³)</th>");
  client.println("<th>Status</th>");
  client.println("</tr>");
  client.println("</thead>");
  client.println("<tbody id='historyTableBody'>");
  client.println("</tbody>");
  client.println("</table>");
  
  client.println("<div id='pagination' class='pagination'></div>");
  client.println("</div>");
  
  client.println("<div class='footer'>");
  client.println("<p>Sistem Monitoring Kualitas Udara &copy; " + String(rtc.now().year()) + "</p>");
  client.println("</div>");
  
  client.println("</div>");
  client.println("</body></html>");
  client.stop();
}

// Helper function untuk ekstraksi parameter waktu dari URL
bool extractTimeParam(String &request, const char* param, int &value) {
  if (request.indexOf(param) != -1) {
    int start = request.indexOf(param) + strlen(param);
    int end = request.indexOf("&", start);
    if (end == -1) end = request.length();
    
    String valueStr = request.substring(start, end);
    if (valueStr.length() > 0) {
      value = valueStr.toInt();
      return true;
    }
  }
  return false;
}

// Fungsi untuk validasi parameter waktu yang diterima
bool isValidDateTime(int year, int month, int day, int hour, int minute, int second) {
  // Basic validation
  if (year < 2000 || year > 2099) return false;
  if (month < 1 || month > 12) return false;
  if (day < 1 || day > 31) return false;
  if (hour < 0 || hour > 23) return false;
  if (minute < 0 || minute > 59) return false;
  if (second < 0 || second > 59) return false;
  
  // Check days in month
  if (month == 4 || month == 6 || month == 9 || month == 11) {
    if (day > 30) return false;
  } else if (month == 2) {
    // Leap year check
    bool isLeapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (day > (isLeapYear ? 29 : 28)) return false;
  }
  
  return true;
}
