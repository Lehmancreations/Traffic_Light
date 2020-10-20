#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson (version 5 not 6)
#include <ESP8266mDNS.h>


//This was built for a NodeMCU
#define D5 14 // SPI Bus SCK (clock)
#define D6 12 // SPI Bus MISO
#define D7 13 // SPI Bus MOSI

#define RED D5
#define YELLOW D6
#define GREEN D7

//define your default values here, if there are different values in config.json, they are overwritten.
char host[34];
char httpUser[40];
char httpPass[40];

//Form Custom SSID
String ssidAP = "Traffic Light";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () 
{
  shouldSaveConfig = true;
}

ESP8266WebServer server(80);
WiFiClient net;


uint8_t red = 0;
uint8_t yellow = 0;
uint8_t green = 1;

uint8_t sequence = 0;

uint8_t flashRed = 0;
uint8_t flashYellow = 0;
uint8_t flashGreen = 0;


void setup() {
  // setup LEDs
  pinMode(RED, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);

  setLights(LOW, HIGH, LOW);

  Serial.begin(115200);

  // setup wifi - STATUS blinking red

 setLights(HIGH, LOW, LOW);

  if (SPIFFS.begin()) 
  {
    if (SPIFFS.exists("/config.json")) 
    {
      //file exists, reading and loading
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) 
      {
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) 
        {
          strcpy(host, json["host"]);
          strcpy(httpUser, json["httpUser"]);
          strcpy(httpPass, json["httpPass"]);
          
        } 
        else 
        {
        }
      }
    }
  } 
  else 
  {
  }
  //end read


// The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_text0("<p>Select your wifi network and type in your password, if you do not see your wifi then scroll down to the bottom and press scan to check again.</p>");
  WiFiManagerParameter custom_text1("<h1>Hostname/MQTT ID</h1>");
  WiFiManagerParameter custom_text2("<p>Enter a name for this device which will be used for the hostname on your network.</p>");
  WiFiManagerParameter custom_host("name", "Device Name", host, 32);
  
  WiFiManagerParameter custom_text5("<h1>API User/Pass</h1>");
  WiFiManagerParameter custom_text6("<p> Please input a username and password to use with the API </p>");
  WiFiManagerParameter custom_update_username("user", "Username For Web Updater", httpUser, 40);
  WiFiManagerParameter custom_update_password("password", "Password For Web Updater", httpPass, 40);

  
 //WiFiManager
