#include <RTClib.h>

/*

*/
#include <Wire.h>
#include <Dusk2Dawn.h>
#include  "x10rf.h"


//#define DEBUG


//Coordonnée GPS du poulailler
Dusk2Dawn maison(44.80389, 1.63350, 2);

int HeureLeverInt=0;
int HeureCoucherInt=0;
uint8_t soleilHeur=0;
uint8_t soleilMinut=0;

DateTime DTHeureLeve;
DateTime DTHeureCouche;


#define TX_PIN 4                  // pin 3 // Emeteur *RF433
#define FinCHaut A2               // Interupteur Fin de course Haut
#define FinCBas A1                // Interupteur Fin de course Bas
#define moteur_haut 8             // Cablage commande moteur via L298 IN1
#define moteur_bas 7              // Cablage commande moteur via L298 IN2
#define enable 9                  // Cablage activation moteur via L298 "A enable  "        
#define bt_haut 5                 // Bouton Haut (ouverture manuel)
#define bt_bas 6                  // Bouton Bas (Fermeture manuel)
#define bt_stop 2                 // Bouton Stop (pour faire un arret) non utilisé...

// LED Mode manuel sur la PinLed Arduino (13)
#define LEDbleu 12 // Led Bleu
#define LEDverte 11 // Led vert
#define LEDrouge 10 // Led rouge 

#define TempModeManuel 60000      // Durée du mode manuel en ms    // 1 800 000 = 30mins // 60 000 = 1mins
#define TempLedStatut 900000      // Durée d'affichage de la LED en ms    // 900 000 = 15mins

#define HeurMini 7 // Heur d'ouverture minimum
#define MinMini 30 // Minute lie à l'heure ouverture minimum



bool bugPorte;
bool etatPorte;
bool ancBugPorte;
bool sensMoteur=true;
byte nbrEssai;
uint8_t oldValueA = -1;
bool etatPorteAncien;
bool HeureEte = false;
bool ModeAuto = true;
bool ancModeAuto = false;

x10rf myx10 = x10rf(TX_PIN, 0, 3); // no blink led and send msg three times
uint8_t valueE;

RTC_DS3231 RTC;     // Instance du module RTC de type DS3231
DateTime now;




uint8_t ancHeur;

unsigned long previousMillis = 0;
unsigned long interval = TempModeManuel;
unsigned long previousMillis2 = 0;
unsigned long interval2 = TempLedStatut;


/* Couleurs (format RGB) */
const byte COLOR_BLACK = 0b000;
const byte COLOR_RED = 0b100;
const byte COLOR_GREEN = 0b010;
const byte COLOR_BLUE = 0b001;
const byte COLOR_CYAN = 0b011;


