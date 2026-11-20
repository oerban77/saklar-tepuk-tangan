// Saklar Tepuk Tangan — ESP32 + Sensor Suara + Relay
//
// Cara kerja:
//  - Sensor suara (modul KY-038 / sejenis, keluaran digital DO) mendengar
//    tepukan tangan. Ambang keras suara diatur lewat potensiometer di modul.
//  - Tepuk DUA KALI dalam jendela 800 ms -> lampu berbalik status
//    (menyala jadi mati, mati jadi menyala). Tepuk TUNGGAL diabaikan,
//    jadi suara lain yang sekali bunyi (batuk, pintu tertutup) tidak
//    menyalakan lampu.
//  - LED hijau ikut status lampu (menyala = lampu menyala).
//  - Setiap lampu berubah status, notifikasi Telegram terkirim
//    (maksimal 1 pesan per 5 detik).
//
// Wiring:
//  - Sensor suara : DO -> GPIO27, VCC -> 3V3, GND -> GND
//    (pin AO modul tidak dipakai; ambang diatur dari potensiometer modul)
//  - Relay lampu  : IN -> GPIO26, VCC -> VIN 5V, GND -> GND
//    Kontak relay (COM - NO) memutus kabel FASA lampu 220 V.
//    PERINGATAN: 220 V berbahaya — matikan listrik (MCB) sebelum memasang
//    kabel lampu, dan jangan sentuh terminal relay saat listrik menyala.
//  - LED hijau    : GPIO32 (seri resistor 220 ohm ke GND)
//
// Library yang dibutuhkan (semua tersedia di Library Manager):
//  - UniversalTelegramBot 1.3.0 (hanya bila USE_TELEGRAM 1)
//  - ArduinoJson 6.21.5 (dipakai UniversalTelegramBot)
// Compile: arduino-cli compile --fqbn esp32:esp32:esp32 SaklarTepukTangan
// (terverifikasi 20 Nov 2026: 1065049 byte (81%) flash, 50536 byte (15%) RAM,
//  ESP32 core 4.0.0)

#include <Arduino.h>

// ---------- KONFIGURASI ----------
#define USE_TELEGRAM 1  // 1 = kirim notifikasi Telegram, 0 = tanpa Telegram

// Pin
const int PIN_SENSOR_SUARA = 27;  // DO sensor suara (digital)
const int PIN_RELAY = 26;         // IN modul relay lampu
const int PIN_LED_HIJAU = 32;     // LED hijau, ikut status lampu

// Logika level (sesuaikan dengan modulmu bila terbalik)
const int SENSOR_AKTIF = HIGH;  // DO sensor = HIGH saat suara terdeteksi
const int RELAY_AKTIF = HIGH;   // IN relay = HIGH saat relay menutup kontak
                                // (modul relay aktif LOW: ubah jadi LOW)

// Waktu (milidetik)
const unsigned long JENDELA_TEPUK_GANDA_MS = 800;  // tepuk ke-2 harus masuk jendela ini
const unsigned long DEBOUNCE_TEPuk_MS = 150;       // jeda minimum antar deteksi tepuk
const unsigned long TELEGRAM_DEBOUNCE_MS = 5000;   // jeda minimum antar pesan Telegram
const unsigned long WIFI_TIMEOUT_MS = 20000;       // batas tunggu sambung WiFi

const char *WIFI_SSID = "NAMA_WIFI";
const char *WIFI_PASS = "PASSWORD_WIFI";
const char *BOT_TOKEN = "TOKEN_BOT_TELEGRAM";
const char *CHAT_ID = "CHAT_ID_TELEGRAM";

// ---------- STATE ----------
bool lampuNyala = false;             // status lampu saat ini
int jumlahTepuk = 0;                 // 0 = belum ada, 1 = menunggu tepuk ke-2
unsigned long waktuTepukPertama = 0; // millis() saat tepuk pertama
unsigned long waktuTepukTerakhir = 0; // millis() saat tepuk terakhir (debounce)
unsigned long telegramTerakhir = 0;  // millis() saat pesan Telegram terakhir
int sensorSebelumnya = LOW;          // pembacaan sensor loop sebelumnya

#if USE_TELEGRAM
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

WiFiClientSecure telegramClient;
UniversalTelegramBot bot(BOT_TOKEN, telegramClient);

// Kirim pesan Telegram; diam bila WiFi putus atau masih dalam debounce.
void kirimTelegram(const String &pesan) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - telegramTerakhir < TELEGRAM_DEBOUNCE_MS) return;
  telegramTerakhir = millis();
  bot.sendMessage(CHAT_ID, pesan, "");
}
#else
void kirimTelegram(const String &) {}
#endif

// Sambung ke WiFi (maksimal WIFI_TIMEOUT_MS), lalu berhenti mencoba.
void sambungWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long mulai = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - mulai < WIFI_TIMEOUT_MS) {
    delay(250);
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "WiFi tersambung."
                                                : "WiFi gagal — jalan tanpa Telegram.");
}

// Atur relay + LED hijau mengikuti status lampu, lalu beri tahu Telegram.
void kendaliLampu(bool nyala) {
  lampuNyala = nyala;
  digitalWrite(PIN_RELAY, nyala ? RELAY_AKTIF : !RELAY_AKTIF);
  digitalWrite(PIN_LED_HIJAU, nyala ? HIGH : LOW);
  Serial.println(nyala ? "Lampu MENYALA (tepuk 2x)." : "Lampu MATI (tepuk 2x).");
  kirimTelegram(nyala ? "Lampu menyala — tepuk tangan 2x terdeteksi."
                      : "Lampu mati — tepuk tangan 2x terdeteksi.");
}

// Deteksi tepuk ganda. Dipanggil tiap loop; mengembalikan true tepat saat
// tepuk kedua masuk jendela. Tepuk tunggal kedaluwarsa sendiri dan diabaikan.
bool deteksiTepuk() {
  int sensor = digitalRead(PIN_SENSOR_SUARA);
  unsigned long sekarang = millis();

  // Tepuk pertama kedaluwarsa: tepuk kedua tidak datang dalam jendela.
  if (jumlahTepuk == 1 &&
      sekarang - waktuTepukPertama > JENDELA_TEPUK_GANDA_MS) {
    jumlahTepuk = 0;
    Serial.println("Tepuk tunggal diabaikan.");
  }

  // Hanya proses sisi naik (LOW -> SENSOR_AKTIF) dan lewati debounce.
  bool sisiNaik = (sensor == SENSOR_AKTIF && sensorSebelumnya != SENSOR_AKTIF);
  sensorSebelumnya = sensor;
  if (!sisiNaik) return false;
  if (sekarang - waktuTepukTerakhir < DEBOUNCE_TEPuk_MS) return false;
  waktuTepukTerakhir = sekarang;

  if (jumlahTepuk == 0) {
    // Tepuk pertama: mulai hitung jendela.
    jumlahTepuk = 1;
    waktuTepukPertama = sekarang;
    return false;
  }

  // Tepuk kedua masih di dalam jendela (kedaluwarsa sudah dicek di atas).
  jumlahTepuk = 0;
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SENSOR_SUARA, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  kendaliLampu(false);  // mulai dari lampu mati

#if USE_TELEGRAM
  sambungWiFi();
  telegramClient.setInsecure();
#endif

  Serial.println("Saklar tepuk tangan siap. Tepuk 2x untuk nyala/mati.");
}

void loop() {
  if (deteksiTepuk()) {
    kendaliLampu(!lampuNyala);
  }
}
