/*
 * ============================================================================
 *  OpenDrop
 *  Low-power wireless irrigation controller
 * ============================================================================
 *
 *  Open-source autonomous irrigation controller based on ESP32-C3.
 *
 *  Features:
 *   - Deep sleep low-power operation
 *   - DS3231 RTC scheduling
 *   - Local Wi-Fi configuration portal
 *   - Relay-controlled irrigation valve
 *   - EEPROM configuration storage
 *   - Battery-powered operation
 *
 *  Project:
 *      https://github.com/jsniel/OpenDrop
 *
 *  Author:
 *      Jean-Sébastien Niel
 *
 *  Firmware License:
 *      GNU General Public License v3.0 or later
 *      SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  Hardware License:
 *      CERN Open Hardware Licence Version 2 - Strongly Reciprocal
 *      SPDX-License-Identifier: CERN-OHL-S-2.0
 *
 *  Copyright (C) 2026 Jean-Sébastien Niel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * ============================================================================
 */
 
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include <time.h>

// ===== Réglages =====
#define BUTTON_POLL_SEC      10        // Intervalle de réveil pour scruter les boutons (tenir le bouton ≤10 s)
#define WEB_WINDOW_MS        300000UL  // 5 min d’AP

// === Broches (inchangées) ===
#define BUTTON_WEB_PIN     10
#define BUTTON_AUTO_PIN    21
#define RELAY_OPEN_PIN      4
#define RELAY_CLOSE_PIN     1
#define LED_PIN             2
#define SDAPIN              8
#define SCLPIN              9

// === EEPROM (inchangée) ===
#define EEPROM_SIZE 16
#define EEPROM_ADDR_ENABLED      0
#define EEPROM_ADDR_START_HOUR   1
#define EEPROM_ADDR_END_HOUR     2
#define EEPROM_ADDR_DURATION     3
#define EEPROM_ADDR_NB_CYCLES    4
#define EEPROM_ADDR_START_MIN    5
#define EEPROM_ADDR_END_MIN      6

// === Réseau/AP ===
const char* ssid = "arrosageESP";
const char* password = "secretESP32";
IPAddress local_IP(192,168,4,1), gateway(192,168,4,1), subnet(255,255,255,0);
WebServer server(80);
DNSServer dnsServer;

// === RTC Externe ===
RTC_DS3231 rtc;
bool rtcOk = false;

// === État & config ===
bool autoEnabled = true;
uint8_t heureDebut = 8, minuteDebut = 0;
uint8_t heureFin   = 20, minuteFin   = 0;
uint8_t dureeArrosage = 10;
uint8_t nombreArrosages = 12;

// === Persistance entre deep sleeps ===
RTC_DATA_ATTR bool    vanneOuverte = false;
RTC_DATA_ATTR uint8_t nextEventType = 0;   // 0 = OUVERTURE, 1 = FERMETURE
RTC_DATA_ATTR time_t  nextEventEpoch = 0;  // epoch de l’événement programmé

// ====== LED active-LOW : helpers ======
constexpr bool LED_ACTIVE_LOW = true;
inline void ledOn()  { digitalWrite(LED_PIN, LED_ACTIVE_LOW ? LOW  : HIGH); }
inline void ledOff() { digitalWrite(LED_PIN, LED_ACTIVE_LOW ? HIGH : LOW ); }

// ================== Utilitaires temps ==================
static inline int toMinutes(int h, int m){ return h*60 + m; }
static inline int spanMinutes(int debut, int fin){
  int span = fin - debut; if (span <= 0) span += 24*60; return span;
}

// Convertit DateTime -> epoch (en se basant sur mktime local)
time_t dtToEpoch(const DateTime& dt){
  struct tm t = {};
  t.tm_year = dt.year() - 1900;
  t.tm_mon  = dt.month() - 1;
  t.tm_mday = dt.day();
  t.tm_hour = dt.hour();
  t.tm_min  = dt.minute();
  t.tm_sec  = dt.second();
  return mktime(&t);
}

// Calcule la prochaine OUVERTURE (epoch) à partir de “now”
time_t computeNextOpenEpoch(const DateTime& now){
  int debut = toMinutes(heureDebut, minuteDebut);
  int fin   = toMinutes(heureFin,   minuteFin);
  int span  = spanMinutes(debut, fin);
  if (nombreArrosages == 0) nombreArrosages = 1;
  float step = (float)span / (float)nombreArrosages;

  for (int dayOffset=0; dayOffset<2; ++dayOffset){
    DateTime base = now + TimeSpan(dayOffset, 0, 0, 0);
    for (int i=0;i<nombreArrosages;i++){
      int mc = (debut + (int)round(i*step)) % (24*60);
      int h = mc/60, m = mc%60;
      DateTime cand(base.year(), base.month(), base.day(), h, m, 0);
      if (dayOffset==0 && cand < now) continue;
      return dtToEpoch(cand);
    }
  }
  return time(nullptr) + 60;
}

