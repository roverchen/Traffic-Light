#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define LED_GPIO 8  // 確認你的板載 LED 是接在 GPIO 8

// BLE UUIDs (可自訂，但目前是示範用)
#define SERVICE_UUID               "12345678-1234-1234-1234-1234567890ab"
#define CMD_CHARACTERISTIC_UUID    "abcdefab-1234-1234-1234-abcdefabcdef"
#define STATUS_CHARACTERISTIC_UUID "abcdefab-5678-5678-5678-abcdefabcdef"

BLECharacteristic *pStatusChar;
char lastCommand = 'S';  // 預設關閉 LED

// 狀態回報
void sendStatus(const String &msg) {
  Serial.println("[BLE] Status: " + msg);
  if (pStatusChar) {
    pStatusChar->setValue(msg.c_str());
    pStatusChar->notify();
  }
}

// BLE 指令回調處理
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      lastCommand = toupper(value[0]);  // ← 儲存為大寫

      Serial.print("[BLE] Received: ");
      Serial.println(lastCommand);

      switch (lastCommand) {
        case 'F':
          sendStatus("LED ON");
          break;
        case 'S':
          sendStatus("LED OFF");
          break;
        default:
          sendStatus("LED Blinking");
          break;
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[BOOT] ESP32-C3 BLE LED Controller");

  // 設定 LED 腳位
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LOW);

  // BLE 初始化
  BLEDevice::init("ESP32C3-LED");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pCmdChar = pService->createCharacteristic(
    CMD_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCmdChar->setCallbacks(new MyCallbacks());

  pStatusChar = pService->createCharacteristic(
    STATUS_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("[BLE] Waiting for commands...");
  sendStatus("Ready");
}

void loop() {
  if (lastCommand == 'F') {
    digitalWrite(LED_GPIO, LOW);
    delay(100);
  } else if (lastCommand == 'S') {
    digitalWrite(LED_GPIO, HIGH);
    delay(100);
  } else {
    digitalWrite(LED_GPIO, HIGH);
    delay(250);
    digitalWrite(LED_GPIO, LOW);
    delay(250);
  }
}
