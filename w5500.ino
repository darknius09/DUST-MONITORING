#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <Ethernet2.h>
#include <RTClib.h>
#include <GP2Y1010sensor.h>

// Pin Configuration
#define SD_CS        D8  
#define W5500_CS     D0
#define BUZZER_PIN   D3
#define SHARP_LED_PIN D4
#define SHARP_VO_PIN A0

// Set the typical output voltage in Volts when there is zero dust
static float Voc = 0.6;
// Use the typical sensitivity in units of V per 100ug/m3
const float K = 0.5;
// Number of last N raw voltage readings
#define N 100

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);
EthernetServer server(80);
GP2Y1010sensor dustSensor;
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

float lastDust = -1.0f;
float lastVoltage = -1.0f;
unsigned long lastSaveTime = 0;
const unsigned long saveInterval = 600000; // 10 minutes
const unsigned long updateInterval = 2000; // 2 seconds

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC not detected!");
    lcd.setCursor(0, 0);
    lcd.print("RTC Error!");
  } else {
    if (rtc.lostPower()) {
      // Set RTC to compile time + 7 hours for WIB (UTC+7)
      DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
      rtc.adjust(DateTime(compileTime.unixtime() + 7 * 3600));
      Serial.println("RTC set to compile time + 7 hours (WIB)");
    }
    printDateTime();
    Serial.println("RTC initialized");
  }

  // Initialize SD Card
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
  } else {
    Serial.println("SD card initialized");
    // Create log file if it doesn't exist
    if (!SD.exists("dust_log.csv")) {
      File logFile = SD.open("dust_log.csv", FILE_WRITE);
      if (logFile) {
        logFile.println("Timestamp,Dust_Level,Voltage_mV");
        logFile.close();
        Serial.println("Created dust_log.csv");
      }
    }
  }

  // Initialize Ethernet
  Ethernet.init(W5500_CS);
  delay(1000); // Give Ethernet time to initialize
  Ethernet.begin(mac, ip);
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());
  server.begin();
  
  // Initialize dust sensor
  dustSensor.init(SHARP_LED_PIN, SHARP_VO_PIN, K, N);
  Serial.println("Dust sensor initialized");

  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  static unsigned long lastUpdateTime = 0;
  unsigned long currentMillis = millis();
  
  // Read sensor and update display every 2 seconds
  if (currentMillis - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentMillis;
    
    float dustDensity;
    float voltage;
    dustSensor.read(&dustDensity, &voltage);
    float mvolt = voltage * 1000; // Convert to mV
    DateTime now = rtc.now();

    // Update LCD
    updateLCD(dustDensity, mvolt, now);

    // Control buzzer - updated threshold to 55.4
    digitalWrite(BUZZER_PIN, dustDensity > 55.4 ? HIGH : LOW);

    // Save data periodically or when value changes significantly
    if (dustDensity != lastDust || voltage != lastVoltage || (currentMillis - lastSaveTime) > saveInterval) {
      saveToSD(now, dustDensity, mvolt);
      lastDust = dustDensity;
      lastVoltage = voltage;
      lastSaveTime = currentMillis;
    }
  }

  // Handle web clients
  EthernetClient client = server.available();
  if (client) {
    handleWebClient(client);
  }
}

void updateLCD(float dustDensity, float mvolt, DateTime now) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dust:");
  lcd.print(dustDensity);
  lcd.print(" ug/m3");
  
  lcd.setCursor(0, 1);
  char timeStr[17];
  sprintf(timeStr, "%02d:%02d V:%dmV", 
          now.hour(), now.minute(), (int)mvolt);
  lcd.print(timeStr);
}

void saveToSD(DateTime dt, float dustValue, float voltage) {
  File logFile = SD.open("dust_log.csv", FILE_WRITE);
  if (logFile) {
    logFile.print(formatDateTime(dt));
    logFile.print(",");
    logFile.print(dustValue);
    logFile.print(",");
    logFile.println(voltage);
    logFile.close();
    Serial.print("Data saved: ");
    Serial.print(formatDateTime(dt));
    Serial.print(", Dust: ");
    Serial.print(dustValue);
    Serial.print(" ug/m3, Voltage: ");
    Serial.print(voltage);
    Serial.println(" mV");
  } else {
    Serial.println("Failed to open dust_log.csv");
  }
}

