#include "EspNowManager.h"

const char* ap_ssid = "ESP-NOW-GATEWAY";
const char* password = "12345678";

EspNowManager::EspNowManager(int buttonPin) {
  _buttonPin = buttonPin;
}

void EspNowManager::begin() {

  Serial.begin(115200);

  pinMode(_buttonPin, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(LED_PIN, HIGH);     // Start with LED OFF (active LOW)

  WiFi.mode(WIFI_OFF);
}

void EspNowManager::update() {

  if (digitalRead(_buttonPin) == LOW && !apMode) {
    delay(300);
    startAP();
  }

  if(apMode){
    toggleLed();
  }

  if (apMode && millis() - apStartTime > apDuration) {

    Serial.println("AP timeout");
    digitalWrite(LED_PIN, HIGH);     // Start with LED OFF (active LOW)

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    apMode = false;
  }
}

void EspNowManager::startAP() {

  Serial.println("Starting AP mode");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, password);

  WiFi.onSoftAPModeStationConnected(onStationConnected);

  apMode = true;
  apStartTime = millis();
}

void EspNowManager::startEspNow() {

  Serial.println("Switching to ESP-NOW");

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);
}

void EspNowManager::onStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {

  Serial.println("Device connected");

  Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
  evt.mac[0], evt.mac[1], evt.mac[2],
  evt.mac[3], evt.mac[4], evt.mac[5]);
  digitalWrite(16, LOW);     // Start with LED OFF (active LOW)

  EspNowManager manager(0);   // temporary object to call method
  manager.startEspNow();
}

void EspNowManager::onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {

  Serial.printf("Data from %02X:%02X:%02X:%02X:%02X:%02X\n",
  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void EspNowManager::toggleLed(){
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= INTERVAL) {
      previousMillis = currentMillis;       // Update last toggle time
      ledState = !ledState;                 // Toggle state
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);  // Active LOW LED
    }
}