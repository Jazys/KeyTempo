#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLECharacteristic.h>
#include <Preferences.h>
#include <WiFi.h>
#include <string.h>
#include <AESLib.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library
#include <ArduinoJson.h>
#include "Arduino.h"
#include "OneButton.h"

#define API_URL "http://worldtimeapi.org/api/timezone/Europe/Paris"

TFT_eSPI tft = TFT_eSPI(); 
OneButton button(0, true);


BLEServer *pServer;
BLECharacteristic *pSSIDCharacteristic;
BLECharacteristic *pPasswordCharacteristic;
BLECharacteristic *pResetWifiCharacteristic;
BLECharacteristic *pConnectWifiCharacteristic;
BLECharacteristic *pAuthCharacteristic; // Caractéristique pour l'authentification

Preferences preferences;

char ssid[50] = "";
char password[50] = "";
bool wifiConfigured = false;
unsigned long lastBluetoothDataTime = 0;
const unsigned long bluetoothTimeout = 20000; // Temps d'attente en millisecondes (20 secondes)

const uint16_t serviceUUID = 0x180F; // Exemple de service UUID

// Clé de chiffrement (256 bits = 32 octets)
byte aes_key[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; 
byte iv[N_BLOCK] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

String JourJ;
String JourJ1;
String dateJour;
JsonVariant variant;

AESLib aesLib;

unsigned long previousMillis = 0; 
const long interval = 600000; 

String decrypt(char * msg, byte iv[]) {
  int msgLen = strlen(msg);
  char decrypted[msgLen] = {0}; // half may be enough
  aesLib.decrypt64(msg, msgLen, (byte*)decrypted, aes_key, sizeof(aes_key), iv);
  return String(decrypted);
}



void resetWiFi() {
  preferences.remove("ssid");
  preferences.remove("password");
  WiFi.disconnect(true);
  wifiConfigured = false;
}

void connectToWiFi(const char *ssid, const char *password) {
  Serial.println("Connecting to WiFi...");
  Serial.println(ssid);
  Serial.println(password);
  WiFi.begin(ssid, password);
  tft.println("Try to connect on  ");
  tft.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }

  tft.println("..Wifi Connected..");
  Serial.println("Connected to WiFi");
  httpGet();
  // Autres opérations après la connexion au WiFi
}

class BLECallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    String receivedData = String(value.c_str());


    // Exécuter les autres opérations seulement après l'authentification réussie
    if (pCharacteristic == pSSIDCharacteristic) {
      strncpy(ssid, receivedData.c_str(), sizeof(ssid) - 1);
      ssid[sizeof(ssid) - 1] = '\0';
      preferences.putString("ssid", ssid);
      wifiConfigured = true;
      lastBluetoothDataTime = millis();
      //tft.println("SSID is ");
      //tft.println(ssid);
      Serial.println("SSID is setting");
    } else if (pCharacteristic == pPasswordCharacteristic) {
      String decrypted= "";
         Serial.println("SSID is setting1");
         Serial.println(receivedData.c_str());
         /*decrypted= decrypt((char*)receivedData.c_str(), iv);
          preferences.putString("password", decrypted);

           tft.println(decrypted);*/
           strncpy(password, receivedData.c_str(), sizeof(password) - 1);
           password[sizeof(password) - 1] = '\0';
           preferences.putString("password", password);
           //tft.println(password);
          wifiConfigured = true;
          lastBluetoothDataTime = millis();
          Serial.println("Password is setting");

    } else if (pCharacteristic == pResetWifiCharacteristic) {
      resetWiFi();
      //SerialBT.println("WiFi settings reset.");
      wifiConfigured = false;
      Serial.println("Reset Wifi");
    } else if (pCharacteristic == pConnectWifiCharacteristic) {
      Serial.println("Ask to connect to Wifi");;
      connectToWiFi(ssid, password);
      wifiConfigured = true;
      lastBluetoothDataTime = millis();
    }
  }
};