//Local intialization. Once its business is done, there is no need to keep it around

  WiFiManager wifiManager;

  //set hostname
  wifi_station_set_hostname();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  
  //add all your parameters here
  wifiManager.setCustomHeadElement("<style>.c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:95%;} body{text-align: center;font-family:oswald;} button{border:0;background-color:#313131;color:white;line-height:2.4rem;font-size:1.2rem;text-transform: uppercase;width:100%;font-family:oswald;} .q{float: right;width: 65px;text-align: right;} body{background-color: #575757;}h1 {color: white; font-family: oswald;}p {color: white; font-family: open+sans;}a {color: #78C5EF; text-align: center;line-height:2.4rem;font-size:1.2rem;font-family:oswald;}</style>");
  wifiManager.addParameter(&custom_text0);
  wifiManager.addParameter(&custom_text1);
  wifiManager.addParameter(&custom_text2);
  wifiManager.addParameter(&custom_host);


  wifiManager.addParameter(&custom_text5);
  wifiManager.addParameter(&custom_text6);
  wifiManager.addParameter(&custom_update_username);
  wifiManager.addParameter(&custom_update_password);

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(ssidAP.c_str())) 
  {
    delay(3000);
    // Possibly put a string to the serial or make it blink a light
    delay(5000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //read updated parameters
  strcpy(host, custom_host.getValue());
  strcpy(httpUser, custom_update_username.getValue());
  strcpy(httpPass, custom_update_password.getValue());
 
  //save the custom parameters to FS
  if (shouldSaveConfig) 
  {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["host"] = host;
    json["httpUser"] = httpUser;
    json["httpPass"] = httpPass;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) 
    {
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  
  
  MDNS.begin(host);
  
  
 

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  setLights(LOW, HIGH, LOW);

  // setup server - STATUS yellow
  server.on("/api/lights", lightsApi);

  server.begin();
  MDNS.addService("http", "tcp", 80);

  // done - STATUS green
  setLights(LOW, LOW, HIGH);
  delay(3000);
}


void loop() {
  
  server.handleClient();

  delay(100);
}

void lightsApi() {
  if (!server.authenticate(httpUser, httpPass)) {
    return server.requestAuthentication();
  }

  String v = server.arg("red");
  red = v.toInt();
  v = server.arg("flashRed");
  flashRed = v.toInt();

  v = server.arg("yellow");
  yellow = v.toInt();
  v = server.arg("flashYellow");
  flashYellow = v.toInt();

  v = server.arg("green");
  green = v.toInt();
  v = server.arg("flashGreen");
  flashGreen = v.toInt();

  v=server.arg("sequence");
  sequence = v.toInt();

  server.send(200, "text/plain", "OK");


  if (sequence == 1) {
    Serial.print("Run the squence I \n");
    runSequence();
  }
    
setLights(red, yellow, green);
  
   Serial.print("Handle API \n");
}

void setLights(uint8_t red, uint8_t yellow, uint8_t green) {
  digitalWrite(RED, red);
  digitalWrite(YELLOW, yellow);
  digitalWrite(GREEN, green);

     Serial.print("Set Lights \n");

}

void runSequence() {
  int  var = 0; //defines and sets initial value for variables used below
    int var1 = 0; //defines and sets initial value for variables used below
   while(var < 25){
  // repeats normal cycle 25 times
  digitalWrite(GREEN, 1);   // turns the green light on
  delay(20000);               // holds the green light on for 20 seconds
  digitalWrite(GREEN, 0);    // turns the green light off
  delay(600);               // slight pause between lights
  digitalWrite(YELLOW, 1);  //turns the yellow light on
  delay(4000); //holds the yellow light for 4 seconds (watch out for that red-light camera!)
  digitalWrite(YELLOW, 0); //turns the yellow light off
  delay(600);  //slight pause between lights
  digitalWrite(RED, 1);  //turns the red light on
  delay(20000);  //holds the red light on for 20 seconds
  digitalWrite(RED, 0);  //turns the red light off
  delay(600);  //slight pause between lights
  var++;}  //adds 1 to variable "var" for repeat count

  // after 25 cycles above, the light switches to "power outage mode", flashing red
  delay(600); //slight delay
  var1=0; //resets variable "var1" to 0
  while(var1 < 120) {
    // repeats power outage cycle 120 times - 2 minutes
   digitalWrite(RED, 1);
   delay(600);
   digitalWrite(RED, 0);
   delay(400);
   var1++;}
var = 0;

//switches back to normal cycle after "power outage" cycle is done
while(var < 25){\
  // back to normal light cycle for 25 cycles
  digitalWrite(GREEN, 1);   // turn the LED on (HIGH is the voltage level)
  delay(20000);               // wait for a second
  digitalWrite(GREEN, 0);    // turn the LED off by making the voltage LOW
  delay(600);               // wait for a second
  digitalWrite(YELLOW, 1);
  delay(4000);
  digitalWrite(YELLOW, 0);
  delay(600);
  digitalWrite(RED, 1);
  delay(20000);
  digitalWrite(RED, 0);
  delay(600);
  var++;}
  delay(600);

 //switches to "late night cycle" flashing yellow for 2 minutes, similar to flashing red above
 var1=0;
  while(var1 < 120) {
   digitalWrite(YELLOW, LOW);
   delay(600);
   digitalWrite(YELLOW, HIGH);
   delay(400);
   var1++;}


  }
// End of Sequence
