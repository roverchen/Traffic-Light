#include <Arduino.h>
#include <DHT.h>

// 定義 DHT11 感測器的腳位和類型
#define DHTPIN D7      // DHT11 連接到 D7
#define DHTTYPE DHT11  // 使用 DHT11 感測器

// 初始化 DHT 感測器
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // 初始化序列埠通訊
  Serial.begin(115200);
  Serial.println("DHT11 溫濕度感測器讀取開始");

  // 啟動 DHT 感測器
  dht.begin();
}

void loop() {
  // 讀取濕度
  float humidity = dht.readHumidity();

  // 讀取溫度（攝氏）
  float temperature = dht.readTemperature();

  // 檢查是否讀取失敗
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("讀取 DHT11 數據失敗！");
    delay(2000); // 等待 2 秒後重試
    return;
  }

  // 在序列埠監控器中顯示溫濕度數值
  Serial.print("濕度: ");
  Serial.print(humidity);
  Serial.print("%  溫度: ");
  Serial.print(temperature);
  Serial.println("°C");

  // 延遲 2 秒，避免過多輸出
  delay(2000);
}