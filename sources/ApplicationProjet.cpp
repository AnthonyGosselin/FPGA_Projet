#include "pch.h"
#include <iostream>
#include <string>

#include "CommunicationFPGA.h"
//Git2



using namespace std;
const int NUM_FILTERS = 4;
const int NUM_PHONEMES = 4;
const double MAX_VALEUR = 255;
const double DETECTION_TOLERANCE = 0.05; // (en %) Si le signal pour un filtre a un écart plus grand que ce pourcentage, il n'y aura pas de match

string PHONEMES[NUM_PHONEMES] = {
	"AH", // 0
	"EUH", // 1
	"Ii", // 2
	"OH", // 3
};


//Input est un objet qui va être créé pour chaque lecture (peut en avoir plusieurs par secondes)
class Input
{
public:
	double filterVals[NUM_FILTERS] = { 0,0,0,0 }; //Valeurs en pourcentage (sur 1)

	Input(double newFilterVals[NUM_FILTERS]) {
		for (int i = 0; i < NUM_FILTERS; i++) {
			double newVal = newFilterVals[i] / MAX_VALEUR; // Conversion se fait ici!
			//cout << "New val for filter #" << i << ": " << newVal << endl;
			filterVals[i] = newVal;
		}
	}

	//Au cas ou l'entree sont des entiers... (un des constructeurs pourrait etre enlevé à l'avenir
	Input(int newFilterVals[NUM_FILTERS]) {
		for (int i = 0; i < NUM_FILTERS; i++) {
			double newVal = (double)newFilterVals[i] / MAX_VALEUR; // Conversion se fait ici!
			//cout << "New val for filter #" << i << ": " << newVal << endl;
			filterVals[i] = newVal;
		}
	}
};

//Un objet qui fait la moyenne des «Input» pour un phonème, est utilisé pour détecter les phonèmes
class PhonemeRef
{
public:
	double referenceTab[NUM_FILTERS];
	int numInputs = 0;

	PhonemeRef() {}
	PhonemeRef(double forcedTab[NUM_FILTERS]) {
		for (int i = 0; i < NUM_FILTERS; i++) {
			referenceTab[i] = forcedTab[i];
		}
	}

	//Ajouter par un input
	void addInput(Input newInput) {
		compileInput(newInput.filterVals);
	}

	//Ajouter par un autre PhonemeRef existant (déjà une composition d'inputs)
	void addInput(PhonemeRef newPhonemeRef) {
		compileInput(newPhonemeRef.referenceTab);
	}

private:

	void compileInput(double newRefTab[NUM_FILTERS]) {
		//Ajouter nouvelle valeur pour trouver une valeur moyenne pour chaque phoneme
		//Pourrait calculer si c'est une valeur rejet et ne pas le prendre en compte
		//Pourrait etre une bonne idee que chaque phoneme ait deja une configuration de base comme minimum

		int currNum = ++numInputs;

		for (int i = 0; i < NUM_FILTERS; i++) {
			double nouvMoy = (referenceTab[i] * (currNum - 1) / currNum) + (newRefTab[i] / currNum); //Calcul de la moyenne à fur et a mesure
			//cout << "Nouvelle moyenne: " << nouvMoy << " (filtre: " << i << ") \n";
			referenceTab[i] = nouvMoy;
		}
	}

};

//On génère un «CustomSignature» propre à chaque utilisateur lors de la calibration
//«CustomSignature» regroupe un «PhonemeRef» par phonème
class CustomSoundSignature
{
public:
	PhonemeRef phonemeRefTab[NUM_PHONEMES];

	CustomSoundSignature() {}

	CustomSoundSignature(PhonemeRef newPhonemeRefTab[NUM_PHONEMES]) {
		for (int i = 0; i < NUM_PHONEMES; i++) {
			phonemeRefTab[i] = newPhonemeRefTab[i];
		}
	}
};

double randRange(int low, int high) {
	return low + (rand() % (high - low + 1));
}

Input generateInputTest(int n) {
	double randomTab[NUM_FILTERS];
	for (int i = 0; i < NUM_FILTERS; i++) {
		randomTab[i] = (randRange(0, (300) * (n + 1) / 4) / 100);
	}
	Input newInput(randomTab);

	return newInput;
}

Input getInputFromPort(CommunicationFPGA port) {
	int filterInputTab[4];

	bool success0 = port.lireRegistre(LECT_CAN0, filterInputTab[0]);
	bool success1 = port.lireRegistre(LECT_CAN1, filterInputTab[1]);
	bool success2 = port.lireRegistre(LECT_CAN2, filterInputTab[2]);
	bool success3 = port.lireRegistre(LECT_CAN3, filterInputTab[3]);

	//La conversion devrait se faire dans le constructeur de la class Input
	Input newInput(filterInputTab);

	return newInput;
}

PhonemeRef readPhonemeFromPort(CommunicationFPGA &port, int p = 0) {

	PhonemeRef newRef;
	//Quelques lectures par phoneme
	for (int i = 0; i < 5; i++) {
		newRef.addInput(generateInputTest(p)); //newRef.addInput(getInputFromPort(port)); // Pour des vraies valeurs
		Sleep(10); //... 
	}

	return newRef;
}

