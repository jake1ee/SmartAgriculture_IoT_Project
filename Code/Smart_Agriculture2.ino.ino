#include "VOneMqttClient.h"

// Define constants and pins for Soil Moisture Sensor
int MinMoistureValue = 4095;
int MaxMoistureValue = 1500;
int MinMoisture = 0;
int MaxMoisture = 100;
int Moisture = 0;

const char* MoistureSensor = "d9d60312-2a83-4d12-b4ec-4551e3e8f83c"; 
const int moisturePin = 34;
const int SOIL_MOISTURE_THRESHOLD = 30; // Threshold for low soil moisture

// Define constants and pins for Water Level Sensor (Digital)
const char* WaterLevel = "836e15a0-b32f-45cf-ae8a-50c0660bf17e"; 
const int depthPin = 39; // Pin connected to the digital output of the water level sensor

// Define constants and pins for Rain Sensor (Analog)
int MinRainValue = 4095; 
int MaxRainValue = 0; // Adjust based on calibration
int MinRain = 0;
int MaxRain = 100;
int rainLevel = 0;

const char* RainSensor = "ba662b8a-bd2d-472f-a80a-36dff37bcaa6"; 
const int rainPin = 35;
const int RAIN_THRESHOLD = 40; // Threshold for rain detection

// Define constants and pins for Relay
const char* Relay = "64a2d9ea-61a9-42f9-888f-f6c170ac6906"; 
const int relayPin = 32;

// Create an instance of VOneMqttClient
VOneMqttClient voneClient;

// Last message time
unsigned long lastMsgTime = 0;
const unsigned long LOOP_INTERVAL = 10000; // Adjust the interval as needed

// Prototype for the callback function
void triggerActuator_callback(const char* actuatorDeviceId, const char* actuatorCommand);

// Relay control function
void controlPump(bool turnOn) {
  if (turnOn) {
    Serial.println("Relay ON: Activating Water Pump");
    digitalWrite(relayPin, HIGH);
  } else {
    Serial.println("Relay OFF: Deactivating Water Pump");
    digitalWrite(relayPin, LOW);
  }
}

// Filtered analog reading function
int readFilteredAnalog(int pin, int samples = 10) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return sum / samples;
}

// Callback function definition
void triggerActuator_callback(const char* actuatorDeviceId, const char* actuatorCommand) {
  Serial.print("Main received callback : ");
  Serial.print(actuatorDeviceId);
  Serial.print(" : ");
  Serial.println(actuatorCommand);

  if (String(actuatorDeviceId) == Relay) {
    JSONVar commandObjct = JSON.parse(actuatorCommand);
    bool commandValue = (bool)commandObjct["relay"];

    if (commandValue) {
      Serial.println("Relay ON");
      digitalWrite(relayPin, HIGH);
    } else {
      Serial.println("Relay OFF");
      digitalWrite(relayPin, LOW);
    }

    voneClient.publishActuatorStatusEvent(actuatorDeviceId, actuatorCommand, true);
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Setup function
void setup() {
  Serial.begin(9600);
  setup_wifi();
  voneClient.setup();
  voneClient.registerActuatorCallback(triggerActuator_callback);

  pinMode(depthPin, INPUT); // Digital pin for water level sensor
  pinMode(rainPin, INPUT);  // Analog pin for rain sensor
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); // Ensure pump is off at startup
}

// Loop function
void loop() {
  if (!voneClient.connected()) {
    voneClient.reconnect();
    voneClient.publishDeviceStatusEvent(RainSensor, true);
    voneClient.publishDeviceStatusEvent(WaterLevel, true);
    voneClient.publishDeviceStatusEvent(MoistureSensor, true);
  }
  voneClient.loop();

  unsigned long cur = millis();
  if (cur - lastMsgTime > LOOP_INTERVAL) {
    lastMsgTime = cur;

    // Soil Moisture Sensor
    int soilValue = readFilteredAnalog(moisturePin);
    Moisture = map(soilValue, MinMoistureValue, MaxMoistureValue, MinMoisture, MaxMoisture);
    voneClient.publishTelemetryData(MoistureSensor, "Soil moisture", Moisture);

    // Water Level Sensor (Digital)
    int waterDetected = digitalRead(depthPin); // Digital HIGH/LOW signal
    voneClient.publishTelemetryData(WaterLevel, "Depth", waterDetected);

    // Rain Sensor (Analog)
    int rainValue = readFilteredAnalog(rainPin);
    rainLevel = map(rainValue, MinRainValue, MaxRainValue, MinRain, MaxRain);
    voneClient.publishTelemetryData(RainSensor, "Raining", rainLevel);

    // Control Logic for Water Pump
    if (Moisture < SOIL_MOISTURE_THRESHOLD && rainLevel < RAIN_THRESHOLD && waterDetected == HIGH) {
      controlPump(true); // Activate water pump
    } else {
      controlPump(false); // Deactivate water pump
    }
  }
}