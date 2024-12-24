#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char *ssid = "byte";
const char *password = "pass1234";

// Device configuration
const int section_device_id = 6;
const int valve_pin = 2; // The GPIO pin controlling the valve

// MQTT configuration
const char *mqtt_server = "192.168.189.156"; // Ip address of mqtt brocker (RPi)
const int mqtt_port = 1883;
const char *mqtt_username = "arecanut";
const char *mqtt_password = "123456";

// WiFi and MQTT client objects
WiFiClient espClient;
PubSubClient client(espClient);

// Timer and status variables
unsigned long previousMillis = 0;
int manual_off_timer = 0;
bool valve_status = false;
bool valve_mode_auto = false;
int auto_on_threshold = 0;
int auto_off_threshold = 0;
int avg_section_moisture = 0;

// Function declarations
void connectToWiFi();
void reconnectMQTT();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void publishValveStatus(String mode, String status);

void setup()
{
  Serial.begin(9600);
  pinMode(valve_pin, OUTPUT);
  digitalWrite(valve_pin, LOW);

  connectToWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  // Ensure valve is off initially
  valve_status = false;
  valve_mode_auto = false;
}

void loop()
{
  if (!client.connected())
  {
    reconnectMQTT();
  }
  client.loop();

  // Handle manual off timer
  if (!valve_mode_auto && valve_status && manual_off_timer > 0)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= manual_off_timer * 60000)
    {
      // Time elapsed, manual turn off valve
      digitalWrite(valve_pin, LOW);
      Serial.println("\nvalve manual off");
      valve_status = false;
      publishValveStatus("manual", "off");
      previousMillis = currentMillis;
    }
  }
}

void connectToWiFi()
{
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}

void reconnectMQTT()
{
  while (!client.connected())
  {
    Serial.print("Connecting to MQTT...");

    String client_id = "valve_client_" + String(section_device_id);
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("connected");
      String topic = "/farm/valve/" + String(section_device_id);
      client.subscribe(topic.c_str());
      Serial.println("Subscribed to topic: " + topic);
    }
    else
    {
      Serial.print("Failed to connect, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // Print the topic and raw payload to check the received data
  Serial.print("\nMessage received on topic: ");
  Serial.println(topic);

  // Convert the payload to a string
  String payloadStr;
  for (unsigned int i = 0; i < length; i++)
  {
    payloadStr += (char)payload[i];
  }

  // Parse the incoming MQTT message
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payloadStr);

  if (error)
  {
    Serial.println("Failed to parse JSON");
    return;
  }

  // Extract the necessary values
  String valve_mode = doc["valve_mode"];
  String valve_status_str = doc["valve_status"];
  auto_on_threshold = doc["auto_on_threshold"];
  auto_off_threshold = doc["auto_off_threshold"];
  avg_section_moisture = doc["avg_section_moisture"];
  manual_off_timer = doc["manual_off_timer"];

  // Check for mode change from manual to auto
  if (valve_mode == "auto" && !valve_mode_auto)
  {
    Serial.println("Switching to auto mode");

    if (valve_status_str == "on")
    {
      digitalWrite(valve_pin, HIGH);
      Serial.println("Valve turned on automatically.");
      valve_status = true;
    }
    else if (valve_status_str == "off")
    {
      digitalWrite(valve_pin, LOW);
      Serial.println("Valve turned off automatically.");
      valve_status = false;
    }
  }

  // Auto Mode Logic
  if (valve_mode == "auto")
  {
    if (valve_status_str == "on" && avg_section_moisture >= auto_off_threshold)
    {
      digitalWrite(valve_pin, LOW);
      Serial.println("Valve turned off automatically.");
      valve_status = false;
      publishValveStatus("auto", "off");
    }
    else if (valve_status_str == "off" && avg_section_moisture <= auto_on_threshold)
    {
      digitalWrite(valve_pin, HIGH);
      Serial.println("Valve turned on automatically.");
      valve_status = true;
      publishValveStatus("auto", "on");
    }
  }
  // Manual Mode Logic
  else if (valve_mode == "manual")
  {
    if (valve_status_str == "on" && manual_off_timer > 0)
    {
      digitalWrite(valve_pin, HIGH);
      valve_status = true;
      Serial.println("Manual mode: Valve turned on.");
      Serial.println("Manual mode: Valve will turn off after the timer.");
      previousMillis = millis();
    }
    else if (valve_status_str == "off")
    {
      digitalWrite(valve_pin, LOW);
      Serial.println("Manual mode: Valve turned off immediately.");
      valve_status = false;
    }
  }

  // Update valve mode status
  valve_mode_auto = (valve_mode == "auto");
}

void publishValveStatus(String mode, String status)
{
  DynamicJsonDocument doc(1024);
  doc["section_device_id"] = section_device_id;
  doc["mode"] = mode;
  doc["status"] = status;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
  client.publish(("/farm/valve/post/" + String(section_device_id)).c_str(), jsonBuffer);
  Serial.println("Published: " + String(jsonBuffer));
}