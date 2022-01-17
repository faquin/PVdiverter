STARTUP(WiFi.selectAntenna(ANT_EXTERNAL)); // selects the u.FL antenna

//WATCHDOG
ApplicationWatchdog wd(30000, System.reset);    // reset the system after 5 seconds if the application is unresponsive
// AWDT count reset automatically after loop() ends

// pour les variables Cloud
String sCT4="";

void setup() {
    
    Particle.variable("ct4", sCT4);

    Serial1.begin(115200); // Uses RX/TX to communicate to Arduino
    Serial1.setTimeout(10); // Temps max d'attente pour readStringUntil(). On envoi 115.2 octets en 10ms à 115200bits/s
    
    Serial.begin(115200); //for debug
    Serial.println("Particle ready, waiting for event");
    
    pinMode(D7, OUTPUT);    //LED
}

void loop() {
    // read from port 0, send to port 1:
    if (Serial.available())
    {
        int inByte = Serial.read();
        Serial1.write(inByte);
    }
    
    // read from port 1, send to port 0:
    /*if (Serial1.available())
    {
    
        int inByte = Serial1.read();
        Serial.write(inByte);
    }*/
    
    if (Serial1.available()){
        String s = Serial1.readStringUntil('\r');
    	Serial.printlnf("%s", s.c_str());
    	sCT4="";
    	
    	if(s.startsWith("ct4:")){   // si c'est une ligne de données
    	    Serial.printlnf("Contient c4t: on publie");
    	    //sCT4 = s.substring(4,s.indexOf(","));
    	    sCT4 = s.substring(4);
    	    Particle.publish("emoncmsSolar", s, PRIVATE);
    	    digitalWrite(D7, HIGH);
    	    delay(100);
    	    digitalWrite(D7, LOW);
    	}
    }
}
