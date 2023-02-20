/*
 TALLY SLAVE

 Firmware per gestionar un sistema de Tally's inhalambrics.

 */

/*
TODO

Implentar hora
*/

/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/?s=esp-now
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  Based on JC Servaye example: https://github.com/Servayejc/esp_now_sender/
*/
#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h> //Control neopixels
#include <LiquidCrystal_I2C.h> //Control display cristall liquid
#include <time.h>              //Donar hora real

#define VERSIO "S1.3" // Versió del software

// Bool per veure missatges de debug
bool debug = true;

// Set your Board and Server ID
#define BOARD_ID 1 // Cal definir cada placa amb el seu numero
// TODO Poder definir en el menu el numero
#define MAX_CHANNEL 13 // for North America // 13 in Europe

// Configurem LED BUILTIN
#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // definim el LED local de la placa
#endif

// Define PINS
// Botons i leds locals
#define POLSADOR_ROIG_PIN 16
#define POLSADOR_VERD_PIN 5
#define LED_ROIG_PIN 17
#define LED_VERD_PIN 18
#define MATRIX_PIN 4
// Definim temps per debouncer polsadors
#define DEBOUNCE_DELAY 100 // Delay debouncer
// Define Quantitat de leds
#define LED_COUNT 8 // 8x8 + 8

// Define sensor battery
#define BATTERY_PIN 36
#define FULL_VOLTAGE 4.2
#define EMPTY_VOLTAGE 3.3
#define NUM_SAMPLES 10

// Declarem neopixels
Adafruit_NeoPixel llum(LED_COUNT, MATRIX_PIN, NEO_GRB + NEO_KHZ800);

// Definim els colors RGB
const uint8_t COLOR[8][3] = {{0, 0, 0},        // 0- NEGRE
                             {255, 0, 0},      // 1- ROIG
                             {0, 0, 255},      // 2- BLAU
                             {255, 0, 255},    // 3- MAGENTA
                             {0, 255, 0},      // 4- VERD
                             {255, 80, 0},     // 5- GROC
                             {255, 25, 0},     // 6- TARONJA
                             {255, 200, 125}}; // 7- BLANC

uint8_t funcio_local = 0;           // 0 = TALLY, 1 = CONDUCTOR, 2 = PRODUCTOR
uint8_t color_matrix[] = {0, 0, 0}; // Primera fila mode 1 Segona fila mode 2: 0 = TALLY, 1 = CONDUCTOR, 2 = PRODUCTOR
// Modes:
// Mode 1: El tally sempre mostra el mateix color
// Mode 2: El tally canvia de color quan s'envia un misatge a COND o PROD (Blau) i ESTU (lila)
const uint8_t ModeColor = 1;

// Declarem el display LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // 0x27 adreça I2C 16 = Caracters 2= Linees

// Variables
bool debouncing_roig = false; // Flag debouncing
bool debouncing_verd = false; // Flag debouncing
unsigned long last_time_roig; // Temps debouncing roig
unsigned long last_time_verd; // Temps debouncing verd

// Fem arrays de dos valors la 0 és anterior la 1 actual
bool POLSADOR_LOCAL_ROIG[] = {false, false};
bool POLSADOR_LOCAL_VERD[] = {false, false};

// Valor dels leds (dels polsadors)
bool LED_LOCAL_ROIG = false;
bool LED_LOCAL_VERD = false;
//Valor bateria
float bat_local_volt = 0;      // Variable per lectura local de la bateria volts
uint8_t bat_local_percent = 0; // Variable per lectura local de la bateria percntil

// Variables de gestió
bool LOCAL_CHANGE = false; // Per saber si alguna cosa local ha canviat

unsigned long temps_set_config = 0;      // Temps que ha d'estar apretat per configuracio
const unsigned long temps_config = 5000; // Temps per disparar opció config
unsigned long temps_post_config = 0;     // Temps per sortir config
bool pre_mode_configuracio = false;      // Inici mode configuració
bool mode_configuracio = false;          // Mode configuració
bool post_mode_configuracio = false;     // Final configuració

