#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

// === WebSocket & HTTP Server ===
WebSocketsServer webSocket(81);
AsyncWebServer server(80);

// === Motor A (Forward/Backward) ===
const int motorA_pwm_fwd = 6;
const int motorA_pwm_rev = 5;

// === Motor B (Left/Right) ===
const int motorB_pwm_left  = 20;
const int motorB_pwm_right = 21;

// === Motor Standby Pin ===
const int motor_stby = 7; // Set HIGH to enable motors

// LEDC channels
const int CH_A_FWD = 0;
const int CH_A_REV = 1;
const int CH_B_LEFT = 2;
const int CH_B_RIGHT = 3;
const int PWM_FREQ = 20000; // 20 kHz (不可聽範圍)
const int PWM_RES = 8; // 8-bit -> duty 0-255

void setupPWM() {
  ledcSetup(CH_A_FWD, PWM_FREQ, PWM_RES);
  ledcSetup(CH_A_REV, PWM_FREQ, PWM_RES);
  ledcSetup(CH_B_LEFT, PWM_FREQ, PWM_RES);
  ledcSetup(CH_B_RIGHT, PWM_FREQ, PWM_RES);

  ledcAttachPin(motorA_pwm_fwd, CH_A_FWD);
  ledcAttachPin(motorA_pwm_rev, CH_A_REV);
  ledcAttachPin(motorB_pwm_left, CH_B_LEFT);
  ledcAttachPin(motorB_pwm_right, CH_B_RIGHT);
}

// === Movement Modes ===
enum DriveMode { AUTO, MANUAL };
DriveMode currentMode = MANUAL;

// === HTML UI ===
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32-C3 Car Joystick</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body {
      margin:0;
      background:#111;
      color:white;
      text-align:center;
      font-family:sans-serif;
    }
    button {
      cursor: pointer;
    }
    #status {
      margin:10px;
      font-size:1.2em;
    }
    #joystick {
      width:90vmin;
      height:90vmin;
      max-width:300px;
      max-height:300px;
      margin:20px auto;
      background:rgba(0,191,255,0.15); /* joystick 背景顏色帶藍色 */
      border-radius:50%;
      border:2px solid #00BFFF; /* 邊框亮藍 */
      position:relative;
    }
  </style>
  <script src="https://cdn.jsdelivr.net/npm/nipplejs@0.9.0/dist/nipplejs.min.js"></script>
