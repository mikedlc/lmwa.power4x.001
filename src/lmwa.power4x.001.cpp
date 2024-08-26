/***************************************************************************************************************************/
/*
    A library test using either a MLP201185 or MLP201188 ESP32 4-Channel CT Sensor Board
    Based On:   EmonLib https://github.com/openenergymonitor/EmonLib
    Auther:     David Mottram
    Updated:    16th May 2023
*/
/***************************************************************************************************************************/

//Device Information
const char* ProgramID = "LMWA.p4x.001";
const char* SensorType = "Boost Pumps";

// I/O items
#define Run_LED 4 

// library for the ESP32 Boards (12-bit A/D)
#define AD_1 34          // GPIO pin for A/D input channel 1
#define AD_2 35          // GPIO pin for A/D input channel 2
#define AD_3 36          // GPIO pin for A/D input channel 3
#define AD_4 39          // GPIO pin for A/D input channel 4
#define Cal_value 90   //was 6000   // calibration value
#define AD_Samples 500  // A/D samples taken per channel read
#define Sample_rate 500   // delay in uS between A/D sample readings



//Power Reading Stuff
#include <ESP32_4CH_CT.h>
// make an instance of ESP32_4CH_CT
ESP32_4CH_CT My_PCB(AD_1, AD_2, AD_3, AD_4, AD_Samples, Sample_rate, Cal_value);
int8_t ADS_Input = 0;              // A/D channel select
double PowerReadings[4] = { 0, 0, 0, 0 };  // array for results

//For 1.3in displays
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Timing
unsigned long currentMillis = 0;
int uptimeSeconds = 0;
int uptimeDays;
int uptimeHours;
int secsRemaining;
int uptimeMinutes;
char uptimeTotal[30];

//Wifi Stuff
void printWifiStatus();
#include <WiFi.h>
#include <PubSubClient.h>
//const char *ssid =	"LMWA-PumpHouse";		// cannot be longer than 32 characters!
//const char *pass =	"ds42396xcr5";		//
const char *ssid =	"WiFiFoFum";		// cannot be longer than 32 characters!
const char *pass =	"6316EarlyGlow";		//
WiFiClient wifi_client;
String wifistatustoprint;


//Tago.io server address and device token
void httpRequest();
char server[] = "api.tago.io";
String Device_Token = "******************************************"; //d1_002_pressure_sensor Default token
unsigned long lastConnectionTime = 0;            // last time you connected to the server, in milliseconds
unsigned long postingInterval = 10 * 1000; // delay between updates, in milliseconds
int counter = 1;

//MQTT Stuff
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void sendMQTT(double PowerReading);
const char* mqtt_server = "homeassistant.local";  //Your network's MQTT server (usually same IP address as Home Assistant server)
PubSubClient pubsub_client(wifi_client);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setup() {

  // setup I/O
  pinMode(Run_LED, OUTPUT);

  // start the serial connection
  Serial.begin(9600);
  delay(3000);
  Serial.println("");
  Serial.println("");
  Serial.println("Up and Clackin!");
  Serial.println(__FILE__);

  // echo settings values sent to library
  digitalWrite(Run_LED, HIGH);
  My_PCB.report();
  delay(5000);
  digitalWrite(Run_LED, LOW);

  //1.3" OLED Setup
  delay(250); // wait for the OLED to power up
  display.begin(i2c_Address, true); // Address 0x3C default
  display.display(); //Turn on
  delay(2000);

  //MQTT Setup
  pubsub_client.setServer(mqtt_server, 1883);
  pubsub_client.setCallback(callback);

  // Clear the buffer & start drawing
  display.clearDisplay(); // Clear display
  display.drawPixel(64, 64, SH110X_WHITE); // draw a single pixel
  display.display();   // Show the display buffer on the hardware.
  delay(2000); // Wait a couple
  display.clearDisplay(); // Clear display

}  // end of setup


