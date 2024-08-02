
#include <Arduino.h>
#include "PMS.h"
#include <MQUnifiedsensor.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <FirebaseFirestore.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Connect To wifi And Setup Firebase
#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-wifi-pass"
#define PROJECT_ID "your-firebase-projectId"
#define API_KEY "your-firebase-project-api-key"

//timestamp | adjust to your local time
#include <TimeLib.h>
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;

// Pin Alarm
// #define buzzer 2
#define LED 4

//DHT
#include "DHT.h"
DHT dht(15, DHT22);
float t;
float h;

// Configuration Firebase
FirebaseData fbdo;  
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

//LCD Configuration
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1
#define ID 002
Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,OLED_RESET);

// PMS Configuration
PMS pms(Serial2);
PMS::DATA data;
float particulate;
int pm2;

// Sulfur Configuration
#define placa "ESP32"
#define Voltage_Resolution 3.3
#define pin 34 //Analog input 0 of your arduino
#define type "MQ-136" //MQ136
#define ADC_Bit_Resolution 12 // For arduino UNO/MEGA/NANO
#define RatioMQ136CleanAir 3.6//RS / R0 = 3.6 ppm  
float sulfur;
MQUnifiedsensor MQ136(placa, Voltage_Resolution, ADC_Bit_Resolution, pin, type);

// Ammonia Configuration
#define ammonia 35
float m = -0.263; // Slope
float b = 0.42; // Y-Intercept
float VRL; // Define variable for sensor voltage 
float RS_air; // Define variable for sensor resistance
float R0 ; // Define variable for R0
float RL = 47;
float ratio; // Define variable for ratio
float sensorValue; // Define variable for analog readings 
float ppm;
int baca;
float analog_value;
float VRL1;
float Rs1;

//RTOS Configuration
TaskHandle_t mainTask; //nama bebas
TaskHandle_t DisplayTask; //nama bebas

// Calibrate RO One Time Ammonia Sensor
void Sensor1(){
for(int test_cycle = 1 ; test_cycle <= 500 ; test_cycle++) //Read the analog output of the sensor for 200 times
  {
    analog_value = analog_value + analogRead(ammonia); //add the values for 200
  }
  analog_value = analog_value/500.0; //Take average
  VRL1 = analog_value*(3.3/4095.0); //Convert analog value to voltage
  //RS = ((Vc/VRL)-1)*RL is the formulae we obtained from datasheet
  Rs1= ((3.3/VRL1)-1) * RL;
  //RS/RO is 3.6 as we obtained from graph of datasheet
  R0= Rs1/3.6;
}

void programCore0(void *pvParameters){ 
  for(;;){
  // Display LCD 
  display.clearDisplay();  //clear display
  display.setTextSize(1);   // display temperature
  display.setCursor(10,3);
  display.print("Temperature ");
  display.setTextSize(1);
  display.setCursor(85,3);
  display.print(t);

  display.setTextSize(1);   // display humidity
  display.setCursor(10, 17);
  display.print("Humidity ");
  display.setTextSize(1);
  display.setCursor(85, 15);
  display.print(h);

  display.setTextSize(1);     // display NH3
  display.setCursor(10, 27);
  display.print("NH3 ");
  display.setTextSize(1);
  display.setCursor(85, 27);
  display.print(ppm);

  
  display.setTextSize(1);     // display sulfur
  display.setCursor(10, 37);
  display.print("Sulfur ");
  display.setTextSize(1);
  display.setCursor(85, 37);
  display.print(sulfur);
 

 display.setTextSize(1);     // display sulfur
  display.setCursor(10, 47);
  display.print("PMS ");
  display.setTextSize(1);
  display.setCursor(85, 47);
  display.print(pm2);
  display.display(); 
  }
  
}
// Program Display To LCD
void programCore1(void *pvParameters){ 

  for(;;){
  // Particulate Readings
  pms.read(data);
  particulate = data.PM_AE_UG_2_5;
  pm2 = data.PM_AE_UG_2_5;
  
  // Pembacaan Sulfur
  MQ136.update();
  sulfur = MQ136.readSensor();
  
  // Ammonia Readings
   VRL = analogRead(ammonia) * (3.3 / 4095.0);
   RS_air = ((3.3 * RL) / VRL) - RL;
   ratio = RS_air / R0;
   ppm = pow(10, ((log10(ratio) - b) / m));
  
  //DHT
   t = dht.readTemperature();
   h = dht.readHumidity();
   delay(100);

  }
}

