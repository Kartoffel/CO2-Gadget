// Library Button2 by Lennart Hennigs https://github.com/LennartHennigs/Button2

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                   GENERAL GLOBAL DEFINITIONS AND OPTIONS                          *********/
/*********                                                                                   *********/
/*****************************************************************************************************/

#define SUPPORT_BLE
// #define SUPPORT_WIFI           // HTTP SERVER NOT WORKING CURRENTLY. AWAITING FIX
// #define SUPPORT_MQTT           // Needs SUPPORT_WIFI
// #define SUPPORT_OTA            // Needs SUPPORT_WIFI - CURRENTLY NOT WORKING AWAITING FIX
// #define SUPPORT_OLED
#define SUPPORT_TFT
#define SUPPORT_ARDUINOMENU
#define ALTERNATIVE_I2C_PINS   // For the compact build as shown at https://emariete.com/medidor-co2-display-tft-color-ttgo-t-display-sensirion-scd30/

#define UNITHOSTNAME "TEST"

#include <Wire.h>
#include "soc/soc.h"           // disable brownout problems
#include "soc/rtc_cntl_reg.h"  // disable brownout problems

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                              SETUP CO2 SENSOR SCD30                               *********/
/*********                                                                                   *********/
/*****************************************************************************************************/

#include "SparkFun_SCD30_Arduino_Library.h"
SCD30 airSensor;

#ifdef ALTERNATIVE_I2C_PINS
#define I2C_SDA 22
#define I2C_SCL 21
#else
#define I2C_SDA 21
#define I2C_SCL 22
#endif

bool pendingCalibration = false;
uint16_t calibrationValue = 415;
uint16_t customCalibrationValue = 415;
bool pendingAmbientPressure = false;
uint16_t ambientPressureValue = 0;
uint16_t altidudeMeters = 600;
bool autoSelfCalibration = false;

uint16_t co2, co2Previous = 0;
float temp, hum, tempPrevious, humPrevious  = 0;

uint16_t  co2OrangeRange = 700;
uint16_t  co2RedRange = 1000;

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                              SETUP BLE FUNCTIONALITY                              *********/
/*********                                                                                   *********/
/*****************************************************************************************************/

#ifdef SUPPORT_BLE
#include "Sensirion_GadgetBle_Lib.h"
#ifdef ALTERNATIVE_I2C_PINS  // Asume "compact sandwitch version". Temp and Humididy unusable. Show just CO2 on MyAmbiance
GadgetBle gadgetBle = GadgetBle(GadgetBle::DataType::T_RH_CO2);
#else
GadgetBle gadgetBle = GadgetBle(GadgetBle::DataType::T_RH_CO2_ALT);
#endif
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                                   SETUP WIFI                                      *********/
/*********                                                                                   *********/
/*****************************************************************************************************/

#ifdef SUPPORT_WIFI
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>
// #include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "index.h"  //Web page header file

#include "credentials.h"
WiFiClient espClient;
WiFiMulti WiFiMulti;
AsyncWebServer server(80);

void onWifiSettingsChanged(std::string ssid, std::string password) {
  Serial.print("WifiSetup: SSID = ");
  Serial.print(ssid.c_str());
  Serial.print(", Password = ");
  Serial.println(password.c_str());

  WiFiMulti.addAP(ssid.c_str(), password.c_str());
}

////===============================================================
//// This function is called when you open its IP in browser
////===============================================================
//void handleRoot() {
// String s = MAIN_page; //Read HTML contents
// server.send(200, "text/html", s); //Send web page
//}
//
//void handleADC() {
// int a = analogRead(A0);
// String co2Value = String(co2);
//
// server.send(200, "text/plane", co2Value); //Send ADC value only to client ajax request
//}
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                            SETUP MQTT FUNCTIONALITY                               *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_MQTT
#include <PubSubClient.h>

// ----------------------------------------------------------------------------
// MQTT handling
// ----------------------------------------------------------------------------
// const char *mqtt_server = "192.168.1.145";
String rootTopic;
String topic;
char charPublish[20];
PubSubClient mqttClient(espClient);
#endif

