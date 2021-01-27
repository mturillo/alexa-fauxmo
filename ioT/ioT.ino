#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "fauxmoESP.h"

// Rename the credentials.sample.h file to credentials.h and 
// edit it according to your router configuration
#include "credentials.h"

fauxmoESP fauxmo;

char* topic = "channels/1279939/publish/HSF04ETFRM8AV3FH"; // Arduino Test 3
char* server = "mqtt.thingspeak.com";
String payload;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);

// -----------------------------------------------------------------------------

#define SERIAL_BAUDRATE     115200
#define TEMP_SENSOR          4
#define ID_SENSOR           "Sensorone"

float temp = 0, humid = 0; 

// -----------------------------------------------------------------------------
// Wifi
// -----------------------------------------------------------------------------

void wifiSetup() {

    // Set WIFI module to STA mode
    WiFi.mode(WIFI_STA);

    // Connect
    Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Wait
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(100);
    }
    Serial.println();

    client.setCallback(mqttCallback);

    // Connected!
    Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  
    String clientName="ESP-Thingspeak";
    Serial.print("Connecting to ");
    Serial.print(server);
    Serial.print(" as ");
    Serial.println(clientName);
    
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker");
      Serial.print("Topic is: ");
      Serial.println(topic);
      
      if (client.publish(topic, "hello from ESP8266")) {
        Serial.println("Publish ok");
      }
      else {
        Serial.println("Publish failed");
      }
    }
    else {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }

}

void setup() {

    // Init serial port and clean garbage
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println();
    Serial.println();

    // LEDs
    pinMode(TEMP_SENSOR, OUTPUT);
    digitalWrite(TEMP_SENSOR, LOW);


    // Wifi
    wifiSetup();

    // By default, fauxmoESP creates it's own webserver on the defined port
    // The TCP port must be 80 for gen3 devices (default is 1901)
    // This has to be done before the call to enable()
    fauxmo.createServer(true); // not needed, this is the default value
    fauxmo.setPort(80); // This is required for gen3 devices

    // You have to call enable(true) once you have a WiFi connection
    // You can enable or disable the library at any moment
    // Disabling it will prevent the devices from being discovered and switched
    fauxmo.enable(true);

    // You can use different ways to invoke alexa to modify the devices state:
    // "Alexa, turn yellow lamp on"
    // "Alexa, turn on yellow lamp
    // "Alexa, set yellow lamp to fifty" (50 means 50% of brightness, note, this example does not use this functionality)

    // Add virtual devices
    fauxmo.addDevice(ID_SENSOR);

    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        
        // Callback when a command from Alexa is received. 
        // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
        // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
        // Just remember not to delay too much here, this is a callback, exit as soon as possible.
        // If you have to do something more involved here set a flag and process it in your main loop.
        
        Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);

        // Checking for device_id is simpler if you are certain about the order they are loaded and it does not change.
        // Otherwise comparing the device_name is safer.

        if (strcmp(device_name, ID_SENSOR)==0) {

            Serial.println("Send data to the provided MQTT channel");
            publishMQTT();
        }

        digitalWrite(TEMP_SENSOR, state ? HIGH : LOW);

    });

}

void publishMQTT() {

  float humid = 70.00;
  float temp = 22.00;
  
  payload="field1=";
          payload+=String(temp);
          payload+="&field2=";
          payload+=String(humid);
    
  if (client.publish(topic, (char*) payload.c_str())) {
    Serial.println("Perfect! Data have been published to the thingspeak.com!");
  }

}

void handleMqttConnection(void) {
          
  char clientID[] = "00000000"; // null terminated 8 chars long
  
  // Loop until reconnected.
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    
    // Generate ClientID in order to create a new connection handler
    for (int i = 0; i < 8; i++) {
      clientID[i] = random(65, 91); // use only letters A to Z
    }
  
    // Connect to the MQTT broker
    if (client.connect(clientID)) {
      Serial.print("Connected with Client ID: ");
      Serial.print(String(clientID));
      publishMQTT();
      client.subscribe(payload.c_str());
      
    } else {
      Serial.print("failed, rc=");
      // Print to know why the connection failed.
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(1000);
    }
  }
  client.loop();
}


void mqttCallback(char * topicChar, byte * payloadBytes, unsigned int length) {
    char payloadChar[100];
    int i;
    for (i = 0; i < length; i++) {
        payloadChar[i] = payloadBytes[i];
    }
    payloadChar[i] = '\0';

    String topic = String(topicChar);
    String payload = String(payloadChar);

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] (");
    Serial.print(length);
    Serial.print(" bytes) :");
    Serial.print(payload);
    Serial.println();

    if (topic == payload) {
        Serial.print("Processing ");
        Serial.println(payload);

        if ((char) payload[0] == '0') {
            // setSwitchLow(false);
            Serial.print("SET THE STATUS TO OFF");
        } else {
            Serial.print("SET THE STATUS TO ON");
        }
    } 
}



void loop() {

    // fauxmoESP uses an async TCP server but a sync UDP server
    // Therefore, we have to manually poll for UDP packets
    fauxmo.handle();

    // This is a sample code to output free heap every 5 seconds
    // This is a cheap way to detect memory leaks
    static unsigned long last = millis();
    if (millis() - last > 5000) {
        last = millis();
        Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
    }

    // If your device state is changed by any other means (MQTT, physical button,...)
    // you can instruct the library to report the new state to Alexa on next request:
    // fauxmo.setState(ID_SENSOR, true, 255);
    
    handleMqttConnection();
}