void setup() {

  #ifdef DEBUG
  // démarrage la liaison série entre entrée analogique et ordi
  Serial.begin(9600);
  Serial.println("MODE DEBUG");
  #endif
  
  //Déclaration des Pull Up sont des résistances internes à l'arduino.
  //Donc de base lorsque le boutton n'est pas appuyé on lit un état haut (5V = niveau logique 1)
  pinMode(FinCHaut, INPUT_PULLUP);
  pinMode(FinCBas, INPUT_PULLUP);
  pinMode(bt_haut, INPUT_PULLUP);
  pinMode(bt_bas, INPUT_PULLUP);
  pinMode(bt_stop, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(bt_stop), ArretMoteur, LOW);

  pinMode(LEDbleu, OUTPUT);
  pinMode(LEDverte, OUTPUT);
  pinMode(LEDrouge, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);


  //pinMode(bt_stop, INPUT_PULLUP);
  digitalWrite(enable, LOW);

  // Initialise la liaison I2C
  Wire.begin();

  // Initialise le module RTC
  RTC.begin();

  // indique erreur
  bugPorte = false;
  ancBugPorte = false;

  etatPorteAncien = true;
  etatPorte = true;
  nbrEssai=0;

  now = RTC.now();
  
  if (periodeEte(now.year() - 2000, now.month(), now.day(), now.hour())) {
    HeureEte = true;
  }
  
  HeureCoucherInt=maison.sunset(now.year(), now.month(), now.day(), !HeureEte);
  soleilHeur = min2Heur(HeureCoucherInt);
  soleilMinut = min2Min(HeureCoucherInt);
  DTHeureCouche = DateTime(now.year(), now.month(), now.day(), soleilHeur, soleilMinut, 0);
  DateTime future2 (DTHeureCouche + TimeSpan(0, 0, 20, 0));
   DTHeureCouche = future2;
  
  HeureLeverInt=maison.sunrise(now.year(), now.month(), now.day(), !HeureEte);
  soleilHeur = min2Heur(HeureLeverInt);
  soleilMinut = min2Min(HeureLeverInt);
  
  if(soleilHeur < HeurMini){
    soleilHeur=HeurMini;
    soleilMinut=MinMini;
  }
  if(soleilHeur == HeurMini && soleilMinut <MinMini){
    soleilHeur=HeurMini;
    soleilMinut=MinMini;
  }
  DTHeureLeve = DateTime(now.year(), now.month(), now.day(), soleilHeur, soleilMinut, 0);
  
  ancHeur = now.hour();

  #ifdef DEBUG
      Serial.println(" ");
      Serial.println("///-- Dans le SETUP() --///");
      affiche_date_heure(now);  //Affiche la date en langue humaine
      Serial.println("-/-/-/-/-/-/-/-/-/-");

      Serial.print("Ephe-Heure Lever : ");
      affiche_date_heure(DTHeureLeve);
      Serial.println("-----------------");

      Serial.print("Ephe-Heure Coucher : ");
      affiche_date_heure(DTHeureCouche);
      Serial.println("-----------------");

      if (ModeAuto) {
        Serial.print("Mode Auto");
        Serial.println("--------***-----");
      } else {
        Serial.print("Mode Manuel");
        Serial.println(" ");
      }
      Serial.println(now.secondstime());
  #endif

  // si la porte est pas ouverte ouverture pour 5sec
  if(digitalRead(FinCHaut)){
    ouvrirPorte();
    delay(5000);
  }
  
  
  //Controle des Leds
  displayColor(COLOR_BLUE, 0);
  delay(1000);
  displayColor(COLOR_GREEN, 0);
  delay(1000);
  displayColor(COLOR_CYAN, 0);
  delay(1000);
  displayColor(COLOR_RED, 0);
  delay(1000);
  displayColor(COLOR_BLACK, 0);

  /* // Code en cas de porte à moitier ouverte, retiré suite ouverture forcé dans setup
    if (bugPorte && digitalRead(FinCBas) && digitalRead(FinCHaut)) {
      fermerPorte();
    }
  */
    // Force Mode Auto au Boot
    previousMillis = millis()+interval;
}

