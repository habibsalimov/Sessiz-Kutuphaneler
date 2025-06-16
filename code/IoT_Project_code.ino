#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// WiFi credentials - update these with your network details
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Telegram bot credentials
#define BOT_TOKEN "XXX-XXX-XXX"
#define CHAT_ID "XXX-XXX-XXX"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

bool wifiConnected = false;
unsigned long lastTelegramMessage = 0;
const unsigned long TELEGRAM_INTERVAL = 30000; // Send message every 30 seconds when in loud environment

#define MIC_PIN 34
#define THRESHOLD 70

#define GREEN_LED1 12
#define GREEN_LED2 13
#define YELLOW_LED 27
#define ORANGE_LED 25
#define RED_LED 32

#define BUZZER_PIN 26

int baseline = 1430;
int micValues[10];
int micIndex = 0;
int highCount = 0;

unsigned long lastBuzzerTime = 0;
const unsigned long BUZZER_DURATION = 1000;
bool buzzerActive = false;

enum SoundRegion {
  REGION_QUIET,
  REGION_NORMAL,
  REGION_LOUD,
  REGION_VERY_LOUD,
  REGION_UNKNOWN
};

SoundRegion lastRegion = REGION_UNKNOWN;

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED1, OUTPUT);
  pinMode(GREEN_LED2, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(ORANGE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(GREEN_LED1, LOW);
  digitalWrite(GREEN_LED2, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(ORANGE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // WiFi connection attempt
  WiFi.begin(ssid, password);
  Serial.print("WiFi'ye bağlanıyor");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println();
    Serial.println("WiFi bağlandı!");
    Serial.print("IP adresi: ");
    Serial.println(WiFi.localIP());
    
    // Configure Telegram SSL client
    client.setInsecure();
    
    // Send initial message
    bot.sendMessage(CHAT_ID, "Gürültü Ölçer ESP32 başlatıldı ve WiFi'ye bağlandı!", "");
  } else {
    wifiConnected = false;
    Serial.println();
    Serial.println("WiFi bağlantısı başarısız! Sistem durduruluyor...");
    Serial.println("Lütfen WiFi ayarlarını kontrol edin ve cihazı yeniden başlatın.");
    
    // Tüm LED'leri yakıp söndürerek hata göster
    while (true) {
      digitalWrite(RED_LED, HIGH);
      delay(500);
      digitalWrite(RED_LED, LOW);
      delay(500);
    }
  }
}

void sendTelegramMessage(String message) {
  if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
    return; // Don't try to send if not connected
  }
  
  unsigned long currentTime = millis();
  if (currentTime - lastTelegramMessage > TELEGRAM_INTERVAL) {
    if (bot.sendMessage(CHAT_ID, message, "")) {
      Serial.println("Telegram mesajı gönderildi: " + message);
    } else {
      Serial.println("Telegram mesajı gönderilemedi!");
    }
    lastTelegramMessage = currentTime;
  }
}

void controlLEDs(int soundLevelDB) {
  digitalWrite(GREEN_LED1, LOW);
  digitalWrite(GREEN_LED2, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(ORANGE_LED, LOW);
  digitalWrite(RED_LED, LOW);

  if (soundLevelDB < 40) {
    digitalWrite(GREEN_LED1, HIGH);
    digitalWrite(GREEN_LED2, HIGH);
  } else if (soundLevelDB < 70) {
    digitalWrite(YELLOW_LED, HIGH);
  } else if (soundLevelDB > 70 && soundLevelDB <= 75) {
    digitalWrite(ORANGE_LED, HIGH);
  } else {
    digitalWrite(RED_LED, HIGH);
    
    // Buzzer'ı başlat (sadece henüz aktif değilse)
    if (!buzzerActive) {
      buzzerActive = true;
      lastBuzzerTime = millis();
      digitalWrite(BUZZER_PIN, HIGH);
    }
  }
}

void updateBuzzer() {
  if (buzzerActive) {
    unsigned long currentTime = millis();
    if (currentTime - lastBuzzerTime >= BUZZER_DURATION) {
      digitalWrite(BUZZER_PIN, LOW);
      buzzerActive = false;
    }
  }
}

void loop() {
  // WiFi bağlantısını sürekli kontrol et
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi bağlantısı kesildi! Sistem beklemeye alınıyor...");
    wifiConnected = false;
    
    // Tüm LED'leri kapat
    digitalWrite(GREEN_LED1, LOW);
    digitalWrite(GREEN_LED2, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(ORANGE_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false; // Buzzer durumunu sıfırla
    
    // WiFi tekrar bağlanana kadar bekle
    while (WiFi.status() != WL_CONNECTED) {
      // Kırmızı LED ile hata göster
      digitalWrite(RED_LED, HIGH);
      delay(500);
      digitalWrite(RED_LED, LOW);
      delay(500);
      
      Serial.print(".");
    }
    
    // WiFi tekrar bağlandı
    wifiConnected = true;
    digitalWrite(RED_LED, LOW); // Kırmızı LED'i kapat
    Serial.println();
    Serial.println("WiFi tekrar bağlandı! Sistem çalışmaya devam ediyor...");
    
    // Telegram'a yeniden bağlantı mesajı gönder
    bot.sendMessage(CHAT_ID, "WiFi bağlantısı yeniden kuruldu. Gürültü ölçer sistemi çalışmaya devam ediyor.", "");
    
    return; // Bu döngüyü atla, bir sonraki döngüde normal çalışmaya devam et
  }
  
  // Buzzer durumunu sürekli kontrol et
  updateBuzzer();
  
  int micValue = analogRead(MIC_PIN);
  int soundLevel = abs(micValue - baseline);
  int soundLevelDB = map(soundLevel, 0, 1200, 30, 100);
  soundLevelDB = constrain(soundLevelDB, 30, 100);

  micValues[micIndex] = soundLevelDB;
  micIndex = (micIndex + 1) % 10;

  highCount = 0;
  for (int i = 0; i < 10; i++) {
    if (micValues[i] > THRESHOLD) {
      highCount++;
    }
  }

  String statusMessage = "";
  if (highCount >= 3) {
    if (soundLevelDB < 40) {
      statusMessage = "Sessiz ortam veya fısıltı";
      Serial.println(statusMessage);
    } else if (soundLevelDB < 70) {
      statusMessage = "Normal konuşma sesi";
      Serial.println(statusMessage);
    } else {
      statusMessage = "GÜRÜLTÜLÜ ORTAM UYARISI! Ses seviyesi: " + String(soundLevelDB) + " dB";
      Serial.println("Gürültülü ortam");
    }
  }

  SoundRegion currentRegion;
  if (soundLevelDB < 40) {
    currentRegion = REGION_QUIET;
  } else if (soundLevelDB < 70) {
    currentRegion = REGION_NORMAL;
  } else if (soundLevelDB > 70 && soundLevelDB <= 75) {
    currentRegion = REGION_LOUD;
  } else {
    currentRegion = REGION_VERY_LOUD;
  }

  if (currentRegion != lastRegion) {
    lastRegion = currentRegion;
    controlLEDs(soundLevelDB);
    
    // Send Telegram message for loud environments
    if ((currentRegion == REGION_LOUD || currentRegion == REGION_VERY_LOUD) && !statusMessage.isEmpty()) {
      sendTelegramMessage(statusMessage);
    }
  }

  Serial.print(" | Sound Level (dB): ");
  Serial.println(soundLevelDB);

  delay(100);
}