// ================== EEPROM ==================
void saveConfig(){
  EEPROM.write(EEPROM_ADDR_ENABLED, autoEnabled);
  EEPROM.write(EEPROM_ADDR_START_HOUR, heureDebut);
  EEPROM.write(EEPROM_ADDR_START_MIN, minuteDebut);
  EEPROM.write(EEPROM_ADDR_END_HOUR, heureFin);
  EEPROM.write(EEPROM_ADDR_END_MIN, minuteFin);
  EEPROM.write(EEPROM_ADDR_DURATION, dureeArrosage);
  EEPROM.write(EEPROM_ADDR_NB_CYCLES, nombreArrosages);
  EEPROM.commit();
}

void loadConfig(){
  autoEnabled     = EEPROM.read(EEPROM_ADDR_ENABLED);
  heureDebut      = EEPROM.read(EEPROM_ADDR_START_HOUR);
  minuteDebut     = EEPROM.read(EEPROM_ADDR_START_MIN);
  heureFin        = EEPROM.read(EEPROM_ADDR_END_HOUR);
  minuteFin       = EEPROM.read(EEPROM_ADDR_END_MIN);
  dureeArrosage   = EEPROM.read(EEPROM_ADDR_DURATION);
  nombreArrosages = EEPROM.read(EEPROM_ADDR_NB_CYCLES);
}

// ================== Relais ==================
void ouvrirVanne(){
  Serial.println("🔁 Ouverture vanne (200 ms)");
  digitalWrite(RELAY_CLOSE_PIN, HIGH);
  digitalWrite(RELAY_OPEN_PIN, LOW);
  delay(200);
  digitalWrite(RELAY_OPEN_PIN, HIGH);
  vanneOuverte = true;
  ledOn(); // LED fixe pendant arrosage
}

void fermerVanne(){
  Serial.println("🔁 Fermeture vanne (200 ms)");
  digitalWrite(RELAY_OPEN_PIN, HIGH);
  digitalWrite(RELAY_CLOSE_PIN, LOW);
  delay(200);
  digitalWrite(RELAY_CLOSE_PIN, HIGH);
  vanneOuverte = false;
  ledOff();
}

// ================== LED feedback ON/OFF ==================
void blinkAutoState() {
  if (vanneOuverte) return; // priorité à l'état "vanne ouverte" (LED fixe)
  if (autoEnabled) {
    // 2 brefs
    for (int i=0;i<2;i++){ ledOn(); delay(120); ledOff(); delay(180); }
  } else {
    // 1 long
    ledOn(); delay(400); ledOff(); delay(300);
  }
}