void loop() {
  now = RTC.now(); //Récupère l'heure et le date courante


  if (now.hour() != ancHeur) {

    // Ajuste l'heure été
    bool CalculHeureEte = periodeEte(now.year() - 2000, now.month(), now.day(), now.hour());
    if (HeureEte != CalculHeureEte) {
      HeureEte = CalculHeureEte;
      if (CalculHeureEte) {
        DateTime set_time = DateTime(now.year(), now.month(), now.day(), now.hour() + 1, now.minute(), now.second());
        RTC.adjust(set_time);
      } else {
        DateTime set_time = DateTime(now.year(), now.month(), now.day(), now.hour() - 1, now.minute(), now.second());
        RTC.adjust(set_time);
      }
    }

    //------------------------------------------------------------------------------------------------------
    HeureCoucherInt=maison.sunset(now.year(), now.month(), now.day(), !HeureEte);
    soleilHeur = min2Heur(HeureCoucherInt);
    soleilMinut = min2Min(HeureCoucherInt);
    DTHeureCouche = DateTime(now.year(), now.month(), now.day(), soleilHeur, soleilMinut, 0);
    DateTime future2 (DTHeureCouche + TimeSpan(0, 0, 20, 0));
    DTHeureCouche = future2;

    
    HeureLeverInt=maison.sunrise(now.year(), now.month(), now.day(), !HeureEte);
    soleilHeur = min2Heur(HeureLeverInt);
    soleilMinut = min2Min(HeureLeverInt);
    
    if(soleilHeur < HeurMini){
      soleilHeur=HeurMini;
      soleilMinut=MinMini;
    }
    if(soleilHeur == HeurMini && soleilMinut <MinMini){
      soleilHeur=HeurMini;
      soleilMinut=MinMini;
    }
    
    DTHeureLeve = DateTime(now.year(), now.month(), now.day(), soleilHeur, soleilMinut, 0);
    ancHeur = now.hour();
  }




//---------------------------------------------------------------------------------------------------- 
#ifdef DEBUG
  Serial.println(" ");
  Serial.println("-/-/-/-/-/-/-/-/-/-");
  affiche_date_heure(now);  //Affiche la date en langue humaine
  Serial.println("-/-/-/-/-/-/-/-/-/-");

  Serial.print("Ephe-Heure Lever : ");
  affiche_date_heure(DTHeureLeve);
  Serial.println("-----------------");

  Serial.print("Ephe-Heure Coucher : ");
  affiche_date_heure(DTHeureCouche);
  Serial.println("-----------------");
  
  if (ModeAuto) {
    Serial.print("Mode Auto");
    Serial.println("--------***-----");
  } else {
    Serial.print("Mode Manuel");
    Serial.println(" ");
  }
  Serial.println(now.secondstime());
#endif
//----------------------------------------------------------------------------------------------------


  if (ModeAuto != ancModeAuto) {
    uint8_t valueAuto = (ModeAuto == true ? ON : OFF);
    myx10.x10Switch('c', 4, valueAuto);
    delay(2000);
    myx10.x10Switch('c', 4, valueAuto); // envoi 2 fois l'info en cas de surcharge d'info sur la bande RF433.
    
    ancModeAuto = ModeAuto;
  }


  if (ModeAuto) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }


  if (!(millis() - previousMillis2 >= interval2)) { // Si le temps passé est suppérieur à interval ------- Gestion des couleurs RGB
    if (!bugPorte) {
      if (ModeAuto) {
        if (etatPorte) {
          displayColor(COLOR_GREEN, 1);
        } else {
          displayColor(COLOR_RED, 1);
        }
      } else {
        if (etatPorte) {
          displayColor(COLOR_GREEN, 0);
        } else {
          displayColor(COLOR_RED, 0);
        }
      }
    } else {
      displayColor(COLOR_BLUE, 1);
    }
  } else {
    displayColor(COLOR_BLACK, 0);
  }

  

  // Si au moment de la fermeture la porte n'ai pas fermé, alors on alerte via RF433
  if (bugPorte != ancBugPorte) {
    displayColor(COLOR_BLUE, 0); //Force couleur led en bleu
    
    valueE = (true ? ON : OFF);
    myx10.x10Switch('c', 6, valueE);
    delay(10000); // att. 10sec avant envoi nouveau message 
    valueE = (false ? ON : OFF);
    myx10.x10Switch('c', 6, valueE);
    
    ancBugPorte=bugPorte;
  }


// Gestion des boutons poussoir Haut - Bas

  if (digitalRead(bt_bas) == 0)         //Condition : Detection appui bouton bas
    //Alors on ferme la porte
  {
    previousMillis = millis();
    fermerPorte();
  }

  if (digitalRead(bt_haut) == 0)         //Condition : Detection appui bouton haut
    //Alors on ouvre la porte
  {
    previousMillis = millis();
    ouvrirPorte();
  }

