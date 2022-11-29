/*
 TALLY SLAVE

 Firmware per gestionar un sistema de Tally's inhalambrics.

 */

/* 
TODO

Fer menu selecció funció local
Implentar Display 
Implentar hora
Implentar mostrar texete
Lectura valors reals bateria

*/

/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/?s=esp-now
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  Based on JC Servaye example: https://github.com/Servayejc/esp_now_sender/
*/
#include <Arduino.h>
#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h> //Control neopixels

#define VERSIO S1 // Versió del software

// Bool per veure missatges de debug
bool debug = true;

// Set your Board and Server ID
#define BOARD_ID 1 // Cal definir cada placa amb el seu numero
// TODO Poder definir en el menu el numero
#define MAX_CHANNEL 13 // for North America // 13 in Europe

// Configurem LED BUILTIN
#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // efinim el LED local de la placa
#endif

// Define PINS
// Botons i leds locals
#define BOTO_ROIG_PIN 16
#define BOTO_VERD_PIN 5
#define LED_ROIG_PIN 17
#define LED_VERD_PIN 18
#define MATRIX_PIN 4

// Define Quantitat de leds
#define LED_COUNT 72 // 8x8 + 8

// Define sensor battery
#define BATTERY_PIN 36

// Declarem neopixels
Adafruit_NeoPixel llum(LED_COUNT, MATRIX_PIN, NEO_GRB + NEO_KHZ800);

// Definim els colors GRB
const uint8_t COLOR[][6] = {{0, 0, 0},        // 0- NEGRE
                            {255, 0, 0},      // 1- ROIG
                            {0, 0, 255},      // 2- BLAU
                            {255, 0, 0},      // 3- VERD
                            {255, 128, 0},    // 4- GROC
                            {128, 128, 0},    // 5- TARONJA
                            {255, 255, 255}}; // 6- BLANC

uint8_t color_matrix = 0; // Per determinar color local

// Variables
// Fem arrays de dos valors la 0 és anterior la 1 actual
bool BOTO_LOCAL_ROIG[] = {false, false};
bool BOTO_LOCAL_VERD[] = {false, false};

// Valor dels leds (dels polsadors)
bool LED_LOCAL_ROIG = false;
bool LED_LOCAL_VERD = false;

// Variables de gestió
bool LOCAL_CHANGE = false; // Per saber si alguna cosa local ha canviat

unsigned long temps_set_config = 0;      // Temps que ha d'estar apretat per configuracio
const unsigned long temps_config = 5000; // Temps per disparar opció config
bool pre_mode_configuracio = false;      // Inici mode configuració
bool mode_configuracio = false;          // Mode configuració

uint8_t serverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Structure to send data
// Must match the receiver structure
//  Structure example to receive data
//  Must match the sender structure
// TODO ELIMINAR
typedef struct struct_message
{
  uint8_t msgType;
  uint8_t id;
  float temp;
  float hum;
  unsigned int readingId;
} struct_message;
// ELIMINAR FINS AQUI

// Estructura pairing
typedef struct struct_pairing
{ // new structure for pairing
  uint8_t msgType;
  uint8_t id;
  uint8_t macAddr[6];
  uint8_t channel;
} struct_pairing;

// Estrucrtura dades rebuda de master
typedef struct struct_message_from_master
{
  uint8_t msgType;
  uint8_t funcio;      // Identificador de la funcio del tally
  bool led_roig;       // llum confirmació cond polsador roig
  bool led_verd;       // llum confirmació cond polsador verd
  uint8_t color_tally; // Color indexat del tally
  // text per mostrar a pantalla
} struct_message_from_master;

// Estrucrtura dades per enviar a master
typedef struct struct_message_to_master
{
  uint8_t msgType;
  uint8_t id;     // Identificador del tally
  uint8_t funcio; // Identificador de la funcio del tally
  bool boto_roig;
  bool boto_verd;
} struct_message_to_master;

// Estructura dades per rebre bateries
typedef struct struct_bateria_info
{
  uint8_t msgType;
  uint8_t id;             // Identificador del tally
  float volts;            // Lectura en volts
  float percent;          // Percentatge carrega
  // Revisar !!!! unsigned int readingId; // Identificador de lectura
} struct__bateria_info;

// Estructura dades per rebre clock
// TODO

