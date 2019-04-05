/************************************************************
Borrow Bike arduino program to handle checking in/out bikes
and handle theft


************************************************************/

//////////////////////
// Library Includes //
//////////////////////
// SoftwareSerial is required (even you don't intend on
// using it).
#include <SoftwareSerial.h> 
#include <SparkFunESP8266WiFi.h>

#include <AddicoreRFID.h>
#include <SPI.h>

//////////////////////////////
// WiFi Network Definitions //
//////////////////////////////
// Replace these two character strings with the name and
// password of your WiFi network.
const char mySSID[] = "";
const char myPSK[] = "";

bool wifi_connected; //bool to say if wifi is connected
bool checkedin; //bool to say if the bike is available
bool locked; //bool to say if the bike is locked
int last_user; //ID of the last user
// To use the ESP8266 as a TCP client, use the 
// ESP8266Client class. First, create an object:
ESP8266Client client;

//////////////////
// HTTP Strings //
//////////////////
const char destServer[] = "http://www.the-borrow-bike.us-west-2.elasticbeanstalk.com";

const char error_message[] = "PUT /api/Bikes/1 HTTP/1.1\n"
                             "Host: www.the-borrow-bike.us-west-2.elasticbeanstalk.com\n"
                             "content-type: application/json\n"
                             "content-length: 34\n\n"
                             "{\"Bike_ID\": \"1\",\"Status\": \"Error\"}";


const char checkin_message[] = "PUT /api/Bikes/1 HTTP/1.1\n"
                             "Host: www.the-borrow-bike.us-west-2.elasticbeanstalk.com\n"
                             "content-type: application/json\n"
                             "content-length: 38\n\n"
                             "{\"Bike_ID\": \"1\",\"Status\": \"Available\"}";

 
#define uchar unsigned char
#define uint  unsigned int

uchar fifobytes;
uchar fifoValue;

AddicoreRFID myRFID; // create AddicoreRFID object to control the RFID module

/////////////////////////////////////////////////////////////////////
//set the pins
/////////////////////////////////////////////////////////////////////
const int chipSelectPin = 10;
const int NRSTPD = 5;
int solenoidPin = 4;  
int tone_pin = 2;
int cable_pin = 3;

//Maximum length of the array
#define MAX_LEN 16


// All functions called from setup() are defined below the
// loop() function. They modularized to make it easier to
// copy/paste into sketches of your own.
void setup() 
{
  pinMode(solenoidPin, OUTPUT); 
  pinMode(cable_pin, INPUT_PULLUP);
  // Serial Monitor is used to control the demo and view
  // debug information.
  Serial.begin(9600);

  // initializeESP8266() verifies communication with the WiFi
  // shield, and sets it up.
  initializeESP8266();

  // connectESP8266() connects to the defined WiFi network.
  connectESP8266();

   // start the SPI library:
  SPI.begin();

  checkedin = true;
  pinMode(chipSelectPin,OUTPUT);              // Set digital pin 10 as OUTPUT to connect it to the RFID /ENABLE pin 
  digitalWrite(chipSelectPin, LOW);         // Activate the RFID reader
  pinMode(NRSTPD,OUTPUT);                     // Set digital pin 10 , Not Reset and Power-down
  digitalWrite(NRSTPD, HIGH);
    
  myRFID.AddicoreRFID_Init();  
  checkedin = true;
}