//----------------------------------

  
  if (millis() - previousMillis >= interval) { // Si le temps passé est suppérieur à interval
    //digitalWrite(LED_BUILTIN, LOW);
    ModeAuto = true;
  } else {
    //digitalWrite(LED_BUILTIN, HIGH);
    ModeAuto = false;
  }


// Code d'action d'ouverture et fermeture en fonction de l'heure définie en auto

  if (ModeAuto && !bugPorte) {   // Si mode Auto activé et qu'il n'y a pas de Bug de la porte

    //ouvrir porte
    if ((now.secondstime() >= DTHeureLeve.secondstime()) && (now.secondstime() < DTHeureCouche.secondstime()) && !etatPorte)
    {
      #ifdef DEBUG
      Serial.print("Ouverture Auto");
      #endif
      ouvrirPorte();
    }

    //fermer porte
    if ((now.secondstime() >= DTHeureCouche.secondstime()) && etatPorte)
    {
      #ifdef DEBUG
          Serial.print("Fermeture Auto");
      #endif
      if(nbrEssai <3) fermerPorte();
    }
  }
//----------------------------------

}

int min2Heur(int mints) {
  //bool isError = false;
  int heur=0;
  int minut=0;
  if (mints < 0 || mints >= 1440) {
    return 0;
  }

   heur = mints/60;
   return heur;
   //return !isError;
}
int min2Min(int mints) {
  //bool isError = false;
  int heur=0;
  int minut=0;
  if (mints < 0 || mints >= 1440) {
    return 0;
  }

   heur = mints/60;
   minut = mints-((mints/60)*60);
   return minut;
   //return !isError;
}

void affiche_date_heure(DateTime rtcPrint) {

  Serial.print(rtcPrint.year(), DEC);
  Serial.print('/');
  Serial.print(rtcPrint.month(), DEC);
  Serial.print('/');
  Serial.print(rtcPrint.day(), DEC);
  Serial.print(" ");
  Serial.print(rtcPrint.hour(), DEC);
  Serial.print(':');
  Serial.print(rtcPrint.minute(), DEC);
  Serial.print(':');
  Serial.print(rtcPrint.second(), DEC);
  Serial.println();

}

void displayColor(byte color, bool cligno) {
  if (cligno) {
    if ((now.second() % 2) == 0) { // si sec pair (sec % 2) == 0
      // Assigne l'état des broches
      // Version cathode commune
      digitalWrite(LEDrouge, bitRead(color, 2));
      digitalWrite(LEDverte, bitRead(color, 1));
      digitalWrite(LEDbleu, bitRead(color, 0));
    } else {
      digitalWrite(LEDrouge, 0);
      digitalWrite(LEDverte, 0);
      digitalWrite(LEDbleu, 0);
    }
  } else {
    // Assigne l'état des broches
    // Version cathode commune
    digitalWrite(LEDrouge, bitRead(color, 2));
    digitalWrite(LEDverte, bitRead(color, 1));
    digitalWrite(LEDbleu, bitRead(color, 0));
  }

}

bool periodeEte(uint8_t anneeUTC, uint8_t moisUTC, uint8_t jourUTC, uint8_t heureUTC)
{
  //En France métropolitaine :
  //Passage de l'heure d'hiver à l'heure d'été le dernier dimanche de mars à 1h00 UTC (à 2h00 locales il est 3h00)
  //Passage de l'heure d'été à l'heure d'hiver le dernier dimanche d'octobre à 1h00 UTC (à 3h00 locales il est 2h00)
  const uint8_t MARS = 3;
  const uint8_t OCTOBRE = 10;
  if (moisUTC == MARS)
  {
    uint8_t dernierDimancheMars = 31 - ((5 + anneeUTC + (anneeUTC >> 2)) % 7); //Pas évidente à trouver celle-là
    return jourUTC > dernierDimancheMars || (jourUTC == dernierDimancheMars && heureUTC != 0);
  }
  if (moisUTC == OCTOBRE)
  {
    uint8_t dernierDimancheOctobre = 31 - ((2 + anneeUTC + (anneeUTC >> 2)) % 7);
    return jourUTC < dernierDimancheOctobre || (jourUTC == dernierDimancheOctobre && heureUTC == 0);
  }
  return MARS < moisUTC && moisUTC < OCTOBRE;
}

