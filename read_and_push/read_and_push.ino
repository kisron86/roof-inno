#include <ESP8266WebServer.h> //memasukan library NodeMCU sebagai web server
#include <ESP8266WiFi.h> //memasukan library NodeMCU
#include <WiFiClient.h> //memasukan library NodeMCU sebagai client
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include "HLW8012.h"


String ID_DEVICE = "roof001";
String PASSWORD_DEVICE = "roof001";

const char *ssid = "Luthfan48";
const char *password = "0313537387";

float power = 0;
float voltage = 220;
float current = 0;
unsigned long measureDuration = 5000; //ms
unsigned long measureMillis = 0;    
unsigned long statusDuration = 2000; //ms
unsigned long statusMillis = 0;

const char* hostGet = "roof-inno.com";
bool isSave = false;

// GPIOs
#define RELAY_PIN                       12
#define SEL_PIN                         5
#define CF1_PIN                         13
#define CF_PIN                          14

#define CURRENT_MODE                    HIGH


// These are the nominal values for the resistors in the circuit
#define CURRENT_RESISTOR                0.001
#define VOLTAGE_RESISTOR_UPSTREAM       ( 5 * 470000 ) // Real: 2280k
#define VOLTAGE_RESISTOR_DOWNSTREAM     ( 1000 ) // Real 1.009k

double currentMulti, voltageMulti, powerMulti;

HLW8012 hlw8012;

void EEPROM_ESP8266_GRABAR(String buffer, int N) {
  EEPROM.begin(512); delay(10);
  for (int L = 0; L < 32; ++L) {
    EEPROM.write(N + L, buffer[L]);
  }
  EEPROM.commit();
}
//
String EEPROM_ESP8266_LEER(int min, int max) {
  EEPROM.begin(512); delay(10); String buffer;
  for (int L = min; L < max; ++L)
    buffer += char(EEPROM.read(L));
  return buffer;
}
bool isModeMeasure;

void saveSetting(String ssid_params, String pass_params){
  EEPROM_ESP8266_GRABAR(ssid_params, 0); //Primero de 0 al 32, del 32 al 64, etc
  EEPROM_ESP8266_GRABAR(pass_params, 32); //SAV
  
  Serial.print("SSID: ");
  Serial.println(EEPROM_ESP8266_LEER(0, 32));//Primero de 0 al 32, del 32 al 64, etc
  Serial.print("Pass: ");
  Serial.println(EEPROM_ESP8266_LEER(32, 64));
}

void setupModeMeasure(){
  Serial.print("SSID: ");
  Serial.println(EEPROM_ESP8266_LEER(0, 32));//Primero de 0 al 32, del 32 al 64, etc
  Serial.print("Pass: ");
  Serial.println(EEPROM_ESP8266_LEER(32, 64));
  
  WiFi.begin(EEPROM_ESP8266_LEER(0,32).c_str(), EEPROM_ESP8266_LEER(32,64).c_str());
  Serial.println("Connecting to Wi-Fi");
  delay(3000);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.println("-----------------------------------");
  Serial.println("----------Begin Set Test-----------");
  Serial.println("-----------------------------------");
  Serial.println();

  hlw8012.begin(CF_PIN, CF1_PIN, SEL_PIN, CURRENT_MODE, false, 2000000);
  hlw8012.setResistors(CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);
//  calibrate(&currentMulti, &voltageMulti, &powerMulti);

  hlw8012.setCurrentMultiplier(12249.82);   // change to determined values
  hlw8012.setVoltageMultiplier(430143.70);
  hlw8012.setPowerMultiplier(12537711.19);
}

void postData() {
  WiFiClient clientGet;
  const int httpGetPort = 80;

   //the path and file to send the data to:
  String urlGet = "/Api/kirim_data";
  
  urlGet += "?id=" + ID_DEVICE + "&p=" + String(power, 3) + "&v=" + String(voltage, 3) + "&i=" + String(current, 3);
   
  Serial.print(">>> Connecting to host: ");
  Serial.println(hostGet);
      
  if (!clientGet.connect(hostGet, httpGetPort)) {
    Serial.print("Connection failed: ");
    Serial.print(hostGet);
  } else {
    Serial.print(urlGet);
    
    clientGet.println("GET " + urlGet + " HTTP/1.1");
    clientGet.print("Host: ");
    clientGet.println(hostGet);
    clientGet.println("User-Agent: ESP8266/1.0");
    clientGet.println("Connection: close\r\n\r\n");
          
    unsigned long timeoutP = millis();
    while (clientGet.available() == 0) {
            
      if (millis() - timeoutP > 10000) {
        Serial.print(">>> Client Timeout: ");
        Serial.println(hostGet);
        clientGet.stop();
        return;
      }
    }

    //just checks the 1st line of the server response. Could be expanded if needed.
    while(clientGet.available()){
      String retLine = clientGet.readStringUntil('\r');
      Serial.println(retLine);
      break; 
    }
  } //end client connection if else
                        
  Serial.print(">>> Closing host: ");
  Serial.println(hostGet);
          
  clientGet.stop();

}

void getDataStatus() {
  HTTPClient http;
  http.begin("http://roof-inno.com/Api/ambil_status/?kode="+ID_DEVICE);
  int statusCode = http.GET();
  String output = http.getString();
  Serial.println(output);
  if(output.equals("{\"is_status\":\"1\"}")){
    digitalWrite(RELAY_PIN, LOW);
  }else{
    digitalWrite(RELAY_PIN, HIGH);
  }
  http.end();
}

void loopModeMeasure(){ 
  while ((unsigned long) millis() > measureMillis + (unsigned long) measureDuration){
    measureMillis = millis();

    hlw8012.setMode(MODE_VOLTAGE);
    voltage = hlw8012.getVoltage();
    voltage = 220.0;
    
    hlw8012.setMode(MODE_CURRENT);
    current = hlw8012.getCurrent();
    
    power = hlw8012.getActivePower();
    //power = (float) current * voltage;
    
    Serial.print("current: ");
    Serial.println(current);
    Serial.print("voltage: ");
    Serial.println(voltage);
    Serial.print("power: ");
    Serial.println(power);
    postData();
    
    hlw8012.toggleMode();
  }

  while ((unsigned long) millis() > statusMillis + (unsigned long) statusDuration){
    statusMillis = millis();
    getDataStatus();
  }
}


void setup() {
  delay(1000); //waktu tunda 1 detik
    Serial.begin(115200);
    Serial.println();

    /*pinMode(MODE_PIN, INPUT_PULLUP);//input mode
    pinMode(LED_PIN, OUTPUT);//led mode
    digitalWrite(LED_PIN, LOW);*/

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);

    saveSetting(ssid, password);
    
    if(true){
      isModeMeasure = true;
      setupModeMeasure();
      //digitalWrite(LED_PIN, HIGH);
      delay(5000);
    }else{
      isModeMeasure = false;
      //setupModeSetting();
      //digitalWrite(LED_PIN, HIGH);
      delay(500);
     // digitalWrite(LED_PIN, LOW);
      delay(500);
      //digitalWrite(LED_PIN, HIGH);
    }
}

void loop() {
  // put your main code here, to run repeatedly:
  loopModeMeasure();
}
