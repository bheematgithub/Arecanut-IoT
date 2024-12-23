import paho.mqtt.client as mqtt
import json
import requests
import time
from datetime import datetime

mqtt_broker = "localhost"
mqtt_port = 1883
mqtt_username = "arecanut"
mqtt_password = "123456"
mqtt_sub_topic = "/farm/valve/post/#"
mqtt_pub_topic = "/farm/valve/"

farm_id = 1
farm_key = "farm_key"

cloud_url = "http://localhost:3000/iot/valve/"

last_published_timestamps = {}

client = mqtt.Client()
client.username_pw_set(mqtt_username, mqtt_password)

def fetch_data_from_cloud():
    try:
        print(f"Fetching valve data from cloud...")
        data = {"farm_key": farm_key}
        headers = {"Content-Type": "application/json"}
        get_url = f"{cloud_url}{farm_id}"
        response = requests.get(get_url, json=data, headers=headers)

        if response.status_code == 200:
            return response.json()
        else:
            print(f"Failed to fetch data from cloud. HTTP Status Code: {response.status_code}")
            print(response.text)
            return None
    except requests.exceptions.RequestException as e:
        print(f"Error while fetching data from cloud: {e}")
        return None

def publish_data_to_mqtt(data):
    section_device_id = data.get("section_device_id")
    mode = data.get("valve_mode")

    if not section_device_id or not mode:
        print("Error: section_device_id or valve_mode not found in data.")
        return


    timestamp = data.get("timestamp")
    last_published_time = last_published_timestamps.get(section_device_id)
    
    if last_published_time == timestamp and mode != "auto":
        print(f"Valve data for section_device_id {section_device_id} already published.")
        return

    last_published_timestamps[section_device_id] = timestamp
    mqtt_pub_topic = f"/farm/valve/{section_device_id}"

    try:
        print(f"Publishing data to MQTT topic '{mqtt_pub_topic}'")
        client.publish(mqtt_pub_topic, payload=json.dumps(data), qos=1)
    except Exception as e:
        print(f"Error while publishing data to MQTT: {e}")

def post_data_to_cloud(section_device_id, data):
    post_url = f"{cloud_url}{section_device_id}"
    data["timestamp"] = datetime.now().strftime("%Y-%m-%dT%H:%M:%S.%fZ")
    data["farm_id"] = farm_id
    data["farm_key"] = farm_key
    data.pop("section_device_id", None)
    
    try:
        response = requests.post(post_url, json=data)
        if response.status_code == 200:
            print(f"Successfully posted data.")
        else:
            print(f"Failed to post data. HTTP Status Code: {response.status_code}")
            print(response.text)
    except requests.exceptions.RequestException as e:
        print(f"Error while posting data: {e}")

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
        print(f"\nReceived message on {msg.topic}: {json.dumps(payload, indent=2)}")

        if "section_device_id" in payload:
            section_device_id = payload["section_device_id"]
            post_data_to_cloud(section_device_id, payload)
        else:
            print("Error: section_device_id is missing in the received message.")
    
    except json.JSONDecodeError:
        print(f"Error decoding message: {msg.payload.decode()}")
    except Exception as e:
        print(f"Error processing message: {e}")

client.on_message = on_message

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker successfully!")
        client.subscribe(mqtt_sub_topic)
        print(f"Subscribed to topic: {mqtt_sub_topic}")
    else:
        print(f"Failed to connect to MQTT broker, return code {rc}")

def connect_mqtt():
    try:
        print(f"Connecting to MQTT broker at {mqtt_broker}:{mqtt_port}")
        client.on_connect = on_connect
        client.connect(mqtt_broker, mqtt_port, 60)
        client.loop_start()
    except Exception as e:
        print(f"Error while connecting to MQTT broker: {e}")
        time.sleep(5)
        connect_mqtt()

connect_mqtt()

while True:
    valve_data = fetch_data_from_cloud()

    if valve_data:
        print(f"\nFetched data: {json.dumps(valve_data, indent=2)}")
        if "data" in valve_data:
            for valve_entry in valve_data["data"]:
                publish_data_to_mqtt(valve_entry)

    time.sleep(60)