void fermerPorte() { // fermeture de la porte et envoi du signal RF si l'état change
  previousMillis2 = millis();

  // On descend un peu la porte le temp que le capteur haut soit désactivé.
  
  while (digitalRead(FinCHaut) == 1) { //Tant que le capteur haut n'est pas laché, on descend la porte
    digitalWrite(enable, HIGH);
    if(sensMoteur){
      digitalWrite(moteur_haut, LOW);
      digitalWrite(moteur_bas, HIGH);
    }else{
      digitalWrite(moteur_haut, HIGH);
      digitalWrite(moteur_bas, LOW);
    }
    displayColor(COLOR_CYAN, 0);
  }
  
  while (digitalRead(FinCBas) == 1 || digitalRead(FinCHaut) == 1) { //Tant que la porte n'est pas fermé, le moteur tourne et si jamais sa boucle et réouvre la porte sa coupe.
    //Fermeture
    digitalWrite(enable, HIGH);
    if(sensMoteur){
      digitalWrite(moteur_haut, LOW);
      digitalWrite(moteur_bas, HIGH);
    }else{
      digitalWrite(moteur_haut, HIGH);
      digitalWrite(moteur_bas, LOW);
    }
    displayColor(COLOR_CYAN, 0);
  }
  digitalWrite(enable, LOW);
  if(sensMoteur){
    digitalWrite(moteur_bas, LOW);    //On arrete le moteur car le contact fin de course est activé
  }else{
    digitalWrite(moteur_haut, LOW);    //On arrete le moteur car le contact fin de course est activé
  }
  
  if(digitalRead(FinCHaut)){  // Si la fermeture n'a pas fonctionné, et que la porte est ouverte alors la corde a bouclé donc on inverse le sens
    sensMoteur=!sensMoteur;
    bugPorte = true;
    etatPorte = true;
    nbrEssai++;
    delay(20000); // on attend 20sec au cas ou se soit une poule qui bloque, et quel passe
  }else{
    bugPorte = false;
    ancBugPorte = false;
    etatPorte = false;
    nbrEssai=0;
  }
  bugPorte = false;
  etatPorte = false;

  //envoi RF433
  uint8_t valueA = (etatPorte == true ? ON : OFF);
  if (valueA != oldValueA) {
    // Send in the new value
    myx10.x10Switch('c', 3, valueA);
    oldValueA = valueA;
  }
}

void ouvrirPorte() { // Ouverture de la porte et envoi du signal RF si l'état change
  previousMillis2 = millis();
  while (digitalRead(FinCHaut) == 1) { //Tant que la porte n'est pas ouverte, le moteur tourne
    //Ouverture
    digitalWrite(enable, HIGH); 
    if(sensMoteur){
      digitalWrite(moteur_haut, HIGH);
      digitalWrite(moteur_bas, LOW);
    }else{
      digitalWrite(moteur_haut, LOW);
      digitalWrite(moteur_bas, HIGH);
    }
    displayColor(COLOR_CYAN, 0);
  }
  digitalWrite(enable, LOW);
  if(sensMoteur){
    digitalWrite(moteur_haut, LOW);
  }else{
    digitalWrite(moteur_bas, LOW);
  }
  bugPorte = false;
  ancBugPorte = false;
  etatPorte = true;
  uint8_t valueA = (etatPorte == true ? ON : OFF);

  if (valueA != oldValueA) {
    // Send in the new value
    myx10.x10Switch('c', 3, valueA);
    oldValueA = valueA;
  }
}
