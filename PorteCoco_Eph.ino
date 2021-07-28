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


#define TX_PIN 4                  // pin 3 // Emeteur *RF
#define FinCHaut A2
#define FinCBas A1
#define moteur_haut 8
#define moteur_bas 7
#define enable 9
#define bt_haut 5
#define bt_bas 6
#define bt_stop 2
// LED Mode manuel sur la PinLed Arduino (13)
#define LEDbleu 12 // Bleu
#define LEDverte 11 // vert
#define LEDrouge 10 // rouge 
#define TempModeManuel 60000 // 1 800 000 = 30mins // 60 000 = 1mins
#define TempLedStatut 900000 // 900 000 = 15mins
#define HeurMini 7 // Heur ouverture minimum
#define MinMini 30 // Minute lie à l'heure ouverture minimum



bool bugMoteur;
bool etatPorte;
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
  bugMoteur = false;

  etatPorteAncien = true;
  etatPorte = true;

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
    if (bugMoteur && digitalRead(FinCBas) && digitalRead(FinCHaut)) {
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

  if (ModeAuto != ancModeAuto) {
    uint8_t valueAuto = (ModeAuto == true ? ON : OFF);
    myx10.x10Switch('c', 4, valueAuto);
    ancModeAuto = ModeAuto;
  }


  if (ModeAuto) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }


  if (!(millis() - previousMillis2 >= interval2)) { // Si le temps passé est suppérieur à interval ------- Gestion des couleurs RGB
    if (!bugMoteur) {
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

  

  // Si on est pas en manuel et que la porte est en état OUVERT mais que le Fin de course Haut n'est pas détecté, alors envoi signal et ouverture porte
  if (ModeAuto && etatPorte && digitalRead(FinCHaut) == 1) {
    valueE = (etatPorte == true ? ON : OFF);
    myx10.x10Switch('c', 6, valueE);
    ouvrirPorte();
    delay(1000);
    valueE = (etatPorte == true ? ON : OFF);
    myx10.x10Switch('c', 6, valueE);
  }


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

  if (millis() - previousMillis >= interval) { // Si le temps passé est suppérieur à interval
    //digitalWrite(LED_BUILTIN, LOW);
    ModeAuto = true;
  } else {
    //digitalWrite(LED_BUILTIN, HIGH);
    ModeAuto = false;
  }

  if (ModeAuto) {
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
      fermerPorte();
    }
  }


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
  while (digitalRead(FinCBas) == 1) { //Tant que la porte n'est pas fermé, le moteur tourne
    //Fermeture
    digitalWrite(enable, HIGH);
    digitalWrite(moteur_haut, LOW);
    digitalWrite(moteur_bas, HIGH);
    displayColor(COLOR_CYAN, 0);
  }
  digitalWrite(enable, LOW);
  digitalWrite(moteur_bas, LOW);    //On arrete le moteur car le contact fin de course est activé
  bugMoteur = false;
  etatPorte = false;
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
    digitalWrite(moteur_haut, HIGH);
    digitalWrite(moteur_bas, LOW);
    displayColor(COLOR_CYAN, 0);
  }
  digitalWrite(enable, LOW);
  digitalWrite(moteur_haut, LOW);
  bugMoteur = false;
  etatPorte = true;
  uint8_t valueA = (etatPorte == true ? ON : OFF);

  if (valueA != oldValueA) {
    // Send in the new value
    myx10.x10Switch('c', 3, valueA);
    oldValueA = valueA;
  }
}
