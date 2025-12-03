#include <WiFi.h>
#include <Firebase_ESP_Client.h>

const char* ssid = "Halo";      // Ganti dengan Nama Wi-Fi Anda (SSID)
const char* password = "12345908";  // Ganti dengan Password Wi-Fi Anda

// --- Kredensial Firebase ---
// Catatan: Pastikan Anda telah mengaktifkan 'Email/Password' di Firebase Authentication
#define API_KEY "AIzaSyAge_ZYN3uFaRG5qsfPbNYQ0pTAHEWfXq4" // Ganti dengan Project API Key Anda (Ditemukan di Project Settings > General)
#define DATABASE_URL "https://latm11-default-rtdb.asia-southeast1.firebasedatabase.app/" // Ganti dengan URL Realtime Database Anda (Contoh: https://nama-projek-default-rtdb.asia-east1.firebasedatabase.app)
#define USER_EMAIL "kenny27@gmail.com" // Ganti dengan email akun yang terdaftar di Firebase Auth
#define USER_PASSWORD "kenny123"           // Ganti dengan password akun yang terdaftar di Firebase Auth

// --- Definisi Pin Sensor (Contoh) ---
// Ini menentukan pin GPIO mana yang digunakan oleh sensor di mikrokontroler Anda (misalnya ESP32)
#define dht 23 
#define ldr 19  
#define soil 18



void setup() {
  // 1. Inisialisasi Komunikasi Serial
  Serial.begin(115200); // Memulai komunikasi serial pada baud rate 115200
  delay(100);
  Serial.println("\n=== SMART PLANT GREENHOUSE ===");
  Serial.println("Inisialisasi sistem...\n");

  // 2. Pengaturan Pin (Pin modes)
  // Catatan: Pastikan Anda telah mendefinisikan LDR_PIN, SOIL_PIN, dll. di bagian awal kode (seperti di gambar sebelumnya).
  pinMode(LDR_PIN, INPUT);      // Pin untuk sensor LDR (cahaya) sebagai INPUT
  pinMode(SOIL_PIN, INPUT);     // Pin untuk sensor Kelembaban Tanah sebagai INPUT
  pinMode(PIR_PIN, INPUT);      // Pin untuk sensor PIR (gerakan) sebagai INPUT
  pinMode(FLAME_PIN, INPUT);    // Pin untuk sensor Api sebagai INPUT
  pinMode(OBJECT_PIN, INPUT);   // Pin untuk sensor Objek/Proximity (ganti nama pin sesuai kebutuhan) sebagai INPUT

  // 3. Koneksi WiFi
  // Fungsi connectWiFi() adalah fungsi terpisah yang harus Anda buat
  // untuk menginisialisasi dan menghubungkan ke jaringan WiFi menggunakan SSID/Password.
  Serial.print("Menghubungkan ke WiFi...");
  connectWiFi();

  // 4. Pengaturan Waktu NTP (Network Time Protocol)
  // configTime() digunakan untuk mengatur zona waktu agar perangkat memiliki waktu yang akurat.
  // Anda harus mendefinisikan variabel gmtOffset_sec, daylightOffset_sec, dan ntpServer
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sinkronisasi waktu dengan NTP...");
  delay(2000);

  // 5. Konfigurasi Firebase
  // Menggunakan kredensial dari define/const char di bagian atas kode.
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Set otentikasi pengguna
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Menetapkan fungsi callback untuk memantau status token otentikasi Firebase
  // Anda harus membuat fungsi tokenStatusCallback(FB_AuthResult result)
  config.token_status_callback = tokenStatusCallback;

  Serial.println("Menghubungkan ke Firebase...");
  
  // Memulai koneksi Firebase
  Firebase.begin(&config, &auth);

  // Mengizinkan Firebase untuk mencoba menyambungkan kembali WiFi jika terputus
  Firebase.reconnectWiFi(true);

  // 6. Loop Tunggu Koneksi Firebase (Maksimal 10 detik)
  unsigned long fbStart = millis(); // Mencatat waktu mulai
  // Loop akan berjalan selama Firebase belum siap DAN waktu belum mencapai 10 detik
  while (!Firebase.ready() && (millis() - fbStart < 10000)) {
    Serial.print(".");
    delay(500);
  }

  // 7. Cek Hasil Koneksi
  if (Firebase.ready()) {
    Serial.println("\n✓ Firebase terhubung!");
    Serial.println("✓ Sistem siap monitoring!\n");
  } else {
    // Jika gagal terhubung ke Firebase setelah 10 detik
    Serial.println("\n✕ Firebase gagal terhubung, sistem tetap berjalan namun tanpa koneksi data.");
  }
}

void loop() {
  // 1. Cek koneksi WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus! Mencoba reconnect...");
    // Panggil kembali fungsi koneksi WiFi untuk menyambung ulang
    connectWiFi();
  }

  // 2. Update sensor secara berkala
  unsigned long now = millis();
  // Cek apakah sudah waktunya untuk update sensor (lebih besar dari sensorInterval)
  if (now - lastSensorUpdate > sensorInterval) {
    lastSensorUpdate = now; // Reset waktu update terakhir
    // Fungsi untuk membaca data dari sensor dan mengirimkannya ke Firebase
    bacaDanKirimData();
  }

  // Anda dapat menambahkan delay() singkat di sini jika perlu
  // delay(10);
}

