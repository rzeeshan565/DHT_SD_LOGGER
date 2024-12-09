// Example testing sketch for various DHT humidity/temperature sensors written by ladyada
// REQUIRES the following Arduino libraries:
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor
/*
  To upload FW binary, through terminal you can use: curl -F "image=@firmware.bin" esp32-webupdate.local/update
*/
#include <Arduino.h>
#include "DHT.h"
#include <WiFi.h>
#include <AsyncTCP.h>
//#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>
//#include <WebServer.h>
//#include <ESPmDNS.h>
//#include <Update.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <SPI.h>
#include "time.h"
//#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

//#define RTC_INCLUDE
#define SD_INCLUDE
//#define SD_DEBUG_LOG //only print fail message for now

const char* ssid_ = "Vodafone-70DC";
const char* pass_ = "2PyHmTptCxzePxxe";

#ifdef RTC_INCLUDE
#include <RTClib.h>
#endif
#ifdef SD_INCLUDE
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#endif

#define DAYLIGH_SAVING_H    0

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
struct tm timeinfo, tm_loc;
static String hour, minute, sec, day, mon;

// LED state variable
//String ledState = "red"; // Can be "red", "green", or "yellow"
enum State {
  NONE_S,
  ERROR_S,
  OK_S
};

enum State ledState;
#define DHTPIN 4     // Digital pin connected to the DHT sensor
// Feather HUZZAH ESP8266 note: use pins 3, 4, 5, 12, 13 or 14 --
// Pin 15 can work but DHT must be disconnected during program upload.

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);

//WiFiManager wm;

//WebServer server(80);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 1000;
// Json Variable to Hold Sensor Readings
JSONVar readings;

const char* host = "esp32";

#ifdef RTC_INCLUDE
RTC_DS3231 rtc;
static bool rtc_is_running = false;;
#endif

#ifdef SD_INCLUDE
static const String sd_log_dir = "/DHT_LOGS";
const char* log_hdr = "date, time, temperature(°C), humidity(%), state\n";
static bool sd_mount_failed = false;
#endif
/*/Users/zeeshanrehman/.platformio/packages/framework-arduinoespressif32/libraries/WiFiManager-master/examples/Basic/Basic.ino
 * Login page
 */
const char* loginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
             "<td>Username:</td>"
             "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";

/*
 * Server Index Page
 */

const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

String getSensorReadings(void);
bool sd_write_logs(JSONVar data);

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

static void setup_wifi(String _ssid, String _pass)
{
  // Connect to WiFi network
  const char* ssid = _ssid.c_str();
  const char* password = _pass.c_str();

  Serial.println("");
  Serial.print("Got SSID: ");
  Serial.println(ssid);
  Serial.println("");
  Serial.print("Got Pass: ");
  Serial.println(password);

  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  // Init and get the time
  configTime(gmtOffset_sec, 0, ntpServer);
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");

  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String();
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 1000);
  });
  server.addHandler(&events);
#if 0
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK_S");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
#endif
  server.begin();
}

