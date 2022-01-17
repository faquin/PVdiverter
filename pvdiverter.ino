
/* Formule pour calculer le delay en fonction de la puissance demandée:
//firingDelayInMicros = (asin(((1800 - energyInBucket) / 500)) + (PI/2)) * (10000/PI); // entre 1300W et 2300W
//firingDelayInMicros = (asin(((375 - energyInBucket) / 357)) + (PI/2)) * (10000/PI); // entre 0W et 750W

//#define PUISSANCE_MAX 750
//firingDelayInMicros = (asin(((PUISSANCE_MAX/2-energyInBucket) / PUISSANCE_MAX/2)) + (PI/2)) * (10000/PI);

// source circuit: https://www.instructables.com/Arduino-controlled-light-dimmer-The-circuit/
// schema utilisé:  https://www.sonelec-musique.com/images/electronique_triac_001e.gif
// explications     https://www.sonelec-musique.com/electronique_theorie_triac.html

 13/03/2020: ajouté watchdog de 5sec
 03/06/2020: ajouté enregistrement de bCmdActive en EEPROM
 08/11/2020: ajouté filtrage dans les interruptions avec interval_min_interrupt. Si on rentre 2 fois en 5ms dans l'interruption, c'est une fausse detection, on ignore la commande. 
             Ajouté pour pallier aux soucis quand le Xenon ne répond plus. Je pense qu'il reçoit trop d'interruptions. J'ai trouvé un faux contact dans le connecteur bleu sur la borne 'P', c'est peut-être lié
 09/11/2020: si firingDelayInMicros vaut 9700, on n'allume plus le triac dans l'interruption, on sort directement. Ca permet de liberer du temps CPU quand il ne faut pas passer de pusisance
             Ajouté une fonction pour envoyer au démarrage la cause de la dernière extinction. C'est pour voir si le watchdog fonctionne. Du coup au démarrage on attend 10sec de se connecter au Mesh, si ça ne suffit pas, tant pis on execute la loop()
 09/01/2022: modif dans PVdivCmdVA(): si on a bCmdActive=0 on ne prend plus en compte les valeurs envoyées par le Mesh par Emon
			 ActiveCmd Modifié si on desactive les ordres Mesh, on reinitialise CmdVA a 0W
*/

SYSTEM_THREAD(ENABLED);
STARTUP(System.enableFeature(FEATURE_RESET_INFO));

#include <math.h>

#define addCmdActEEPROM 0
#define interval_min_interrupt 5000   //5ms

bool bLed=0;


#define pinZeroCrossDetection A1    //A1
#define pinCmdTriac D8  //D8
volatile int iNumZeroCross=0;
bool bMeshStatus=0;

int iDefZeroCross=0;

//COMMANDE VA
float PUISSANCE_MAX=1500;    // valeur par défaut. Peut-être changé par la fonction Cloud defPuissanceMax()
float iCmdVA=0;   // puissance en VA que doit commander le triac. Peut-être changé par la fonction Cloud defCmdVA(), et le message Mesh PVdivCmdVA
volatile float firingDelayInMicros=9700;
volatile bool bCmdActive=0;
double freq;

//TIMERS
unsigned long TimeLastMeasure=0;
int interval=1000;
volatile unsigned long lastInterrupt=0;


//WATCHDOG
//ApplicationWatchdog wd(5000, System.reset);    // reset the system after 5 seconds if the application is unresponsive
ApplicationWatchdog *wd;
// AWDT count reset automatically after loop() ends

// variable calculée
String returnfiringDelay(){
 return String(firingDelayInMicros,0);
}




void setup() {
    Serial.begin(115200);
    Mesh.selectAntenna(MeshAntennaType::EXTERNAL);
    
    if (waitFor(Particle.connected, 10000)) {
        resetReason();  //publish event with last update cause
    }

    //whatchdog
    wd = new ApplicationWatchdog(5000, System.reset, 1536);
    
    Particle.function("defPuissanceMax", defPuissanceMax);
    Particle.function("defCmdVA", defCmdVA);
    Particle.function("ActiveCmd", ActiveCmd);
   
    Particle.variable("freq", freq);
    Particle.variable("firingDelayInMicros", returnfiringDelay);    // variable calculée dans returnfiringDelay()
    Particle.variable("iDefZeroCross",iDefZeroCross);   // renvoie le nombre de fois où on a eu des fausses detections
    
    Mesh.subscribe("PVdivCmdVA", PVdivCmdVA);
    
    
    
    pinMode(pinZeroCrossDetection, INPUT);
    attachInterrupt(pinZeroCrossDetection, ZeroCrossDetected, RISING);
    
    pinMode(pinCmdTriac, OUTPUT);

    TimeLastMeasure=0;  // reset de la mesure

    pinMode(D7, OUTPUT);
    
    readAuthEEPROM();
    
}