#ifdef SUPPORT_MQTT
void mqttReconnect()
{
  if (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    topic = rootTopic + "/#";
    if (mqttClient.connect((topic).c_str()))
    {
      Serial.println("connected");
      // Subscribe
      mqttClient.subscribe((topic).c_str());
    }
    else
    {
      Serial.println(" not possible to connect");
    }
  }

  // Loop until we're reconnected
  // while (!mqttClient.connected())
  // {
  //   Serial.print("Attempting MQTT connection...");
  //   // Attempt to connect
  //   topic = rootTopic + "/#";
  //   if (mqttClient.connect((topic).c_str()))
  //   {
  //     Serial.println("connected");
  //     // Subscribe
  //     mqttClient.subscribe((topic).c_str());
  //   }
  //   else
  //   {
  //     Serial.print("failed, rc=");
  //     Serial.print(mqttClient.state());
  //     Serial.println(" try again in 5 seconds");
  //     // Wait 5 seconds before retrying
  //     delay(5000);
  //   }
  // }
}

// Function called when data is received via MQTT
void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Payload: ");
  String messageTemp;
  String topicTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (strcmp(topic, "SCD30/calibration") == 0) {
    printf("Received calibration command with value %d\n", messageTemp.toInt());
    pendingCalibration = true;
    calibrationValue = messageTemp.toInt();
  }

  if (strcmp(topic, "SCD30/ambientpressure") == 0) {
    printf("Received ambient pressure with value %d\n", messageTemp.toInt());
    pendingAmbientPressure = true;
    ambientPressureValue = messageTemp.toInt();
  }
}

void publishIntMQTT(String topic, int16_t payload)
{
  dtostrf(payload, 0, 0, charPublish);
  topic = rootTopic + topic;
  Serial.printf("Publishing %d to ", payload);
  Serial.println("topic: " + topic);
  mqttClient.publish((topic).c_str(), charPublish);
}

void publishFloatMQTT(String topic, float payload)
{
  dtostrf(payload, 0, 2, charPublish);
  topic = rootTopic + topic;
  Serial.printf("Publishing %.0f to ", payload);
  Serial.println("topic: " + topic);
  mqttClient.publish((topic).c_str(), charPublish);
}
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                             SETUP OTA FUNCTIONALITY                               *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_OTA
#include <AsyncElegantOTA.h>
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                             SETUP BATTERY FUNCTIONALITY                           *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#define ADC_PIN 34
int vref = 1100;
#include "CO2_Gadget_Battery.h"

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                          SETUP OLED DISPLAY FUNCTIONALITY                         *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_OLED
#include <U8x8lib.h>
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
char oled_msg[20];
int displayWidth = 128;
int displayHeight = 64;

#endif

void initDisplayOLED() {
#if defined SUPPORT_OLED
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 1, "  eMariete.com");
  u8x8.drawString(0, 2, "   Sensirion");
  u8x8.drawString(0, 3, "SCD30 BLE Gadget");
  u8x8.drawString(0, 4, "  CO2 Monitor");
#endif
}

void showValuesOLED(String text) {
#if defined SUPPORT_OLED
  u8x8.clearLine(2);
  u8x8.clearLine(3);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 4, "CO2: ");
  u8x8.setFont(u8x8_font_courB18_2x3_r);
  sprintf(oled_msg, "%4d", co2); // If parameter string then: co2.c_str()
  u8x8.drawString(4, 3, oled_msg);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(12, 4, "ppm");

  u8x8.clearLine(6);
  sprintf(oled_msg, "T:%.1fC RH:%.0f%%", temp, hum);
  u8x8.drawUTF8(0, 6, oled_msg);

#ifdef SUPPORT_WIFI
  if (WiFiMulti.run() != WL_CONNECTED) {
    u8x8.clearLine(7);
    u8x8.drawUTF8(0, 6, "WiFi unconnected");
  } else {
    u8x8.clearLine(7);
    IPAddress ip = WiFi.localIP();
    sprintf(oled_msg, "%s", ip.toString().c_str());
    // sprintf("IP:%u.%u.%u.%u\n", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, ip >> 24);
    u8x8.drawString(0, 7, oled_msg);
  }
