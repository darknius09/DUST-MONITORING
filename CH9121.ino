 EthernetClient client = server.available();
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