// Get Sensor Readings and return JSON object
String getSensorReadings(){
  String jsonString, state;
  char time_buf[3]; // 2 digits + null terminator

#ifdef RTC_INCLUDE
  if(rtc_is_running) {
    DateTime now = rtc.now();

    sprintf(time_buf, "%02d", now.hour());
    hour = String(time_buf);
    sprintf(time_buf, "%02d", now.minute());
    minute = String(time_buf);
    sprintf(time_buf, "%02d", now.second());
    sec = String(time_buf);
    sprintf(time_buf, "%02d", now.month());
    mon = String(time_buf);
    sprintf(time_buf, "%02d", now.day());
    day = String(time_buf);

    readings["time"] =  hour + ":" + minute + ":" + sec;
    readings["date"] = String(now.year()) + "-" + mon + "-" + day;
    memcpy(&tm_loc, gmtime(&now.unixtime()), sizeof (struct tm));
  }
#else
  sprintf(time_buf, "%02d", timeinfo.tm_hour);
  hour = String(time_buf);
  sprintf(time_buf, "%02d", timeinfo.tm_min);
  minute = String(time_buf);
  sprintf(time_buf, "%02d", timeinfo.tm_sec);
  sec = String(time_buf);
  sprintf(time_buf, "%02d", timeinfo.tm_mon);
  mon = String(time_buf);
  sprintf(time_buf, "%02d", timeinfo.tm_mday);
  day = String(time_buf);

  //use internet time
  readings["time"] =  hour + ":" + minute + ":" + sec;
  readings["date"] = String(timeinfo.tm_year) + "-" + mon + "-" + day;
#endif

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    ledState = ERROR_S;
    return jsonString;
  } else {
    ledState = OK_S;
  }

  readings["temperature"] = String(t);
  readings["humidity"] =  String(h);

  // set LED state
  switch (ledState) {
    case ERROR_S:
      state = "red";
      break;
    case OK_S:
      state = "green";
      break;
    case NONE_S:
      state = "yellow";
      break;
  }
  readings["state"] = state;

  // write SD_data
#ifdef SD_INCLUDE
  if(!sd_mount_failed) {
    if(sd_write_logs(readings)) {
      ledState = OK_S;
#ifdef SD_DEBUG_LOG
      Serial.println("SD Logs write Success.");
#endif
    } else {
      ledState = ERROR_S;
      Serial.println("SD Logs write Fail.");
    }
  }
#endif

  jsonString = JSON.stringify(readings);
  return jsonString;
}

#ifdef SD_INCLUDE
#if 0
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char * path){
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}
#endif

// create sub directories (3 times)
static int createDirHelper(void){
  int res = 0;
  FS fs = SD;
  String path = sd_log_dir;

  if(!fs.mkdir(path.c_str())){
    res = -1;
  }

  path += "/" + String(tm_loc.tm_year);
  if(!fs.mkdir(path.c_str())){
    res = -1;
  }

  path += "/" + String(tm_loc.tm_mon);
  if(!fs.mkdir(path.c_str())){
    res = -1;
  }

  return res;
}

static int createDir(fs::FS &fs, const char * path){
#ifdef SD_DEBUG_LOG
  Serial.printf("Creating Dir: %s\n", path);
#endif
  int res = 0;
  if(fs.mkdir(path)){
#ifdef SD_DEBUG_LOG
    Serial.println("Dir created");
#endif  
  } else {
    Serial.println("mkdir failed");
    res = -1;
  }

  return res;
}

static void init_sd(void) {
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    sd_mount_failed = true;
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

#if 0
  listDir(SD, "/", 0);
  createDir(SD, "/mydir");
  listDir(SD, "/", 0);
  removeDir(SD, "/mydir");
  listDir(SD, "/", 2);
  writeFile(SD, "/hello.txt", "Hello ");
  appendFile(SD, "/hello.txt", "World!\n");
  readFile(SD, "/hello.txt");
  deleteFile(SD, "/foo.txt");
  renameFile(SD, "/hello.txt", "/foo.txt");
  readFile(SD, "/foo.txt");
  testFileIO(SD, "/test.txt");
#endif
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}
#endif

static int sd_write(const char * path, const char * buf) {

#ifdef SD_DEBUG_LOG
  Serial.printf("Writing file: %s\n", path);
#endif

  FS fs = SD;
  File file;
  int res = 0;
  
  if (!fs.exists(path)) {
    //new file
    file = fs.open(path, FILE_WRITE);
    if(file.print(log_hdr)){
#ifdef SD_DEBUG_LOG
      Serial.println("Header written");
#endif
    } else {
      Serial.println("Header Write failed");
      res = -1;
    }
  } else {
    file = fs.open(path, FILE_APPEND);
  }

  if(!file){
    Serial.println("Failed to open file for writing");
    res = -1;
    return res;
  }

  if(file.print(buf)){
#ifdef SD_DEBUG_LOG
    Serial.println("File written");
#endif
  } else {
    Serial.println("Write failed");
    res = -1;
  }
  file.close();

  return res;
}

