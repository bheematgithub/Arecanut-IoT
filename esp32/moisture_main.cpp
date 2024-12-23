#include <WiFi.h>
#include <PubSubClient.h>

#define SOIL_MOISTURE_PIN 35

RTC_DATA_ATTR char *ssid = "byte";
RTC_DATA_ATTR char *password = "pass1234";

RTC_DATA_ATTR int moisture_device_id = 4;

RTC_DATA_ATTR char *mqtt_server = "192.168.189.156";
RTC_DATA_ATTR int mqtt_port = 1883;
RTC_DATA_ATTR char *mqtt_username = "arecanut";
RTC_DATA_ATTR char *mqtt_password = "123456";

WiFiClient espClient;
PubSubClient client(espClient);

// Deep Sleep configuration
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for microseconds to seconds */
#define TIME_TO_SLEEP 30       /* Time ESP32 will sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0; // Retains value across deep sleep cycles

void connectWiFi();
void reconnectMQTT();
void sendMoistureData();

void setup()
{
  Serial.begin(9600);

  // Increment boot count
  bootCount++;
  Serial.println("Boot number: " + String(bootCount));

  connectWiFi();
  client.setServer(mqtt_server, mqtt_port);

  reconnectMQTT();

  // Publish soil moisture data
  sendMoistureData();

  // Configure timer wake-up for deep sleep
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Going to sleep now...");

  // Enter deep sleep
  WiFi.disconnect(true);
  Serial.println("disconnected");
  Serial.println();
  Serial.flush(); // Ensure all data is transmitted before deep sleep

  esp_deep_sleep_start();
}

void loop()
{
  // Not used because the ESP32 goes into deep sleep in the setup()
}

void connectWiFi()
{
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to Wi-Fi");
}

void reconnectMQTT()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wi-Fi is disconnected. Reconnecting...");
    connectWiFi();
  }

  while (!client.connected())
  {
    Serial.print("Connecting to MQTT...");

    String client_id = "moisture_client_"+String(moisture_device_id);
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("Connected to MQTT broker");
    }
    else
    {
      Serial.print("Failed to connect to MQTT. Error: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void sendMoistureData()
{
  int soil_moisture = analogRead(SOIL_MOISTURE_PIN);
  int send_moisture = map(soil_moisture, 4095, 0, 0, 100);

  Serial.print("Soil Moisture: ");
  Serial.println(send_moisture);

  String topic = "farm/moisture/"+String(moisture_device_id);
  String payload = "{\"moisture_device_id\":" + String(moisture_device_id) + ", \"value\":" + String(send_moisture) + "}";

  if (client.publish(topic.c_str(), payload.c_str()))
  {
    Serial.println("Moisture data sent successfully:");
    Serial.println(payload);
  }
  else
  {
    Serial.println("Failed to send moisture data.");
  }
}
