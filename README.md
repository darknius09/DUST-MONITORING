# DUST-MONITORING

🛠 Wiring GP2Y1014AU dengan ESP8266
1.	Pin GP2Y1014AU	Ke ESP8266
2.	VLED (Pin 1)	3.3V
3.	LED-GND (Pin 2)	GND (melalui resistor 150Ω)
4.	LED (Pin 3)	D0 (GPIO16, bisa diubah sesuai kebutuhan)
5.	S-GND (Pin 4)	GND
6.	Vo (Pin 5)	A0 (Sensor output ke ADC ESP8266)
7.	VCC (Pin 6)	5V (ESP8266 hanya 3.3V, jadi pakai step-up 3.3V ke 5V jika perlu)

    Catatan:
        Vo (Output sensor) masuk ke A0 ESP8266, karena ini adalah output tegangan yang akan dikonversi ke konsentrasi debu.
        LED (Pin 3) butuh pulsa ke GND melalui resistor 150Ω untuk memicu pengukuran.

🛠 Wiring LCD I2C dengan ESP8266
1.	Pin LCD I2C	Ke ESP8266
2.	VCC	3.3V atau 5V (sesuai LCD)
3.	GND	GND
4.	SDA	D2 (GPIO4)
5.	SCL	D1 (GPIO5)

🛠 Wiring ENC28J60 dengan ESP8266
Pin ENC28J60	Ke ESP8266
1.	VCC	3.3V
2.	GND	GND
3.	SCK	D5 (GPIO14)
4.	MISO	D6 (GPIO12)
5.	MOSI	D7 (GPIO13)
6.	CS (Chip Select)	D8 (GPIO15)

    Catatan:
        Pastikan MOSI (D7) tidak bentrok dengan perangkat lain, karena D7 digunakan bersama SPI.

🛠 Wiring Buzzer & Tombol Reset
Komponen	Pin ESP8266
1.	Buzzer	D3 (GPIO0)
2.	Push Button Reset	D4 (GPIO2, dengan PULLDOWN_16)
