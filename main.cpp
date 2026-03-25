/**
 * Speech → Haptic Controller
 * FireBeetle ESP32-S3  |  NimBLE Nordic UART Service (NUS)
 *
 * Wiring:
 *   Vibration motor driver (MOSFET or DRV2605):
 *     MOTOR_PIN → GPIO 4  (PWM-capable, adjust if needed)
 *   Onboard trigger LED: GPIO 21 (D13)
 *   BLE status LED:      GPIO 2
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

// ── Feature toggles ───────────────────────────────────────────────────────────
static constexpr bool ENABLE_MOTOR = true;   // vibration motor PWM output
static constexpr bool ENABLE_LED   = true;   // onboard LED (GPIO21) mirrors motor
static constexpr bool ENABLE_BLE   = true;   // BLE stack, advertising & receive
static constexpr bool ENABLE_ACK   = true;   // notify browser when pattern finishes

// ── Pin & PWM config ──────────────────────────────────────────────────────────
static constexpr int MOTOR_PIN   = 4;    // motor MOSFET gate
static constexpr int PWM_CHANNEL = 0;
static constexpr int PWM_FREQ    = 5000; // Hz — lower to ~200 for ERM coin motors
static constexpr int PWM_RES     = 8;   // bits → 0-255
static constexpr int LED_PIN     = 21;  // onboard LED, mirrors motor pattern
static constexpr int BLE_LED_PIN = 2;   // lights when BLE central is connected

// ── BLE Nordic UART Service UUIDs (must match the HTML) ──────────────────────
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // central writes here
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // notify central

static NimBLEServer*         pServer  = nullptr;
static NimBLECharacteristic* pTxChar  = nullptr;
static bool                  connected = false;

// ── Incoming data buffer ──────────────────────────────────────────────────────
static String rxBuffer;

// ── Vibration job ─────────────────────────────────────────────────────────────
struct VibJob {
  uint8_t  intensity = 255;
  uint32_t duration  = 500;
  String   pattern   = "single";
};
static VibJob           pendingJob;
static volatile bool    jobPending = false;
static TaskHandle_t     vibTaskHandle = nullptr;

// ── Motor & LED helpers ───────────────────────────────────────────────────────
void motorOff() {
  if (ENABLE_MOTOR) ledcWrite(PWM_CHANNEL, 0);
  if (ENABLE_LED)   digitalWrite(LED_PIN, LOW);
}

void motorOn(uint8_t intensity) {
  if (ENABLE_MOTOR) ledcWrite(PWM_CHANNEL, intensity);
  if (ENABLE_LED)   digitalWrite(LED_PIN, HIGH);
}

// ── Pattern player (runs inside FreeRTOS task) ────────────────────────────────
void playPattern(const String& pattern, uint32_t duration, uint8_t intensity) {
  if (pattern == "single") {
    motorOn(intensity); vTaskDelay(pdMS_TO_TICKS(duration));
    motorOff();

  } else if (pattern == "double") {
    uint32_t pulse = duration / 3;
    motorOn(intensity);  vTaskDelay(pdMS_TO_TICKS(pulse));
    motorOff();          vTaskDelay(pdMS_TO_TICKS(pulse / 2));
    motorOn(intensity);  vTaskDelay(pdMS_TO_TICKS(pulse));
    motorOff();

  } else if (pattern == "triple") {
    uint32_t pulse = duration / 5;
    for (int i = 0; i < 3; i++) {
      motorOn(intensity); vTaskDelay(pdMS_TO_TICKS(pulse));
      motorOff();         if (i < 2) vTaskDelay(pdMS_TO_TICKS(pulse / 2));
    }

  } else if (pattern == "long") {
    motorOn(intensity); vTaskDelay(pdMS_TO_TICKS(duration));
    motorOff();

  } else if (pattern == "sos") {
    auto dot  = [&]{ motorOn(intensity); vTaskDelay(pdMS_TO_TICKS(80));  motorOff(); vTaskDelay(pdMS_TO_TICKS(80)); };
    auto dash = [&]{ motorOn(intensity); vTaskDelay(pdMS_TO_TICKS(240)); motorOff(); vTaskDelay(pdMS_TO_TICKS(80)); };
    for (int i = 0; i < 3; i++) dot();  vTaskDelay(pdMS_TO_TICKS(160));
    for (int i = 0; i < 3; i++) dash(); vTaskDelay(pdMS_TO_TICKS(160));
    for (int i = 0; i < 3; i++) dot();
  }
}

// ── FreeRTOS vibration task (core 1 — BLE runs on core 0) ────────────────────
void vibTask(void*) {
  for (;;) {
    if (jobPending) {
      jobPending = false;
      VibJob job = pendingJob; // local copy before next job can overwrite
      Serial.printf("[VIB] pattern=%s dur=%ums intensity=%u\n",
                    job.pattern.c_str(), job.duration, job.intensity);
      playPattern(job.pattern, job.duration, job.intensity);
      if (ENABLE_ACK && pTxChar && connected) {
        String ack = "{\"ack\":\"done\",\"pattern\":\"" + job.pattern + "\"}";
        pTxChar->setValue(ack.c_str());
        pTxChar->notify();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ── JSON command handler ──────────────────────────────────────────────────────
void handleCommand(const String& json) {
  Serial.println("[RX] " + json);
  JsonDocument doc;                          // ArduinoJson v7 — no size needed
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.println("[ERR] JSON: " + String(err.c_str()));
    return;
  }
  const char* cmd = doc["cmd"] | "";
  if (strcmp(cmd, "vibrate") != 0) return;

  pendingJob.intensity = (uint8_t)constrain((int)(doc["intensity"] | 255), 0, 255);
  pendingJob.duration  = (uint32_t)max((int)(doc["duration"]  | 500), 50);
  pendingJob.pattern   = (const char*)(doc["pattern"] | "single");
  jobPending = true;
}

// ── BLE server callbacks ──────────────────────────────────────────────────────
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*) override {
    connected = true;
    digitalWrite(BLE_LED_PIN, HIGH);
    Serial.println("[BLE] Central connected");
  }
  void onDisconnect(NimBLEServer*) override {
    connected = false;
    digitalWrite(BLE_LED_PIN, LOW);
    Serial.println("[BLE] Central disconnected — restarting advertising");
    NimBLEDevice::startAdvertising();
  }
};

// ── BLE RX characteristic callbacks ──────────────────────────────────────────
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    std::string val = pChar->getValue();
    rxBuffer += String(val.c_str());
    int nl;
    while ((nl = rxBuffer.indexOf('\n')) != -1) {
      String cmd = rxBuffer.substring(0, nl);
      cmd.trim();
      if (cmd.length()) handleCommand(cmd);
      rxBuffer = rxBuffer.substring(nl + 1);
    }
  }
};

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n=== Speech → Haptic Controller ===");

  // Motor PWM
  if (ENABLE_MOTOR) {
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
    ledcAttachPin(MOTOR_PIN, PWM_CHANNEL);
    motorOff();
  }

  // Trigger LED (GPIO 21)
  if (ENABLE_LED) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
  }

  // BLE status LED (GPIO 2)
  pinMode(BLE_LED_PIN, OUTPUT);
  digitalWrite(BLE_LED_PIN, LOW);

  // BLE stack
  if (ENABLE_BLE) {
    NimBLEDevice::init("Haptic-Controller");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(NUS_SERVICE_UUID);

    NimBLECharacteristic* pRxChar = pService->createCharacteristic(
      NUS_RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxChar->setCallbacks(new RxCallbacks());

    pTxChar = pService->createCharacteristic(NUS_TX_UUID, NIMBLE_PROPERTY::NOTIFY);

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(NUS_SERVICE_UUID);
    pAdv->setScanResponse(true);
    NimBLEDevice::startAdvertising();
    Serial.println("[BLE] Advertising as 'Haptic-Controller'");
  } // end ENABLE_BLE

  // Vibration task pinned to core 1
  xTaskCreatePinnedToCore(vibTask, "vibTask", 4096, nullptr, 1, &vibTaskHandle, 1);
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
}