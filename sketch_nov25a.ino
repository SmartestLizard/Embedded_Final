
//state values
bool on = false;
bool error = false;
bool idle = false;

//USART/Serial Monitor registers
#define RDA 0x80
#define TBE 0x20  
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

//real time clock
#include <RTClib.h>
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//humidity and temp library
#include "DHT.h"
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
float temp;
float humi;

//threshold values
const int tempthreshold = 75; //adjust
const int waterthreshold = 12; //adjust


//remove
int water = 11;


void setup() {
  U0init(9600); //Serial initialization
  rtc.begin(); //real-time clock initialization
  dht.begin(); // initialize the temp and humidity sensor
  temp = dht.readTemperature(true);
  humi  = dht.readHumidity();
  Serial.begin(9600);//////////////////////////////////////////remove////////////////////////////
}

void loop() {
  char input = Serial.read();///////////////////////////////remove////////////////////////////////
  DateTime now = rtc.now();
  if(now.day() == 0){
    humi  = dht.readHumidity();
    // read temperature as Fahrenheit
    temp = dht.readTemperature(true);
    //Send to LCD display
  }
  ////////////////////////create an interupt//////////////
  if(input == 's'){//change to when start button pressed
    //start
    if(!on){
      U0putchar("\nswitch to idle at ");
      U0putchar(timeReport());
      idle = true;
    }
    on = true;
  }
  /////////////////////////////////////////////////////
  if(input == 'f'){//change to when stop button pressed
    //stop
    if(on){
      U0putchar("\nswitch to disabled at ");
      U0putchar(timeReport());
    }
    on = false;
    error = false;
    idle = false;
    //turn Yellow LED on
  }
  if(on && input == 'r'){
    //reset and red LED off, green LED on
    if(error){
      U0putchar("\nswitch to idle at ");
      U0putchar(timeReport());
    }
    error = false;
    idle = true;
  }
  if(on && error){
    //display error
    //red LED on
  }
  if(on && !error){
    if(water < waterthreshold){
      //display error message and red LED on
      U0putchar("\nswitch to error at ");
      U0putchar(timeReport());
      error = true;
      idle = false;
    }
    else if(temp > tempthreshold){
      //blue LED on, run fan and delay, blue LED off
      //display temp and humidity if seconds = 0
      if(idle){
        U0putchar("\nswitch to running at ");
        U0putchar(timeReport());
      }
      idle = false;
    }
    else{
      //fan off and green LED on
      //display temp and humidity
      idle = true;
    }
  }
}


//Serial functions
void U0putchar(String U0pdata)
{
  for(int i = 0; i < U0pdata.length(); i++){
    while(!(*myUCSR0A & TBE)){};
    *myUDR0 = U0pdata[i];
  }
}
void U0init(unsigned long U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
String timeReport()//////////////////////////Gives the wrong info. Debug with teacher/////////////////////////
{
  DateTime now = rtc.now();
  String report = (String)now.year();
  report += '/';
  report += (String)now.month();
  report += '/';
  report += (String)now.day();
  report += " (";
  report += daysOfTheWeek[now.dayOfTheWeek()];
  report += ") ";
  report += (String)now.hour();
  report += ':';
  report += (String)now.minute();
  report += ':';
  report += (String)now.second();
  report += '\n';
  return report;
}