// Temps per rutines lectura bateria
unsigned long ultima_lectura_bat;
const unsigned long interval_lectura_bat = 300000; //Cada 5 minuts - ajustar si cal

bool No_time = true; // No tenim sincro amb hora

uint8_t serverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Text a mostrar
uint8_t display_text_1; // Primera linea
uint8_t display_text_2; // Segona linea

// Icones bateria
byte baticon[6][8] = {
    {
        B01110, // 0%
        B11111,
        B10001,
        B10001,
        B10001,
        B10001,
        B10001,
        B11111,
    },
    {
        B01110, // 20%
        B11111,
        B10001,
        B10001,
        B10001,
        B10001,
        B11111,
        B11111,
    },
    {
        B01110, // 40%
        B11111,
        B10001,
        B10001,
        B10001,
        B11111,
        B11111,
        B11111,
    },
    {
        B01110, // 60%
        B11111,
        B10001,
        B10001,
        B11111,
        B11111,
        B11111,
        B11111,
    },
    {
        B01110, // 80%
        B11111,
        B10001,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
    },
    {
        B01110, //100%
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
    }};

//     TEXT_1[] = "12345678""90123456"
//                          "HH:MM:SS"
//                          "NO CLOCK"
String TEXT_1[] = {"       ",  // 0
                   " TALLY ",  // 1
                   " COND  ",  // 2
                   " PROD  ",  // 3
                   "CONFIG:",  // 4
                   "NO LINK"}; // 5

//     TEXT_2[] = "1234567890123456"
String TEXT_2[] = {"                ",  // 0
                   " FORA DE SERVEI ",  // 1
                   " ERROR GPI VIA  ",  // 2
                   "  ERROR GPI QL  ",  // 3
                   "**** ON AIR ****",  // 4
                   "ORD PROD A COND ",  // 5
                   "ORD DE PRODUCTOR",  // 6
                   "ORD A CONDUCTOR ",  // 7
                   "ORD COND A PROD ",  // 8
                   "ORD DE CONDUCTOR",  // 9
                   "ORD A PRODUCTOR ",  // 10
                   "ORD PROD A ESTUD",  // 11
                   "ORDRES A ESTUDI ",  // 12
                   "ORD COND A ESTUD",  // 13
                   "TANCAT LOCALMENT",  // 14
                   "TANCAT DE ESTUDI",  // 15
                   "  MICRO TANCAT  ",  // 16
                   "* ON AIR LOCAL *",  // 17
                   "<MODE TALLY    >",  // 18
                   "<MODE CONDUCTOR>",  // 19
                   "<MODE PRODUCTOR>",  // 20
                   " BATERIA BAIXA! "}; // 21

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
  uint8_t funcio;         // Identificador de la funcio del tally
  bool led_roig[3];       // llum confirmació cond polsador roig
  bool led_verd[3];       // llum confirmació cond polsador verd
  uint8_t color_tally[3]; // Color indexat del tally
  uint8_t text_2[3];      // Missatge per mostrar
} struct_message_from_master;

// Estrucrtura dades per enviar a master
typedef struct struct_message_to_master
{
  uint8_t msgType;
  uint8_t id;     // Identificador del tally
  uint8_t funcio; // Identificador de la funcio del tally
  bool polsador_roig;
  bool polsador_verd;
} struct_message_to_master;

// Estructura dades per enviar bateries
typedef struct struct_bateria_info
{
  uint8_t msgType;
  uint8_t id;              // Identificador del tally
  float bateria_volts;     // Lectura en volts
  uint8_t bateria_percent; // Percentatge carrega
} struct__bateria_info;

// Estructura dades per rebre clock
typedef struct struct_clock_from_master
{
  uint8_t msgType;
  tm temps_rebut;
} struct_clock_from_master;

// Create 2 struct_message
struct_message myData; // data to send
struct_message inData; // data received
struct_pairing pairingData;
struct_message_from_master fromMaster;     // dades del master cap al tally
struct_message_to_master toMaster;         // dades del tally cap al master
struct_bateria_info bat_info;              // dades de la bateria cap al master
struct_clock_from_master clock_fromMaster; // dades de clock del master

struct tm timeinfo; // Estructura per temps

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