// Main Program Loop
void loop() 
{
  if(wifi_connected == false)
  {
     // initializeESP8266() verifies communication with the WiFi
    // shield, and sets it up.
    initializeESP8266();

    // connectESP8266() connects to the defined WiFi network.
    connectESP8266();
  }
  
  int cable_value = digitalRead(cable_pin); //read status of cable  
  digitalWrite(solenoidPin, LOW); //Set the magnet to off

  uchar i, tmp, checksum1;
  uchar status;
        uchar str[MAX_LEN];
        uchar RC_size;
        uchar blockAddr;  //Selection operation block address 0 to 63
        String mynum = "";

        str[1] = 0x4400;

  if(locked and cable_value == HIGH)
  {
    //report error/theft
    report_theft();
  }

  //Cable is connected so try and return bike
  if(cable_value == LOW and checkedin == false)
  {
    //return bike
    checkedin = checkin_bike(char data[]);
    locked = true;
  }
  
  //Find tags, return tag type
  status = myRFID.AddicoreRFID_Request(PICC_REQIDL, str); 
 //Anti-collision, return tag serial number 4 bytes
  status = myRFID.AddicoreRFID_Anticoll(str);
  
  // Checkout bike if the card worked, the bike is checked in and wifi is connected
  if (status == MI_OK and checkedin == true and wifi_connected == true)
  {
    char cstr[16];
    itoa(int(str[0]), cstr, 10);
    char data[] = "{\"Bike_ID\": \"1\",\"User_ID\": \"";
    char ending[] = "\"}\n";
    strcat(data, cstr);
    strcat(data, ending);
        
    bool res = false;
    //digitalWrite(solenoidPin, HIGH);
    res = checkout_bike(data);

    // IF successful check out
    if(res)
    {
      
      checkedin = false; // false so checkout
      last_user = int(str[0]); // save last user
      digitalWrite(solenoidPin, HIGH); // release the magnet
      tone(tone_pin, 500, 3000); // play a tone
      noTone(tone_pin); //turn off tone
      locked = false; // set it to unlocked
      delay(5000); // wait 5 seconds
      //digitalWrite(solenoidPin, LOW);

    }
    else
    {
      wifi_connected = false;
      tone(tone_pin, 100, 1000);
      noTone(tone_pin);
    }
   
  }
  else if(status == MI_OK and checkedin == false and wifi_connected == false and locked) // Else if the bike is checked out but locked without wifi, allow only to open to the last user
  {
    int user = int(str[0]);
    if(user == last_user)
    {
      digitalWrite(solenoidPin, HIGH);
      tone(tone_pin, 500, 3000);
      noTone(tone_pin);
      locked = false;
      delay(5000);
      digitalWrite(solenoidPin, LOW);
    }
  }
  else
  {
    tone(tone_pin, 100, 1000);
    noTone(tone_pin);
  }
    
  myRFID.AddicoreRFID_Halt();      //Command tag into hibernation     
  
}

//Set up the wifi shield
void initializeESP8266()
{
  // esp8266.begin() verifies that the ESP8266 is operational
  // and sets it up for the rest of the sketch.
  // It returns either true or false -- indicating whether
  // communication was successul or not.
  // true
  int test = esp8266.begin();
  if (test != true)
  {
    wifi_connected = false;
    Serial.println(F("Error talking to ESP8266."));
    //errorLoop(test);
  }
  Serial.println("ESP8266 Shield Present");
}

// Connect to wifi
void connectESP8266()
{
  // The ESP8266 can be set to one of three modes:
  //  1 - ESP8266_MODE_STA - Station only
  //  2 - ESP8266_MODE_AP - Access point only
  //  3 - ESP8266_MODE_STAAP - Station/AP combo
  // Use esp8266.getMode() to check which mode it's in:
  int retVal = esp8266.getMode();
  if (retVal != ESP8266_MODE_STA)
  { // If it's not in station mode.
    // Use esp8266.setMode([mode]) to set it to a specified
    // mode.
    retVal = esp8266.setMode(ESP8266_MODE_STA);
    if (retVal < 0)
    {
      wifi_connected = false;
      Serial.println(F("Error setting mode."));
    }
  }
  //Serial.println(F("Mode set to station"));

  // esp8266.status() indicates the ESP8266's WiFi connect
  // status.
  // A return value of 1 indicates the device is already
  // connected. 0 indicates disconnected. (Negative values
  // equate to communication errors.)
  retVal = esp8266.status();
  if (retVal <= 0)
  {
    Serial.print(F("Connecting to "));
    Serial.println(mySSID);
    // esp8266.connect([ssid], [psk]) connects the ESP8266
    // to a network.
    // On success the connect function returns a value >0
    // On fail, the function will either return:
    //  -1: TIMEOUT - The library has a set 30s timeout
    //  -3: FAIL - Couldn't connect to network.
    retVal = esp8266.connect(mySSID, myPSK);
    if (retVal < 0)
    {
      wifi_connected = false;
      Serial.println(F("Error connecting"));
    }
    else
    {
      Serial.println("Connected");
      wifi_connected = true;
    }
  }
}

