# PVdiverter
**Pins**
* A1: sortie du H11AA1. Le signal est à HIGH (pull-up au 3.3V) quand la tension sinusoïdale est à 0V (donc toutes les 10ms)
* D8: commande du triac. Quand le signal est HIGH, le MOC3012M commande au triac de se mettre en conduction.

**Define**
* #define PUISSANCE_MAX 750 => remplacer par la puissance max à commander. Utilisé pour calculer le delai d'activation du triac. Si on demande une puissance de PUISSANCE_MAX, le triac sera piloté durant toute la demi onde.
