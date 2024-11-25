

bool on = false;
bool error = false;
const int tempthreshold = 12;
const int waterthreshold = 12;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  int temp = 13;
  int water = 45;
  char input = Serial.read();
  if(input == 's'){
    //start
    on = true;
  }
  if(input == 'f'){
    //stop
    on = false;
    //turn Yellow LED on
  }
  if(on && input == 'r'){
    //reset and red LED off
    error = false;
  }
  if(on && error){
    //display error
    Serial.write("error");
  }
  if(on && !error){
    if(water < waterthreshold){
      //display error message and red LED on
      error = true;
    }
    else if(temp > tempthreshold){
      //blue LED on, run fan and delay, blue LED off
      //display temp and humidity
      Serial.write("running");
    }
    else{
      //fan off and green LED on
      //display temp and humidity
    }
  }
}
