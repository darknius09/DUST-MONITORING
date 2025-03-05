# DUST-MONITORING GP2Y1014AU
monitoring debu GP2Y1014AU yang dihubungkan ke PC/Laptop Menggunakan module CH9121 melalui fiber optic

# Perbaikan dan Optimalisasi
**Sensor Debu Ditingkatkan**
1.	Menambahkan aktivasi LED sensor sebelum membaca data (penting untuk GP2Y1014AU).
2.	Menggunakan map() untuk mengkonversi hasil analog menjadi ug/m3.
   
**Alarm Berfungsi Otomatis**
1.	Alarm menyala saat debu > 50 ug/m3.
2.	Alarm mati otomatis jika debu turun di bawah 50 ug/m3.

# WIRING DUST-MONITORING
Wiring w5500 ke ESP8266
1.	CH9121 Pin	ESP8266 Pin
2. SCK D5 (GPIO14)
3. MISO D6 (GPIO12)
4. MOSI D7 (GPIO13)
5. CS D3
6.	VCC	3.3V
7.	GND	GND

    Catatan: W5500 beroperasi pada 3.3V, jangan langsung sambungkan ke 5V.

**Wiring Sensor Debu GP2Y1014AU**
GP2Y1014AU Pin	ESP8266 Pin
1.	VCC (Pin 1)	5V
2.	GND (Pin 2)	GND
3.	LED (Pin 3)	D0
4.	Vo (Pin 4)	A0
5.	V-LED (Pin 5)	5V
6.	Resistor 150Ω	D0 → LED

    Catatan: Pin LED (D0) digunakan untuk menyalakan laser pada sensor GP2Y1014AU.

**Wiring LCD I2C ke ESP8266**
LCD Pin	ESP8266 Pin
1.	VCC	3.3V
2.	GND	GND
3.	SDA	D2
4.	SCL	D1

**Wiring Buzzer**
Komponen	ESP8266 Pin
1.	Buzzer (+)	D4
2.	Buzzer (-)	GND

Dengan wiring ini, sistem seharusnya dapat berjalan dengan W5500 sebagai Ethernet WEBSERVER, GP2Y1014AU sebagai sensor debu, LCD I2C sebagai display, dan buzzer untuk alarm. 🚀
