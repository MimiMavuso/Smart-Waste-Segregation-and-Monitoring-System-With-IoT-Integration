// ESP32 Code – Smart Waste Segregation (Standard MQTT, No Secure Client)
// Uses 5% increment per waste detection.
// Waste-type block only shows Dry / Wet / Metal.

#include <WiFi.h>
#include <PubSubClient.h>
#include <GeoIP.h>

// ========== YOUR CREDENTIALS ==========
const char* ssid     = "Mimi";
const char* password = "Aa12345678";

#define AIO_USERNAME  "Mimi_Mavuso"
#define AIO_KEY       "aio_Eoaa32ZtU53AmefLzXiJfgzh4IyG"

// ========== MQTT (Standard Port) ==========
const char* mqtt_server = "io.adafruit.com";
const int mqtt_port = 1883;          // Non-secure port – no WiFiClientSecure needed

// Feeds
const char* dryLevelTopic   = AIO_USERNAME "/feeds/dry-level";
const char* wetLevelTopic   = AIO_USERNAME "/feeds/wet-level";
const char* metalLevelTopic = AIO_USERNAME "/feeds/metal-level";
const char* lastWasteTopic  = AIO_USERNAME "/feeds/last-waste";
const char* mapLocationTopic = AIO_USERNAME "/feeds/location";

WiFiClient espClient;                // Standard WiFiClient, not secure
PubSubClient client(espClient);

// GeoIP
GeoIP geoIP;
location_t geoLoc;
bool locationObtained = false;

// Bin levels (increment by 5% per detection)
float dryLevel = 0.0;
float wetLevel = 0.0;
float metalLevel = 0.0;
const float INCREMENT = 5.0;

// ------------------------------------------------------------------
void setup_wifi() {
  delay(10);
  Serial.begin(115200);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ------------------------------------------------------------------
void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Connecting to Adafruit IO MQTT...");
    if (client.connect("ESP32WasteClient", AIO_USERNAME, AIO_KEY)) {
      Serial.println(" connected!");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 10 seconds");
      delay(10000);
    }
  }
}

// ------------------------------------------------------------------
void getGeoLocation() {
  geoLoc = geoIP.getGeoFromWiFi();
  if (geoLoc.status) {
    Serial.print("📍 Location: ");
    Serial.print(geoLoc.latitude, 6);
    Serial.print(", ");
    Serial.println(geoLoc.longitude, 6);
    locationObtained = true;
  } else {
    Serial.println("❌ Could not obtain location (will retry later)");
  }
}

// ------------------------------------------------------------------
void publishAllLevels() {
  if (client.publish(dryLevelTopic, String(dryLevel, 1).c_str()))
    Serial.println("📤 Dry level: " + String(dryLevel, 1));
  if (client.publish(wetLevelTopic, String(wetLevel, 1).c_str()))
    Serial.println("📤 Wet level: " + String(wetLevel, 1));
  if (client.publish(metalLevelTopic, String(metalLevel, 1).c_str()))
    Serial.println("📤 Metal level: " + String(metalLevel, 1));
}

// ------------------------------------------------------------------
void processWaste(String wasteType) {
  // Only update bin levels for real waste types
  if (wasteType == "Dry") {
    dryLevel += INCREMENT;
    if (dryLevel > 100) dryLevel = 100;
  } 
  else if (wasteType == "Wet") {
    wetLevel += INCREMENT;
    if (wetLevel > 100) wetLevel = 100;
  } 
  else if (wasteType == "Metal") {
    metalLevel += INCREMENT;
    if (metalLevel > 100) metalLevel = 100;
  } 
  else {
    // Ignore LevelUpdate or any other string
    Serial.println("Ignoring message (not a waste type): " + wasteType);
    return;
  }

  // Publish updated levels
  publishAllLevels();

  // Publish only real waste type to text feed
  if (client.publish(lastWasteTopic, wasteType.c_str()))
    Serial.println("📤 Last waste: " + wasteType);
}

// ------------------------------------------------------------------
void setup() {
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  
  // Serial1 for Arduino communication (pins 3 = RX, 1 = TX)
  Serial1.begin(9600, SERIAL_8N1, 3, 1);
  
  getGeoLocation();
  Serial.println("\n🚀 ESP32 ready – 5% increment per waste detection.");
}

// ------------------------------------------------------------------
void loop() {
  if (!client.connected()) reconnect_mqtt();
  client.loop();

  // Publish location once (when obtained)
  static bool locationSent = false;
  if (!locationObtained) {
    getGeoLocation();
  } else if (!locationSent) {
    String locMsg = "0," + String(geoLoc.latitude, 6) + "," + String(geoLoc.longitude, 6) + ",0";
    if (client.publish(mapLocationTopic, locMsg.c_str())) {
      Serial.println("📤 Location sent to map.");
      locationSent = true;
    }
  }

  // Read from Arduino
  if (Serial1.available()) {
    String line = Serial1.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    Serial.print("Arduino: ");
    Serial.println(line);

    // Parse "WASTE:Wet,LEVEL:45.2,LOC:1" – we ignore LEVEL and LOC
    int wasteIdx = line.indexOf("WASTE:");
    int levelIdx = line.indexOf(",LEVEL:");
    
    if (wasteIdx != -1 && levelIdx != -1) {
      String wasteType = line.substring(wasteIdx + 6, levelIdx);
      processWaste(wasteType);
    } else {
      Serial.println("⚠️ Invalid message format");
    }
  }
  delay(50);
}