void setup() {

  tft.init();

  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_GREEN);

  // Set "cursor" at top left corner of display (0,0) and select font 4
  tft.setCursor(0, 4, 2);

  // Set the font colour to be white with a black background
  tft.setTextColor(TFT_WHITE);
  Serial.begin(115200);

  Serial.println("Begin...");
  tft.println("Boot... ");

  button.attachClick([] {
    tft.println("Boot... ");
  });

  BLEDevice::init("ESP32_BLE");
  pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(serviceUUID);

  pSSIDCharacteristic = pService->createCharacteristic(
      BLEUUID::fromString("11111111-2222-3333-4444-555555555555"),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pPasswordCharacteristic = pService->createCharacteristic(
      BLEUUID::fromString("66666666-7777-8888-9999-000000000000"),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pResetWifiCharacteristic = pService->createCharacteristic(
      BLEUUID::fromString("77777777-8888-9999-0000-111111111111"),
      BLECharacteristic::PROPERTY_WRITE);

  pConnectWifiCharacteristic = pService->createCharacteristic(
      BLEUUID::fromString("88888888-9999-0000-1111-222222222222"),
      BLECharacteristic::PROPERTY_WRITE);

  pAuthCharacteristic = pService->createCharacteristic(
      BLEUUID::fromString("99999999-0000-1111-2222-333333333333"),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pSSIDCharacteristic->setCallbacks(new BLECallbacks());
  pPasswordCharacteristic->setCallbacks(new BLECallbacks());
  pResetWifiCharacteristic->setCallbacks(new BLECallbacks());
  pConnectWifiCharacteristic->setCallbacks(new BLECallbacks());
  pAuthCharacteristic->setCallbacks(new BLECallbacks());

  pService->start();
  pServer->getAdvertising()->start();

  preferences.begin("wifi", false);

  strcpy(ssid, preferences.getString("ssid", "").c_str());
  strcpy(password, preferences.getString("password", "").c_str());

  if (strlen(ssid) > 0 && strlen(password) > 0) {
    wifiConfigured = true;
    connectToWiFi(ssid, password);
  }
}

void httpGet() {
  HTTPClient http;
   http.begin(API_URL);
  int httpCode = http.GET();

  // Vérifier le code de retour
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // Si la requête a réussi
    if (httpCode == HTTP_CODE_OK) {
      // Récupérer la réponse
      String payload = http.getString();
      Serial.println(payload);

      // Créer un objet JSON pour analyser la réponse
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      // Vérifier si l'analyse a réussi
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
      } else {
        // Récupérer l'heure depuis l'objet JSON
        dateJour = doc["datetime"].as<String>();
      }
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  // Fermer la connexion
  http.end();

}

String getDate() {
  // Vérifier si l'analyse a réussi
  return dateJour.substring(0,10);
}

// Définir une fonction qui retourne l'heure au format "HH:MM:SS"
String getHeure() {
   return dateJour.substring(11,19);
}

void tempoInfo(){
  HTTPClient http;
  http.begin("https://particulier.edf.fr/services/rest/referentiel/searchTempoStore?dateRelevant="+getDate());
  int httpCode = http.GET();
  // Vérifier le code de retour
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // Si la requête a réussi
    if (httpCode == HTTP_CODE_OK) {
      // Récupérer la réponse
      String payload = http.getString();
      Serial.println(payload);

   

      // Créer un objet JSON pour analyser la réponse
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      // Vérifier si l'analyse a réussi
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
      } else {
        // Récupérer les couleurs des jours depuis l'objet JSON
        JourJ = doc["couleurJourJ"].as<String>(); // Peut être "BLEU", "BLANC", "ROUGE" ou "NON_DEFINI"
        JourJ1 = doc["couleurJourJ1"].as<String>(); // Peut être "BLEU", "BLANC", "ROUGE" ou "NON_DEFINI"
        Serial.print("La couleur du jour est : ");
        Serial.println(JourJ);
        Serial.print("La couleur du lendemain est : ");
        Serial.println(JourJ1);
      }
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  // Fermer la connexion
  http.end();

}

String enleverTempo(String chaine) {
  // Si la chaîne commence par "TEMPO_"
  if (chaine.startsWith("TEMPO_")) {
    // Retourner la sous-chaîne qui suit "TEMPO_"
    return chaine.substring(6);
  } else {
    // Sinon, retourner la chaîne inchangée
    return chaine;
  }
}

void loop() {
  /*String encrypted = "aaVnovPpWZc4qV9QKAoG";
  String decrypted = decrypt((char*)encrypted.c_str(), iv);
  Serial.println("Décrypté: " + decrypted);
  delay(3000);*/
  unsigned long currentMillis = millis();
  button.tick();

  if( ((previousMillis == 0) || (currentMillis - previousMillis >= interval)) && WiFi.status() == WL_CONNECTED){
    previousMillis = currentMillis;
    httpGet();
    tempoInfo();

    JourJ=enleverTempo(JourJ);
    JourJ1=enleverTempo(JourJ1);

    tft.setRotation(1);   
    
    if(JourJ.equals("BLEU")){
      tft.fillScreen(TFT_BLUE);    
      tft.setTextColor(TFT_BLACK);
    }else if(JourJ.equals("BLANC")){
      tft.fillScreen(TFT_WHITE);
      tft.setTextColor(TFT_BLACK);
    }else if(JourJ.equals("ROUGE")){
      tft.fillScreen(TFT_RED);
      tft.setTextColor(TFT_BLACK);
    }else{
      
    }

    if(JourJ1.equals("ROUGE")){
      tft.drawRect(0, tft.height()/2+17, tft.width(), tft.height(), TFT_RED);
      tft.fillRoundRect(0, tft.height()/2+17, tft.width(), tft.height(),3, TFT_RED);
    }else if(JourJ1.equals("BLEU")){
      tft.drawRect(00, tft.height()/2+17, tft.width(), tft.height(), TFT_BLUE);
      tft.fillRoundRect(0, tft.height()/2+17, tft.width(), tft.height(),3, TFT_BLUE);
    }else if(JourJ1.equals("BLANC")){
      tft.drawRect(0, tft.height()/2+170, tft.width(), tft.height(), TFT_WHITE);
      tft.fillRoundRect(0, tft.height()/2+17, tft.width(), tft.height(),3, TFT_WHITE);
    }else{
      tft.drawRect(0, tft.height()/2+17, tft.width(), tft.height(), TFT_DARKGREY);
      tft.fillRoundRect(0, tft.height()/2+17, tft.width(), tft.height(),3, TFT_DARKGREY);
    }
    tft.setCursor(tft.width()/2, 0);       // Définit la position du curseur
    tft.setTextColor(TFT_BLACK);  // Définit la couleur du texte
    tft.setTextSize(2);        // Définit la taille du texte
    tft.drawString("Aujourd'hui", 10, 10);
    tft.drawString("Demain", 39, tft.height()/2+15);

    if (JourJ1.equals(JourJ))
      tft.drawLine(0, tft.height()/2+17, tft.width(), tft.height()/2+17, TFT_DARKGREY);
    
    /*tft.setCursor(0, 4, 2);
    tft.println("jour : " + getDate() );
    tft.println("Heure : " + getHeure());
    tft.println("J : " +JourJ );
    tft.println("J+1 : "+JourJ1);*/

    

    //Rajouter le nombre de jour restants : https://particulier.edf.fr/services/rest/referentiel/getNbTempoDays?TypeAlerte=TEMPO
    
  }

  delay(5);
}
