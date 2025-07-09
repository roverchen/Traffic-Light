#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define M1_IN1 1
#define M1_IN2 2
#define M2_IN1 3
#define M2_IN2 4

void forward();
void backward();
void turnLeft();
void turnRight();
void stopMotors();

// BLE UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CMD_CHARACTERISTIC_UUID "abcdefab-1234-1234-1234-abcdefabcdef"  // for write
#define STATUS_CHARACTERISTIC_UUID "abcdefab-5678-5678-5678-abcdefabcdef" // for notify

BLECharacteristic *pStatusChar;

void sendStatus(String status) {
  Serial.println("[BLE] Notifying: " + status);
  pStatusChar->setValue(status.c_str());
  pStatusChar->notify();
}

// BLE write callback
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string command = pCharacteristic->getValue();

    if (command.length() > 0) {
      char c = toupper(command[0]);
      Serial.print("[BLE] Received command: ");
      Serial.println(c);

      switch (c) {
        case 'F':
          forward();
          sendStatus("Forward");
          break;
        case 'B':
          backward();
          sendStatus("Backward");
          break;
        case 'L':
          turnLeft();
          sendStatus("Left");
          break;
        case 'R':
          turnRight();
          sendStatus("Right");
          break;
        case 'S':
          stopMotors();
          sendStatus("Stop");
          break;
        default:
          Serial.println("[BLE] Unknown command. Stopping.");
          stopMotors();
          sendStatus("Unknown");
          break;
      }
    }
  }
};

// Motor control
void forward() {
  Serial.println("[MOTOR] Moving Forward");
  digitalWrite(M1_IN1, HIGH);
  digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN1, HIGH);
  digitalWrite(M2_IN2, LOW);
}

void backward() {
  Serial.println("[MOTOR] Moving Backward");
  digitalWrite(M1_IN1, LOW);
  digitalWrite(M1_IN2, HIGH);
  digitalWrite(M2_IN1, LOW);
  digitalWrite(M2_IN2, HIGH);
}

void turnLeft() {
  Serial.println("[MOTOR] Turning Left");
  digitalWrite(M1_IN1, LOW);
  digitalWrite(M1_IN2, HIGH);
  digitalWrite(M2_IN1, HIGH);
  digitalWrite(M2_IN2, LOW);
}

void turnRight() {
  Serial.println("[MOTOR] Turning Right");
  digitalWrite(M1_IN1, HIGH);
  digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN1, LOW);
  digitalWrite(M2_IN2, HIGH);
}

void stopMotors() {
  Serial.println("[MOTOR] Stopped");
  digitalWrite(M1_IN1, LOW);
  digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN1, LOW);
  digitalWrite(M2_IN2, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("[BOOT] Starting up...");

  pinMode(M1_IN1, OUTPUT);
  pinMode(M1_IN2, OUTPUT);
  pinMode(M2_IN1, OUTPUT);
  pinMode(M2_IN2, OUTPUT);
  stopMotors();

  BLEDevice::init("ESP32C3-Car");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Write characteristic
  BLECharacteristic *pCmdChar = pService->createCharacteristic(
    CMD_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCmdChar->setCallbacks(new MyCallbacks());

  // Notify characteristic
  pStatusChar = pService->createCharacteristic(
    STATUS_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("[BLE] BLE service ready. Waiting for commands...");
  sendStatus("Ready");
}

void loop() {
  // Empty loop — BLE handles all interaction
}