void loop() {

    if ((millis() - TimeLastMeasure) >= (interval)) { // interval=1000ms = 1s
		freq=(double)iNumZeroCross/(millis()-TimeLastMeasure)*0.5*1000;   // 2 comptages par periode: on divise le nombre par deux
		
		if(freq>55){
	        String strTemp = "freqPvDiverter="+String(freq);
	        Particle.publish("toPB", strTemp, PRIVATE);
		}
		
		//digitalWrite(D7, bLed);
		//bLed = ! bLed;
		
		//Serial.print("freq=");
        //Serial.println(freq);
		
		iNumZeroCross=0;
		TimeLastMeasure = millis();
		
		// TESTS
    	
        
        
		//Serial.println();
    }
    
    
    if (Serial.available() > 0)
    {
        defCmdVA(Serial.readString());
    }
    
    /* NE MARCHE PAS: apaprement le Mesh.ready switch souvent, donc les interruptions sont regulierement désactivées/réactivées (plusieurs fois par seconde!)
    if(Mesh.ready() == true){  //  connecté au MESH
        attachInterrupt(pinZeroCrossDetection, ZeroCrossDetected, RISING);  // activation de l'interruption
        bMeshStatus=1;
    }
    else{// pas connecté au MESH
        detachInterrupt(pinZeroCrossDetection); // suppression de l'interruption
        bMeshStatus=0;
    }*/
}

int ActiveCmd(String command)
{
  bCmdActive = command.toInt();
  if(bCmdActive==0) defCmdVA("0");	// 09/01/2022: si on desactive les ordres Mesh, on reinitialise CmdVA a 0W
  EEPROM.write(addCmdActEEPROM, bCmdActive);
  return bCmdActive;
}

void PVdivCmdVA(const char *event, const char *data)
{
    //Serial.print("Mesh CmdVA=");
    //Serial.println(data);
    if(bCmdActive){			// 09/01/2022: ajouté pour empecher de prendre en compte les demandes de l'Emon si bCmdActive=0. Ca permet de forcer la valeur par la fonction Console defCmdVA
		defCmdVA(data);
	}
	
}

int defCmdVA(String command){
    float temp=command.toFloat();
    if(temp>=0){
        iCmdVA = temp;
        //Serial.print("iCmdVA=");
        //Serial.println(iCmdVA);
        firingDelayInMicros = (asinf(((PUISSANCE_MAX/2)-iCmdVA) / (PUISSANCE_MAX/2)) + (M_PI/2)) * (10000/M_PI);
    	if(firingDelayInMicros<1600){
    	    firingDelayInMicros = 1600;
    	}
    	else if(firingDelayInMicros>9700){
    	    firingDelayInMicros = 9700;
    	}
    	//Serial.print("firingDelayInMicros=");
        //Serial.println(firingDelayInMicros);
        Particle.publish("firingDelayInMicros", String(firingDelayInMicros), PRIVATE);
        return temp;
    }
    else
        return -1;
}

int defPuissanceMax(String command){
    float temp=command.toFloat();
    if(temp>0){
        PUISSANCE_MAX = temp;
        Serial.print("PUISSANCE_MAX=");
        Serial.println(PUISSANCE_MAX);
        return PUISSANCE_MAX;
    }
    else
        return -1;
}

void ZeroCrossDetected(void){
    if((micros()-lastInterrupt) > interval_min_interrupt){   // si ca fait au moins 5ms qu'on est venu, on execute le code. Sinon on ne fait rien, c'est une fausse detection de ZeroCrossing
        lastInterrupt=micros();
        iNumZeroCross++;
        if(firingDelayInMicros<9700){ // 09/11/20 si firingDelayInMicros>=9700, on demande 0W, donc le mieux est de laisser la pinCmdTriac à 0
            //digitalWrite(D7, LOW);
            delayMicroseconds((unsigned int)firingDelayInMicros);  
            
            pinSetFast(pinCmdTriac);
            //digitalWrite(D7, HIGH);
            delayMicroseconds(10);
            pinResetFast(pinCmdTriac);
        }
        else{
            pinResetFast(pinCmdTriac);
        }
    }
    else
        iDefZeroCross++;    // on incremente le compteur de défauts
}

void readAuthEEPROM(void){
    bCmdActive=EEPROM.read(addCmdActEEPROM);
}

void resetReason(void)
{
	String strTemp = "PVDiverter ResetReason=";
	switch( System.resetReason() )
	{
		case RESET_REASON_PIN_RESET:		strTemp += "PIN_RESET"; break;
		case RESET_REASON_POWER_MANAGEMENT:	strTemp += "POWER_MANAGEMENT";	break;
		case RESET_REASON_POWER_DOWN:		strTemp += "POWER_DOWN"; break;
		case RESET_REASON_POWER_BROWNOUT:	strTemp += "POWER_BROWNOUT"; break;
		case RESET_REASON_WATCHDOG:		    strTemp += "WATCHDOG"; break;
		case RESET_REASON_UPDATE:		    strTemp += "UPDATE"; break;
		case RESET_REASON_UPDATE_TIMEOUT:	strTemp += "UPDATE_TIMEOUT"; break;
		case RESET_REASON_FACTORY_RESET:	strTemp += "FACTORY_RESET"; break;
		case RESET_REASON_DFU_MODE:		    strTemp += "DFU_MODE"; break;
		case RESET_REASON_PANIC:		    strTemp += "PANIC"; break;
		case RESET_REASON_USER:			    strTemp += "USER"; break;
		case RESET_REASON_UNKNOWN:		    strTemp += "UNKNOWN"; break;
		case RESET_REASON_NONE:		 	    strTemp += "NONE"; break;
	}

	Particle.publish("toPB", strTemp, PRIVATE);

}
