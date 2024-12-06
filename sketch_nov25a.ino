// Names: Alyssa Workman, Chanel Koh, Cole Kauffman, Jaydon McElvain
// Assignment: CPE 301 Final Project
// Date: Dec 14 2024

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

//ADC registers
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

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

// Define Port A Register Pointers
volatile unsigned char* port_a = (unsigned char*) 0x22; 
volatile unsigned char* ddr_a  = (unsigned char*) 0x21; 
volatile unsigned char* pin_a  = (unsigned char*) 0x20;

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
#define DHTPIN 38
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
float temp;
float humi;

//threshold values
const int tempthreshold = 75; //adjust
const int waterthreshold = 20; //adjust


unsigned int water = 0;


void setup() {
  U0init(9600); //Serial initialization
  rtc.begin(); //real-time clock initialization
  dht.begin(); // initialize the temp and humidity sensor
  temp = dht.readTemperature(true);
  humi  = dht.readHumidity();

  adc_init();
  
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
  //set PJ1 (pin 14) to OUTPUT (red LED)
  *ddr_j |= 0x02;
  //set PJ0 (pin 15) to OUTPUT (yellow LED)
  *ddr_j |= 0x01;
  //set PH1 (pin 16) to OUTPUT (green LED)
  *ddr_h |= 0x02;
  //set PH0 (pin 17) to OUTPUT (blue LED)
  *ddr_h |= 0x01;

  // ********** for fan motor ***************
  // set PA1 (pin 23) to OUTPUT 
  *ddr_a |= 0x02;

  // ********** for output vent *************
  // right button: set PH3 to INPUT
  *ddr_h &= 0xF7;
  // enable pullup resistor on PH3
  *port_h |= 0x08;
  
  // left button: set PH4 to INPUT
  *ddr_h &= 0xEF;
  // enable pullup resistor on PH4
  *port_h |= 0x10;

    //turn Yellow LED on
    *port_j &= ~(0x02);  // red off
    *port_j |= 0x01;     // yellow ON
    *port_h &= ~(0x02);  // green off
    *port_h &= ~(0x01);  // blue off

}

void loop() {
  if(interruptTime == true){
    interruptTime = false;
    U0putchar(timeReport());
  }
  water = adc_read(0);
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

    // fan motor off
    *port_a &= ~(0x02);


  }
  if(on && !error){
    // fan motor on
    *port_a |= 0x02;

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

void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}
unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
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