#ifdef SAVE_CHANNEL
int lastChannel;
#endif
int channel = 1;

unsigned long currentMillis = millis();
unsigned long previousMillis = 0; // Stores last time batery was published
const long interval = 10000;      // Interval at which to publish bateria sensor readings
unsigned long start;              // used to measure Pairing time
unsigned int readingId = 0;

// lectura bateria
// Bateria 4,2V - 3,2V si posem dos diodes en serie ens queda en 4.2 - 1.4 = 2,8 Max
// 3,2V -1,4 = 1,8 Min

void llegir_bateria()
{
  bat_local_volt = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    bat_local_volt += analogRead(BATTERY_PIN) * 3.3 / 4096;
  }
  bat_local_volt /= NUM_SAMPLES; //Fem la mitjana de les lectures
  bat_local_volt = bat_local_volt + 1.4; //Compensem la caiguda de tensió de 2 diodes en serie 0.7 + 0.7V
  bat_local_percent = 100 * (bat_local_volt - EMPTY_VOLTAGE) / (FULL_VOLTAGE - EMPTY_VOLTAGE);
}


void escriure_display_1(uint8_t txt1)
{
  lcd.setCursor(0, 0); // Situem cursor primer caracter, primera linea
  lcd.print(TEXT_1[txt1]);
}
void escriure_display_2(uint8_t txt2)
{
  lcd.setCursor(0, 1); // Primer caracter, segona linea
  lcd.print(TEXT_2[txt2]);
}
// Dibuixem bateria en Display
void escriure_display_bateria(uint8_t bat_icona_percent)
{
  lcd.setCursor(15, 0); // Ultimr caracter, primera linea
  lcd.write((byte)bat_icona_percent);
}
// Seleccionem icona nivell bateria
void mostrar_bat()
{
  if (bat_local_percent < 10)
  {
    escriure_display_bateria(0);
  }
  if (bat_local_percent > 10 && bat_local_percent < 20)
  {
    escriure_display_bateria(1);
  }
  if (bat_local_percent > 20 && bat_local_percent < 40)
  {
    escriure_display_bateria(2);
  }
  if (bat_local_percent > 40 && bat_local_percent < 60)
  {
    escriure_display_bateria(3);
  }
  if (bat_local_percent > 60 && bat_local_percent < 80)
  {
    escriure_display_bateria(4);
  }
  if (bat_local_percent > 80)
  {
    escriure_display_bateria(5);
  }
}

void escriure_display_clock()
{
  if (No_time)
  {
    lcd.setCursor(7, 0);   // Caracter 8, primera linea
    lcd.print("        "); //
  }
  else
  {
    lcd.setCursor(7, 0); // Caracter 7, primera linea
    lcd.print(&timeinfo, "%H");
    lcd.setCursor(9, 0); // Caracter 9, primera linea
    lcd.print(":");
    lcd.setCursor(10, 0); // Caracter 10, primera linea
    lcd.print(&timeinfo, "%M");
    lcd.setCursor(12, 0); // Caracter 12, primera linea
    lcd.print(":");
    lcd.setCursor(13, 0); // Caracter 13, primera linea
    lcd.print(&timeinfo, "%S");
  }
}

// Llum arrencada
void llum_rgb()
{
  llum.setPixelColor(0, llum.Color(0, 0, 10));
  llum.setPixelColor(1, llum.Color(0, 0, 25));
  llum.setPixelColor(2, llum.Color(0, 0, 100));
  llum.setPixelColor(3, llum.Color(0, 0, 255));
  llum.setPixelColor(4, llum.Color(0, 0, 255));
  llum.setPixelColor(5, llum.Color(0, 0, 100));
  llum.setPixelColor(6, llum.Color(0, 0, 25));
  llum.setPixelColor(7, llum.Color(0, 0, 10));
  llum.show();
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
    llum.setPixelColor(i, llum.Color(R, G, B));
  }
  llum.show();
  /*if (debug)
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
  */
}

