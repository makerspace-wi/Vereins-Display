/*
MQTT-Commands - Makerspace Display an der Eingangstüre - Prozessor ESP8266 (WEMOS D1 mini)

6 * Topic "setdisplay/lineX", wobei X die Zeilennummer ist (0-5)
Beispiel: Send payload: "Das ist die erste Zeile" an topic "setdisplay/line0" - um den Text für Zeile 1 zu setzen

6 * Topic "setmode/lineX", wobei X die Zeilennummer ist (0-5)
Beispiel: Send json-payload: '{"speed":1000, "pause":0, "inFX":500 "outFX":0}' an topic "setmode/line0" - um die Parameter für Zeile 1 zu setzen

Topic "switch/pin"// Topic für das Schalten der isplay Beleuchtung
Beispiel: Send payload: ON oder OFF an topic "switch/pin" - um die Beleuchtung ein- oder auszuschalten

Topic "msdisplay/reset" // Topic für Device RESET
Beispiel: Send payload: "reset" an topic "msdisplay/reset" - um den Controller neu zu starten

PUBLISHED Values:
* wifi/ssid - z.B. Makerspace-Intern
* wifi/ip - z.B. 192.168.1 123
* info/swRev - z.B. 3.4
* info/hwRev - z.B. 2.0
* info/status - online oder offline

OTP-Programming (Over The Air Programming)
Enter via browser ip of >IPMakerspace Display</update and select the firmware.bin file from platformio folder
*/

#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266WebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <Credentials_hm.h>

WiFiClient espClient;
PubSubClient client(espClient);

// MD_Parola Einstellungen
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 16
#define CLK_PIN D5  // GPIO14
#define DATA_PIN D7 // GPIO13
#define CS_PIN D8   // GPIO15

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Variablen für die Textzeilen
String lines[6];
int currentLine = 0;

String swRev = "3.6";
String hwRev = "1.0";

// Pin zum Schalten
const int switchPin = D1; // GPIO5

typedef struct
{
  uint16_t speed; // speed multiplier of library default
  uint16_t pause; // pause multiplier for library default
  uint16_t inFX;  // in animation for library default
  uint16_t outFX; // out animation for library default
} sCatalog;

sCatalog catalog[6]{
    {50, 1000, 6, 6},
    {25, 0, 1, 1},
    {25, 0, 1, 1},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}};

textEffect_t effect[] =
    {
        PA_PRINT,
        PA_SCROLL_RIGHT,
        PA_SCROLL_LEFT,
        PA_SCROLL_UP_RIGHT,
        PA_SCROLL_UP_LEFT,
        PA_SCROLL_DOWN_RIGHT,
        PA_SCROLL_DOWN_LEFT,
        PA_SCROLL_UP,
        PA_SCROLL_DOWN,
        PA_GROW_UP,
        PA_GROW_DOWN,
        PA_OPENING,
        PA_CLOSING,
        PA_OPENING_CURSOR,
        PA_WIPE,
        PA_WIPE_CURSOR,
        PA_MESH,
        PA_NO_EFFECT,
        PA_SLICE,
        PA_FADE,
        PA_CLOSING_CURSOR,
        PA_BLINDS,
        PA_DISSOLVE,
        PA_SCAN_HORIZ,
        PA_SCAN_HORIZX,
        PA_SCAN_VERT,
        PA_SCAN_VERTX,
        PA_SCAN_HORIZ,
        PA_SCAN_VERT,
        PA_SPRITE,
        PA_RANDOM};


ESP8266WebServer server(80);
// MQTT-Topics für WiFi-Daten
const char *mqtt_topic_wifi_ssid = "msdisplay/wifi/ssid";
const char *mqtt_topic_wifi_ip = "msdisplay/wifi/ip";
const char *mqtt_topic_info_swrev = "msdisplay/info/swrev";
const char *mqtt_topic_info_hwrev = "msdisplay/info/hwrev";
const char *mqtt_topic_info_status = "msdisplay/info/status";

void publishWifiData();
void reconnect();

