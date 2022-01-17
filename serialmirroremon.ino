/* 13/03/2020
    Ajouté watchdog
    Ajouté bAuthDivert: quand c'est mis à 0, la divertion n'est pas réactivée en sortie du timer 1min
    Activé par défaut (au demarrage) la divertion, et mis la marge par défaut à 15W
    Changé la valeur max de divertion à 1200W (depuis 750W) pour detection de panne divertion

    03/06/2020: ajouté enregistrement de bAuthDivert en EEPROM
    10/06/2021: ajouté variable Cloud pour ct1, divert et bAuthDivert

*/

#define addAuthDivertEEPROM 0


int safetyMargin_watts = 15;
float lastCmdVA=0;
bool bActDivert=0;  // pour couper la diversion pendant 1min
bool bAuthDivert=0; // active la diversion

// pour les variables Cloud
String sCT1="";
String sDivert="";


// créer un timer de durée 1min
Timer timer1min(1000*60, ReactiveDivert1min, true);  


//WATCHDOG
ApplicationWatchdog wd(30000, System.reset);    // reset the system after 30 seconds if the application is unresponsive
// AWDT count reset automatically after loop() ends


void setup() {
    
    Mesh.selectAntenna(MeshAntennaType::EXTERNAL);
    
    Particle.function("defSafetyMargin", defSafetyMargin);
    Particle.function("AuthDivert", AuthDivert);
    
    Particle.function("UpdateDBPV", UpdateDBPV);
    Particle.subscribe("hook-response/GetLastSolarKwh", HandlerLastSolar, MY_DEVICES);
    
    Particle.variable("ct1", sCT1);
    Particle.variable("divert", sDivert);
    Particle.variable("AuthDivert", bAuthDivert);

    Serial1.begin(115200); // Uses RX/TX to communicate to Arduino
    Serial1.setTimeout(10); // Temps max d'attente pour readStringUntil(). On envoi 115.2 octets en 10ms à 115200bits/s
    
    Serial.begin(115200); //for debug
    Serial.println("Particle ready, waiting for event");
    
    
    pinMode(D7, OUTPUT);    //LED
    
    readAuthEEPROM();   // va lire en EEPROM si AuthDivert était activé ou non
    
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
    	if(s.startsWith("ct1:")){   // si c'est une ligne de données
    	    Serial.printlnf("Contient ct1: on publie");
    	    float CmdVA=0;
    	    
    	    sCT1="";
    	    sDivert="";
    	    
    	    sCT1 = s.substring(4,s.indexOf(","));
    	    float fCT1 = sCT1.toFloat(); 
    	    sDivert = s.substring(s.indexOf("divert:")+7);
    	    
    	    
    	    if(bActDivert){
    	        
    	        
        	    if((fCT1<(0-safetyMargin_watts))||(lastCmdVA>safetyMargin_watts)){   // si on exporte plus que safetyMargin_watts (30W) OU que l'on divertait déjà, il faut recalculer la nouvelle diversion
        	        CmdVA = 0- fCT1 + lastCmdVA - safetyMargin_watts;
        	        if(CmdVA >1200){    // si valeur calculée est trop grande c'est que la diversion ne marche pas. On met pause pour 1min
        	            CmdVA=0;    //
        	            bActDivert=0;   // on désactive la diversion
        	            timer1min.start();  // on lance le timer de 1min
        	            Particle.publish("DivertHalt", PRIVATE);
        	            
        	        }
        	    }
        	    else{
        	        CmdVA = 0;  // sinon on supprime la diversion
        	    }
        	    

    	    }
    	    else{
    	        CmdVA = 0;  // sinon on supprime la diversion
    	    }
    	    s.concat(",divert:");
    	    s.concat(lastCmdVA);    // on publie le lastCmdVA, ca correspond à la puissance deviée pour le ct mesuré actuel
    	    Mesh.publish("PVdivCmdVA", String(CmdVA));
    	    
    	    lastCmdVA = CmdVA;
    	    Particle.publish("emoncms", s, PRIVATE);
    	    
    	  
    	    digitalWrite(D7, HIGH);
    	    delay(100);
    	    digitalWrite(D7, LOW);
    	}
    }
}

void ReactiveDivert1min(){
    if(bAuthDivert){
        bActDivert = 1; // on reactive la diversion
    }
}

int defSafetyMargin(String command){
    safetyMargin_watts = command.toInt();
    return safetyMargin_watts;
}

int AuthDivert(String command){
    bAuthDivert = command.toInt();
    bActDivert = bAuthDivert;
    EEPROM.write(addAuthDivertEEPROM, bAuthDivert);
    return bActDivert;
}

void readAuthEEPROM(void){
    bAuthDivert = EEPROM.read(addAuthDivertEEPROM);
    bActDivert = bAuthDivert;
}

int UpdateDBPV(String command){
    // on envoie un webhook pour demander la dernière valeur de EmonCMS
    Particle.publish("GetLastSolarKwh", PRIVATE);
    return 1;
}

void HandlerLastSolar(const char *event, const char *data) {
  double totalSolarKwh = atof(data);
  Particle.publish("SendBDPV", String(totalSolarKwh*1000), PRIVATE);  // on envoie un webhook pour mettre à jour DPDV, en Wh au  lieu de Kwh
}
