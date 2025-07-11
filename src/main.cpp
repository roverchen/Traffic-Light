#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// --- SPI 腳位定義 (FSPI) ---
#define SS_PIN    7
#define RST_PIN   8
#define SCK_PIN   4
#define MOSI_PIN  6
#define MISO_PIN  5

SPIClass spiRFID(FSPI);
MFRC522 rfid(SS_PIN, RST_PIN);

// --- Servo 腳位 ---
#define SERVO_PIN 9
Servo myServo;

// --- 授權 UID ---
// byte authorizedUID[4] = {0x1A, 0x2B, 0x3C, 0x4D};
const byte authorizedUID[4] = {0x42, 0xDD, 0xB5, 0x01};  // ✅ 將這組設為授權卡

void setup() {
  Serial.begin(115200);
  delay(2000);  // 等待序列埠穩定
  Serial.println("🔧 初始化中...");

  // 初始化 SPI
  spiRFID.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);  // 某些庫會用 default SPI
  rfid.PCD_Init();
  delay(50);

  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.printf("📦 RC522 版本號: 0x%02X\n", version);
  if (version == 0x00 || version == 0xFF) {
    Serial.println("❌ 無法讀取 RC522，請檢查接線與供電");
    while (true);
  }

  Serial.println("✅ RC522 初始化成功，請靠近卡片感應...");

  // 初始化伺服馬達
  myServo.attach(SERVO_PIN);
  myServo.write(0);  // 預設鎖定
}

bool isAuthorized(byte *uid) {
  for (byte i = 0; i < 4; i++) {
    if (uid[i] != authorizedUID[i]) return false;
  }
  return true;
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  Serial.print("📡 UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.printf("%02X ", rfid.uid.uidByte[i]);
  }
  Serial.println();

  if (isAuthorized(rfid.uid.uidByte)) {
    Serial.println("🔓 授權成功，開鎖中...");
    myServo.write(90);  // 開鎖
  } else {
    Serial.println("🔒 未授權卡片，鎖定");
    myServo.write(0);   // 鎖定
  }

  delay(3000);
  rfid.PICC_HaltA();      // 停止與當前卡片通訊
  rfid.PCD_StopCrypto1(); // 關閉加密
}
