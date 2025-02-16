# DUST-MONITORING
Wiring CH9121 ke ESP8266
CH9121 Pin	ESP8266 Pin
TX	D6 (RX)
RX	D7 (TX)
VCC	3.3V
GND	GND

    Catatan: CH9121 beroperasi pada 3.3V, jangan langsung sambungkan ke 5V.

Wiring Sensor Debu GP2Y1014AU
GP2Y1014AU Pin	ESP8266 Pin
VCC (Pin 1)	5V
GND (Pin 2)	GND
LED (Pin 3)	D0
Vo (Pin 4)	A0
V-LED (Pin 5)	5V
Resistor 150Ω	D0 → LED

    Catatan: Pin LED (D0) digunakan untuk menyalakan laser pada sensor GP2Y1014AU.

Wiring LCD I2C ke ESP8266
LCD Pin	ESP8266 Pin
VCC	3.3V
GND	GND
SDA	D2
SCL	D1
Wiring Buzzer & Tombol Reset
Komponen	ESP8266 Pin
Buzzer (+)	D3
Buzzer (-)	GNDPush Button	D4 (INPUT_PULLUP)

Dengan wiring ini, sistem seharusnya dapat berjalan dengan CH9121 sebagai Ethernet, GP2Y1014AU sebagai sensor debu, LCD I2C sebagai display, dan buzzer serta tombol reset untuk alarm. 🚀