void setup() {
   // Display Configuration
  Serial.begin(9600);
  Serial2.begin(9600);

  // Set NTP server addresses via DHCP
  // sntp_servermode_dhcp(1);

  // Configure time with NTP servers and time offsets
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  
  //DHT
  dht.begin();
  // Connect to Firebase
    Serial.println("OK");
    // Koneksi ke WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting To Wifi ");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED) {
      Serial.println("Not Connected to Wifi...");
      delay(300);
      attempts++;
      if (attempts > 5){
        ESP.restart();
      }
    }
    Serial.print("Connect to Wifi...");
    Serial.println(WiFi.localIP());

    // 
    config.api_key= API_KEY;
   

if (Firebase.signUp(&config, &auth, "", "")){
    Serial.print("OK");
    signupOK = true;
} else {
  // Serial.printf("%s\n", config.signer.signupError.message.c_str());
}
config.token_status_callback = tokenStatusCallback;
    // Inisialisasi Firebase
//    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Serial.println("Terhubung ke Firebase...");
    digitalWrite (LED,HIGH);
   
  // LCD Set
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);
 display.display();

  // Konfigurasi PinMode
  pinMode(ammonia, INPUT);
  pinMode(pin, INPUT);
  // pinMode(buzzer,OUTPUT);
  pinMode(LED,OUTPUT);
 

  // // Sulfur dan Ammonia Configuration
  Sensor1();
  MQ136.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ136.setA(36.737); MQ136.setB(-3.536); // Configure the equation to to calculate H2S Concentration 
  MQ136.init();
  float calcR0 = 3.6;
  for(int i = 1; i<=10; i ++)
  {
    MQ136.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0 += MQ136.calibrate(RatioMQ136CleanAir);
    Serial.print(".");
  }
  MQ136.setR0(calcR0/10);
  
// Konfigurasi Core to Core
  xTaskCreatePinnedToCore(
    programCore0, //nama bebas, ini nama fungsinya
    "mainTask", //sesuaikan sama nama yang di "TaskHandle_t"
    10000,
    NULL,
    1,
    &mainTask,
    0 //ini core yang mau dipake
  );
  xTaskCreatePinnedToCore(
    programCore1, //nama bebas, ini nama fungsinya
    "DisplayTask", //sesuaikan sama nama yang di "TaskHandle_t"
    10000,
    NULL,
    1,
    &DisplayTask,
    1 //ini core yang mau dipake
  );
}

void loop() {
  while (true) {
    FirebaseJson content;

    pms.read(data);
    particulate = data.PM_AE_UG_2_5;
    pm2 = data.PM_AE_UG_2_5;

    MQ136.update();
    sulfur = MQ136.readSensor();

    VRL = analogRead(ammonia) * (3.3 / 4095.0);
    RS_air = ((3.3 * RL) / VRL) - RL;
    ratio = RS_air / R0;
    ppm = pow(10, ((log10(ratio) - b) / m));

    t = dht.readTemperature();
    h = dht.readHumidity();

    time_t epochTime = time(nullptr);
    char formattedTimestamp[80];
    strftime(formattedTimestamp, sizeof(formattedTimestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));


    content.set("fields/temperature/doubleValue", t);
    content.set("fields/humidity/doubleValue", h);
    content.set("fields/ammonia/doubleValue", ppm);
    content.set("fields/sulfur/doubleValue", sulfur);
    content.set("fields/particulate/doubleValue", particulate);
    content.set("fields/timestamp/timestampValue", formattedTimestamp);



  // Path dokumen
  String documentPath = "sensors/PabrikA/logData/";

  // Send sensor data to Firebase Firestore with different IDs
 if (Firebase.Firestore.patchDocument(&fbdo, PROJECT_ID, "", documentPath.c_str(), content.raw(), "particulate, sulfur, ammonia, temperature, humidity, timestamp")) {
    // ... Handle successful document update
  } else {
    // ... Handle failed document update
  }

  if (Firebase.Firestore.createDocument(&fbdo, PROJECT_ID, "", documentPath.c_str(), content.raw())) {
    // ... Handle successful document creation
  } else {
    // ... Handle failed document creation
  }

  delay(60000);
  }
}