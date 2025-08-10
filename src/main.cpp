#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

// === Wi-Fi Credentials ===
//const char* ssid = "CHEN";
//const char* password = "22131081";

// === WebSocket & HTTP Server ===
WebSocketsServer webSocket(81);
AsyncWebServer server(80);

// === Motor A (Forward/Backward) ===
const int motorA_pwm_fwd = 5;
const int motorA_pwm_rev = 6;

// === Motor B (Left/Right) ===
const int motorB_pwm_left  = 7;
const int motorB_pwm_right = 8;

// === Motor Standby Pin ===
const int motor_stby = 9; // Set HIGH to enable motors

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
  <style>
    body { margin:0; background:#111; color:white; text-align:center; font-family:sans-serif; }
    #status { margin:10px; font-size:1.2em; }
    #joystick { width:300px; height:300px; margin:20px auto; background:rgba(255,255,255,0.05); border-radius:50%; }
    button { margin:5px; padding:10px 20px; font-size:1em; }
  </style>
  <script src="https://cdn.jsdelivr.net/npm/nipplejs@0.9.0/dist/nipplejs.min.js"></script>
</head>
<body>
  <h1>ESP32-C3 Car Control</h1>
  <div id="status">Connecting...</div>
  <div id="joystick"></div>
  <div>
    <button onclick="sendCmd('F')">Forward</button>
    <button onclick="sendCmd('B')">Backward</button>
    <button onclick="sendCmd('L')">Left</button>
    <button onclick="sendCmd('R')">Right</button>
    <button onclick="sendCmd('S')">Stop</button>
  </div>

  <script>
    const ws = new WebSocket(`ws://${location.hostname}:81`);

    ws.onopen = () => { document.getElementById('status').textContent = "Connected ✅"; };
    ws.onclose = () => { document.getElementById('status').textContent = "Disconnected ❌"; };
    ws.onerror = (err) => { document.getElementById('status').textContent = "Error ⚠️"; console.error(err); };

    function sendCmd(cmd) {
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(cmd);
      }
    }

    const joystick = nipplejs.create({
      zone: document.getElementById('joystick'),
      mode: 'static',
      position: { left: '50%', top: '50%' },
      color: 'white',
      size: 200
    });

    joystick.on('move', (evt, data) => {
      if (data.direction) {
        let steer = Math.round(Math.sin(data.angle.radian) * 255);
        let throttle = Math.round(Math.cos(data.angle.radian) * 255);
        ws.send(JSON.stringify({ steer, throttle }));
      }
    });

    joystick.on('end', () => {
      ws.send(JSON.stringify({ steer: 0, throttle: 0 }));
    });
  </script>
</body>
</html>
)rawliteral";

// === Motor Control Functions ===
void motorEnable(bool enable) {
  digitalWrite(motor_stby, enable ? HIGH : LOW);
}

void setMotorA(int speed) {
  motorEnable(true);
  if (speed > 0) { // Forward
    analogWrite(motorA_pwm_fwd, abs(speed));
    analogWrite(motorA_pwm_rev, 0);
  } else if (speed < 0) { // Reverse
    analogWrite(motorA_pwm_fwd, 0);
    analogWrite(motorA_pwm_rev, abs(speed));
  } else { // Stop
    analogWrite(motorA_pwm_fwd, 0);
    analogWrite(motorA_pwm_rev, 0);
  }
}

void setMotorB(int speed) {
  motorEnable(true);
  if (speed > 0) { // Turn Right
    analogWrite(motorB_pwm_right, abs(speed));
    analogWrite(motorB_pwm_left, 0);
  } else if (speed < 0) { // Turn Left
    analogWrite(motorB_pwm_right, 0);
    analogWrite(motorB_pwm_left, abs(speed));
  } else { // Stop
    analogWrite(motorB_pwm_right, 0);
    analogWrite(motorB_pwm_left, 0);
  }
}

void stopAllMotors() {
  analogWrite(motorA_pwm_fwd, 0);
  analogWrite(motorA_pwm_rev, 0);
  analogWrite(motorB_pwm_left, 0);
  analogWrite(motorB_pwm_right, 0);
  motorEnable(false);
}

// === Command Handling ===
void handleCarCommand(char cmd) {
  switch (cmd) {
    case 'F': setMotorA(255); break;
    case 'B': setMotorA(-255); break;
    case 'L': setMotorB(-255); break;
    case 'R': setMotorB(255); break;
    case 'S': stopAllMotors(); break;
    case 'A': currentMode = AUTO; Serial.println("Mode: AUTO"); break;
    case 'M': currentMode = MANUAL; Serial.println("Mode: MANUAL"); break;
  }
}

void controlByJoystick(int steer, int throttle) {
  if (steer == 0 && throttle == 0) {
    stopAllMotors();
  } else {
    setMotorA(throttle);
    setMotorB(steer);
  }
}

// === WebSocket Event ===
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload);
    Serial.printf("[WS] Received: %s\n", msg.c_str());

    if (msg.length() == 1) {
      handleCarCommand(msg.charAt(0));
    } else {
      StaticJsonDocument<200> doc;
      if (!deserializeJson(doc, msg)) {
        int steer = doc["steer"];
        int throttle = doc["throttle"];
        controlByJoystick(steer, throttle);
      }
    }
  }
}

void connectToWiFi() {
  WiFi.persistent(true);  // store credentials in NVS
  WiFi.mode(WIFI_STA);

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
    Serial.println("\nConnected!");
    Serial.println("IP address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFailed to connect — starting AP mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_Setup");
    Serial.println("AP IP address: " + WiFi.softAPIP().toString());
  }
}
/*
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.println("IP address: " + WiFi.localIP().toString());
}
*/
void setupOTA() {
  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);

  pinMode(motorA_pwm_fwd, OUTPUT);
  pinMode(motorA_pwm_rev, OUTPUT);
  pinMode(motorB_pwm_left, OUTPUT);
  pinMode(motorB_pwm_right, OUTPUT);
  pinMode(motor_stby, OUTPUT);
  motorEnable(false); // motors off at boot

  connectToWiFi();
  setupOTA();

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
}