// ================== Web UI ==================
String htmlPage(){
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Arrosage</title>"
                "<style>body{font-family:sans-serif;text-align:center;}input,select{padding:5px;margin:5px;}</style></head><body>";
  if (rtcOk){
    DateTime now = rtc.now();
    html += "<p>Heure RTC : <strong>"+String(now.hour())+"h"+(now.minute()<10?"0":"")+String(now.minute())+"</strong></p>";
  } else {
    html += "<p style='color:#c00'>RTC non détectée</p>";
  }
  html += "<h1>Arrosage automatique</h1>";
  html += "<form action='/save' method='GET'>";
  html += "<label><input type='checkbox' name='enabled'"+String(autoEnabled?" checked":"")+"> Automatisme actif</label><br>";
  html += "Heure début: <select name='hd'>";
  for (int i=0;i<24;i++) html += "<option value='"+String(i)+"'"+(i==heureDebut?" selected":"")+">"+String(i)+"</option>";
  html += "</select>:<select name='md'>";
  for (int i=0;i<60;i++) html += "<option value='"+String(i)+"'"+(i==minuteDebut?" selected":"")+">"+String(i)+"</option>";
  html += "</select><br>";
  html += "Heure fin: <select name='hf'>";
  for (int i=0;i<24;i++) html += "<option value='"+String(i)+"'"+(i==heureFin?" selected":"")+">"+String(i)+"</option>";
  html += "</select>:<select name='mf'>";
  for (int i=0;i<60;i++) html += "<option value='"+String(i)+"'"+(i==minuteFin?" selected":"")+">"+String(i)+"</option>";
  html += "</select><br>";
  html += "Durée (min): <select name='dur'>";
  int opts[] = {5,10,15,20,30,45,60};
  for (int i=0;i<7;i++) html += String("<option value='")+opts[i]+"'"+(opts[i]==dureeArrosage?" selected":"")+">"+opts[i]+"</option>";
  html += "</select><br>Nombre arrosages: <select name='nb'>";
  for (int i=1;i<=24;i++) html += String("<option value='")+i+"'"+(i==nombreArrosages?" selected":"")+">"+i+"</option>";
  html += "</select><br><br><input type='submit' value='Enregistrer'></form><hr>";

  if (rtcOk && nombreArrosages>0){
    int debutTotal = heureDebut*60 + minuteDebut;
    int finTotal   = heureFin*60 + minuteFin;
    int plageTotale = spanMinutes(debutTotal, finTotal);
    float intervalle = (float)plageTotale / (float)nombreArrosages;
    html += "<h3>Heures programmées (jour courant) :</h3><ul style='list-style:none;padding:0;'>";
    for (int i=0;i<nombreArrosages;i++){
      int minuteCible = (debutTotal + (int)round(i*intervalle)) % (24*60);
      int h = minuteCible/60, m = minuteCible%60;
      html += "<li>"+String(h)+"h"+(m<10?"0":"")+String(m)+"</li>";
    }
    html += "</ul>";
  }

  html += "<hr><h3>Régler l'heure de la RTC</h3>";
  html += "</form><form action='/setrtc' method='GET'>";
  html += "Heure : <select name='h'>";
  for (int i=0;i<24;i++) html += "<option value='"+String(i)+"'>"+String(i)+"</option>";
  html += "</select> Minute : <select name='m'>";
  for (int i=0;i<60;i++) html += "<option value='"+String(i)+"'>"+(i<10?"0":"")+String(i)+"</option>";
  html += "</select> <input type='submit' value='Valider'></form>";
  html += "<p><a href='/sleep'>Fermer le Wi-Fi et dormir</a></p>";
  html += "</body></html>";
  return html;
}

void startWeb(){
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, [](){ server.send(200, "text/html", htmlPage()); });

  server.on("/save", HTTP_GET, [](){
    autoEnabled     = server.hasArg("enabled");
    heureDebut      = server.arg("hd").toInt();
    minuteDebut     = server.arg("md").toInt();
    heureFin        = server.arg("hf").toInt();
    minuteFin       = server.arg("mf").toInt();
    dureeArrosage   = server.arg("dur").toInt();
    nombreArrosages = server.arg("nb").toInt();
    saveConfig();
    if (!autoEnabled && vanneOuverte) fermerVanne();
    server.sendHeader("Location","/",true); server.send(302,"text/plain","");
  });

  server.on("/setrtc", HTTP_GET, [](){
    if (rtcOk && server.hasArg("h") && server.hasArg("m")){
      int newH = server.arg("h").toInt();
      int newM = server.arg("m").toInt();
      DateTime now = rtc.now();
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), newH, newM, 0));
    }
    server.sendHeader("Location","/",true); server.send(302,"text/plain","");
  });

  server.on("/sleep", HTTP_GET, [](){
    server.send(200,"text/plain","Fermeture Wi-Fi et dodo...");
    delay(250);
    server.stop(); WiFi.softAPdisconnect(true);
  });

  server.onNotFound([](){ server.sendHeader("Location","/",true); server.send(302,"text/plain",""); });

  server.begin();
}

// ===== Deep sleep helper : éteindre LED vraiment, puis dormir =====
void goDeepSleep(time_t sleepSec){
  // feedback visuel du mode avant dodo (si vanne fermée)
  blinkAutoState();

  // Timer wake
  if (sleepSec < 1) sleepSec = 1;
  esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);

  // Éteindre la LED réellement (active-LOW) et passer en entrée pour éviter la lueur
  ledOff();
  pinMode(LED_PIN, INPUT);
  gpio_pulldown_dis((gpio_num_t)LED_PIN);
  gpio_pullup_dis((gpio_num_t)LED_PIN);

  esp_deep_sleep_start();
}

