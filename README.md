# Saklar Tepuk Tangan — ESP32 + Sensor Suara + Relay

Lampu kamar menyala dan mati cuma dari tepukan tangan — tanpa sentuh
saklar sama sekali. Sensor suara mendengar tepukan, ESP32 yang
memutuskan, relay yang menyalakan lampu.

Tepuk DUA KALI dalam 800 ms → lampu berbalik status (menyala jadi mati,
mati jadi menyala). Tepuk TUNGGAL diabaikan — jadi suara sekali bunyi
seperti batuk atau pintu tertutup tidak menyalakan lampu. LED hijau ikut
status lampu, dan setiap lampu berubah status, notifikasi Telegram
terkirim (maksimal 1 pesan per 5 detik).

Video penjelasan (60 detik):
https://www.youtube.com/shorts/RhxapjbZe6k

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| Sensor suara KY-038 (modul) | Keluaran digital DO; ambang keras suara diatur lewat potensiometer di modul |
| Modul relay 5V | Kontak COM–NO memutus kabel FASA lampu 220 V |
| Lampu 220 V | Beban yang dikendalikan relay |
| LED hijau | Menyala saat lampu menyala (seri 220 Ω ke GND) |

## Wiring

| Dari | Ke |
|---|---|
| Sensor suara DO | ESP32 GPIO27 |
| Sensor suara VCC | ESP32 3V3 |
| Sensor suara GND | GND (digabung) |
| Relay IN | ESP32 GPIO26 |
| Relay VCC | ESP32 VIN 5V |
| Relay GND | GND (digabung) |
| Relay COM | Kabel FASA lampu (sisi sumber) |
| Relay NO | Kabel FASA lampu (sisi lampu) |
| LED hijau | ESP32 GPIO32 (seri 220 Ω ke GND) |

Skema lengkap: [schematic.png](schematic.png)

> **PERINGATAN — 220 V berbahaya.** Kontak relay memutus kabel FASA
> lampu 220 V. Matikan listrik (MCB) sebelum memasang kabel lampu, dan
> jangan sentuh terminal relay saat listrik menyala.

## Cara kerja

1. ESP32 membaca DO sensor suara tiap loop; sisi naik (LOW → HIGH)
   dihitung sebagai satu tepukan, dengan debounce 150 ms.
2. Tepuk pertama membuka jendela 800 ms. Tepuk kedua di dalam jendela
   → lampu berbalik status; kalau tidak ada tepuk kedua, tepuk pertama
   kedaluwarsa dan diabaikan.
3. `kendaliLampu()` mengatur relay + LED hijau mengikuti status lampu,
   lalu mengirim notifikasi Telegram (bila `USE_TELEGRAM` 1 dan WiFi
   tersambung, maksimal 1 pesan per 5 detik).
4. Kalau WiFi gagal tersambung dalam 20 detik, alat tetap jalan tanpa
   Telegram.

## Konfigurasi (di awal sketch)

- `USE_TELEGRAM` (1) — kirim notifikasi Telegram. Set 0 untuk tanpa
  Telegram (dan tanpa WiFi).
- `WIFI_SSID` / `WIFI_PASS` — WiFi rumahmu.
- `BOT_TOKEN` / `CHAT_ID` — token bot dan chat ID Telegram.
- `JENDELA_TEPUK_GANDA_MS` (800) — tepuk ke-2 harus masuk jendela ini.
- `DEBOUNCE_TEPUK_MS` (150) — jeda minimum antar deteksi tepuk.
- `TELEGRAM_DEBOUNCE_MS` (5000) — jeda minimum antar pesan Telegram.
- `SENSOR_AKTIF` (HIGH) — level DO saat suara terdeteksi; ubah ke LOW
  bila modulmu terbalik.
- `RELAY_AKTIF` (HIGH) — level IN saat relay menutup kontak; ubah ke
  LOW bila modul relaymu aktif LOW.

## Library yang dibutuhkan

| Library | Versi | Keterangan |
|---|---|---|
| UniversalTelegramBot | 1.3.0 | Notifikasi Telegram (hanya bila `USE_TELEGRAM` 1) |
| ArduinoJson | 6.21.5 | Dipakai UniversalTelegramBot (WAJIB v6 — v7 tidak dipakai di sini) |

Semua library di atas juga tersedia sebagai ZIP di folder [`libraries/`](libraries/)
— install lewat Arduino IDE: Sketch → Include Library → Add .ZIP Library.

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 SaklarTepukTangan
```

Terverifikasi 20 Nov 2026: sketch 1.065.049 byte (81% flash),
RAM 50.536 byte (15%), ESP32 core 4.0.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).
