# PVdiverter
**Pins**
* pinZeroCrossDetection A1: sortie du H11AA1. Le signal est à HIGH (pull-up au 3.3V) quand la tension sinusoïdale est à 0V (donc toutes les 10ms)
* pinCmdTriac D8: commande du triac. Quand le signal est HIGH, le MOC3012M commande au triac de se mettre en conduction.

**Parametres**
* #define PUISSANCE_MAX 750 => remplacer par la puissance max à commander. Utilisé pour calculer le delai d'activation du triac. Si on demande une puissance de PUISSANCE_MAX, le triac sera piloté durant toute la demi onde.
* iCmdVA est la consigne de puissance exprimée en VA. 

**Calcul du delai du triac**
* firingDelayInMicros = (asin(((PUISSANCE_MAX/2-iCmdVA) / PUISSANCE_MAX/2)) + (M_PI/2)) * (10000/M_PI);
* La valeur de firingDelayInMicros est le retard en us à l'allumage du triac. Si il vaut 0, le triac est commandé immediatement.
* Reglable entre 0 et 10000us
