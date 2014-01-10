/*
    Hue_W5100_HT6P20B
    http://youtu.be/BRcG0C0mo08
    
    Philips Hue wireless physical switch with brightness and hue control
    Uses HT6P20B based remote controls, 433.92MHz receiver and Arduino + W5100 ethernet shield
    
    Gilson Oguime
    oguime (at) gmail.com
    Jan/14    
 
    HT6P20B decoder created by Afonso Celso Turcato 
    acturcato (at) gmail.com
    http://acturcato.wordpress.com/2014/01/04/decoder-for-ht6p20b-encoder-on-arduino-board-english/
    
*/
#include <SPI.h>
#include <Ethernet.h>
/*
     Remote     Address  Light
  0  Intelbras    37492  Hue 1
  1  Intelbras  2321684  Hue 2
  2  Intelbras  2144096  Hue 3
  3  Intelbras   605044  Hue 4
  4  ECP        2911907  Empty
  
*/
const int deviceMax = 4;
const long devices[] = {37492,2321684,2144096,605044,2911907};
 
//  Hue constants
 
const char hueHubIP[] = "192.168.1.2";  // Hue hub IP
const char hueUsername[] = "newdeveloper";  // Hue username
const int hueHubPort = 80;
 
const int hueBriMax = 255;  // max brightness value
const int hueBriStep = 64;  // brightness step size when button 1 is pressed (256/64 = 4 steps)
 
const long hueHueMax = 65535;  // max hue value
const long hueHueStep = 10922;  // hue step size when button 2 is pressed (65536/10922 = 6 steps)
 
//  Hue variables
 
int hueBriDir[deviceMax];  // holds brightness step direction for each remote/light (up=1,down=-1)
long hueHueDir[deviceMax];  // holds hue step direction for each remote/light (up=1,down=-1)
 
unsigned int hueButton;  // button pressed (0 to 3)
unsigned int hueLight;  // target light
boolean hueOn;  // on/off
int hueBri;  // brightness value
long hueHue;  // hue value
String hueCmd;  // Hue command
 
//  ACT_HT6P20B variables
 
byte pinRF;      // RF Module pin
 
boolean startbit;
boolean dataok;
 
int counter;  //received bits counter: 22 of Address + 2 of Data + 4 of EndCode (Anti-Code)
int lambda;  // on pulse clock width (if fosc = 2KHz than lambda = 500 us)
int dur0, dur1;  // pulses durations (auxiliary)
 
unsigned long buffer=0;  //buffer for received data storage
unsigned long addr;
 
//  Ethernet
 