String formatDateTime(DateTime dt) {
  char buffer[30];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          dt.year(), dt.month(), dt.day(),
          dt.hour(), dt.minute(), dt.second());
  return String(buffer);
}

void printDateTime() {
  DateTime now = rtc.now();
  Serial.print("RTC Time: ");
  Serial.print(formatDateTime(now));
  Serial.print(" (");
  Serial.print(rtc.getTemperature(), 2);
  Serial.println("°C)");
}

void handleWebClient(EthernetClient client) {
  String request = client.readStringUntil('\r');
  client.flush();
  
  // Handle AJAX request for real-time data
  if (request.indexOf("/getdata") != -1) {
    sendJSONResponse(client, lastDust, lastVoltage, rtc.now());
    return;
  }
  
  // Handle history data request
  if (request.indexOf("/gethistory") != -1) {
    sendHistoryData(client);
    return;
  }
  
  // Handle time setting
  if (request.indexOf("/settime") != -1) {
    handleTimeSetting(client, request);
    return;
  }
  
  // Handle sync time with browser
  if (request.indexOf("/synctime") != -1) {
    handleTimeSync(client);
    return;
  }
  
  // Handle data export
  if (request.indexOf("/export") != -1) {
    exportData(client);
    return;
  }
  
  // Handle clear history
  if (request.indexOf("/clear") != -1) {
    clearHistory(client);
    return;
  }
  
  // Serve main page
  sendMainPage(client);
}

void sendJSONResponse(EthernetClient &client, float dustValue, float voltage, DateTime now) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print("{\"dustLevel\":");
  client.print(dustValue);
  client.print(",\"voltage\":");
  client.print(voltage * 1000); // Convert to mV
  client.print(",\"timestamp\":\"");
  client.print(formatDateTime(now));
  client.print("\",\"temperature\":\"");
  client.print(rtc.getTemperature(), 2);
  client.println("\"}");
  client.stop();
}

void sendHistoryData(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print("{\"data\":[");
  
  // We'll store the last 50 entries in memory temporarily
  const int maxEntries = 50;
  String entries[maxEntries];
  int entryCount = 0;
  
  // Read the file and store the last 50 lines
  File logFile = SD.open("dust_log.csv");
  if (logFile) {
    // Skip header line
    logFile.readStringUntil('\n');
    
    // Read file line by line
    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        // Use circular buffer approach: overwrite oldest entry when full
        entries[entryCount % maxEntries] = line;
        entryCount++;
      }
    }
    logFile.close();
  }
  
  // Output the newest entries first
  bool firstEntry = true;
  int startIndex = entryCount < maxEntries ? 0 : entryCount % maxEntries;
  int count = min(entryCount, maxEntries);
  
  for (int i = 0; i < count; i++) {
    int index = (startIndex + i) % maxEntries;
    
    if (!firstEntry) client.print(",");
    firstEntry = false;
    
    // Parse line to get timestamp, dust value, and voltage
    int firstComma = entries[index].indexOf(',');
    int secondComma = entries[index].indexOf(',', firstComma + 1);
    
    if (firstComma > 0) {
      client.print("{\"time\":\"");
      client.print(entries[index].substring(0, firstComma));
      client.print("\",\"dust\":");
      
      if (secondComma > 0) {
        // Has voltage data
        client.print(entries[index].substring(firstComma + 1, secondComma));
        client.print(",\"voltage\":");
        client.print(entries[index].substring(secondComma + 1));
      } else {
        // No voltage data (older entries)
        client.print(entries[index].substring(firstComma + 1));
        client.print(",\"voltage\":0");
      }
      
      client.print("}");
    }
  }
  
  client.println("]}");
  client.stop();
}

