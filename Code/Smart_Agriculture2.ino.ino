#include <ESP32Servo.h>
#include "VOneMqttClient.h"

// Define constants and pins for Soil Moisture Sensor
int MinMoistureValue = 4095;
int MaxMoistureValue = 1500;
int MinMoisture = 0;
int MaxMoisture = 100;
int Moisture = 0;

const char* MoistureSensor = "440e0ccc-7edb-4ee0-b06d-f01afc0454a3"; 
const int moisturePin = 34;
const int SOIL_MOISTURE_THRESHOLD = 30; // Threshold for low soil moisture

// Define constants and pins for Water Level Sensor (Digital)
const char* WaterLevel = "93646920-0678-45d8-b7cf-0d6db2d03f14"; 
const int depthPin = 39; // Pin connected to the digital output of the water level sensor

// Define constants and pins for Rain Sensor (Analog)
int MinRainValue = 4095; 
int MaxRainValue = 0;
int MinRain = 0;
int MaxRain = 100;
int rainLevel = 0;

const char* RainSensor = "d84c3975-b887-48d3-826a-875d4f53f838"; 
const int rainPin = 35;
const int RAIN_THRESHOLD = 40; // Threshold for rain detection

// Define constants and pins for Relay
const char* Relay = "64a2d9ea-61a9-42f9-888f-f6c170ac6906"; 
const int relayPin = 32;

// Define constants and pins for Servo
const char* ServoMotor = "5d22ee01-0881-4c4e-9ec6-36d80850d45e";
const int servoPin = 16;
Servo servo;

const int minPulse = 500; 
const int maxPulse = 2500; 
bool pumpOn = false;

// Create an instance of VOneMqttClient
VOneMqttClient voneClient;

// Last message time
unsigned long lastMsgTime = 0;
const unsigned long LOOP_INTERVAL = 10000; // Adjust the interval as needed

// Prototype for the callback function
void triggerActuator_callback(const char* actuatorDeviceId, const char* actuatorCommand);

// Function to control the relay (water pump)
void controlPump(bool turnOn) {
    pumpOn = turnOn;
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

        controlPump(commandValue);
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

    pinMode(depthPin, INPUT);
    pinMode(rainPin, INPUT);
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW); // Ensure pump is off at startup
    
    servo.attach(servoPin, minPulse, maxPulse);
    servo.write(90); // Set initial position
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
        int waterDetected = digitalRead(depthPin);
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

    // Servo Oscillation moves left-right continuously when pump is on
    if (pumpOn) {
        static unsigned long lastServoMove = 0;
        static bool servoToggle = false;
        
        if (millis() - lastServoMove > 1000) { // Change direction every 1s
            servoToggle = !servoToggle;
            servo.write(servoToggle ? 45 : 135); // Moves between 45° and 135°
            lastServoMove = millis();
        }
    }
}