byte mac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };  // W5100 MAC address
IPAddress ip(192,168,1,251);  // Arduino IP
EthernetClient client;
/*
 
    Setup
 
*/
void setup()
{
  Serial.begin(9600);
  Serial.println("[Hue_W5100_HT6P20B]");
  
  Ethernet.begin(mac,ip);
 
  pinRF = 2;
  pinMode(pinRF, INPUT);
  
  delay(2000);
  Serial.println("Ready.");
}
/*
 
    ACT_HT6P20B intercepts remote control button press.
    If the remote is recognized, GetHue captures the corresponding light state (on, bri and hue).
    Button 0 toggles light On/Off.
    Button 1 steps brightness value up or down, depending on brightness direction.
    Button 2 steps hue value up or down, depending on hue direction.
    SetHue sends hueCmd command.
    
*/
void loop() 
{
  if (ACT_HT6P20B(addr, hueButton))
  {
    for (hueLight = 0; hueLight <= deviceMax; hueLight++)  // scan devices
    {
      if (addr == devices[hueLight])  // compare to device list
      {
        if (GetHue())  // get light state
        {
          hueCmd = "";  // initialize command
          
          if ((hueButton == 0) && hueOn)  // toggle light Off
            hueCmd = "{\"on\":false}";
    
          else if ((hueButton == 0) && !hueOn)  // toggle light On
            hueCmd = "{\"on\":true}";
    
          else if ((hueButton == 1) && hueOn)  // brightness button
          {
            if (hueBriDir[hueLight] == 0)  // initialize brightness direction
            {
              if (hueBri >= hueBriMax)
                hueBriDir[hueLight] = -1;  // decrease if already max
              else
                hueBriDir[hueLight] = 1;  // increase otherwise
            }
            hueBri += (hueBriStep * hueBriDir[hueLight]);  // brightness = brightness + step
            
            if (hueBri >= hueBriMax)  // brightness higher or equal to max brightness
            {
              hueBri = hueBriMax;  // set brightness to max
              hueBriDir[hueLight] = -1;  // set brightness direction down
            }
            if (hueBri <= 0)  // brightness lower or equal to 0
            {
              hueBri = 0;  // set brightness to 0
              hueBriDir[hueLight] = 1;  // set brightness direction up
            }
            hueCmd = "{\"bri\": " + String(hueBri) +"}";  // light brightness command
          }
          else if ((hueButton == 2) && hueOn)  // hue button
          {
            if (hueHueDir[hueLight] == 0)  // initialize hue direction
            {
              if (hueHue >= hueHueMax)
                hueHueDir[hueLight] = -1;  // decrease if already max
              else
                hueHueDir[hueLight] = 1;  // increase otherwise
            }
            hueHue += (hueHueStep * hueHueDir[hueLight]);  //  hue = hue + step
            
            if (hueHue >= hueHueMax)  // hue higher than max hue
            {
              hueHue = hueHueMax;  // set hue to max
              hueHueDir[hueLight] = -1;  // set hue direction down
            } 
            if (hueHue <= 0)  // hue lower or equal to 0
            {
              hueHue = 0;  // set hue to 0
              hueHueDir[hueLight] = 1;  // set hue direction up
            }
            hueCmd = "{\"hue\": " + String(hueHue) +"}";  // hue hue command
          }
          if (hueCmd.length()>0)  // command constructed
          {
            if (SetHue())  // set hue light state
            {
              Serial.print("Button : "); Serial.println(hueButton);
              Serial.print("Light  : "); Serial.println(hueLight + 1);
              Serial.print("Cmd    : "); Serial.println(hueCmd);
            }
            else  // SetHue failed
            {
              Serial.print("Could not execute ");Serial.print(hueCmd);
              Serial.print(" on Light ");Serial.print(hueLight + 1);Serial.print(" !");
            }
          }
          else  // command not defined
          {
            Serial.print("Could not build command for light ");
            Serial.print(hueLight + 1);Serial.println(" !");
          }
        }
        else  // GetHue failed
        {
          Serial.print("Could not get Light ");Serial.print(hueLight + 1);Serial.println(" state!");
        }
        break;  // corresponding light found, no reason to keep looking
      }
    }
    if (hueLight > deviceMax)  // recognized device not in list
    {
      Serial.print("Addr ");Serial.print(addr);Serial.println(" not in list!");
    }
  Serial.println();
  }
}
/*
 
    GetHue
    Get light state (on,bri,hue)
 
*/
boolean GetHue()
{
  if (client.connect(hueHubIP, hueHubPort))
  {
    client.print("GET /api/");
    client.print(hueUsername);
    client.print("/lights/");
    client.print(hueLight + 1);  // hueLight zero based, add 1
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(hueHubIP);
    client.println("Content-type: application/json");
    client.println("keep-alive");
    client.println();
    while (client.connected())
    {
      if (client.available())
      {
        client.findUntil("\"on\":", "\0");
        hueOn = (client.readStringUntil(',') == "true");  // if light is on, set variable to true
 
        client.findUntil("\"bri\":", "\0");
        hueBri = client.readStringUntil(',').toInt();  // set variable to brightness value
 
        client.findUntil("\"hue\":", "\0");
        hueHue = client.readStringUntil(',').toInt();  // set variable to hue value
        
        break;  // not capturing other light attributes yet
      }
    }
    client.stop();
    return true;  // captured on,bri,hue
  }
  else
    return false;  // error reading on,bri,hue
}
/*
 
    SetHue
    Set light state using hueCmd command
 
*/
boolean SetHue()
{
  if (client.connect(hueHubIP, hueHubPort))
  {
    while (client.connected())
    {
      client.print("PUT /api/");
      client.print(hueUsername);
      client.print("/lights/");
      client.print(hueLight + 1);  // hueLight zero based, add 1
      client.println("/state HTTP/1.1");
      client.println("keep-alive");
      client.print("Host: ");
      client.println(hueHubIP);
      client.print("Content-Length: ");
      client.println(hueCmd.length());
      client.println("Content-Type: text/plain;charset=UTF-8");
      client.println();  // blank line before body
      client.println(hueCmd);  // Hue command
    }
    client.stop();
    return true;  // command executed
  }
  else
    return false;  // command failed
}
/*
 
    ACT_HT6P20B
    Based on ACT_HT6P20B_RX.ino from AFONSO CELSO TURCATO
    Modified to return boolean indicating successful button press captured
    
*/
boolean ACT_HT6P20B(unsigned long &addr, unsigned int &button)
{
  digitalWrite(13, digitalRead(pinRF)); //blink de onboard LED when receive something
  
  if (!startbit)
  {// Check the PILOT CODE until START BIT;
    dur0 = pulseIn(pinRF, LOW);  //Check how long DOUT was "0" (ZERO) (refers to PILOT CODE)
        
    // If Start Bit is OK, then starts measure os how long the signal is level "1" and check is value is into acceptable range.
    if((dur0 > 9200) && (dur0 < 13800) && !startbit)
    {    
      lambda = dur0 / 23;  //calculate wave length - lambda
      
      dur0 = 0;
      buffer = 0;
      counter = 0;
      
      dataok = false;
      startbit = true;
    }
  }
  // If Start Bit is OK, then starts measure os how long the signal is level "1" and check is value is into acceptable range.
  if (startbit && !dataok && counter < 28)
  {
    ++counter;
    
    dur1 = pulseIn(pinRF, HIGH);
    
    if((dur1 > 0.5 * lambda) && (dur1 < (1.5 * lambda)))  //If pulse width at "1" is between "0.5 and 1.5 lambda", means that pulse is only one lambda, so the data é "1".
    {
      buffer = (buffer << 1) + 1;      // add "1" on data buffer
    }
    else if((dur1 > 1.5 * lambda) && (dur1 < (2.5 * lambda)))  //If pulse width at "1" is between "1.5 and 2.5 lambda", means that pulse is two lambdas, so the data é "0".
    {
      buffer = (buffer << 1);       // add "0" on data buffer
    }
    else
    {
      //Reset the loop
      startbit = false;
    }
  }
  //Check if all 28 bits were received (22 of Address + 2 of Data + 4 of Anti-Code)
  if (counter==28) 
  { 
    // Check if Anti-Code is OK (last 4 bits of buffer equal "0101")
    if ((bitRead(buffer, 0) == 1) && (bitRead(buffer, 1) == 0) && (bitRead(buffer, 2) == 1) && (bitRead(buffer, 3) == 0))
    {
      dataok = true;
      
      counter = 0;
      startbit = false;
      
      addr = buffer >> 6;
      button = (2*!bitRead(buffer,5)+!bitRead(buffer,4));
 
      Serial.print("Address: "); Serial.println(addr);
      
      return true;
    }
    else
    {
      //Reset the loop
      startbit = false;
    }
  }
  return false;
}