#endif
#endif
}

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                          SETUP TFT DISPLAY FUNCTIONALITY                          *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_TFT
// Go to TTGO T-Display's Github Repository
// Download the code as zip, extract it and copy the Folder TFT_eSPI
//  => https://github.com/Xinyuan-LilyGO/TTGO-T-Display/archive/master.zip
// to your Arduino library path
#include <TFT_eSPI.h>
#include <SPI.h>
#include "bootlogo.h"

#define SENSIRION_GREEN 0x6E66
#define sw_version "v0.1"

#define GFXFF 1
#define FF99  &SensirionSimple25pt7b
#define FF90  &ArchivoNarrow_Regular10pt7b
#define FF95  &ArchivoNarrow_Regular50pt7b

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke library, pins defined in User_Setup.h
// TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h
#endif

void initDisplayTFT() {
#if defined SUPPORT_TFT
  tft.init();
  tft.setRotation(1);
#endif
}

void displaySplashScreenTFT() {
#if defined SUPPORT_TFT
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(SENSIRION_GREEN, TFT_WHITE);

  uint8_t defaultDatum = tft.getTextDatum();
  tft.setTextDatum(1); // Top centre

  tft.setTextSize(1);
  tft.setFreeFont(FF99);
  tft.drawString("B", 120, 40);

  tft.setTextSize(1);
  tft.drawString(sw_version, 120, 90, 2);

  // Revert datum setting
  tft.setTextDatum(defaultDatum);
  delay(500);
  tft.fillScreen(TFT_WHITE);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0,  240, 135, bootlogo);

#endif
}

void showValuesTFT(uint16_t co2) {
#if defined SUPPORT_TFT
  if (co2 > 9999) {
    co2 = 9999;
  }

  tft.fillScreen(TFT_BLACK);

  uint8_t defaultDatum = tft.getTextDatum();

  tft.setTextSize(1);
  tft.setFreeFont(FF90);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setTextDatum(6); // bottom left
  tft.drawString("CO2", 10, 125);

  tft.setTextDatum(8); // bottom right
  tft.drawString(gadgetBle.getDeviceIdString(), 230, 125);

  // Draw CO2 number
  if (co2 >= co2RedRange) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  } else if (co2 >= co2OrangeRange) {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  }

  tft.setTextDatum(8); // bottom right
  tft.setTextSize(1);
  tft.setFreeFont(FF95);
  tft.drawString(String(co2), 195, 105);

  // Draw CO2 unit
  tft.setTextSize(1);
  tft.setFreeFont(FF90);
  tft.drawString("ppm", 230, 90);

  // Revert datum setting
  tft.setTextDatum(defaultDatum);

  // set default font for menu
  tft.setFreeFont(NULL);
  tft.setTextSize(2);
#endif
}

void showVoltageTFT()
{
#if defined SUPPORT_TFT
  static uint64_t timeStamp = 0;
  if (millis() - timeStamp > 1000) {
    timeStamp = millis();
    String voltage = "Voltage :" + String(battery_voltage) + "V";
    Serial.println(voltage);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(voltage,  tft.width() / 2, tft.height() / 2 );
  }
#endif
}

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                                SETUP ARDUINOMENU                                  *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_ARDUINOMENU
#include <CO2_Gadget_Menu.h>
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                          SETUP PUSH BUTTONS FUNCTIONALITY                         *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#include "Button2.h"
// Button2 button;
// #define BUTTON_PIN  35 // Menu button (Press > 1500ms for calibration, press > 500ms to show battery voltage)
// void longpress(Button2& btn);
#define LONGCLICK_MS 300 // https://github.com/LennartHennigs/Button2/issues/10
#define BTN_UP 35 // Pinnumber for button for up/previous and select / enter actions
#define BTN_DWN 0 // Pinnumber for button for down/next and back / exit actions
Button2 btnUp(BTN_UP); // Initialize the up button
Button2 btnDwn(BTN_DWN); // Initialize the down button

