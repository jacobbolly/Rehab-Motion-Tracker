#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// WiFi network created by ESP32
const char* ssid = "RehabDevice";
const char* password = "12345678";

// XIAO ESP32-C3 I2C pins
#define SDA_PIN D4
#define SCL_PIN D5

WebServer server(80);
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

// Rep tracking
int repCount = 0;
bool inRep = false;

float startAngle = 0;
float currentAngle = 0;
float movement = 0;
float maxMovement = 0;

const float START_THRESHOLD = 15.0;
const float END_THRESHOLD = 8.0;
const float MIN_GOOD_DEPTH = 35.0;
const unsigned long MIN_REP_TIME = 1200;

unsigned long repStartTime = 0;
String formMessage = "Ready";

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial;
      text-align: center;
      background: #172326;
      color: white;
      padding: 30px;
    }
    .card {
      background: #1E293B;
      border-radius: 18px;
      padding: 24px;
      margin: 18px auto;
      max-width: 420px;
    }
    h1 { color: #caa11c; }
    .big {
      font-size: 48px;
      font-weight: bold;
    }
  </style>
</head>

<body>
  <h1>ReForm Rehab Tracker</h1>

  <div class="card">
    <p>Reps</p>
    <div class="big" id="reps">0</div>
  </div>

  <div class="card">
    <p>Movement</p>
    <div class="big"><span id="movement">0.0</span>&deg;</div>
  </div>

  <div class="card">
    <p>Status</p>
    <h2 id="status">Ready</h2>
  </div>

  <script>
    setInterval(() => {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('reps').innerText = data.reps;
          document.getElementById('movement').innerText = data.movement;
          document.getElementById('status').innerText = data.status;
        });
    }, 250);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"reps\":" + String(repCount) + ",";
  json += "\"movement\":\"" + String(movement, 1) + "\",";
  json += "\"status\":\"" + formMessage + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!bno.begin()) {
    Serial.println("BNO055 not detected. Check wiring.");
    while (1);
  }

  delay(1000);
  bno.setExtCrystalUse(true);

  sensors_event_t event;
  bno.getEvent(&event);
  startAngle = event.orientation.y;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("WiFi started");
  Serial.print("Network: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("Open browser to: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  Serial.println("Rehab tracker ready.");
  Serial.println("Stand still when device starts.");
}

void loop() {
  server.handleClient();

  sensors_event_t event;
  bno.getEvent(&event);

  currentAngle = event.orientation.y;
  movement = abs(currentAngle - startAngle);

  if (!inRep && movement > START_THRESHOLD) {
    inRep = true;
    maxMovement = movement;
    repStartTime = millis();
    formMessage = "Rep started";
  }

  if (inRep) {
    if (movement > maxMovement) {
      maxMovement = movement;
    }

    if (movement < END_THRESHOLD) {
      repCount++;

      unsigned long repTime = millis() - repStartTime;

      if (maxMovement < MIN_GOOD_DEPTH) {
        formMessage = "Form warning: shallow rep";
      } 
      else if (repTime < MIN_REP_TIME) {
        formMessage = "Form warning: too fast";
      } 
      else {
        formMessage = "Good rep";
      }

      Serial.print("Rep: ");
      Serial.println(repCount);
      Serial.print("Max movement: ");
      Serial.println(maxMovement);
      Serial.print("Rep time: ");
      Serial.println(repTime);
      Serial.println(formMessage);
      Serial.println("-----");

      inRep = false;
      maxMovement = 0;
    }
  }

  delay(20);
}