</head>
<body>
  <h1>ESP32-C3 Car Control</h1>
  <div id="status">Connecting...</div>
  <div id="joystick" style="width:100%;height:50vh;position:relative;"></div>
  <div id="motorStatus" style="margin-top:10px;font-size:1.1em;color:#0f0;">
    Motor A: 0 | Motor B: 0
  </div>

  <div style="text-align:center;margin-top:10px;display:none;">
    <button onclick="sendCmdName('forward')" style="padding:15px;margin:5px;background:#00bfff;color:white;border:none;border-radius:8px;">Forward</button>
    <br>
    <button onclick="sendCmdName('left')" style="padding:15px;margin:5px;background:#00bfff;color:white;border:none;border-radius:8px;">Left</button>
    <button onclick="sendCmdName('stop')" style="padding:15px;margin:5px;background:#ff4d4d;color:white;border:none;border-radius:8px;">Stop</button>
    <button onclick="sendCmdName('right')" style="padding:15px;margin:5px;background:#00bfff;color:white;border:none;border-radius:8px;">Right</button>
    <br>
    <button onclick="sendCmdName('backward')" style="padding:15px;margin:5px;background:#00bfff;color:white;border:none;border-radius:8px;">Backward</button>
  </div>

  <script>
    const ws = new WebSocket(`ws://${location.hostname}:81`);

    ws.onopen = () => { document.getElementById('status').textContent = "Connected ✅"; };
    ws.onclose = () => { document.getElementById('status').textContent = "Disconnected ❌"; };
    ws.onerror = (err) => { document.getElementById('status').textContent = "Error ⚠️"; console.error(err); };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.motorA !== undefined && data.motorB !== undefined) {
          document.getElementById('motorStatus').textContent =
            `Motor A: ${data.motorA} | Motor B: ${data.motorB}`;
        }
        if (data.debug !== undefined) {
          console.log("DEBUG:", data.debug);
        }
      } catch (e) {
        console.log("Non-JSON message:", event.data);
      }
    };

    function sendCmd(cmd) {
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(cmd);
      }
    }

    // 按鍵控制（用 name）
    function sendCmdName(name) {
      let steer = 0, throttle = 0;
      switch(name) {
        case 'forward': throttle = 255; break;
        case 'backward': throttle = -255; break;
        case 'left': steer = -255; break;
        case 'right': steer = 255; break;
        case 'stop': steer = 0; throttle = 0; break;
      }
      sendCmd(JSON.stringify({ steer, throttle, name }));
    }

    // 自適應 joystick 大小
    function getJoystickSize() {
      const isMobile = /Mobi|Android/i.test(navigator.userAgent);
      if (isMobile) {
        return Math.min(window.innerWidth, window.innerHeight) * 0.7;
      } else {
        return Math.min(300, Math.min(window.innerWidth, window.innerHeight) * 0.4);
      }
    }

    let joystick = nipplejs.create({
      zone: document.getElementById('joystick'),
      mode: 'static',
      position: { left: '50%', top: '50%' },
      color: '#00bfff',
      size: getJoystickSize()
    });

    let lastSend = 0;
    joystick.on('move', (evt, data) => {
      const now = Date.now();
      if (now - lastSend > 50) { // 每 50ms 最多一次
        if (data && data.vector) {
          let steer = Math.round(data.vector.x * 255);
          let throttle = Math.round(data.vector.y * 255);
    
          // 死區範例
          const deadzone = 0.1;
          steer = (Math.abs(steer) < deadzone * 255) ? 0 : steer;
          throttle = (Math.abs(throttle) < deadzone * 255) ? 0 : throttle;

          ws.send(JSON.stringify({ steer, throttle }));
        }
        lastSend = now;
      }
    });

    joystick.on('end', () => {
      sendCmd(JSON.stringify({ steer: 0, throttle: 0 }));
    });

    // 視窗大小變化時重建 Joystick
    window.addEventListener('resize', () => {
      joystick.destroy();
      joystick = nipplejs.create({
        zone: document.getElementById('joystick'),
        mode: 'static',
        position: { left: '50%', top: '50%' },
        color: '#00bfff',
        size: getJoystickSize()
      });
    });
  </script>
</body>
</html>
)rawliteral";

// === Motor Control Functions ===
// ---- 參數（可調） ----
const int MAX_DUTY = 200;           // 最大 PWM（你目前用 200）
const unsigned long RAMP_INTERVAL_MS = 30; // ramp 更新間隔
//const int RAMP_STEP = 6;            // 每次 ramp 增量（越小越溫和）
const int RAMP_STEP = MAX_DUTY / 34; // roughly 6 if MAX_DUTY=200

// ---- 狀態變數 ----
volatile int targetA = 0;  // 目標速度 -MAX..MAX (Motor A: 前後)
volatile int targetB = 0;  // 目標速度 -MAX..MAX (Motor B: 左右)
int currentA = 0;          // 當前實際輸出（會漸進）
int currentB = 0;

unsigned long lastRampMillis = 0;
unsigned long lastActivityMillis = 0;
const unsigned long STBY_IDLE_TIMEOUT_MS = 1500; // 停止後多久關 STBY
unsigned long lastCommandTime = 0;
const unsigned long COMMAND_TIMEOUT = 300; // 單位 ms, 0.3 秒沒收到新指令就停止

void sendMotorStatus() {
  StaticJsonDocument<64> status;
  status["motorA"] = currentA;
  status["motorB"] = currentB;
  char buffer[64];
  size_t len = serializeJson(status, buffer);
  webSocket.broadcastTXT(buffer, len); // send to all connected clients
}

