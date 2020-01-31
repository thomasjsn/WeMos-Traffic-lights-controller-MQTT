#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// i'm loading settings from this file
#include "credentials.h"

// but you can set them here instead
//const char* ssid = "";
//const char* password = "";
//const char* mqtt_server = "";

#define CLIENT_ID "6xv"

#define AUX_1 15

#define GR 1
#define YE 2
#define RE 3

const byte traffic1[] = { 1, 4, 0, 16 };     // green, yellow, red
const byte traffic2[] = { 2, 14, 12, 13 };   // green, yellow, red

const char* willTopic = "$CONNECTED/"CLIENT_ID;

const char* setTopic[] = {
  "node/"CLIENT_ID"/yellow_flash/set",
  "node/"CLIENT_ID"/aux_1/set"
};

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
float temp = 0;

bool yellow_flash = false;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid); //We don't want the ESP to act as an AP
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void msgPublish(char* topic, char* payload, byte idx=NULL, bool retain=false) {
  char tmpTopic[30];
  sprintf(tmpTopic, "node/%s/%s", CLIENT_ID, topic);

  if (idx != NULL) {
    sprintf(tmpTopic, "%s/%d", tmpTopic, idx);
  }

  client.publish(tmpTopic, payload, retain);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // set yellow flash mode
  if (strcmp(topic, setTopic[0]) == 0) {
    if ((char)payload[0] == '1') {
      msgPublish("yellow_flash", "1", NULL, true);
      yellow_flash = true;
    } else {
      msgPublish("yellow_flash", "0", NULL, true);
      yellow_flash = false;
    }
  } 

  // turn on aux output
  if (strcmp(topic, setTopic[1]) == 0) {
    if ((char)payload[0] == '1') {
      msgPublish("aux_1", "1", NULL, true);
      digitalWrite(AUX_1, HIGH);
    } else {
      msgPublish("aux_1", "0", NULL, true);
      digitalWrite(AUX_1, LOW);
    }
  }

}

void reconnect() {
  // Loop until we're reconnected
  digitalWrite(LED_BUILTIN, LOW);
  
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(CLIENT_ID, willTopic, 0, true, "0")) {
      Serial.println("connected");
      client.publish(willTopic, "1", true);
      client.subscribe(setTopic[0]);
      client.subscribe(setTopic[1]);
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(traffic1[GR], OUTPUT);
  pinMode(traffic1[YE], OUTPUT);
  pinMode(traffic1[RE], OUTPUT);
  pinMode(traffic2[GR], OUTPUT);
  pinMode(traffic2[YE], OUTPUT);
  pinMode(traffic2[RE], OUTPUT);
  pinMode(AUX_1, OUTPUT);
  
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  lightPowerUp();
}

void waitAndLoop(int ms) {
  //Serial.print("Waiting...");
  long startTime = millis();
  
  while (millis() - startTime < ms) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    
    if (yellow_flash) {
      yellowFlash();
    }
  }

//  Serial.print(millis() - startTime);
//  Serial.println(" ms");
}

void lightPowerUp() {
  Serial.println("Power-up sequence");
  
  // 5 second yellow flash
  for (int x = 0; x < 10; x++) {
    // yellow on
    digitalWrite(traffic1[YE], HIGH);
    digitalWrite(traffic2[YE], HIGH);
    waitAndLoop(500);

    // yellow off
    digitalWrite(traffic1[YE], LOW);
    digitalWrite(traffic2[YE], LOW);
    waitAndLoop(500);
  }

  // 5 seconds steady yellow
  digitalWrite(traffic1[YE], HIGH);
  digitalWrite(traffic2[YE], HIGH);
  waitAndLoop(5000);
  digitalWrite(traffic1[YE], LOW);
  digitalWrite(traffic2[YE], LOW);

  // minimum 5 seconds red
  digitalWrite(traffic1[RE], HIGH);
  digitalWrite(traffic2[RE], HIGH);
  msgPublish("status", "red", traffic1[0]);
  msgPublish("status", "red", traffic2[0]);
  msgPublish("color", "#ff0000", traffic1[0]);
  msgPublish("color", "#ff0000", traffic2[0]);
  waitAndLoop(5000);

  Serial.println("Normal operation");
  loop();
}

void turnGreen(const byte values[]) {
  // yellow
  digitalWrite(values[YE], HIGH);
  msgPublish("status", "red_yellow", values[0]);
  msgPublish("color", "#ffff00", values[0]);
  waitAndLoop(1000);

  // green
  digitalWrite(values[RE], LOW);
  digitalWrite(values[YE], LOW);
  digitalWrite(values[GR], HIGH);
  msgPublish("status", "green", values[0]);
  msgPublish("color", "#00ff00", values[0]);
}

void turnRed(const byte values[]) {
  // red/yellow
  digitalWrite(values[GR], LOW);
  digitalWrite(values[YE], HIGH);
  msgPublish("status", "yellow", values[0]);
  msgPublish("color", "#ffff00", values[0]);
  waitAndLoop(3000);

  // red
  digitalWrite(values[YE], LOW);
  digitalWrite(values[RE], HIGH);
  msgPublish("status", "red", values[0]);
  msgPublish("color", "#ff0000", values[0]);
}

void yellowFlash() {
  while (yellow_flash) {
    // green off
    digitalWrite(traffic1[GR], LOW);
    digitalWrite(traffic2[GR], LOW);

    // yellow on
    digitalWrite(traffic1[YE], HIGH);
    digitalWrite(traffic2[YE], HIGH);

    // red off
    digitalWrite(traffic1[RE], LOW);
    digitalWrite(traffic2[RE], LOW);

    msgPublish("status", "yellow_flash");
    msgPublish("color", "#ffff00", traffic1[0]);
    msgPublish("color", "#ffff00", traffic2[0]);
    client.loop();
    delay(500);

    // yellow off
    digitalWrite(traffic1[YE], LOW);
    digitalWrite(traffic2[YE], LOW);

    msgPublish("color", "#000000", traffic1[0]);
    msgPublish("color", "#000000", traffic2[0]);
    client.loop();
    delay(500);
  }

  lightPowerUp();
}

void loop()
{
  // direction 1
  turnGreen(traffic1);
  waitAndLoop(10000);
  turnRed(traffic1);

  // switch time
  waitAndLoop(3000);

  // direction 2
  turnGreen(traffic2);
  waitAndLoop(10000);
  turnRed(traffic2);

  // switch time
  waitAndLoop(3000);
}
