
#include <Arduino.h>
#include <WiFi.h>
#include <DHT.h>
#include <ThingSpeak.h>
#include <math.h>
#include <WebServer.h>
#include "dashboard.h"

// ========================================
// KONFIGURASI WIFI
// ========================================

const char* WIFI_SSID = "MR-FK-01";
const char* WIFI_PASSWORD = "adzanmasjid";

// ========================================
// KONFIGURASI THINGSPEAK
// ========================================

const unsigned long CHANNEL_ID = 0;
const char* WRITE_API_KEY = "ISI_WRITE_API_KEY";

// ========================================
// KONFIGURASI SENSOR
// ========================================

#define DHT_PIN 25
#define DHT_TYPE DHT22
#define LDR_PIN 26
#define PIR_PIN 27

// LOW = TERANG, HIGH = GELAP
// Ubah ke false jika modul terbalik.
const bool LDR_LOW_ARTINYA_TERANG = true;

// ========================================
// INTERVAL PEMBACAAN
// ========================================

const unsigned long INTERVAL_BACA = 2500;
const unsigned long INTERVAL_KIRIM = 20000;
const unsigned long INTERVAL_WIFI = 10000;

// ========================================
// INISIALISASI LIBRARY
// ========================================

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient client;
WebServer server(80);

// ========================================
// VARIABEL SENSOR
// ========================================

float suhu = NAN;
float kelembapan = NAN;

bool dhtValid = false;
bool gelap = false;
bool gerakanTerdeteksi = false;

int nilaiLDR = 0;
int nilaiPIR = 0;

unsigned long waktuBaca = 0;
unsigned long waktuKirim = 0;
unsigned long waktuWiFi = 0;

bool wifiSebelumnya = false;

// Waktu (millis) gerakan terakhir, 0 = belum ada
unsigned long waktuGerakanTerakhir = 0;

// ========================================
// WEB SERVER (DASHBOARD)
// ========================================

void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleData() {

  String json = "{";

  json += "\"dht_valid\":";
  json += dhtValid ? "true" : "false";

  json += ",\"suhu\":";
  json += isnan(suhu) ? "null" : String(suhu, 1);

  json += ",\"kelembapan\":";
  json += isnan(kelembapan) ? "null" : String(kelembapan, 1);

  json += ",\"ldr\":" + String(nilaiLDR);

  json += ",\"gelap\":";
  json += gelap ? "true" : "false";

  json += ",\"gerakan\":";
  json += (digitalRead(PIR_PIN) == HIGH) ? "true" : "false";

  long sejakGerakan = -1;
  if (waktuGerakanTerakhir != 0) {
    sejakGerakan = (millis() - waktuGerakanTerakhir) / 1000;
  }
  json += ",\"detik_sejak_gerakan\":" + String(sejakGerakan);

  json += ",\"uptime\":" + String(millis() / 1000);

  json += ",\"wifi_ssid\":\"" + WiFi.SSID() + "\"";
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"rssi\":" + String(WiFi.RSSI());

  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// ========================================
// TAMPILKAN INFORMASI WIFI
// ========================================

void tampilkanInfoWiFi() {

  Serial.println();
  Serial.println("================================");
  Serial.println("       WIFI TERHUBUNG!");
  Serial.println("================================");

  Serial.print("Nama WiFi : ");
  Serial.println(WiFi.SSID());

  Serial.print("IP ESP32  : ");
  Serial.println(WiFi.localIP());

  Serial.print("Dashboard : http://");
  Serial.println(WiFi.localIP());

  Serial.print("Sinyal    : ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  Serial.println("================================");
}

// ========================================
// MEMBACA SENSOR
// ========================================

void bacaSensor() {

  // -------- DHT22 --------

  float suhuBaru = dht.readTemperature();
  float lembapBaru = dht.readHumidity();

  dhtValid = !isnan(suhuBaru) &&
             !isnan(lembapBaru);

  if (dhtValid) {
    suhu = suhuBaru;
    kelembapan = lembapBaru;
  }

  // -------- LDR DIGITAL --------

  nilaiLDR = digitalRead(LDR_PIN);

  if (LDR_LOW_ARTINYA_TERANG) {
    gelap = (nilaiLDR == HIGH);
  } else {
    gelap = (nilaiLDR == LOW);
  }

  // -------- PIR --------

  nilaiPIR = digitalRead(PIR_PIN);

  if (nilaiPIR == HIGH) {
    gerakanTerdeteksi = true;
  }

  // -------- DASHBOARD --------

  Serial.println();
  Serial.println("========================================");
  Serial.println("       MONITORING RUANG BELAJAR");
  Serial.println("========================================");

  Serial.print("Uptime           : ");
  Serial.print(millis() / 1000);
  Serial.println(" detik");

  Serial.println("----------------------------------------");

  if (dhtValid) {

    Serial.print("Suhu             : ");
    Serial.print(suhu, 1);
    Serial.println(" C");

    Serial.print("Kelembapan       : ");
    Serial.print(kelembapan, 1);
    Serial.println(" %");

  } else {

    Serial.println("Suhu             : ERROR DHT22");
    Serial.println("Kelembapan       : ERROR DHT22");
  }

  Serial.println("----------------------------------------");

  Serial.print("Nilai LDR DO     : ");
  Serial.println(nilaiLDR);

  Serial.print("Kondisi Cahaya   : ");
  Serial.println(gelap ? "GELAP" : "TERANG");

  Serial.print("Gerakan          : ");

  if (nilaiPIR == HIGH) {
    Serial.println("TERDETEKSI");
  } else {
    Serial.println("TIDAK ADA");
  }

  Serial.println("----------------------------------------");

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi             : TERHUBUNG");

    Serial.print("IP ESP32         : ");
    Serial.println(WiFi.localIP());

    Serial.print("Sinyal           : ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

  } else {

    Serial.println("WiFi             : TERPUTUS");
    Serial.println("IP ESP32         : -");
  }

  Serial.println("========================================");
}

// ========================================
// KIRIM KE THINGSPEAK
// ========================================

void kirimThingSpeak() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ThingSpeak] WiFi terputus.");
    return;
  }

  if (CHANNEL_ID == 0 ||
      String(WRITE_API_KEY) == "ISI_WRITE_API_KEY" ||
      strlen(WRITE_API_KEY) == 0) {

    Serial.println(
      "[ThingSpeak] Isi Channel ID dan API Key."
    );
    return;
  }

  if (!dhtValid) {
    Serial.println(
      "[ThingSpeak] Data DHT22 tidak valid."
    );
    return;
  }

  // Field 1: Suhu
  // Field 2: Kelembapan
  // Field 3: Gelap (1) / Terang (0)
  // Field 4: Gerakan terdeteksi (1/0)

  ThingSpeak.setField(1, suhu);
  ThingSpeak.setField(2, kelembapan);
  ThingSpeak.setField(3, gelap ? 1 : 0);
  ThingSpeak.setField(
    4, gerakanTerdeteksi ? 1 : 0
  );

  int status = ThingSpeak.writeFields(
    CHANNEL_ID,
    WRITE_API_KEY
  );

  if (status == 200) {

    Serial.println();
    Serial.println(
      "[ThingSpeak] BERHASIL mengirim data!"
    );

    gerakanTerdeteksi = false;

  } else {

    Serial.print("[ThingSpeak] GAGAL. Kode: ");
    Serial.println(status);
  }
}