void loop() {
  //Start keeping track of time
  currentMillis = millis();

  //Calculat Uptime
  uptimeSeconds=currentMillis/1000;
  uptimeHours= uptimeSeconds/3600;
  uptimeDays=uptimeHours/24;
  secsRemaining=uptimeSeconds%3600;
  uptimeMinutes=secsRemaining/60;
  uptimeSeconds=secsRemaining%60;
  sprintf(uptimeTotal,"Uptime %02dD:%02d:%02d:%02d",uptimeDays,uptimeHours,uptimeMinutes,uptimeSeconds);

  //Wifi Stuff
  if (WiFi.status() != WL_CONNECTED) {
    
    //Write wifi connection to display
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Booting Program ID:");
    display.println(ProgramID);
    display.println("Sensor Type:");
    display.println(SensorType);
    display.println("Connecting To WiFi:");
    display.println(ssid);
    display.println("\nWait for it......");
    display.display();

    //write wifi connection to serial
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.setHostname(ProgramID);
    WiFi.begin(ssid, pass);

    //delay 8 seconds for effect
    delay(8000);

    if (WiFi.waitForConnectResult() != WL_CONNECTED){
      return;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Boost Pum Sensor\nDevice ID: LMWA.p4x.001");
    display.setTextSize(1);
    display.println(" ");
    display.println("Connected To WiFi:");
    display.println(ssid);
    display.println(" ");
    display.display();

    Serial.println("\n\nWiFi Connected! ");
    printWifiStatus();

  }

  if (WiFi.status() == WL_CONNECTED) {
    wifistatustoprint="Wifi Connected!";
  }else{
    wifistatustoprint="Womp, No Wifi!";
  }



  // read A/D values and store in array Value[]
  // these values are representations of Amps (RMS) measured, and still require some calibration
  digitalWrite(Run_LED, HIGH);
  // sampling each channel takes around 400mS. 400 samples (20 cycles @50Hz) with a 1mS per A/D sample.
  // higher sampling rates can have issues when WiFi enabled on the ESP8266
  (PowerReadings[ADS_Input] = My_PCB.power_sample(ADS_Input)) / 100;
  digitalWrite(Run_LED, LOW);

  if (PowerReadings[ADS_Input] < .5){
    PowerReadings[ADS_Input] = 0;
  }
  // inc ready for next A/D channel
  ADS_Input++;
  if (ADS_Input > 3) {
    ADS_Input = 0;
  }  // end if

  // report results
  delay(100);

  String Report = String(PowerReadings[0]) + ", " + String(PowerReadings[1]) + ", " + String(PowerReadings[2]) + ", " + String(PowerReadings[3]) + "      ";
  Serial.println(Report);

  display.clearDisplay(); // clear the display

  //buffer next display payload
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("Sensor: "); display.println(SensorType);
  display.print("Prog.ID: "); display.println(ProgramID);
  display.print("Pump 1 Amps: "); display.println(PowerReadings[0]);
  display.print("Pump 2 Amps: "); display.println(PowerReadings[1]);
  display.print("Pump 3 Amps: "); display.println(PowerReadings[2]);
  display.print("Pump 4 Amps: "); display.println("N/A");
  display.print("IP:"); display.println(WiFi.localIP());
  display.print(uptimeTotal);

  display.display(); // Write the buffer to the display

  //Time to post data?
  //if (currentMillis - lastConnectionTime > postingInterval) {
  //  Serial.print("Time to post to tago.io at "); Serial.println(uptimeTotal);
    // then, send data to Tago
  //  httpRequest();
  //}

  sendMQTT(PowerReadings[0]);

  counter++;

}  // end of loop


// this method makes a HTTP connection to tago.io
void httpRequest() {
/*
  Serial.println("Sending this Pressure:");
  Serial.println(psi);

    // close any connection before send a new request.
    // This will free the socket on the WiFi shield
    client.stop();

    Serial.println("Starting connection to server for Pressure...");
    // if you get a connection, report back via serial:
    String PostPressure = String("{\"variable\":\"pressure\", \"value\":") + String(psi)+ String(",\"unit\":\"PSI\"}");
    String Dev_token = String("Device-Token: ")+ String(Device_Token);
    if (client.connect(server,80)) {                      // we will use non-secured connnection (HTTP) for tests
    Serial.println("Connected to server");
    // Make a HTTP request:
    client.println("POST /data? HTTP/1.1");
    client.println("Host: api.tago.io");
    client.println("_ssl: false");                        // for non-secured connection, use this option "_ssl: false"
    client.println(Dev_token);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(PostPressure.length());
    client.println();
    client.println(PostPressure);
    Serial.println("Pressure sent!\n");
    }  else {
      // if you couldn't make a connection:
      Serial.println("Server connection failed.");
    }

    client.stop();

    // note the time that the connection was made:
    lastConnectionTime = currentMillis;
    */
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  Serial.print("Hostname: ");
  Serial.println(WiFi.getHostname());

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println("");
}

//MQTT Callback
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

//connect MQTT if not
void reconnect() {
  // Loop until we're reconnected
  while (!pubsub_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random pubsub_client ID
    String clientId = "PUMPSENSOR-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (pubsub_client.connect(clientId.c_str(), "mqttuser", "Quik5ilver7")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubsub_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void sendMQTT(double PowerReading) {
  Serial.println("Sending alert via MQTT...");
  Serial.println(PowerReading);
  if (!pubsub_client.connected()) {
    reconnect();
  }

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;

    //msg variable contains JSON string to send to MQTT server
    //snprintf (msg, MSG_BUFFER_SIZE, "\{\"amps\": %4.1f, \"humidity\": %4.1f\}", temperature, humidity);
    snprintf (msg, MSG_BUFFER_SIZE, "\{\"amps\": %4.2f\}", PowerReading);
    //Due to a quirk with escaping special characters, if you're using an ESP8266 you will need to use this instead:
    //snprintf (msg, MSG_BUFFER_SIZE, "{\"temperature\": %4.1f, \"humidity\": %4.1f}", temperature, humidity);

    Serial.print("Publish message: ");
    Serial.println(msg);
    pubsub_client.publish("BOOSTPUMP01", msg);
  }

}