// ================== Planification & Sommeil ==================
void planAndSleep(){
  time_t sleepSec = BUTTON_POLL_SEC;   // par défaut, polling boutons
  time_t nowEpoch = time(nullptr);

  if (autoEnabled){
    if (vanneOuverte){
      // prochaine FERMETURE après la durée (pendant arrosage : pas de polling)
      nextEventType  = 1;
      nextEventEpoch = nowEpoch + (time_t)dureeArrosage*60;
      sleepSec       = (time_t)dureeArrosage*60;
    } else {
      // prochaine OUVERTURE planifiée
      if (!rtcOk){
        nextEventType  = 0;
        nextEventEpoch = nowEpoch + 300; // retry 5 min
        sleepSec       = (BUTTON_POLL_SEC < 300) ? BUTTON_POLL_SEC : 300;
      } else {
        DateTime now = rtc.now();
        nextEventType  = 0;
        nextEventEpoch = computeNextOpenEpoch(now);
        time_t delta   = nextEventEpoch - dtToEpoch(now);
        if (delta < 1) delta = 1;
        sleepSec = (delta < BUTTON_POLL_SEC) ? delta : BUTTON_POLL_SEC;
      }
    }
  } else {
    // Auto OFF : juste polling boutons
    nextEventEpoch = 0;
    nextEventType  = 0;
  }

  Serial.printf("Sleep %ld s (nextType=%u, vanne=%d)\n", (long)sleepSec, (unsigned)nextEventType, (int)vanneOuverte);
  goDeepSleep(sleepSec);
}

// ================== Setup ==================
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(RELAY_OPEN_PIN, OUTPUT);
  pinMode(RELAY_CLOSE_PIN, OUTPUT);
  digitalWrite(RELAY_OPEN_PIN, HIGH);
  digitalWrite(RELAY_CLOSE_PIN, HIGH);

  pinMode(LED_PIN, OUTPUT);
  if (vanneOuverte) ledOn(); else ledOff();

  pinMode(BUTTON_WEB_PIN, INPUT_PULLUP);
  pinMode(BUTTON_AUTO_PIN, INPUT_PULLUP);

  Wire.begin(SDAPIN, SCLPIN);
  rtcOk = rtc.begin();
  if (!rtcOk) Serial.println("⚠️ RTC non détectée !");
  else if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  EEPROM.begin(EEPROM_SIZE);
  loadConfig();

  // Heure courante (epoch) : si RTC OK, on la convertit en epoch
  time_t nowEpoch = rtcOk ? dtToEpoch(rtc.now()) : time(nullptr);

  // 1) Lire boutons (maintenus) juste au réveil (polling)
  bool webHeld  = (digitalRead(BUTTON_WEB_PIN)  == LOW);
  bool autoHeld = (digitalRead(BUTTON_AUTO_PIN) == LOW);

  if (webHeld){
    Serial.println("WEB maintenu → AP 5 minutes");
    startWeb();
    unsigned long t0 = millis();
    while (millis() - t0 < WEB_WINDOW_MS){
      server.handleClient();
      dnsServer.processNextRequest();
      // LED clignote lentement pendant l’AP
      if (((millis()/500)%2)==0) ledOn(); else ledOff();
      delay(10);
    }
    server.stop(); WiFi.softAPdisconnect(true);
    if (vanneOuverte) ledOn(); else ledOff();
    planAndSleep();
  }

  if (autoHeld){
    Serial.println("AUTO maintenu → toggle ON/OFF");
    autoEnabled = !autoEnabled;
    saveConfig();
    if (!autoEnabled && vanneOuverte) fermerVanne();
    planAndSleep();
  }

  // 2) Exécuter une action due (si programmée)
  if (autoEnabled && nextEventEpoch > 0){
    if (!vanneOuverte && nextEventType==0 && nowEpoch >= nextEventEpoch){
      // OUVERTURE due
      ouvrirVanne();
      // enchaîne vers la FERMETURE après dureeArrosage (pas de polling)
      nextEventType  = 1;
      nextEventEpoch = nowEpoch + (time_t)dureeArrosage*60;
      goDeepSleep((time_t)dureeArrosage*60);
    }
    if (vanneOuverte && nextEventType==1 && nowEpoch >= nextEventEpoch){
      // FERMETURE due
      fermerVanne();
      // planifier la prochaine OUVERTURE
      if (rtcOk){
        DateTime now = rtc.now();
        nextEventType  = 0;
        nextEventEpoch = computeNextOpenEpoch(now);
      } else {
        nextEventType  = 0;
        nextEventEpoch = nowEpoch + 300; // RTC absente → retry 5 min
      }
      planAndSleep();
    }
  }

  // 3) Rien de spécial → planifier et dormir (avec polling)
  planAndSleep();
}

void loop(){ /* tout est géré dans setup() avant de redormir */ }