// ========================================
// SETUP
// ========================================

void setup() {

  Serial.begin(115200);
  delay(300);

  pinMode(LDR_PIN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);

  dht.begin();

  Serial.println();
  Serial.println("========================================");
  Serial.println(" SISTEM MONITORING RUANG BELAJAR");
  Serial.println(" ESP32 + DHT22 + LDR DO + PIR");
  Serial.println("========================================");

  Serial.println("DHT22 : GPIO25");
  Serial.println("LDR   : GPIO26");
  Serial.println("PIR   : GPIO27");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println();
  Serial.println("Menghubungkan ke WiFi...");

  ThingSpeak.begin(client);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  waktuBaca = millis();
  waktuKirim = millis();
  waktuWiFi = millis();
}

// ========================================
// LOOP UTAMA
// ========================================

void loop() {

  unsigned long sekarang = millis();

  server.handleClient();

  // Pantau gerakan sepanjang interval
  if (digitalRead(PIR_PIN) == HIGH) {
    gerakanTerdeteksi = true;
    waktuGerakanTerakhir = sekarang == 0 ? 1 : sekarang;
  }

  // -------- CEK WIFI --------

  bool wifiTerhubung =
    WiFi.status() == WL_CONNECTED;

  if (wifiTerhubung && !wifiSebelumnya) {
    tampilkanInfoWiFi();
  }

  if (!wifiTerhubung && wifiSebelumnya) {
    Serial.println("[WiFi] Koneksi terputus!");
  }

  wifiSebelumnya = wifiTerhubung;

  // -------- RECONNECT WIFI --------

  if (!wifiTerhubung &&
      sekarang - waktuWiFi >= INTERVAL_WIFI) {

    waktuWiFi = sekarang;

    Serial.println(
      "[WiFi] Mencoba menghubungkan ulang..."
    );

    WiFi.reconnect();
  }

  // -------- BACA SENSOR --------

  if (sekarang - waktuBaca >= INTERVAL_BACA) {

    waktuBaca = sekarang;
    bacaSensor();
  }

  // -------- KIRIM THINGSPEAK --------

  if (sekarang - waktuKirim >= INTERVAL_KIRIM) {

    waktuKirim = sekarang;
    kirimThingSpeak();
  }

  delay(40);
}