//Pourrait en sort, durant la calibration, que tu ne peux pas enregistrer un phonème s'il est trop similaire à un phonème déjà enregistré (s'il y a des problèmes de collisions)
CustomSoundSignature calibration(CommunicationFPGA port) {

	cout << "--- Calibration des phonemes --- \n";

	//Prend des mesures un phoneme a la fois
	PhonemeRef phonemeRefTab[NUM_PHONEMES];
	for (int p = 0; p < NUM_PHONEMES; p++) {
		cout << "\nCalibration: " << PHONEMES[p] << " ---\n";

		PhonemeRef newRef;

		//On répète le phonème quelques fois
		for (int r = 0; r < 2; r++) {
			cout << "\nAppuyer sur le bouton et dites: " << PHONEMES[p] << "!" << endl;

			// Attend que le bouton soit enfoncé
			int buttonVal = 0;
			while (buttonVal == 0) {
				bool success = port.lireRegistre(LECT_BTN, buttonVal);
				//cout << "Success: " << success << " " << buttonVal << endl;
			}

			//cout << "En train d'écouter...\n";

			//Il va falloir vérifier que le niveau sonore est assez élevé avant de prendre des lectures (pour être sûr qu'on parle) **
			newRef.addInput(readPhonemeFromPort(port, p)); //Prend une lecture
			
			//cout << "Recu!\n";

			// Attend que le bouton soit relâché
			bool success = port.lireRegistre(LECT_BTN, buttonVal);
			if (buttonVal == 0)
				Sleep(500); // Bouton est déjà relâché, donc on attend avant d'initier la prochaine lecture
			else {
				cout << "Relachez le bouton\n";
				
				while (buttonVal != 0) {
					bool success = port.lireRegistre(LECT_BTN, buttonVal);
					//cout << "Success: " << success << " " << buttonVal << endl;
				}
			}
			cout << "loop\n";
		}
		phonemeRefTab[p] = newRef;

	}

	cout << "\n--- Calibration terminee ---\n";

	//On regroupe le tout dans une signature
	CustomSoundSignature newSignature;
	for (int i = 0; i < NUM_PHONEMES; i++) {
		newSignature.phonemeRefTab[i] = phonemeRefTab[i];
		cout << "Phoneme #" << i << " ( ";
		for (int y = 0; y < NUM_FILTERS; y++) {
			cout << phonemeRefTab[i].referenceTab[y] << ", ";
		}
		cout << ")\n";
	}

	return newSignature;
}


//Retourne le numéro du phonème détecté
int identifyPhoneme(CustomSoundSignature refSignature, PhonemeRef phonemeInput) {

	double diffTab[NUM_PHONEMES][NUM_FILTERS] = { DETECTION_TOLERANCE * 2 };

	//Sets difference between input signal and signature for each filter of each phoneme
	for (int p = 0; p < NUM_PHONEMES; p++) {
		for (int f = 0; f < NUM_FILTERS; f++) {
			double diff = abs(refSignature.phonemeRefTab[p].referenceTab[f] - phonemeInput.referenceTab[f]);
			diffTab[p][f] = (diff);
		}
	}

	//C'est un match si tous les signaux des filtres sont en dessous de la tolérance d'écart pour un phonème
	bool gotMatch[NUM_PHONEMES] = { false };
	int matchedPhoneme = -1;
	bool multipleMatches = false;
	for (int p = 0; p < NUM_PHONEMES; p++) {
		bool phonemeMatch = true;
		for (int f = 0; f < NUM_FILTERS; f++) {
			if (diffTab[p][f] > DETECTION_TOLERANCE) {
				phonemeMatch = false;
				break; // Arrete de checker ce phoneme
			}
		}
		gotMatch[p] = phonemeMatch;
		if (phonemeMatch) {
			cout << "Got match: " << p << endl;
			if (matchedPhoneme == -1)
				matchedPhoneme = p;
			else
				multipleMatches = true;
		}
	}

	return multipleMatches ? -1 : matchedPhoneme; // Si l'on détecte plus d'un phonème, on retourne qu'il n'y a pas de match
}

void testReadCanaux(CommunicationFPGA port) {
	while (true) {
		int filterInputTab[4];

		bool success0 = port.lireRegistre(LECT_CAN0, filterInputTab[0]);
		bool success1 = port.lireRegistre(LECT_CAN1, filterInputTab[1]);
		bool success2 = port.lireRegistre(LECT_CAN2, filterInputTab[2]);
		bool success3 = port.lireRegistre(LECT_CAN3, filterInputTab[3]);

		for (int i = 0; i < NUM_FILTERS; i++) {
			cout << filterInputTab[i] << "\t";
		}
		cout << endl;
	}
}


int main(int argc, char **argv) {
	cout << "Start \n\n\n";

	//FPGA setup
	CommunicationFPGA port;   // Instance du port de communication
	if (!port.estOk())
		cout << "Erreur: " << port.messageErreur() << endl;

	//Logic setup
	CustomSoundSignature newSignature;
	newSignature = calibration(port);

	//Lecture des phonèmes
	while (true) {
		int matchPhoneme = identifyPhoneme(newSignature, readPhonemeFromPort(port));
		if (matchPhoneme < 0)
			cout << "No match\n";
		else {
			cout << "Matched: " << matchPhoneme << endl;
			//Appeler la fonction appropriée
		}
		Sleep(2000);
	}

	Sleep(10000);
	return 0;
}


