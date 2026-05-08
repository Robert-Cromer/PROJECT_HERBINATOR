#include <WiFi.h> // wifi module for esp32
#include <string.h> // string management
#include <WebServer.h> // web server utility for esp32
#include <ESPmDNS.h> // map ip address to a name
#include <time.h> // up to date time over NTP

/*This file contains the code for the esp-32-s3 communications. This will eventually be put in the main file for the esp code*/
#define MAX_CONNECT_ATTEMPTS 20

// not using c strings because c++ strings are easier to deal with
//String ROUTER_NAME = "HerbyIOT";
//String ROUTER_PASSWORD = "herbinator";
String ROUTER_NAME;
String ROUTER_PASSWORD;
String mdnsName = "herbnet";
// NTP server
const char* ntpServer = "pool.ntp.org"; // atomic clock ntp pooler
const long  gmtOffset_sec = -28800;   // pst is 28800 secs behind gmt
const int   daylightOffset_sec = 3600; // 3600 secs added for daylight savings

// Web server port open 80 http
WebServer server(80);

// method definitions
String getString(String);
void connect(String, String);
String getTimeString();
void handleRoot();
void handleTime();

void setup(){ // in the futire, this code may be moved to its own function so that we can do more with it on startup
  Serial.begin(9600); // so that terminal can be read
  // this is for dynamic reading of wifi creds, worry about this later. (note i got it working)
  // I want to make that functionality on a web page preferably, or maybe dynamically read from client device?
  while (!Serial){
    ; // waits for esp to connect
  }

  ROUTER_NAME = getString("Enter router name.");
  ROUTER_NAME.trim();
  
  ROUTER_PASSWORD = getString("Enter router password.");
  ROUTER_PASSWORD.trim();

  connect(ROUTER_NAME, ROUTER_PASSWORD);
 
 
  // mDNS name -> herbnet.local
  if (!MDNS.begin(mdnsName)) {
    Serial.println("mDNS start FAILED");
  } else {
    Serial.print("mDNS started: http://");
    Serial.print(mdnsName);
    Serial.println(".local/");
  }

  // Time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Web server
  server.on("/", handleRoot);
  server.on("/time", handleTime);
  server.begin();
  Serial.println("Herbnet started!");

}

void loop(){
  server.handleClient(); // continuous client handling
}


// this function enables the boards to connect to a wifi router
void connect(String name, String password){

  WiFi.begin(ROUTER_NAME.c_str(), ROUTER_PASSWORD.c_str());
 
  for (int attempts = 0; attempts < MAX_CONNECT_ATTEMPTS && WiFi.status() != WL_CONNECTED; attempts++){   // delay for connection to the network
    delay(500); // pauses
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED){
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else Serial.println("Failed to connect. Please try again.");
}

// This is for the user entering creds, hard coded wifi creds for now.
String getString(String prompt){
  Serial.readString(); // flushes whatever is in the input
  Serial.println(prompt);

  while(Serial.available() <= 0); // BLANK--buffer for user to enter input. Will be exited once input provided

  return Serial.readStringUntil('\n');

}

// handle time requests to web page
void handleTime() {
  server.send(200, "text/plain", getTimeString());
}

// Format time for display
String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time not ready";
  }
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

// Root page
void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 Local Time</title>
  <meta http-equiv="refresh" content="1">
</head>
<body>
  <h2>ESP32 Local Time</h2>
  <p>)rawliteral" + getTimeString() + R"rawliteral(</p>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", page);
}