// Create 2 struct_message
struct_message myData; // data to send
struct_message inData; // data received
struct_pairing pairingData;
struct_message_from_master fromMaster; // dades del master cap al tally
struct_message_to_master toMaster;  // dades del tally cap al master
struct_bateria_info bateria_info;   // dades de la bateria cap al master

enum PairingStatus
{
  NOT_PAIRED,
  PAIR_REQUEST,
  PAIR_REQUESTED,
  PAIR_PAIRED,
};
PairingStatus pairingStatus = NOT_PAIRED;

enum MessageType
{
  PAIRING,
  DATA,
  TALLY,
  BATERIA,
  CLOCK
};
MessageType messageType;

// Definim les funcions del Tally
enum TipusFuncio
{
  LLUM,           // Els Tally tan sols s'iluminen amb el color que indica el Master
  CONDUCTOR,      // Els polsadors tenen la funció del CONDUCTOR
  PRODUCTOR       // Els polsadors tenen a funció del PRODUCTOR
};
TipusFuncio funcio_local = LLUM; // Assignem LLUM per decfecte

#ifdef SAVE_CHANNEL
int lastChannel;
#endif
int channel = 1;

// simulate batery level
float volt = 0;
float percent = 0;

unsigned long currentMillis = millis();
unsigned long previousMillis = 0; // Stores last time batery was published
const long interval = 10000;      // Interval at which to publish bateria sensor readings
unsigned long start;              // used to measure Pairing time
unsigned int readingId = 0;

// Simulem lectura de bateria
// TODO Unificar lectura volts i convertir a nivells
float readBateriaVolts()
{
  volt = random(0, 40); // = analogRead(BATTERY_PIN)
  return volt;
}

// Simulem lectura percentatge bateria
float readBateriaPercent()
{
  percent = random(0, 100);
  return percent;
}

// Posar llum a un color
void escriure_matrix(uint8_t color)
{
  // GBR
  uint8_t G = COLOR[color][1];
  uint8_t B = COLOR[color][2];
  uint8_t R = COLOR[color][0];
  for (int i = 0; i < LED_COUNT; i++)
  {
    llum.setPixelColor(i, llum.Color(G, B, R));
  }
  llum.show();
  if (debug)
  {
    Serial.print("Color: ");
    Serial.println(color);
    Serial.print("R: ");
    Serial.println(R);
    Serial.print("G: ");
    Serial.println(G);
    Serial.print("B: ");
    Serial.println(B);
  }
}

void comunicar_polsadors()
{
  toMaster.msgType = TALLY;
  toMaster.id = BOARD_ID;
  toMaster.funcio = funcio_local;
  toMaster.boto_roig = BOTO_LOCAL_ROIG[1];
  toMaster.boto_verd = BOTO_LOCAL_VERD[1];

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(serverAddress, (uint8_t *)&toMaster, sizeof(toMaster));
  if (result == ESP_OK)
  {
    Serial.println("Sent polsadors with success");
  }
  else
  {
    Serial.println("Error sending polsadors data");
  }
}

void comunicar_bateria()
{
  // Set values to send
  bateria_info.msgType = BATERIA;
  bateria_info.id = BOARD_ID;
  bateria_info.volts = readBateriaVolts();
  bateria_info.percent = readBateriaPercent();
  bateria_info.readingId = readingId++;
  esp_err_t result = esp_now_send(serverAddress, (uint8_t *)&bateria_info, sizeof(bateria_info));
}

void llegir_botons()
{
  BOTO_LOCAL_ROIG[1] = !digitalRead(BOTO_ROIG_PIN); // Els botons son PULLUP per tant els llegirem al revés
  BOTO_LOCAL_VERD[1] = !digitalRead(BOTO_VERD_PIN);
  // Detecció canvi de botons locals
  if (BOTO_LOCAL_ROIG[0] != BOTO_LOCAL_ROIG[1])
  {
    /// HEM POLSAT EL BOTO ROIG
    LOCAL_CHANGE = true;
    BOTO_LOCAL_ROIG[0] = BOTO_LOCAL_ROIG[1];
    if (debug)
    {
      Serial.print("Boto local ROIG: ");
      Serial.println(BOTO_LOCAL_ROIG[0]);
    }
  }

  if (BOTO_LOCAL_VERD[0] != BOTO_LOCAL_VERD[1])
  {
    /// HEM POLSAT EL BOTO VERD
    LOCAL_CHANGE = true;
    BOTO_LOCAL_VERD[0] = BOTO_LOCAL_VERD[1];
    if (debug)
    {
      Serial.print("Boto local VERD: ");
      Serial.println(BOTO_LOCAL_VERD[0]);
    }
  }
}

