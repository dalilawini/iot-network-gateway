#include "EspNowManager.h"

const char* ap_ssid = "ESP-NOW-GATEWAY";
const char* password = "12345678";
EspNowManager *EspNowManager::instance = nullptr;

uint8_t EspNowManager::json[MAX_JSON_SIZE];
uint8_t EspNowManager::dataLen = 0;
bool EspNowManager::dataReady = false;
uint8_t EspNowManager::lastMac[6] = {0};
int8_t EspNowManager::lastRssi = 0;
volatile uint8_t EspNowManager::sendResult = EspNowManager::SEND_NONE;

EspNowManager::EspNowManager(int buttonPin) {
  _buttonPin = buttonPin;
  instance = this; 
}

void EspNowManager::begin() {

  Serial.begin(115200);

  pinMode(_buttonPin, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(LED_PIN, HIGH);     // Start with LED OFF (active LOW)

  startEspNow();
}

void EspNowManager::update(SensorData& sensorData) {

  if (pairRequested && !apMode) {
    pairRequested = false;
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
    startEspNow();                   // nobody joined: go back to receiving ESP-NOW
  }

    sensorData.setSource(lastMac, lastRssi);
    sensorData.setData(json, dataLen,dataReady);
    dataReady = false;
}

void EspNowManager::startAP() {

  Serial.println("Starting AP mode");
  //FULL WiFi RESET 
  WiFi.disconnect(true, true);   // Disconnect + erase config
  delay(1000);

  WiFi.mode(WIFI_OFF);           // Turn off WiFi completely
  delay(500);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, password);

  WiFi.onEvent(onWiFiEvent);
  apMode = true;
  apStartTime = millis();
}

void EspNowManager::startEspNow() {

  Serial.println("Switching to ESP-NOW");
  apMode = false;

  //FULL WiFi RESET 
  WiFi.disconnect(true, true);   // Disconnect + erase config
  delay(1000);

  WiFi.mode(WIFI_OFF);           // Turn off WiFi completely
  delay(500);
  WiFi.mode(WIFI_STA);
  delay(500);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  delay(500);

  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
}
void EspNowManager::onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {

  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    Serial.printf("Station connected: %02X:%02X:%02X:%02X:%02X:%02X\n",
      info.wifi_ap_staconnected.mac[0],
      info.wifi_ap_staconnected.mac[1],
      info.wifi_ap_staconnected.mac[2],
      info.wifi_ap_staconnected.mac[3],
      info.wifi_ap_staconnected.mac[4],
      info.wifi_ap_staconnected.mac[5]
    );
    digitalWrite(16, LOW);     // Start with LED OFF (active LOW)

    EspNowManager::instance->startEspNow();
  }

}

void EspNowManager::onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;

  Serial.printf("Received from: %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    memcpy(lastMac, mac, 6);
    lastRssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;

    if (len < 0) return;
    if (len > MAX_JSON_SIZE - 1) {
      Serial.printf("Packet too long (%d bytes), truncated to %d\n", len, MAX_JSON_SIZE - 1);
      len = MAX_JSON_SIZE - 1;
    }

    memcpy(json, data, len);
    json[len] = '\0';
  
     dataLen = len;
     dataReady = true;                        // flag main loop to parse
     Serial.println((char*)json);

}

bool EspNowManager::sendJson(const uint8_t mac[6], const char* json) {
  if (apMode || pairRequested) return false;      // ESP-NOW is off while pairing

  // Peers are lost whenever startEspNow() re-initialises WiFi, so add on demand
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;                             // current channel
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
      Serial.println("ESP-NOW add peer failed");
      return false;
    }
  }

  esp_now_register_send_cb(onDataSent);
  sendResult = SEND_PENDING;
  esp_err_t err = esp_now_send(mac, (const uint8_t*)json, strlen(json));
  if (err != ESP_OK) {
    sendResult = SEND_FAILED;
    Serial.printf("ESP-NOW send failed: %d\n", err);
    return false;
  }
  Serial.printf("Sent: %s\n", json);
  return true;
}

void EspNowManager::onDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  (void)info;
  sendResult = (status == ESP_NOW_SEND_SUCCESS) ? SEND_OK : SEND_FAILED;
}

uint32_t EspNowManager::pairingTimeLeft() const {
  if (!apMode) return pairRequested ? apDuration : 0;
  unsigned long elapsed = millis() - apStartTime;
  return elapsed >= apDuration ? 0 : apDuration - elapsed;
}

void EspNowManager::toggleLed(){
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= INTERVAL) {
      previousMillis = currentMillis;       // Update last toggle time
      ledState = !ledState;                 // Toggle state
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);  // Active LOW LED
    }
}