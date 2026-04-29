import paho.mqtt.client as mqtt
from pysondb import db
import json

# This line creates the file on your computer
test_db = db.getDb("test_storage.json")

def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode())
    test_db.add(data) # This saves the data to the file
    print(f"Received and Saved: {data}")

client = mqtt.Client()
client.on_message = on_message
client.connect("broker.emqx.io", 1883, 60)
client.subscribe("test/jsondb/data") # MUST match your ESP32 topic

print("Python is waiting for ESP32 data...")
client.loop_forever()