void handleTimeSetting(EthernetClient &client, String &request) {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  
  if (extractTimeParam(request, "year=", year) && 
      extractTimeParam(request, "month=", month) && 
      extractTimeParam(request, "day=", day) && 
      extractTimeParam(request, "hour=", hour) && 
      extractTimeParam(request, "minute=", minute) && 
      extractTimeParam(request, "second=", second)) {
    
    if (isValidDateTime(year, month, day, hour, minute, second)) {
      rtc.adjust(DateTime(year, month, day, hour, minute, second));
      
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<html><body><h2>Time set successfully</h2>");
      client.println("<p>New time: " + formatDateTime(rtc.now()) + "</p>");
      client.println("<script>setTimeout(() => window.location.href='/', 2000)</script>");
      client.println("</body></html>");
    } else {
      sendErrorResponse(client, "Invalid time parameters");
    }
  } else {
    sendErrorResponse(client, "Incomplete time parameters");
  }
  client.stop();
}

void handleTimeSync(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<html><body>");
  client.println("<h2>Syncing Time with Browser</h2>");
  client.println("<p>Syncing time...</p>");
  client.println("<script>");
  client.println("var now = new Date();");
  client.println("window.location.href = '/settime?year=' + now.getFullYear() + '&month=' + (now.getMonth()+1) + '&day=' + now.getDate() + '&hour=' + now.getHours() + '&minute=' + now.getMinutes() + '&second=' + now.getSeconds();");
  client.println("</script>");
  client.println("</body></html>");
  client.stop();
}

void exportData(EthernetClient &client) {
  File dataFile = SD.open("dust_log.csv");
  if (dataFile) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/csv");
    client.println("Content-Disposition: attachment; filename=\"dust_log.csv\"");
    client.println("Connection: close");
    client.println();
    
    // Use a buffer for more efficient data transfer
    const int bufferSize = 64;
    uint8_t buffer[bufferSize];
    int bytesRead = 0;
    
    while ((bytesRead = dataFile.read(buffer, bufferSize)) > 0) {
      client.write(buffer, bytesRead);
      // Add a small delay to prevent overflowing the Ethernet buffer
      delay(1);
    }
    
    dataFile.close();
  } else {
    sendErrorResponse(client, "File not found");
  }
  client.stop();
}

void clearHistory(EthernetClient &client) {
  bool success = false;
  
  if (SD.exists("dust_log.csv")) {
    if (SD.remove("dust_log.csv")) {
      File newFile = SD.open("dust_log.csv", FILE_WRITE);
      if (newFile) {
        newFile.println("Timestamp,Dust_Level,Voltage_mV");
        newFile.close();
        success = true;
      }
    }
  } else {
    File newFile = SD.open("dust_log.csv", FILE_WRITE);
    if (newFile) {
      newFile.println("Timestamp,Dust_Level,Voltage_mV");
      newFile.close();
      success = true;
    }
  }
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<html><body>");
  client.println(success ? "<h2>History cleared</h2>" : "<h2>Failed to clear history</h2>");
  client.println("<script>setTimeout(() => window.location.href='/', 2000)</script>");
  client.println("</body></html>");
  client.stop();
}

