import paho.mqtt.client as mqtt
import json
import requests
import time
from datetime import datetime

# MQTT broker configuration
mqtt_broker = "192.168.164.156" # Ip address of mqtt brocker (RPi)
mqtt_port = 1883
mqtt_topic = "farm/moisture/#"
mqtt_username = "arecanut"
mqtt_password = "123456"

# Cloud server URL
cloud_url = "http://{ip address of cloud server}:3000/iot/moisture/"

# Farm identification
farm_id = 1
farm_key = "farm_keey"  

# File to store received data
output_file = "received_data.txt"
file = open(output_file, "a+")  

def post_data_to_http():
    """Post data from file to HTTP server."""
    try:
        file.seek(0)
        data = file.readlines()
        if data:
            data_json = []
            for line in data:
                try:
                    json_data = json.loads(line.strip())  
                    data_json.append(json_data)
                except json.JSONDecodeError:
                    print(f"Invalid JSON in line: {line.strip()}")
                    continue  
            
            if data_json:
                post_data = {
                    "farm_key": farm_key,
                    "data": data_json
                }

                url = f"{cloud_url}{farm_id}"
                headers = {'Content-Type': 'application/json'}

                response = requests.post(url, json=post_data, headers=headers)

                if response.status_code == 200:
                    print(f"Successfully posted data")
                    print(response.text)
                    file.truncate(0)  # Clear file after successful post
                    print("File cleared after posting data.")
                else:
                    print(f"Failed to post data. HTTP Status Code: {response.status_code}")
                    print(response.text)
            else:
                print("No valid data to send.")
        else:
            print("No data in the file to send.")
    except Exception as e:
        print(f"Error while posting data: {e}")

def on_message(client, userdata, msg):
    """Callback function for when a message is received from the MQTT broker."""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
    payload = msg.payload.decode()

    try:
        data = json.loads(payload)  
    except json.JSONDecodeError:
        print(f"Error decoding message: {payload}")
        return

    data["timestamp"] = timestamp
    print(f"Message received: {json.dumps(data)}")
    file.write(json.dumps(data) + "\n")
    file.flush()  # Ensure data is written to file

# Initialize MQTT client
client = mqtt.Client()
client.username_pw_set(mqtt_username, mqtt_password)
client.on_message = on_message

# Connect to MQTT broker
print(f"Connecting to MQTT broker at {mqtt_broker}:{mqtt_port}")
client.connect(mqtt_broker, mqtt_port)

# Subscribe to topic
print(f"Subscribing to topic '{mqtt_topic}'")
client.subscribe(mqtt_topic)

# Start listening for messages
print("Waiting for messages...")
client.loop_start()

# Periodically post data to HTTP server
while True:
    time.sleep(60)  # every minute
    post_data_to_http()
