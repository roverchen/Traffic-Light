#include <Arduino.h>

// 定義LED燈的GPIO腳位
const int redPin = D1;    // 紅燈
const int yellowPin = D2; // 黃燈
const int greenPin = D3;  // 綠燈

void setup() {
  // 初始化 Serial 通訊
  Serial.begin(115200);

  // 設定GPIO腳位為輸出模式
  pinMode(redPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(greenPin, OUTPUT);

  // 顯示啟動訊息
  Serial.println("交通燈模擬啟動");
}

void loop() {
  // 紅燈亮 5 秒
  digitalWrite(redPin, HIGH);
  digitalWrite(yellowPin, LOW);
  digitalWrite(greenPin, LOW);
  Serial.println("紅燈亮");
  delay(5000);

  // 綠燈亮 5 秒
  digitalWrite(redPin, LOW);
  digitalWrite(yellowPin, LOW);
  digitalWrite(greenPin, HIGH);
  Serial.println("綠燈亮");
  delay(5000);

  // 綠燈閃爍 5 次
  Serial.println("綠燈閃");
  for (int i = 0; i < 5; i++) {
    digitalWrite(greenPin, LOW);
    delay(500); // 綠燈滅 0.5 秒
    digitalWrite(greenPin, HIGH);
    delay(500); // 綠燈亮 0.5 秒
  }

  // 黃燈亮 3 秒
  digitalWrite(redPin, LOW);
  digitalWrite(yellowPin, HIGH);
  digitalWrite(greenPin, LOW);
  Serial.println("黃燈亮");
  delay(3000);
}