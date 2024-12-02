//state values
bool on = false;
bool error = false;
bool idle = false;
bool interruptTime = false;

//USART/Serial Monitor registers
#define RDA 0x80
#define TBE 0x20  
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

// Registers for LEDs and vent left/right button and start, reset, and stop buttons
// Define Port J Register Pointers
volatile unsigned char* port_j = (unsigned char*) 0x105; 
volatile unsigned char* ddr_j  = (unsigned char*) 0x104; 
volatile unsigned char* pin_j  = (unsigned char*) 0x103; 

// Define Port H Register Pointers
volatile unsigned char* port_h = (unsigned char*) 0x102; 
volatile unsigned char* ddr_h  = (unsigned char*) 0x101; 
volatile unsigned char* pin_h  = (unsigned char*) 0x100; 

// Define Port E Register Pointers
volatile unsigned char* port_e = (unsigned char*) 0x2E; 
volatile unsigned char* ddr_e  = (unsigned char*) 0x2D; 
volatile unsigned char* pin_e  = (unsigned char*) 0x2C;

// For output vent: stepper library
#include <Stepper.h>
const int stepsPerRevolution = 2038;
Stepper output_vent = Stepper(stepsPerRevolution, 8, 10, 9, 11);

// LCD  
#include <LiquidCrystal.h>
const int RS = 12, EN = 13, D4 = 0, D5 = 1, D6 = 4, D7 = 5;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// real time clock
#include <RTClib.h>
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// humidity and temp library
#include "DHT.h"
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
float temp;
float humi;

//threshold values
const int tempthreshold = 75; //adjust
const int waterthreshold = 12; //adjust


//remove//////////////////////////////////////////////////////////////////////////////////////////////////////
int water = 11;


void setup() {
  U0init(9600); //Serial initialization
  rtc.begin(); //real-time clock initialization
  dht.begin(); // initialize the temp and humidity sensor
  temp = dht.readTemperature(true);
  humi  = dht.readHumidity();
  //interrupts
  *ddr_e &= 0x01;//PE0
  *ddr_e &= 0x02;//PE1
  *ddr_h &= 0x40;//PH6
  attachInterrupt(digitalPinToInterrupt(2), startPressed, FALLING);//start
  attachInterrupt(digitalPinToInterrupt(3), stopPressed, FALLING);//stop
  attachInterrupt(digitalPinToInterrupt(18), resetPressed, FALLING);//reset

  // LCD
  lcd.begin(16, 2); // set up number of columns and rows
  
  // ************ for LEDs *************
  //set PJ1 to OUTPUT (red LED)
  *ddr_j |= 0x02;
  //set PJ0 to OUTPUT (yellow LED)
  *ddr_j |= 0x01;
  //set PH1 to OUTPUT (green LED)
  *ddr_h |= 0x02;
  //set PH0 to OUTPUT (blue LED)
  *ddr_h |= 0x01;

  // ********** for output vent *************
  // right button: set PH3 to INPUT
  *ddr_h &= 0xF7;
  // enable pullup resistor on PH3
  *port_h |= 0x08;
  
  // left button: set PH4 to INPUT
  *ddr_h &= 0xEF;
  // enable pullup resistor on PH4
  *port_h |= 0x10;
}

void loop() {
  if(interruptTime == true){
    interruptTime = false;
    U0putchar(timeReport());
  }
  DateTime now = rtc.now();
  if(now.day() == 0){
    humi  = dht.readHumidity();
    // read temperature as Fahrenheit
    temp = dht.readTemperature(true);
    //Send to LCD display
    lcd.setCursor(0, 0);  // print humidity in first row
    lcd.print("Humidity: ");
    lcd.print(humi);
    lcd.setCursor(1, 0);  // print temp in second row
    lcd.print("Temp: ");
    lcd.print(temp);
  }
  
  if(on && error){
    //display error

    //red LED on
    *port_j |= 0x02;     // red ON
    *port_j &= ~(0x01);  // yellow off
    *port_h &= ~(0x02);  // green off
    *port_h &= ~(0x01);  // blue off


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

      // blue LED on
      *port_j &= ~(0x02);  // red off
      *port_j &= ~(0x01);  // yellow off
      *port_h &= ~(0x02);  // green off
      *port_h |= 0x01;     // blue ON
    }
    else{
      //fan off and green LED on
      *port_j &= ~(0x02);  // red off
      *port_j &= ~(0x01);  // yellow off
      *port_h |= 0x02;     // green ON
      *port_h &= ~(0x01);  // blue off

      //display temp and humidity
      idle = true;
    }
  }

  // ********** output vent **********
  // if right rotate button (PH3) pressed: rotate CW 50 steps at 5 RPM
  if(*pin_h & 0x08){
    U0putchar("Right rotate button pressed\n");
    output_vent.setSpeed(5);
    output_vent.step(50);
  }
  // if left rotate button (PH4) pressed: rotate CCW 50 steps at 5 RPM
  if(*pin_h & 0x10){
    U0putchar("Left rotate button pressed\n");
    output_vent.setSpeed(5);
    output_vent.step(-50);
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
String timeReport()
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

//Interrupt Functions
void startPressed(){
  if(!on){
      U0putchar("\nswitch to idle at ");
      interruptTime = true;
      idle = true;
    }
    on = true;
  //turn green LED on
    *port_j &= ~(0x02);  // red off
    *port_j &= ~(0x01);  // yellow off
    *port_h |= 0x02;     // green ON
    *port_h &= ~(0x01);  // blue off
}

void stopPressed(){
  //stop
    if(on){
      U0putchar("\nswitch to disabled at ");
      interruptTime = true;
    }
    on = false;
    error = false;
    idle = false;

    //turn Yellow LED on
    *port_j &= ~(0x02);  // red off
    *port_j |= 0x01;     // yellow ON
    *port_h &= ~(0x02);  // green off
    *port_h &= ~(0x01);  // blue off
}

void resetPressed(){
  if(on){
    if(error){
      U0putchar("\nswitch to idle at ");
      interruptTime = true;
    }
    error = false;
    idle = true;

    //reset and red LED off, green LED on
    *port_j &= ~(0x02);  // red off
    *port_j &= ~(0x01);  // yellow off
    *port_h |= 0x02;     // green ON
    *port_h &= ~(0x01);  // blue off
  }
}