// --- Fungsi koneksi WiFi ---
void connectWiFi() {
  // Pastikan Anda telah mendefinisikan WIFI_SSID dan WIFI_PASSWORD di awal kode.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan ke WiFi");

  unsigned long start = millis(); // Mencatat waktu mulai koneksi

  // Loop menunggu koneksi terhubung (WL_CONNECTED)
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    // Timeout: Jika waktu tunggu lebih dari 20 detik, sistem akan me-restart ESP
    if ((millis() - start) > 20000) { 
      Serial.println("\n✕ Gagal terhubung WiFi - restart...");
      ESP.restart(); // Restart mikrokontroler
    }
  }
  
  // Jika berhasil terhubung
  Serial.println();
  Serial.println("✓ WiFi Terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

unsigned long getTimestamp() {
  time_t now;
  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
    Serial.println("⚠ Gagal mendapat waktu NTP, gunakan millis()");
    return millis(); 
  }

  time(&now);
  // Konversi ke milidetik untuk format JavaScript
  return (unsigned long) now * 1000; 
}

// Fungsi untuk membaca sensor dan kirim ke Firebase
void bacaDanKirimData() {
  Serial.println("\n----------------------------------");
  Serial.println("|     PEMBACAAN SENSOR GREENHOUSE  |");
  Serial.println("----------------------------------");

  // === BACA LDR (Cahaya) ===
  int rawLdr = analogRead(LDR_PIN);
  
  // Mapping: LDR (4095=Gelap, 0=Terang) --> Persen (0=Gelap, 100=Terang)
  int lightLevel = map(rawLdr, 4095, 0, 0, 100); 
  lightLevel = constrain(lightLevel, 0, 100); 

  Serial.printf("💡 Cahaya: %d %% (ADC=%d)\n", lightLevel, rawLdr);

  // === BACA SOIL MOISTURE (Kelembaban Tanah) ===
  int rawSoil = analogRead(SOIL_PIN);
  
  // Mapping: Soil (4095=Kering, 0=Basah) --> Persen (0=Kering, 100=Basah)
  int soilPercent = map(rawSoil, 4095, 0, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100); 

  Serial.printf("💧 Kelembaban Tanah: %d %% (ADC=%d)\n", soilPercent, rawSoil);
  
  if (soilPercent < 40) {
    Serial.println("⚠ STATUS: KERING - Perlu penyiraman!");
  } else {
    Serial.println("✓ STATUS: Kelembaban cukup!");
  }

motionDetected = digitalRead(PIR_PIN) == HIGH;
flameDetected = digitalRead(FLAME_PIN) == HIGH;
objectDetected = digitalRead(OBJECT_PIN) == HIGH;

Serial.printf("🏃 Gerakan (PIR): %s\\n", motionDetected ? "TERDETEKSI ⚠" : "Tidak ada");
Serial.printf("🔥 Api: %s\\n", flameDetected ? "TERDETEKSI 🚨" : "Aman");
Serial.printf("📦 Objek: %s\\n", objectDetected ? "TERDETEKSI" : "Tidak ada");

// === KIRIM KE FIREBASE ===
if (Firebase.ready()) {
  Serial.println("🚀 Mengirim data ke Firebase...");
  String basePath = "/greenhouse/sensors";
  bool allSuccess = true;

  // Kirim Light Level
  if (Firebase.RTDB.setInt(&fbdo, basePath + "/lightLevel", lightLevel)) {
    Serial.println("✓ lightLevel terkirim");
  } else {
    Serial.printf("✕ lightLevel gagal: %s\\n", fbdo.errorReason().c_str());
    allSuccess = false;
  }

  // Kirim Soil Moisture
  if (Firebase.RTDB.setInt(&fbdo, basePath + "/soilMoisture", soilPercent)) {
    Serial.println("✓ soilMoisture terkirim");
  } else {
    Serial.printf("✕ soilMoisture gagal: %s\\n", fbdo.errorReason().c_str());
    allSuccess = false;
  }

  // Kirim Motion (PIR)
  if (Firebase.RTDB.setBool(&fbdo, basePath + "/motion", motionDetected)) {
    Serial.println("✓ motion terkirim");
  } else {
    Serial.printf("✕ motion gagal: %s\\n", fbdo.errorReason().c_str());
    allSuccess = false;
  }
}

// Kirim Flame
if (Firebase.RTDB.setBool(&fbdo, basePath + "/flame", flameDetected)) {
  Serial.println("✓ flame terkirim");
} else {
  Serial.printf("✕ flame gagal: %s\\n", fbdo.errorReason().c_str());
  allSuccess = false;
}

// Kirim Object
if (Firebase.RTDB.setBool(&fbdo, basePath + "/object", objectDetected)) {
  Serial.println("✓ object terkirim");
} else {
  Serial.printf("✕ object gagal: %s\\n", fbdo.errorReason().c_str());
  allSuccess = false;
}

// Kirim Timestamp (epoch milliseconds untuk JavaScript Date)
unsigned long timestamp = getTimestamp();
if (Firebase.RTDB.setDouble(&fbdo, basePath + "/timestamp", timestamp)) {
  Serial.printf("✓ timestamp terkirim (%lu)\\n", timestamp);
} else {
  Serial.printf("✕ timestamp gagal: %s\\n", fbdo.errorReason().c_str());
  allSuccess = false;
}

if (allSuccess) {
  Serial.println("\n✅ Semua data berhasil dikirim!");
} else {
  Serial.println("\n⚠️ Beberapa data gagal dikirim");
}

} else {
  Serial.println("\n⚠️ Firebase belum siap, skip pengiriman");
}

Serial.println("-----------------------------------------\\n");
// Delay kecil untuk stabilitas
delay(100);