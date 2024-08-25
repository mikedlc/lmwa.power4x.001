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
const char* SensorType = "Power";

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
double Value[4] = { 0, 0, 0, 0 };  // array for results

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


void setup() {

  // setup I/O
  pinMode(Run_LED, OUTPUT);

  // start the serial connection
  Serial.begin(9600);
  delay(1000);
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


  Serial.println("I'm Alive!");
  // read A/D values and store in array Value[]
  // these values are representations of Amps (RMS) measured, and still require some calibration
  digitalWrite(Run_LED, HIGH);
  // sampling each channel takes around 400mS. 400 samples (20 cycles @50Hz) with a 1mS per A/D sample.
  // higher sampling rates can have issues when WiFi enabled on the ESP8266
  (Value[ADS_Input] = My_PCB.power_sample(ADS_Input)) / 100;
  digitalWrite(Run_LED, LOW);

  // inc ready for next A/D channel
  ADS_Input++;
  if (ADS_Input > 3) {
    ADS_Input = 0;
  }  // end if

  // report results
  delay(100);

  String Report = String(Value[0]) + ", " + String(Value[1]) + ", " + String(Value[2]) + ", " + String(Value[3]) + "      ";
  Serial.println(Report);

  display.clearDisplay(); // clear the display

  //buffer next display payload
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("Sensor: "); display.println(SensorType);
  display.print("Prog. ID: "); display.println(ProgramID);
  //display.print("IP:"); display.println(WiFi.localIP());
  display.print(uptimeTotal);

  display.display(); // Write the buffer to the display
}  // end of loop