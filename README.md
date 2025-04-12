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


perbaiki wiring pada gambar seperti dibawah ini

Wiring w5500 ke ESP8266

    SCK D5 (GPIO14)

    MISO D6 (GPIO12)

    MOSI D7 (GPIO13)

    CS D0

    VCC 3.3V

    GND GND

    Catatan: W5500 beroperasi pada 3.3V, jangan langsung sambungkan ke 5V.

Wiring Sensor Debu GP2Y1014AU GP2Y1014AU Pin ESP8266 Pin

    VCC (Pin 1) 5V

    GND (Pin 2) GND

    LED (Pin 3) D4

    Vo (Pin 4) A0

    V-LED (Pin 5) 5V

    Resistor 150Ω D0 → LED

    Catatan: Pin LED (D0) digunakan untuk menyalakan laser pada sensor GP2Y1014AU.

Wiring LCD I2C ke ESP8266 LCD Pin ESP8266 Pin

    VCC 3.3V
    GND GND
    SDA D2
    SCL D1

Wiring Buzzer Komponen ESP8266 Pin

    Buzzer (+) D3
    Buzzer (-) GND

Wiring LCD I2C ke ESP8266 LCD Pin ESP8266 Pin

    VCC 3.3V
    GND GND
    SDA D2
    SCL D1

Wiring RTC DS3231 ke ESP8266 LCD Pin ESP8266 Pin

    VCC 3.3V
    GND GND
    SDA D2
    SCL D1

Wiring Microsd ke ESP8266 Pin

    SCK D5 (GPIO14)

    MISO D6 (GPIO12)

    MOSI D7 (GPIO13)

    CS D8

    VCC 3.3V

    GND GND