//Try and checkin the bike
bool checkin_bike(char data[])
{
  int i = 0;
  int retVal = client.connect(destServer, 80);
  //try and connect to website up to 10 times before erring out
  while(i < 10 and retVal<=0)
  {
    retVal = client.connect(destServer, 80);
    if (retVal <= 0)
    {
      Serial.println(F("Failed to connect to server."));
    }
    i = i + 1;
  }
  if (retVal <= 0)
  {
    Serial.println(F("Failed to connect to server."));
    return false;
  }

  // print and write can be used to send data to a connected
  // client connection.
  client.print(checkin_message);
 
  // available() will return the number of characters
  // currently in the receive buffer.
  while (client.available())
    Serial.write(client.read()); // read() gets the FIFO char
  
  // connected() is a boolean return value - 1 if the 
  // connection is active, 0 if it's closed.
  if (client.connected())
    client.stop(); // stop() closes a TCP connection.

  return true;
}

// Handle checking out the bike
bool checkout_bike(char data[])
{
  // take the data size
  int dataSize = 0;
  while(data[dataSize])
  {
    dataSize++;
  }

  //Serial.println(dataSize);
  /// Generate the http message to send to server
  char httpHead[] = "PUT /api/Bikes/1 HTTP/1.1\nHost: www.the-borrow-bike.us-west-2.elasticbeanstalk.com\ncontent-type: application/json\n";
  char cstr[16];
  itoa(dataSize, cstr, 10);
  char requestSize[] = "content-length: ";
  strcat(requestSize, cstr);
  strcat(requestSize, "\n\n");
  strcat(requestSize, data);
  strcat(httpHead, requestSize);
  //Serial.println(httpHead);
 
  // ESP8266Client connect([server], [port]) is used to 
  // connect to a server (const char * or IPAddress) on
  // a specified port.
  // Returns: 1 on success, 2 on already connected,
  // negative on fail (-1=TIMEOUT, -3=FAIL).
  int i = 0;
  int retVal = client.connect(destServer, 80);
  // If it doesn't connect try and connect up to 10 times
  while(i < 10 and retVal<=0)
  {
    retVal = client.connect(destServer, 80);
    if (retVal <= 0)
    {
      Serial.println(F("Failed to connect to server."));
    }
    i = i + 1;
  }
  //retVal = client.connect(destServer, 80);
  if (retVal <= 0)
  {
    Serial.println(F("Failed to connect to server."));
    return false;
  }
  
  // print and write can be used to send data to a connected
  // client connection.
  Serial.println(F("Sending to website."));
  client.print(httpHead);
 
  // available() will return the number of characters
  // currently in the receive buffer.
  // I don't think this section is necessary
  //while (client.available())
    //Serial.write(client.read()); // read() gets the FIFO char
  
  // connected() is a boolean return value - 1 if the 
  // connection is active, 0 if it's closed.
  if (client.connected())
    client.stop(); // stop() closes a TCP connection.

  return true;
}

// Function to report the cable being cut
void report_theft()
{
  int i = 0;
  int retVal = client.connect(destServer, 80);
  // Only try and send a message to website 4 times, else continue
  while(i < 4 and retVal<=0)
  {
    retVal = client.connect(destServer, 80);
    if (retVal <= 0)
    {
      Serial.println(F("Failed to connect to server."));
    }
    i = i + 1;
  }
  //retVal = client.connect(destServer, 80);
  if (retVal <= 0)
  {
    Serial.println(F("Failed to connect to server."));
  }
  else
  {
    client.print(error_message);
  }

  //LOOP ALARM FOREVER - should be an annoying high pitch
  for(;;)
  {
    tone(tone_pin, 10000, 100000000);
  }
  
}

// serialTrigger prints a message, then waits for something
// to come in from the serial port.
void serialTrigger(String message)
{
  Serial.println();
  Serial.println(message);
  Serial.println();
  while (!Serial.available())
    ;
  while (Serial.available())
    Serial.read();
}