void sendMainPage(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html><html><head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<title>Air Quality Monitor</title>");
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f9f9f9; color: #333; }");
  client.println(".container { max-width: 800px; margin: 0 auto; background: white; border-radius: 8px; padding: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }");
  client.println("h1 { color: #2c3e50; text-align: center; margin-top: 0; }");
  client.println(".card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }");
  client.println(".gauge { height: 20px; background: #f0f0f0; border-radius: 10px; margin: 15px 0; overflow: hidden; }");
  client.println(".gauge-fill { height: 100%; width: 0%; transition: width 0.5s; }");
  client.println(".value { font-size: 24px; font-weight: bold; text-align: center; margin: 10px 0; }");
  client.println(".timestamp { color: #666; text-align: center; }");
  client.println(".btn { background: #3498db; color: white; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer; text-decoration: none; display: inline-block; margin: 5px; }");
  client.println(".btn-danger { background: #e74c3c; }");
  client.println(".btn-success { background: #2ecc71; }");
  client.println("table { width: 100%; border-collapse: collapse; margin-top: 20px; }");
  client.println("th, td { padding: 12px 15px; text-align: left; border-bottom: 1px solid #ddd; }");
  client.println("th { background: #f2f2f2; }");
  client.println(".status { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 8px; }");
  client.println(".good { background: #2ecc71; }"); // Green
  client.println(".moderate { background: #3498db; }"); // Blue
  client.println(".unhealthy { background: #f1c40f; }"); // Yellow
  client.println(".very-unhealthy { background: #e74c3c; }"); // Red
  client.println(".dangerous { background: #95a5a6; }"); // Gray
  client.println(".hidden { display: none; }");
  client.println("</style>");
  
  // Updated JavaScript for the gauge levels and status
  client.println("<script>");
  client.println("function updateData() {");
  client.println("  fetch('/getdata')");
  client.println("    .then(r => r.json())");
  client.println("    .then(data => {");
  client.println("      document.getElementById('dustValue').textContent = data.dustLevel.toFixed(2) + ' ug/m³';");
  client.println("      document.getElementById('voltageValue').textContent = data.voltage.toFixed(1) + ' mV';");
  client.println("      document.getElementById('timeValue').textContent = data.timestamp;");
  client.println("      const dustLevel = data.dustLevel;");
  client.println("      document.getElementById('gaugeFill').style.width = Math.min(dustLevel * 100 / 300, 100) + '%';"); // Scale to max of ~300
  
  // Update gauge fill color based on air quality level
  client.println("      const gaugeFill = document.getElementById('gaugeFill');");
  client.println("      const status = document.getElementById('status');");
  client.println("      status.className = 'status';");
  client.println("      if (dustLevel <= 15.5) {");
  client.println("        status.classList.add('good');");
  client.println("        gaugeFill.style.background = '#2ecc71';"); // Green
  client.println("        status.nextSibling.textContent = 'Baik';");
  client.println("      } else if (dustLevel <= 55.4) {");
  client.println("        status.classList.add('moderate');");
  client.println("        gaugeFill.style.background = '#3498db';"); // Blue
  client.println("        status.nextSibling.textContent = 'Sedang';");
  client.println("      } else if (dustLevel <= 150.4) {");
  client.println("        status.classList.add('unhealthy');");
  client.println("        gaugeFill.style.background = '#f1c40f';"); // Yellow
  client.println("        status.nextSibling.textContent = 'Tidak Sehat';");
  client.println("      } else if (dustLevel <= 250.4) {");
  client.println("        status.classList.add('very-unhealthy');");
  client.println("        gaugeFill.style.background = '#e74c3c';"); // Red
  client.println("        status.nextSibling.textContent = 'Sangat Tidak Sehat';");
  client.println("      } else {");
  client.println("        status.classList.add('dangerous');");
  client.println("        gaugeFill.style.background = '#95a5a6';"); // Gray
  client.println("        status.nextSibling.textContent = 'Berbahaya';");
  client.println("      }");
  client.println("    });");
  client.println("}");
  
  // Update the status helper functions
  client.println("function getStatusClass(dust) {");
  client.println("  if (dust <= 15.5) return 'good';");
  client.println("  if (dust <= 55.4) return 'moderate';");
  client.println("  if (dust <= 150.4) return 'unhealthy';");
  client.println("  if (dust <= 250.4) return 'very-unhealthy';");
  client.println("  return 'dangerous';");
  client.println("}");
  
  client.println("function getStatusText(dust) {");
  client.println("  if (dust <= 15.5) return 'Baik';");
  client.println("  if (dust <= 55.4) return 'Sedang';");
  client.println("  if (dust <= 150.4) return 'Tidak Sehat';");
  client.println("  if (dust <= 250.4) return 'Sangat Tidak Sehat';");
  client.println("  return 'Berbahaya';");
  client.println("}");

  // Rest of your JavaScript remains the same
  client.println("function loadHistory() {");
  client.println("  const tbody = document.getElementById('historyData');");
  client.println("  tbody.innerHTML = '<tr><td colspan=\"4\">Loading...</td></tr>';");  // Show loading state
  client.println("  fetch('/gethistory?' + new Date().getTime())");  // Add timestamp to prevent caching
  client.println("    .then(response => {");
  client.println("      if (!response.ok) throw new Error('Network response was not ok');");
  client.println("      return response.json();");
  client.println("    })");
  client.println("    .then(data => {");
  client.println("      tbody.innerHTML = '';");
  client.println("      if (data.data && data.data.length > 0) {");
  client.println("        data.data.forEach(item => {");
  client.println("          const row = document.createElement('tr');");
  client.println("          const dustValue = parseFloat(item.dust);");
  client.println("          const voltValue = parseFloat(item.voltage || 0);");
  client.println("          row.innerHTML = `");
  client.println("            <td>${item.time}</td>");
  client.println("            <td>${dustValue.toFixed(2)}</td>");
  client.println("            <td>${voltValue.toFixed(1)}</td>");
  client.println("            <td><span class='status ${getStatusClass(dustValue)}'></span>${getStatusText(dustValue)}</td>");
  client.println("          `;");
  client.println("          tbody.appendChild(row);");
  client.println("        });");
  client.println("      } else {");
  client.println("        tbody.innerHTML = '<tr><td colspan=\"4\">No history data available</td></tr>';");
  client.println("      }");
  client.println("    })");
  client.println("    .catch(error => {");
  client.println("      console.error('Error loading history:', error);");
  client.println("      tbody.innerHTML = '<tr><td colspan=\"4\">Error loading data: ' + error.message + '</td></tr>';");
  client.println("    });");
  client.println("}");
  
  // Rest of your JavaScript functions
  client.println("function toggleHistory() {");
  client.println("  const section = document.getElementById('historySection');");
  client.println("  section.classList.toggle('hidden');");
  client.println("  if (!section.classList.contains('hidden')) {");
  client.println("    loadHistory();"); // Call loadHistory when showing the section
  client.println("  }");
  client.println("}");
  client.println("function toggleTimeSettings() {");
  client.println("  document.getElementById('timeSection').classList.toggle('hidden');");
  client.println("}");
  client.println("function confirmClear() {");
  client.println("  return confirm('Clear all history data?');");
  client.println("}");
  client.println("function setCurrentTime() {");
  client.println("  const now = new Date();");
  client.println("  document.getElementById('year').value = now.getFullYear();");
  client.println("  document.getElementById('month').value = now.getMonth() + 1;");
  client.println("  document.getElementById('day').value = now.getDate();");
  client.println("  document.getElementById('hour').value = now.getHours();");
  client.println("  document.getElementById('minute').value = now.getMinutes();");
  client.println("  document.getElementById('second').value = now.getSeconds();");
  client.println("}");
  client.println("document.addEventListener('DOMContentLoaded', () => {");
  client.println("  updateData();");
  client.println("  setInterval(updateData, 2000);");
  client.println("  setCurrentTime();");
  client.println("});");
  client.println("</script>");
  client.println("</head>");
  client.println("<body>");
  client.println("<div class='container'>");
  client.println("<h1>Air Quality Monitor</h1>");
  
  // Current data card
  client.println("<div class='card'>");
  client.println("<h2>Current Air Quality</h2>");
  client.println("<div class='value' id='dustValue'>" + String(lastDust, 2) + " ug/m³</div>");
  client.println("<div class='value' style='font-size: 18px;' id='voltageValue'>" + String(lastVoltage * 1000, 1) + " mV</div>");
  client.println("<div class='gauge'><div id='gaugeFill' class='gauge-fill' style='width:" + String(min(lastDust, 100.0f)) + "%'></div></div>");
  client.println("<div style='text-align: center;'><span id='status' class='status'></span><span id='statusText'></span></div>");
  client.println("<div class='timestamp' id='timeValue'>" + formatDateTime(rtc.now()) + "</div>");
  client.println("<div style='text-align: center; margin-top: 15px;'>");
  client.println("<button class='btn' onclick='toggleHistory()'>Show History</button>");
  client.println("<button class='btn' onclick='toggleTimeSettings()'>Time Settings</button>");
  client.println("<a href='/export' class='btn btn-success'>Export Data</a>");
  client.println("<a href='/clear' onclick='return confirmClear()' class='btn btn-danger'>Clear History</a>");
  client.println("</div>");
  client.println("</div>");
  
  // Time settings section
  client.println("<div id='timeSection' class='card hidden'>");
  client.println("<h2>RTC Time Settings</h2>");
  client.println("<form action='/settime' method='get'>");
  client.println("<div style='display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px;'>");
  client.println("<div><label>Year: <input type='number' id='year' name='year' min='2000' max='2099' required></label></div>");
  client.println("<div><label>Month: <input type='number' id='month' name='month' min='1' max='12' required></label></div>");
  client.println("<div><label>Day: <input type='number' id='day' name='day' min='1' max='31' required></label></div>");
  client.println("<div><label>Hour: <input type='number' id='hour' name='hour' min='0' max='23' required></label></div>");
  client.println("<div><label>Minute: <input type='number' id='minute' name='minute' min='0' max='59' required></label></div>");
  client.println("<div><label>Second: <input type='number' id='second' name='second' min='0' max='59' required></label></div>");
  client.println("</div>");
  client.println("<div style='margin-top: 15px; display: flex; justify-content: space-between;'>");
  client.println("<button type='button' class='btn' onclick='setCurrentTime()'>Current Time</button>");
  client.println("<a href='/synctime' class='btn'>Sync with Browser</a>");
  client.println("<button type='submit' class='btn btn-success'>Save</button>");
  client.println("</div>");
  client.println("</form>");
  client.println("</div>");
  
  // History section
  client.println("<div id='historySection' class='card hidden'>");
  client.println("<h2>Recent Measurements (Last 50)</h2>");
  client.println("<table>");
  client.println("<thead><tr><th>Time</th><th>Dust (ug/m³)</th><th>Voltage (mV)</th><th>Status</th></tr></thead>");
  client.println("<tbody id='historyData'></tbody>");
  client.println("</table>");
  client.println("<div style='text-align: center; margin-top: 15px;'>");
  client.println("<button class='btn' onclick='loadHistory()'>Refresh Data</button>");
  client.println("</div>");
  client.println("</div>");
  
  client.println("</div>");
  client.println("</body></html>");
  client.stop();
}

void sendErrorResponse(EthernetClient &client, const String &message) {
  client.println("HTTP/1.1 400 Bad Request");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<html><body><h2>Error: " + message + "</h2>");
  client.println("<script>setTimeout(() => window.location.href='/', 2000)</script>");
  client.println("</body></html>");
}

bool extractTimeParam(String &request, const char* param, int &value) {
  if (request.indexOf(param) != -1) {
    int start = request.indexOf(param) + strlen(param);
    int end = request.indexOf("&", start);
    if (end == -1) end = request.length();
    value = request.substring(start, end).toInt();
    return true;
  }
  return false;
}

bool isValidDateTime(int year, int month, int day, int hour, int minute, int second) {
  if (year < 2000 || year > 2099) return false;
  if (month < 1 || month > 12) return false;
  if (day < 1 || day > 31) return false;
  if (hour < 0 || hour > 23) return false;
  if (minute < 0 || minute > 59) return false;
  if (second < 0 || second > 59) return false;
  
  // Check for valid days in month
  if (month == 4 || month == 6 || month == 9 || month == 11) {
    if (day > 30) return false;
  } else if (month == 2) {
    bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (day > (isLeap ? 29 : 28)) return false;
  }
  
  return true;
}
