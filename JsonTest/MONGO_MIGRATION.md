# JSON to MongoDB Migration Guide

## Overview
This guide walks you through migrating your IoT sensor data from a JSON file (`test_storage.json`) to MongoDB.

---

## Step 1: Set Up MongoDB

### Option A: Local Installation
1. Download [MongoDB Community Edition](https://www.mongodb.com/try/download/community)
2. Install and ensure the MongoDB service is running
3. Default connection: `mongodb://localhost:27017/`

### Option B: Cloud (Recommended for IoT)
1. Create a free account on [MongoDB Atlas](https://www.mongodb.com/cloud/atlas)
2. Create a free cluster
3. Get your connection string (looks like: `mongodb+srv://username:password@cluster.mongodb.net/`)

---

## Step 2: Install Python MongoDB Driver

Run in your JsonTest virtual environment:

```bash
pip install pymongo
```

---

## Step 3: Modify Your Subscriber Script

Replace the contents of `subscriber.py` with the following MongoDB-enabled version:

```python
import paho.mqtt.client as mqtt
from pymongo import MongoClient
import json
from datetime import datetime

# Connect to MongoDB
# For local installation:
client = MongoClient("mongodb://localhost:27017/")

# For MongoDB Atlas (cloud - uncomment and use your connection string):
# client = MongoClient("mongodb+srv://username:password@cluster.mongodb.net/")

db = client["iot_data"]  # Database name
collection = db["sensor_readings"]  # Collection name

def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode())
    data["timestamp"] = datetime.utcnow()  # Add timestamp
    collection.insert_one(data)
    print(f"Received and Saved: {data}")

mqtt_client = mqtt.Client()
mqtt_client.on_message = on_message
mqtt_client.connect("broker.emqx.io", 1883, 60)
mqtt_client.subscribe("test/jsondb/data")  # MUST match your ESP32 topic

print("Python is waiting for ESP32 data...")
mqtt_client.loop_forever()
```

---

## Step 4: Migrate Existing Data (One-Time)

Create a new Python script called `migrate_to_mongo.py`:

```python
from pymongo import MongoClient
import json

# Connect to MongoDB
client = MongoClient("mongodb://localhost:27017/")
db = client["iot_data"]
collection = db["sensor_readings"]

# Read existing JSON file
with open("test_storage.json", "r") as f:
    data = json.load(f)

# Insert all records
if data.get("data"):
    collection.insert_many(data["data"])
    print(f"✓ Successfully migrated {len(data['data'])} records to MongoDB")
else:
    print("No data found to migrate")

# Verify migration
count = collection.count_documents({})
print(f"Total records in MongoDB: {count}")
```

Run the migration:
```bash
python migrate_to_mongo.py
```

---

## Step 5: Verify Migration

Check that your data is in MongoDB:

```python
from pymongo import MongoClient

client = MongoClient("mongodb://localhost:27017/")
db = client["iot_data"]
collection = db["sensor_readings"]

# Count total records
total = collection.count_documents({})
print(f"Total records: {total}")

# View first 5 records
for doc in collection.find().limit(5):
    print(doc)
```

---

## Advantages of MongoDB vs JSON File

| Feature | JSON File | MongoDB |
|---------|-----------|---------|
| **Query Capability** | Limited | Advanced filtering & aggregation |
| **Scalability** | Slow with large datasets | Handles millions of records |
| **Indexing** | No indexing | Create indexes for fast searches |
| **Concurrent Access** | Not safe for concurrent writes | Safe concurrent read/write |
| **Real-time Stats** | Must load entire file | Efficient aggregation |
| **Backup & Restore** | Manual file copy | Built-in tools |

---

## Common MongoDB Queries

### Find Latest 10 Readings
```python
collection.find().sort("_id", -1).limit(10)
```

### Find by Device
```python
collection.find({"device": "ESP32_Wokwi"})
```

### Find Readings Above Threshold
```python
collection.find({"val": {"$gt": 50}})
```

### Find Records from Last Hour
```python
from datetime import datetime, timedelta

one_hour_ago = datetime.utcnow() - timedelta(hours=1)
collection.find({"timestamp": {"$gte": one_hour_ago}})
```

### Count Records by Device
```python
list(collection.aggregate([
    {"$group": {"_id": "$device", "count": {"$sum": 1}}}
]))
```

### Get Average Sensor Value
```python
list(collection.aggregate([
    {"$group": {"_id": None, "avg_val": {"$avg": "$val"}}}
]))
```

---

## Cleanup (Optional)

After successful migration, you can delete the old JSON file:

```bash
rm test_storage.json
```

Or keep it as a backup for reference.

---

## Troubleshooting

### MongoDB Connection Error
- **Local**: Ensure MongoDB service is running
  - Windows: Check Services → MongoDB → Start
  - Linux: `sudo systemctl start mongod`
- **Atlas**: Verify connection string and IP whitelist settings

### Data Not Inserting
- Check MQTT connection to `broker.emqx.io`
- Verify topic subscription matches ESP32 publisher topic
- Check MongoDB user permissions

### Performance Issues
- Create an index on frequently queried fields:
  ```python
  collection.create_index("device")
  collection.create_index("timestamp")
  ```

---

## Next Steps

1. ✅ Set up MongoDB (local or cloud)
2. ✅ Install pymongo driver
3. ✅ Update subscriber.py
4. ✅ Run migration script
5. ✅ Update your queries to use MongoDB API
6. ✅ Monitor incoming ESP32 data in MongoDB

Your IoT data is now ready for advanced analytics and real-time dashboards!