// ---- 控制馬達啟用狀態 ----
void motorEnable(bool enable) {
  digitalWrite(motor_stby, enable ? HIGH : LOW);
}

// ---- 直接輸出到 PWM（保留你的 ledcWrite channel） ----
void applyMotorA(int speed) {
  speed = constrain(speed, -MAX_DUTY, MAX_DUTY);
  if (speed > 0) {
    ledcWrite(CH_A_FWD, speed);
    ledcWrite(CH_A_REV, 0);
  } else if (speed < 0) {
    ledcWrite(CH_A_FWD, 0);
    ledcWrite(CH_A_REV, abs(speed));
  } else {
    ledcWrite(CH_A_FWD, 0);
    ledcWrite(CH_A_REV, 0);
  }
}

void applyMotorB(int speed) {
  speed = constrain(speed, -MAX_DUTY, MAX_DUTY);
  if (speed > 0) {
    ledcWrite(CH_B_RIGHT, speed);
    ledcWrite(CH_B_LEFT, 0);
  } else if (speed < 0) {
    ledcWrite(CH_B_RIGHT, 0);
    ledcWrite(CH_B_LEFT, abs(speed));
  } else {
    ledcWrite(CH_B_RIGHT, 0);
    ledcWrite(CH_B_LEFT, 0);
  }
}

// ---- 設定目標（由外部呼叫，例如 WebSocket handler） ----
void setTargetMotorA(int speed) {
  targetA = constrain(speed, -MAX_DUTY, MAX_DUTY);
  lastActivityMillis = millis();
  if (targetA != 0) motorEnable(true);
}

void setTargetMotorB(int speed) {
  targetB = constrain(speed, -MAX_DUTY, MAX_DUTY);
  lastActivityMillis = millis();
  if (targetB != 0) motorEnable(true);
}

// ---- 停止所有馬達（漸進到 0） ----
void stopAllMotors() {
  ledcWrite(CH_A_FWD, 0);
  ledcWrite(CH_A_REV, 0);
  ledcWrite(CH_B_LEFT, 0);
  ledcWrite(CH_B_RIGHT, 0);
  motorEnable(false);
}

// ---- 非阻塞 ramp 處理，放在 loop() 中呼叫 ----
void handleMotorRamping() {
  // 超時檢查
  if (millis() - lastCommandTime > COMMAND_TIMEOUT) {
    stopAllMotors();
    return; // 不做後續平滑運算
  }

  // 每 RAMP_INTERVAL_MS 更新一次
  unsigned long now = millis();
  if (now - lastRampMillis < RAMP_INTERVAL_MS) return;
  lastRampMillis = now;

  // Ramp A
  if (currentA < targetA) {
    currentA += RAMP_STEP;
    if (currentA > targetA) currentA = targetA;
  } else if (currentA > targetA) {
    currentA -= RAMP_STEP;
    if (currentA < targetA) currentA = targetA;
  }

  // Ramp B
  if (currentB < targetB) {
    currentB += RAMP_STEP;
    if (currentB > targetB) currentB = targetB;
  } else if (currentB > targetB) {
    currentB -= RAMP_STEP;
    if (currentB < targetB) currentB = targetB;
  }

  // Apply to PWM
  applyMotorA(currentA);
  applyMotorB(currentB);
  sendMotorStatus(); // send live updates

  // STBY 管理：若長時間沒有活動且兩邊都為 0，關閉 STBY
  if (currentA == 0 && currentB == 0 && targetA == 0 && targetB == 0) {
    if (now - lastActivityMillis > STBY_IDLE_TIMEOUT_MS) {
      motorEnable(false);
    }
  } else {
    motorEnable(true);
  }

  // Send motor status + debug
  StaticJsonDocument<128> status;
  status["motorA"] = currentA;
  status["motorB"] = currentB;
  status["debug"] = String("Ramping: currentA=") + currentA +
                    " currentB=" + currentB +
                    " targetA=" + targetA +
                    " targetB=" + targetB;
  char buffer[128];
  size_t len = serializeJson(status, buffer);
  webSocket.broadcastTXT(buffer, len);
}