void callback(char *topic, byte *payload, unsigned int length)
{
  String message = "";
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  // Annahme: Das Topic ist "setdisplay/lineX", wobei X die Zeilennummer ist (0-5)
  if (String(topic).startsWith("setdisplay/line"))
  {
    int lineNumber = String(topic).substring(15).toInt();
    if (lineNumber >= 0 && lineNumber < 6)
    {
      lines[lineNumber] = message;
    }
  }

  // Annahme: Das Topic ist "setmode/lineX", wobei X die Zeilennummer ist (0-5)
  if (String(topic).startsWith("setmode/line"))
  {
    JsonDocument doc;
    deserializeJson(doc, payload, length);
    int lineNumber = String(topic).substring(12).toInt();
    if (lineNumber >= 0 && lineNumber < 6)
    {
      // Extract values from JSON and store in array
      catalog[lineNumber].speed = doc["speed"];
      catalog[lineNumber].pause = doc["pause"];
      catalog[lineNumber].inFX = doc["inFX"];
      catalog[lineNumber].outFX = doc["outFX"];
    }
  }

  // Schalten des Pins über MQTT
  if (String(topic) == "switch/pin")
  {
    if (message == "ON")
    {
      digitalWrite(switchPin, HIGH);
      Serial.println("Pin eingeschaltet");
    }
    else if (message == "OFF")
    {
      digitalWrite(switchPin, LOW);
      Serial.println("Pin ausgeschaltet");
    }
  }
  // Check for Device RESET
  if (String(topic) == "msdisplay/reset" && message == "reset") {
    ESP.restart();
  }
}

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PW);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Verbunden mit WiFi");

  server.on("/", []()
    { server.send(200, "text/plain", "Hi! You can access ElegantOTA interface at http://" + WiFi.localIP().toString() + "/update"); 
  });
  ElegantOTA.begin(&server);
  server.begin();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);

  P.begin();
  P.displayClear();

  P.displayText("RESET completed & online", PA_CENTER, 2000, 0, PA_PRINT, PA_NO_EFFECT);

  // Pin als Ausgang konfigurieren
  pinMode(switchPin, OUTPUT);
  digitalWrite(switchPin, LOW); // Initialzustand: Aus

  // Veröffentliche WiFi-Daten beim Start
  publishWifiData();
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  if (P.displayAnimate())
  {
   P.displayText(lines[currentLine].c_str(), PA_CENTER, catalog[currentLine].speed, catalog[currentLine].pause, effect[catalog[currentLine].inFX], effect[catalog[currentLine].outFX]);
   currentLine = (currentLine + 1) % 6;
  }
  server.handleClient();
  ElegantOTA.loop();
}

// Funktion zum Veröffentlichen der WiFi-Daten
void publishWifiData()
{
  if (client.connected())
  {
    client.publish(mqtt_topic_wifi_ssid, WiFi.SSID().c_str());
    client.publish(mqtt_topic_wifi_ip, WiFi.localIP().toString().c_str());
    client.publish(mqtt_topic_info_swrev, swRev.c_str());
    client.publish(mqtt_topic_info_hwrev, hwRev.c_str());
    client.publish(mqtt_topic_info_status, "online", true);
    Serial.println("WiFi-Daten veröffentlicht");
  }
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Verbinde mit MQTT...");
    if (client.connect("ESP8265Client", MQTT_USER, MQTT_PASS, mqtt_topic_info_status, 1, true, "offline"))
    {
      Serial.println("verbunden");
      client.subscribe("setdisplay/line0");
      client.subscribe("setdisplay/line1");
      client.subscribe("setdisplay/line2");
      client.subscribe("setdisplay/line3");
      client.subscribe("setdisplay/line4");
      client.subscribe("setdisplay/line5");
      client.subscribe("setmode/line0");
      client.subscribe("setmode/line1");
      client.subscribe("setmode/line2");
      client.subscribe("setmode/line3");
      client.subscribe("setmode/line4");
      client.subscribe("setmode/line5");

      client.subscribe("switch/pin"); // Topic für das Schalten des Pins
      client.subscribe("msdisplay/reset"); // Topic für Device RESET

      // Veröffentliche WiFi-Daten bei erneuter Verbindung
      publishWifiData();
    }
    else
    {
      Serial.print("fehlgeschlagen, rc=");
      Serial.print(client.state());
      Serial.println(" versuche es in 5 Sekunden erneut");
      delay(5000);
    }
  }
}