bool sd_write_logs(JSONVar data) {
  String logs_dir, file_name, buf;
  bool res = true;

  buf = JSON.stringify(data["date"]) + "," + 
        JSON.stringify(data["time"]) + "," + 
        JSON.stringify(data["temperature"]) + "," + 
        JSON.stringify(data["humidity"]) + "," +
        String(ledState) + "\n";

  logs_dir = sd_log_dir + "/" + String(tm_loc.tm_year) + "/" + mon;
  if (createDir(SD, logs_dir.c_str()) < 0) {
    if(createDirHelper() < 0) {
      res = false; // fail to create dir
    }
  }
  
  file_name = logs_dir + "/LOGS_" + day+ ".CSV";

  if (sd_write(file_name.c_str(), buf.c_str()) < 0) {
    res = false; // fail to write
  } else {
    res = true;
  }

  return res;
}

void setup() {
  //WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP 
  Serial.begin(115200);

#ifdef SD_INCLUDE
  init_sd();
#endif

#ifdef RTC_INCLUDE
  // SETUP RTC MODULE
  if (! rtc.begin()) {
    Serial.println("RTC module is NOT found");
  }
  else {
    rtc_is_running = true;
  }
  DateTime now = rtc.now();
  DateTime compiled = DateTime(__DATE__, __TIME__);
  if (now.unixtime() < compiled.unixtime()) {
  Serial.println("RTC is older than compile time! Updating");
  // following line sets the RTC to the date & time this sketch was compiled
  rtc.adjust(DateTime(__DATE__, __TIME__));
  }
#endif

  initLittleFS();
  // wifi scan settings
  // wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
  // wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%
  // wm.setShowInfoErase(false);      // do not show erase button on info page
  // wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons
  //WiFiManager wm;
  // wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = true;//wm.autoConnect("AutoConnectAP","password"); // password protected ap

  if(!res) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");

    //setup_wifi(wm.getWiFiSSID(), wm.getWiFiPass());
    setup_wifi(ssid_, pass_);
  }

  Serial.println(F("DHT22 test!"));

  dht.begin();

  // Simulate changing LED state
  ledState = ERROR_S; // Initial state (error)
}

void loop() {

#ifdef RTC_INCLUDE
  DateTime now = rtc.now();
#endif

  if(!getLocalTime(&timeinfo) || WiFi.status() != WL_CONNECTED){
    Serial.println("Failed to obtain internet time");
  }
  else {
    // default add
    timeinfo.tm_year += 1900;
    timeinfo.tm_mon += 1;
    timeinfo.tm_hour += DAYLIGH_SAVING_H;
    tm_loc = timeinfo;

#ifdef RTC_INCLUDE
    // keep rtc time updated as internet time
    rtc.adjust(DateTime(timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
#endif
  }

#if 0
  // Example: Simulate changing LED state
  static unsigned long lastChange = 0;
  if (millis() - lastChange > 5000) { // Change every 5 seconds
      lastChange = millis();
      if (ledState == ERROR_S) {
          ledState = OK_S;
      } else if (ledState == OK_S) {
          ledState = NONE_S;
      } else {
          ledState = ERROR_S;
      }
  }
#endif

  if ((millis() - lastTime) > timerDelay) {
    // Send Events to the client with the Sensor Readings Every 10 seconds
    events.send("ping",NULL,millis());
    events.send(getSensorReadings().c_str(),"new_readings" ,millis());
    lastTime = millis();
    //Serial.printf("LED State: %d\n", ledState);

#ifdef RTC_INCLUDE
  Serial.print("ESP32 RTC Date Time: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);
#endif
  }
}