void button_init()
{
#if defined SUPPORT_ARDUINOMENU
    btnUp.setLongClickHandler([](Button2 & b) {
        // Old way without #define LONGCLICK_MS 300
        // unsigned int time = b.wasPressedFor();
        // if (time >= 300) {
        //   nav.doNav(enterCmd);
        // }
        nav.doNav(enterCmd);
    });
    
    btnUp.setClickHandler([](Button2 & b) {
       // Up
       nav.doNav(downCmd); // It's called downCmd because it decreases the index of an array. Visually that would mean the selector goes upwards.
    });

    btnDwn.setLongClickHandler([](Button2 & b) {
        // // Exit
        // unsigned int time = b.wasPressedFor();
        // if (time >= 300) {
        //   nav.doNav(escCmd);
        // }
        nav.doNav(escCmd);
    });
    
    btnDwn.setClickHandler([](Button2 & b) {
        // Down
        nav.doNav(upCmd); // It's called upCmd because it increases the index of an array. Visually that would mean the selector goes downwards.
    });
#endif
}

void button_loop()
{
    // Check for button presses
    btnUp.loop();
    btnDwn.loop();
}

/*****************************************************************************************************/

static int64_t lastMmntTime = 0;
static int startCheckingAfterUs = 1900000;

void initMQTT()
{
#ifdef SUPPORT_MQTT
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  rootTopic = UNITHOSTNAME;
#endif
}

void processPendingCommands()
{
  if (pendingCalibration == true) {
    if (calibrationValue != 0) {
      printf("Calibrating SCD30 sensor at %d PPM\n", calibrationValue);
      pendingCalibration = false;
      airSensor.setForcedRecalibrationFactor(calibrationValue);
    }
    else
    {
      printf("Avoiding calibrating SCD30 sensor with invalid value at %d PPM\n", calibrationValue);
      pendingCalibration = false;
    }
  }

  if (pendingAmbientPressure == true) {
    if (ambientPressureValue != 0) {
      printf("Setting AmbientPressure for SCD30 sensor at %d mbar\n", ambientPressureValue);
      pendingAmbientPressure = false;
      airSensor.setAmbientPressure(ambientPressureValue);
    }
    else
    {
      printf("Avoiding setting AmbientPressure for SCD30 sensor with invalid value at %d mbar\n", ambientPressureValue);
      pendingAmbientPressure = false;
    }
  }
}

void setup()
{
  uint32_t brown_reg_temp = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG); //save WatchDog register
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

#if defined SUPPORT_OLED
  delay(100);
  initDisplayOLED();
  delay(1000);
#endif

#if defined SUPPORT_TFT
  initDisplayTFT();  
  displaySplashScreenTFT(); // Display init and splash screen  
  delay(2000); // Enjoy the splash screen for 2 seconds
  // tft.fillScreen(Black);
  tft.setTextSize(2);
#endif

#if defined SUPPORT_BLE && defined SUPPORT_WIFI
  // Initialize the GadgetBle Library
  gadgetBle.enableWifiSetupSettings(onWifiSettingsChanged);
  gadgetBle.setCurrentWifiSsid(WIFI_SSID_CREDENTIALS);
#endif

#ifdef SUPPORT_WIFI
  WiFiMulti.addAP(WIFI_SSID_CREDENTIALS, WIFI_PW_CREDENTIALS);
  while (WiFiMulti.run() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  /*use mdns for host name resolution*/
  if (!MDNS.begin(UNITHOSTNAME)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.print("mDNS responder started. CO2 monitor web interface at: http://");
  Serial.print(UNITHOSTNAME);
  Serial.println(".local");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "CO2: " + String(co2) + " PPM");
    //  server.on("/", handleRoot);      //This is display page
    //  server.on("/readADC", handleADC);//To get update of ADC Value only
  });

#ifdef SUPPORT_OTA
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  Serial.println("OTA ready");
#endif

  server.begin();
  Serial.println("HTTP server started");