void comunicar_polsadors()
{
  toMaster.msgType = TALLY;
  toMaster.id = BOARD_ID;
  toMaster.funcio = funcio_local;
  toMaster.polsador_roig = POLSADOR_LOCAL_ROIG[1];
  toMaster.polsador_verd = POLSADOR_LOCAL_VERD[1];

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
  bat_info.msgType = BATERIA;
  bat_info.id = BOARD_ID;
  bat_info.bateria_volts = bat_local_volt;
  bat_info.bateria_percent = bat_local_percent;
  esp_err_t result = esp_now_send(serverAddress, (uint8_t *)&bat_info, sizeof(bat_info));
}

void llegir_polsadors()
{
  LOCAL_CHANGE = false;
  POLSADOR_LOCAL_ROIG[1] = !digitalRead(POLSADOR_ROIG_PIN); // Els POLSADOR son PULLUP per tant els llegirem al revés
  POLSADOR_LOCAL_VERD[1] = !digitalRead(POLSADOR_VERD_PIN);
  // Detecció canvi de POLSADOR locals
  if ((POLSADOR_LOCAL_ROIG[0] != POLSADOR_LOCAL_ROIG[1]) && (!debouncing_roig))
  {
    /// HEM POLSAT EL POLSADOR ROIG PERO NO VALIDEM
    last_time_roig = millis();
    debouncing_roig = true;
  }

  if ((POLSADOR_LOCAL_ROIG[0] != POLSADOR_LOCAL_ROIG[1]) && (debouncing_roig))
  {
    if ((millis() - last_time_roig) > DEBOUNCE_DELAY)
    {
      // HA PASSAT EL TEMPS DE DEBOUNCING
      LOCAL_CHANGE = true;
      POLSADOR_LOCAL_ROIG[0] = POLSADOR_LOCAL_ROIG[1];
      debouncing_roig = false;
      if (debug)
      {
        Serial.print("POLSADOR local ROIG: ");
        Serial.println(POLSADOR_LOCAL_ROIG[0]);
      }
    }
  }

  if ((POLSADOR_LOCAL_VERD[0] != POLSADOR_LOCAL_VERD[1]) && (!debouncing_verd))
  {
    /// HEM POLSAT EL POLSADOR VERD PERO NO VALIDEM
    last_time_verd = millis();
    debouncing_verd = true;
  }
  if ((POLSADOR_LOCAL_VERD[0] != POLSADOR_LOCAL_VERD[1]) && (debouncing_verd))
  {

    if ((millis() - last_time_verd) > DEBOUNCE_DELAY)
    {
      LOCAL_CHANGE = true;
      POLSADOR_LOCAL_VERD[0] = POLSADOR_LOCAL_VERD[1];
      debouncing_verd = false;
      if (debug)
      {
        Serial.print("POLSADOR local VERD: ");
        Serial.println(POLSADOR_LOCAL_VERD[0]);
      }
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
  // local_text_1 = 4; //CONFIG
  // local_text_2 = 18; //MODE TALLY
  int select[] = {18, 19, 20}; // Les tres opciions de config
  int sel = 0;
  escriure_display_1(4);
  post_mode_configuracio = false;
  while (mode_configuracio)
  {
    escriure_display_2(select[sel]); // Dibuixem la opcio
    /*
    if (debug)
    {
      Serial.print("Sel: ");
      Serial.println(sel);
    }
    */
    LOCAL_CHANGE = false;
    llegir_polsadors(); // Llegim els polsadors
    if (LOCAL_CHANGE)
    {
      if (POLSADOR_LOCAL_ROIG[0] && !POLSADOR_LOCAL_VERD[0] && !post_mode_configuracio)
      {
        if (sel == 0)
        {
          sel = 2;
        }
        else
        {
          sel = (sel - 1);
        }
        if (debug)
        {
          Serial.print("Sel: ");
          Serial.println(sel);
        }
      }
      if (POLSADOR_LOCAL_VERD[0] && !POLSADOR_LOCAL_ROIG[0] && !post_mode_configuracio)
      {
        if (sel == 2)
        {
          sel = 0;
        }
        else
        {
          sel = (sel + 1);
        }
        if (debug)
        {
          Serial.print("Sel: ");
          Serial.println(sel);
        }
      }
      if (POLSADOR_LOCAL_VERD[0] && !POLSADOR_LOCAL_ROIG[0] && post_mode_configuracio)
      {
        post_mode_configuracio = false;
      }
      if (!POLSADOR_LOCAL_VERD[0] && POLSADOR_LOCAL_ROIG[0] && post_mode_configuracio)
      {
        post_mode_configuracio = false;
      }
      if (POLSADOR_LOCAL_ROIG[0] && POLSADOR_LOCAL_VERD[0] && !post_mode_configuracio) // Apretem dos botons per sortir config
      {
        temps_post_config = millis();
        post_mode_configuracio = true;
        if (debug)
        {
          Serial.println("ENTREM EN POST CONFIGURACIO MODE");
        }
      }
    }
    if (post_mode_configuracio && (millis() >= (temps_config + temps_post_config)))
    {
      LED_LOCAL_ROIG = false;
      LED_LOCAL_VERD = false;
      escriure_leds();
      funcio_local = sel;
      escriure_display_1(sel + 1); // Escribim la funció local
      escriure_display_2(0);       // Borrem linea inferior
      mode_configuracio = false;
      if (debug)
      {
        Serial.println("SORTIM CONFIGURACIO MODE");
        Serial.println("APAGUEM LEDS");
        Serial.print("Sel: ");
        Serial.println(sel);
      }
    }
  }
  LOCAL_CHANGE = false;
  post_mode_configuracio = false;
}

void detectar_mode_configuracio()
{
  if (LOCAL_CHANGE)
  {
    if (POLSADOR_LOCAL_ROIG[0] && POLSADOR_LOCAL_VERD[0] && !pre_mode_configuracio)
    {
      // Tenim els dos polsadors apretats i no estem en pre_mode_configuracio
      // Entrarem al mode CONFIG
      temps_set_config = millis();  // Llegim el temps actual per entrar a mode config
      pre_mode_configuracio = true; // Situem el flag en pre-mode-confi
      if (debug)
      {
        Serial.println("PRE CONFIGURACIO MODE");
      }
    }

    if ((!POLSADOR_LOCAL_ROIG[0] || !POLSADOR_LOCAL_VERD[0]) && pre_mode_configuracio)
    { // Si deixem de pulsar polsadors i estavem en pre_mode_de_configuracio
      if ((millis()) >= (temps_config + temps_set_config))
      {                                // Si ha pasat el temps d'activació
        mode_configuracio = true;      // Entrem en mode configuracio
        pre_mode_configuracio = false; // Sortim del mode preconfiguracio
        LOCAL_CHANGE = false;
        if (debug)
        {
          Serial.println("CONFIGURACIO MODE");
        }
        Menu_configuracio();
      }
      else
      {
        pre_mode_configuracio = false; // Cancelem la preconfiguracio
        mode_configuracio = false;     // Cancelem la configuracio
        if (debug)
        {
          Serial.println("CANCELEM CONFIGURACIO MODE");
        }
      }
    }
  }
  if (POLSADOR_LOCAL_ROIG[0] && POLSADOR_LOCAL_VERD[0] && (millis() >= (temps_config + temps_set_config)))
  {
    LED_LOCAL_ROIG = true;
    LED_LOCAL_VERD = true;
    escriure_leds();

    if (debug)
    {
      Serial.println("ENCENEM LEDS LOCALS");
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
  if (!mode_configuracio) // Si no estem en mode configuració
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
        Serial.print("Led roig = ");
        Serial.println(fromMaster.led_roig[funcio_local]);
        Serial.print("Led verd= ");
        Serial.println(fromMaster.led_verd[funcio_local]);
        Serial.print("Color tally  = ");
        Serial.println(fromMaster.color_tally[funcio_local]);
        Serial.print("Missatge linea 2 = ");
        Serial.println(fromMaster.text_2[funcio_local]);
      }
      LED_LOCAL_ROIG = fromMaster.led_roig[funcio_local];    // Carreguem el valor rebut al LED roig
      LED_LOCAL_VERD = fromMaster.led_verd[funcio_local];    // Carreguem el valor rebut al LED verd
      escriure_display_1((funcio_local + 1));                  // Per si ha quedat en versio no LINK
      escriure_leds();                                       // CRIDAR SUBRUTINA ESCRIURE LED
      escriure_matrix(fromMaster.color_tally[funcio_local]); // CRIDAR SUBRUTINA ESCRIURE TALLY
      escriure_display_2(fromMaster.text_2[funcio_local]);   // CRIDAR SUBRUTINA ESCRIURE TEXT
      break;

    case CLOCK: // Missatge sincronització hora
      memcpy(&clock_fromMaster, incomingData, sizeof(clock_fromMaster));
      //escriure_display_1(funcio_local + 1);    // Per si ha quedat en versio no LINK
      timeinfo = clock_fromMaster.temps_rebut; // Li passem el valor a clock
      No_time = false;
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
        pairingStatus = PAIR_PAIRED;            // set the pairing status
        comunicar_polsadors();                  // Comuniquem quina funcio te el SLAVE
        escriure_display_1((funcio_local + 1)); // Mostrem funcio local
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
    if (currentMillis - previousMillis > 250) // Original 250
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
    escriure_display_1((funcio_local + 1)); // Dibuixem funcio local
    // pairingStatus = PAIR_PAIRED;
    break;
  }
  return pairingStatus;
}

void setup()
{
  // Initialize Serial Monitor
  Serial.begin(115200);
  llum_rgb();      // LLum inici
  lcd.init();      // Inicialitzem lcd
  lcd.backlight(); // Arrenquem la llum de fons lcd
  // Creem els icons de la bateria
  lcd.createChar(0,baticon[0]); //0%
  lcd.createChar(1,baticon[1]); //20%
  lcd.createChar(2,baticon[2]); //40%
  lcd.createChar(3,baticon[3]); //60%
  lcd.createChar(4,baticon[4]); //80%
  lcd.createChar(5,baticon[5]); //100%
  lcd.clear();     // Esborrem la pantalla

  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);

  // Configurem els pins POLSADORNS/LEDS
  pinMode(POLSADOR_ROIG_PIN, INPUT_PULLUP);
  pinMode(POLSADOR_VERD_PIN, INPUT_PULLUP);
  pinMode(LED_ROIG_PIN, OUTPUT);
  pinMode(LED_VERD_PIN, OUTPUT);

  Serial.println("TALLY SLAVE");
  Serial.print("Versio: ");
  Serial.println(VERSIO);
  Serial.print("Client Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  lcd.setCursor(0, 0); // Situem cursor primer caracter, primera linea
  lcd.print("TALLY SLAVE");
  lcd.setCursor(0, 1); // Primer caracter, segona linea
  lcd.print("Versio: ");
  lcd.setCursor(9, 1); // Caracter 9, segona linea
  lcd.print(VERSIO);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  start = millis();

  // Esborrem llum
  llum.clear();
  lcd.clear();
  escriure_display_1((funcio_local + 1)); //Funcio local
  llegir_bateria(); // Mirem la bateria
  mostrar_bat(); // Dibuixem nivell bateria
  last_time_roig = millis(); // Debouncer polsador
  last_time_verd = millis(); // Debouncer polsador
  LOCAL_CHANGE = false;
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
  llegir_polsadors();           // Funcio per llegir valors
  detectar_mode_configuracio(); // Mirem si estan els dos apretats per CONFIG
  // Llegim bateria cada interval
  if ((millis() - ultima_lectura_bat) > interval_lectura_bat)
  {
    llegir_bateria(); // Llegim valor bateria
    mostrar_bat(); // Dibuixem bateria
    comunicar_bateria(); // Comuniquem bateria
    ultima_lectura_bat = millis();  
  }
  if (autoPairing() == PAIR_PAIRED)
  {
    if (!mode_configuracio) // Si no estem en mode configuracio
    {
      if (LOCAL_CHANGE)
      {
        comunicar_polsadors(); // Funció per comunicar valors
      }
      escriure_display_clock(); // Dibuixem hora
    }
  }
  else
  {
    escriure_display_1(5); // Escrivim NO_LINK
    escriure_display_2(1); // Escrivim FORA DE SERVEI
    escriure_matrix(0); // Color negre
    if (debug)
    {
      Serial.println("No emparellat");
    }
  }
}
