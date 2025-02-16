# DUST-MONITORING GP2Y1014AU
monitoring debu GP2Y1014AU yang dihubungkan ke PC/Laptop Menggunakan module CH9121 melalui fiber optic

# Perbaikan dan Optimalisasi
**Sensor Debu Ditingkatkan**
1.	Menambahkan aktivasi LED sensor sebelum membaca data (penting untuk GP2Y1014AU).
2.	Menggunakan map() untuk mengkonversi hasil analog menjadi ug/m3.
   
**Alarm Berfungsi Otomatis**
1.	Alarm menyala saat debu > 50 ug/m3.
2.	Alarm mati otomatis jika debu turun di bawah 50 ug/m3.

** Reset Manual 10 Menit**
1.	Jika tombol reset ditekan atau "reset" diketik di Serial Monitor, alarm mati sementara selama 10 menit.
2.	Setelah 10 menit, alarm aktif kembali.

# WIRING DUST-MONITORING
3.	Wiring CH9121 ke ESP8266
1.	CH9121 Pin	ESP8266 Pin
2.	TX	D6 (RX)
3.	RX	D7 (TX)
4.	VCC	3.3V
5.	GND	GND

    Catatan: CH9121 beroperasi pada 3.3V, jangan langsung sambungkan ke 5V.

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

**Wiring Buzzer & Tombol Reset**
Komponen	ESP8266 Pin
1.	Buzzer (+)	D3
2.	Buzzer (-)	GND
3.	Push Button (+)	D4 (INPUT_PULLUP)
4.	Push Button (-)

Dengan wiring ini, sistem seharusnya dapat berjalan dengan CH9121 sebagai Ethernet, GP2Y1014AU sebagai sensor debu, LCD I2C sebagai display, dan buzzer serta tombol reset untuk alarm. 🚀