// ---- 立即緊急停（立刻切 PWM=0 並關 STBY） ----
void emergencyStopNow() {
  targetA = targetB = 0;
  currentA = currentB = 0;
  applyMotorA(0);
  applyMotorB(0);
  motorEnable(false);
  Serial.println("EMERGENCY STOP");
}

// === Command Handling ===
void handleCarCommand(char cmd) {
  switch (cmd) {
    case 'A': currentMode = AUTO; Serial.println("Mode: AUTO"); break;
    case 'M': currentMode = MANUAL; Serial.println("Mode: MANUAL"); break;
  }
}

void controlByJoystick(int steer, int throttle) {
  if (steer == 0 && throttle == 0) {
    setTargetMotorA(0);
    setTargetMotorB(0);
  } else {
    setTargetMotorA(throttle);
    setTargetMotorB(steer);
/*
    // Apply to PWM if no ramping needed
    applyMotorA(throttle);
    applyMotorB(steer);
    sendMotorStatus(); // send live updates
*/
  }
}

// === WebSocket Event ===
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload);
    Serial.printf("[WS] Received: %s\n", msg.c_str());

    if (msg.length() == 1) {
      handleCarCommand(msg.charAt(0));
      lastCommandTime = millis(); // update for single-character commands too
    } else {
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, msg);
      if (!err) {
        int steer = doc["steer"] | 0;
        int throttle = doc["throttle"] | 0;
        controlByJoystick(steer, throttle);
        lastCommandTime = millis(); // update timestamp for joystick commands

        // Send debug to browser
        StaticJsonDocument<128> dbg;
        dbg["debug"] = String("Joystick received:") +
                       " throttle=" + throttle +
                       " steer=" + steer +
                       " targetA=" + targetA +
                       " targetB=" + targetB;
        char buffer[128];
        size_t len = serializeJson(dbg, buffer);
        webSocket.broadcastTXT(buffer, len);
      } else {
        Serial.print("JSON parse error: ");
        Serial.println(err.c_str());
      }
    }
  }
}

void connectToWiFi() {
  WiFi.persistent(true); // Store credentials in NVS
  WiFi.mode(WIFI_AP_STA); // Enable both AP and STA

  // Start Access Point immediately
  const char* apSSID = "ESP32_Control";
  const char* apPASS = "12345678"; // min. 8 chars if using password
  WiFi.softAP(apSSID, apPASS);
  Serial.println("AP started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Try connecting as station using stored credentials
  if (WiFi.SSID() != "") {
    Serial.println("Trying stored WiFi credentials...");
    WiFi.begin(); // reconnect using saved SSID/password
  } else {
    Serial.println("No stored credentials, using defaults...");
    WiFi.begin("CHEN", "22131081");
  }

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nSTA connected!");
    Serial.print("STA IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nSTA connection failed — running AP only");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(motorA_pwm_fwd, OUTPUT);
  pinMode(motorA_pwm_rev, OUTPUT);
  pinMode(motorB_pwm_left, OUTPUT);
  pinMode(motorB_pwm_right, OUTPUT);
  pinMode(motor_stby, OUTPUT);
  motorEnable(false); // motors off at boot
  setupPWM();

  connectToWiFi();
  ArduinoOTA.setPassword("mysecurepassword");
  ArduinoOTA.begin();

  // 啟用 mDNS
  if (MDNS.begin("esp32car")) {  // 這裡的 "esp32car" 就是名稱
    Serial.println("mDNS responder started: http://esp32car.local");
  } else {
    Serial.println("Error setting up mDNS!");
  }

  // 啟用 WebSocket 和 HTTP 伺服器
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("Web UI: http://" + WiFi.localIP().toString());
  Serial.println("WebSocket: ws://" + WiFi.localIP().toString() + ":81");
}

void loop() {
  ArduinoOTA.handle();
  webSocket.loop();
  handleMotorRamping(); // 處理馬達漸進
}