void escriure_leds()
{
  digitalWrite(LED_ROIG_PIN, LED_LOCAL_ROIG);
  digitalWrite(LED_VERD_PIN, LED_LOCAL_VERD);
}

void addPeer(const uint8_t *mac_addr, uint8_t chan)
{ // Afegim
  esp_now_peer_info_t peer;
  ESP_ERROR_CHECK(esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE));
  esp_now_del_peer(mac_addr);
  memset(&peer, 0, sizeof(esp_now_peer_info_t));
  peer.channel = chan;
  peer.encrypt = false;
  memcpy(peer.peer_addr, mac_addr, sizeof(uint8_t[6]));
  if (esp_now_add_peer(&peer) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }
  memcpy(serverAddress, mac_addr, sizeof(uint8_t[6]));
}

void printMAC(const uint8_t *mac_addr)
{
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


void Menu_configuracio()
{
  // Desenvolupar aqui el mode configuració
  // TODO:  Seleccionar entre LLUM, CONDUCTOR i PRODUCTOR
  // Veure com generem menu
  /*
  Si opcio 1
    funcio_local = LLUM;
  Si opció 2
    funcio_local = CONDUCTOR;
  Si opcio 3
    funcio_local = PRODUCTOR;
  */
  // Comunicar nova funció
}

void detectar_mode_configuracio()
{
  if (LOCAL_CHANGE)
  {
    if (BOTO_LOCAL_ROIG[0] && BOTO_LOCAL_VERD[0] && !pre_mode_configuracio)
    {
      // Tenim els dos polsadors apretats i no estem en pre_mode_configuracio
      // Entrarem al mode CONFIG
      temps_set_config = millis();      // Llegim el temps actual per entrar a mode config
      pre_mode_configuracio = true;     //Situem el flag en pre-mode-confi
      if (debug)
      {
        Serial.print("PRE CONFIGURACIO MODE");
      }
    }

    if ((!BOTO_LOCAL_ROIG[0] || !BOTO_LOCAL_VERD[0]) && pre_mode_configuracio)
    { // Si deixem de pulsar botons i estavem en pre_mode_de_configuracio
      if ((millis()) >= (temps_config + temps_set_config))
      {                                // Si ha pasat el temps d'activació
        mode_configuracio = true;      // Entrem en mode configuracio
        pre_mode_configuracio = false; // Sortim del mode preconfiguracio
        if (debug)
        {
          Serial.print("CONFIGURACIO MODE");
        }
        // TODO: Cridar mode config
        Menu_configuracio();
      }
      else
      {
        pre_mode_configuracio = false; // Cancelem la preconfiguracio
        mode_configuracio = false;     // Cancelem la configuracio
        if (debug)
        {
          Serial.print("CANCELEM CONFIGURACIO MODE");
        }
      }
    }
  }
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  Serial.print("Packet received from: ");
  printMAC(mac_addr);
  Serial.println();
  Serial.print("data size = ");
  Serial.println(sizeof(incomingData));
  uint8_t type = incomingData[0];
  if (!mode_configuracio) //Si no estem en mode configuració
  {
    switch (type)
    {
    case DATA: // we received data from server
      memcpy(&inData, incomingData, sizeof(inData));
      if (debug)
      {
        Serial.print("ID  = ");
        Serial.println(inData.id);
        Serial.print("Setpoint temp = ");
        Serial.println(inData.temp);
        Serial.print("SetPoint humidity = ");
        Serial.println(inData.hum);
        Serial.print("reading Id  = ");
        Serial.println(inData.readingId);
      }
      if (inData.readingId % 2 == 1)
      {
        digitalWrite(LED_BUILTIN, LOW);
      }
      else
      {
        digitalWrite(LED_BUILTIN, HIGH);
      }
      break;

    case TALLY: // Missatge del tipus TALLY
      memcpy(&fromMaster, incomingData, sizeof(fromMaster));
      if (debug)
      {
        Serial.print("Funció  = ");
        Serial.println(fromMaster.funcio);
        Serial.print("Led roig = ");
        Serial.println(fromMaster.led_roig);
        Serial.print("Led verd= ");
        Serial.println(fromMaster.led_verd);
        Serial.print("Color tally  = ");
        Serial.println(fromMaster.color_tally);
        // TODO Falta el texte
      }
      if (fromMaster.funcio = funcio_local)
      {
        LED_LOCAL_ROIG = fromMaster.led_roig; // Carreguem el valor rebut al LED roig
        LED_LOCAL_VERD = fromMaster.led_verd; // Carreguem el valor rebut al LED verd
        escriure_leds();   // CRIDAR SUBRUTINA ESCRIURE LED
        escriure_matrix(fromMaster.color_tally); // CRIDAR SUBRUTINA ESCRIURE TALLY
        // escriure_text();    // CRIDAR SUBRUTINA ESCRIURE TEXT
      }
      break;

    case CLOCK: // Missatge sincronització hora
                // El que calgui fer
      break;

    case PAIRING: // we received pairing data from server
      memcpy(&pairingData, incomingData, sizeof(pairingData));
      if (pairingData.id == 0)
      { // the message comes from server
        printMAC(mac_addr);
        Serial.print("Pairing done for ");
        printMAC(pairingData.macAddr);
        Serial.print(" on channel ");
        Serial.print(pairingData.channel); // channel used by the server
        Serial.print(" in ");
        Serial.print(millis() - start);
        Serial.println("ms");
        addPeer(pairingData.macAddr, pairingData.channel); // add the server  to the peer list
#ifdef SAVE_CHANNEL
        lastChannel = pairingData.channel;
        EEPROM.write(0, pairingData.channel);
        EEPROM.commit();
#endif
        pairingStatus = PAIR_PAIRED; // set the pairing status
      }
      break;
    }
  }
}

PairingStatus autoPairing()
{
  switch (pairingStatus)
  {
  case PAIR_REQUEST:
    Serial.print("Pairing request on channel ");
    Serial.println(channel);

    // set WiFi channel
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
    if (esp_now_init() != ESP_OK)
    {
      Serial.println("Error initializing ESP-NOW");
    }

    // set callback routines
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    // set pairing data to send to the server
    pairingData.msgType = PAIRING;
    pairingData.id = BOARD_ID;
    pairingData.channel = channel;

    // add peer and send request
    addPeer(serverAddress, channel);
    esp_now_send(serverAddress, (uint8_t *)&pairingData, sizeof(pairingData));
    previousMillis = millis();
    pairingStatus = PAIR_REQUESTED;
    break;

  case PAIR_REQUESTED:
    // time out to allow receiving response from server
    currentMillis = millis();
    if (currentMillis - previousMillis > 250)
    {
      previousMillis = currentMillis;
      // time out expired,  try next channel
      channel++;
      if (channel > MAX_CHANNEL)
      {
        channel = 1;
      }
      pairingStatus = PAIR_REQUEST;
    }
    break;

  case PAIR_PAIRED:
    // nothing to do here
    break;
  }
  return pairingStatus;
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  pinMode(LED_BUILTIN, OUTPUT);

  // Configurem els pins BOTONS/LEDS
  pinMode(BOTO_ROIG_PIN, INPUT_PULLUP);
  pinMode(BOTO_VERD_PIN, INPUT_PULLUP);
  pinMode(LED_ROIG_PIN, OUTPUT);
  pinMode(LED_VERD_PIN, OUTPUT);
  Serial.println("ESP32 TALLY SLAVE");
  Serial.print("Versió: ");
  Serial.println(VERSIO);
  Serial.print("Client Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  start = millis();

  // Esborrem llum
  llum.clear();

#ifdef SAVE_CHANNEL
  EEPROM.begin(10);
  lastChannel = EEPROM.read(0);
  Serial.println(lastChannel);
  if (lastChannel >= 1 && lastChannel <= MAX_CHANNEL)
  {
    channel = lastChannel;
  }
  Serial.println(channel);
#endif
  pairingStatus = PAIR_REQUEST;
}

void loop()
{
  if (autoPairing() == PAIR_PAIRED)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      // Save the last time a new reading was published
      previousMillis = currentMillis;
      readBateriaVolts();           // Llegim bateria volts
      readBateriaPercent();         // Llegim percentatge bateria
      comunicar_bateria();          // Comuniqem valor bateria
    }
    LOCAL_CHANGE = false;
    if (!mode_configuracio)         // Si no estem en mode configuracio
    {
      llegir_botons();              // Funcio per llegir valors
      detectar_mode_configuracio(); // Mirem si estan els dos apretats per CONFIG
      if (LOCAL_CHANGE)
      {
        comunicar_polsadors();      // Funció per comunicar valors
      }
    }
  }
}
