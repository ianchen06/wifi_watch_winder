#include <FS.h>
#include <Wire.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <AM2320.h>

// Stepper motor pins
#define IN4 14 //GPIO 14
#define IN3 12 //GPIO 12
#define IN2 13 //GPIO 13
#define IN1 15 //GPIO 15

ESP8266WebServer server ( 80 );
AM2320 th;

float humidity, temperature;  // Values read from sensor
String webString = "";   // String to display

const int NBSTEPS = 4095 * 10;
const int STEPTIME = 790; //tested fastest speed
int Step = 0;
boolean Clockwise = true;

int arrayDefault[4] = {LOW, LOW, LOW, LOW};

int stepsMatrix[8][4] = {
  {LOW, LOW, LOW, HIGH},
  {LOW, LOW, HIGH, HIGH},
  {LOW, LOW, HIGH, LOW},
  {LOW, HIGH, HIGH, LOW},
  {LOW, HIGH, LOW, LOW},
  {HIGH, HIGH, LOW, LOW},
  {HIGH, LOW, LOW, LOW},
  {HIGH, LOW, LOW, HIGH},
};

unsigned long lastTime = 0L;
unsigned long _time = 0L;

unsigned long Timer1 = 0L;

// Interval between stepper rotation
long Interval1 = 12000;

boolean go = true;

int totalWindsPerDay = 650;
int currentWinds = 0;


// Function prototypes
void writeStep(int outArray[4]);
void stepper();
void setDirection();
String getContentType(String filename);
bool handleFileRead(String path);
void gettemperature();


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  // call sensor.begin() to initialize the library
  Wire.begin(4, 5);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(990);

  //reset saved settings
  //wifiManager.resetSettings();

  //set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  // Connexion WiFi établie / WiFi connexion is OK
  Serial.println ( "" );
  //Serial.print ( "Connected to " ); Serial.println ( ssid );
  Serial.print ( "IP address: " ); Serial.println ( WiFi.localIP() );

  if (!SPIFFS.begin())
  {
    // Serious problem
    Serial.println("SPIFFS Mount failed");
  } else {
    Serial.println("SPIFFS Mount succesfull");
  }

  server.onNotFound([]() {
      if (!handleFileRead(server.uri())) {
      server.sendHeader("Access-Control-Max-Age", "10000");
      server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
      server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
      server.send(404, "text/plain", "FileNotFound");
      }
      });

  server.on("/set_wind", []() { // if you add this subdirectory to your webserver call, you get text below :)
    totalWindsPerDay = server.arg("totalWindsPerDay").toInt();
    webString = String((int)totalWindsPerDay); // Arduino has a hard time with float to string
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    server.send(200, "text/plain", webString);            // send to someones browser when asked
  });

  server.on("/winds", []() { // if you add this subdirectory to your webserver call, you get text below :)
    gettemperature();       // read sensor
    webString = String((int)currentWinds); // Arduino has a hard time with float to string
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    server.send(200, "text/plain", webString);            // send to someones browser when asked
  });

  server.on("/temp", []() { // if you add this subdirectory to your webserver call, you get text below :)
    gettemperature();       // read sensor
    webString = String((float)temperature); // Arduino has a hard time with float to string
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    server.send(200, "text/plain", webString);            // send to someones browser when asked
  });

  server.on("/humd", []() { // if you add this subdirectory to your webserver call, you get text below :)
    gettemperature();           // read sensor
    webString = String((float)humidity);
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    server.send(200, "text/plain", webString);               // send to someones browser when asked
  });

  server.begin();
  Serial.println ( "HTTP server started" );

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();

  unsigned long currentMicros;
  unsigned long currentMillis;

  int stepsLeft = NBSTEPS;
  _time = 0;
  lastTime = micros();
  while (stepsLeft > 0 && go == true) {
    server.handleClient();
    currentMicros = micros();
    if (currentMicros - lastTime >= STEPTIME) {
      stepper();
      _time += micros() - lastTime;
      lastTime = micros();
      stepsLeft--;
    }
    delay(1);
    currentMillis = millis();
    Timer1 = currentMillis;
  }
  //Serial.println(_time);
  //Serial.println("Wait...!");
  if (stepsLeft <= 0) {
    currentWinds++;
  }

  currentMillis = millis();

  if (currentMillis - Timer1 > Interval1) {
    Serial.println("Switch direction!");
    Timer1 = currentMillis;
    Clockwise = !Clockwise;
    stepsLeft = NBSTEPS ;
    go = true;
  } else {
    go = false;
  }
}

void gettemperature() {
  switch (th.Read()) {
    case 2:
      Serial.println("CRC failed");
      break;
    case 1:
      Serial.println("Sensor offline");
      break;
    case 0:
      Serial.print("humidity: ");
      Serial.print(th.h);
      humidity = th.h;
      Serial.print("%, temperature: ");
      Serial.print(th.t);
      temperature = th.t;
      Serial.println("*C");
      break;
  }
}

void writeStep(int outArray[4]) {
  digitalWrite(IN1, outArray[0]);
  digitalWrite(IN2, outArray[1]);
  digitalWrite(IN3, outArray[2]);
  digitalWrite(IN4, outArray[3]);
}

void stepper() {
  if ((Step >= 0) && (Step < 8)) {
    writeStep(stepsMatrix[Step]);
  } else {
    writeStep(arrayDefault);
  }
  setDirection();
}

void setDirection() {
  (Clockwise == true) ? (Step++) : (Step--);

  if (Step > 7) {
    Step = 0;
  } else if (Step < 0) {
    Step = 7;
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".html.gz")) return "text/html";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html.gz";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";   // 路徑結尾加上".gz"

  // 確認請求路徑或者.gz結尾的資源存在
  if ( SPIFFS.exists(path) || SPIFFS.exists(pathWithGz) ) {
    if (SPIFFS.exists(pathWithGz)) {
      path += ".gz";
    }

    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();

    return true;
  }
  return false;
}