#endif

#ifdef SUPPORT_BLE
  // Initialize the GadgetBle Library
  gadgetBle.begin();
  Serial.print("Sensirion GadgetBle Lib initialized with deviceId = ");
  Serial.println(gadgetBle.getDeviceIdString());
#endif

  // Initialize the SCD30 driver
  Wire.begin(I2C_SDA, I2C_SCL);
  if (airSensor.begin() == false)
  {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    //    while (1)
    //      ;
  }
  else
  {
    airSensor.setAutoSelfCalibration(autoSelfCalibration);
  }

  initMQTT();

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, brown_reg_temp); //enable brownout detector

  readBatteryVoltage();
  button_init();
  menu_init();

  Serial.println("Ready.");
}

void loop()
{
#ifdef SUPPORT_MQTT
  mqttClient.loop();
#endif

  processPendingCommands();

  if (esp_timer_get_time() - lastMmntTime >= startCheckingAfterUs) {
    if (airSensor.dataAvailable()) {
      co2Previous = co2;
      tempPrevious = temp;
      humPrevious = hum;
      co2  = airSensor.getCO2();
      temp = airSensor.getTemperature();
      hum  = airSensor.getHumidity();
      if (co2Previous != co2) {
        nav.idleChanged=true;
      }
      #if defined SUPPORT_OLED
      showValuesOLED(String(co2));
      #endif
      #if defined SUPPORT_TFT
      #ifndef SUPPORT_ARDUINOMENU
      showValuesTFT(co2);
      #endif
      #endif

      #ifdef SUPPORT_BLE
      gadgetBle.writeCO2(co2);
      gadgetBle.writeTemperature(temp);
      gadgetBle.writeHumidity(hum);
      gadgetBle.commit();
      #endif
      lastMmntTime = esp_timer_get_time();

      // Provide the sensor values for Tools -> Serial Monitor or Serial Plotter
      Serial.print("CO2[ppm]:");
      Serial.print(co2);
      Serial.print("\t");
      Serial.print("Temperature[℃]:");
      Serial.print(temp);
      Serial.print("\t");
      Serial.print("Humidity[%]:");
      Serial.println(hum);

//      Serial.print("Free heap: ");
//      Serial.println(ESP.getFreeHeap());



      #ifdef SUPPORT_WIFI
      if (WiFiMulti.run() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
      } else {
        Serial.print("WiFi connected - IP = ");
        Serial.println(WiFi.localIP());
        if (!mqttClient.connected())
        {
          mqttReconnect();
        }
      }
      #endif

      #if defined SUPPORT_MQTT && defined SUPPORT_WIFI
      if (WiFiMulti.run() == WL_CONNECTED) {
        publishIntMQTT("/co2", co2);
        publishFloatMQTT("/temp", temp);
        publishFloatMQTT("/humi", hum);
      }
      #endif
    }
  }

#ifdef SUPPORT_BLE
  gadgetBle.handleEvents();
  delay(3);
#endif

#ifdef SUPPORT_OTA
  AsyncElegantOTA.loop();
#endif

  button_loop();
#ifdef SUPPORT_ARDUINOMENU  
  nav.poll();//this device only draws when needed
#endif  
}

// void longpress(Button2& btn) {
//   unsigned int time = btn.wasPressedFor();
//   Serial.print("You clicked ");
//   if (time > 3000) {
//     Serial.print("a really really long time.");
//     calibrationValue = 400;
//     printf("Manually calibrating SCD30 sensor at %d PPM\n", calibrationValue);
//     airSensor.setForcedRecalibrationFactor(calibrationValue);
//     pendingCalibration = false;
//   } else if (time > 1000) {
//     Serial.print("a really long time.");
//   } else if (time > 500) {
//     Serial.print("a long time.");
//     showVoltageTFT();
//     delay(2000);
//   } else {
//     Serial.print("long.");
//   }
//   Serial.print(" (");
//   Serial.print(time);
//   Serial.println(" ms)");
// }
