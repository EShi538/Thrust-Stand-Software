#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

////////////////////////////////////////////////////////////////////////////////////////
//LCD I2C address: 0x27

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

////////////////////////////////////////////////////////////////////////////////////////
//I/O DEFINITIONS

const int PARAMETER_NUM = 4;
const String parameter_names[] = {"TEST #:", "MAX THROTTLE:", "INCREMENT:", "MARKERS:"};
String parameter_values[PARAMETER_NUM];
int parameter_index;

const int TARE_NUM = 2;
const String tare_names[] = {"KNOWN TORQUE:", "KNOWN THRUST:"};
String tare_values[TARE_NUM];
int tare_index;

String input;
String file_num;

////////////////////////////////////////////////////////////////////////////////////////
//KEYBOARD INITIALIZATION (4x4 Membrane keypad)

const byte ROWS = 4; // rows
const byte COLS = 4; // columns

//Non-digit keybindings
const char BACK_BUTTON = 'D';
const char ENTER_INPUT = '#';
const char SEND_INPUT = '*';
const char TARE_MODE = 'B';
 
// Define the keymap
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

//Define pins
byte rowPins[ROWS] = {10, 11, 12, 13};
byte colPins[COLS] = {6, 7, 8, 9};

//Initialize the keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

////////////////////////////////////////////////////////////////////////////////////////
//THROTTLE LOGIC DEFINITIONS
//(For a HARGRAVE MICRODRIVE ESC, accepted PWM frequencies range from 50Hz to 499 Hz

Servo esc; 

const int MIN_THROTTLE = 1000;
int MAX_THROTTLE;

const int ESC_PIN = 3;
const int ARMING_DELAY = 2000;
const int INCREMENT_TIME = 4000;
const int THROTTLE_UP_DELAY = 10;

int throttleIncrement;
int currthrottle;
int pwm_increment;
int cycle_length;

bool start_motor; 
bool done_throttling;
bool throttling_up;

long prev_interval_timestamp;

////////////////////////////////////////////////////////////////////////////////////////
//MANUAL OVERRIDE DEFINITIONS

const int INTERRUPT_PIN = 2;
bool interrupted;

////////////////////////////////////////////////////////////////////////////////////////
//TARING LOGIC

bool tared = false; //only reset taring status after the entire system has been unpowered/restarted

////////////////////////////////////////////////////////////////////////////////////////
//HELPER FUNCTIONS
void lcd_home(){
  input = "";
  lcd.setCursor(0, 3);
  lcd.print("THROTTLE:OFF");
  lcd.setCursor(0, 0);
  lcd.print("                    ");
  lcd.setCursor(0, 0);
  lcd.print(parameter_names[parameter_index] + parameter_values[parameter_index]);
  lcd.setCursor(0, 1);
  lcd.print("                    ");
  lcd.setCursor(0, 2);
  lcd.print("NEXT: " +  String(ENTER_INPUT) + " | BACK: " + String(BACK_BUTTON) + "   ");
  lcd.setCursor(0, 1);
}

void tare_ui(){
  input = "";
  lcd.setCursor(0, 3);
  lcd.print("THROTTLE:OFF");
  lcd.setCursor(0, 0);
  lcd.print("                    ");
  lcd.setCursor(0, 0);
  lcd.print(tare_names[tare_index] + tare_values[tare_index]);
  lcd.setCursor(0, 1);
  lcd.print("                    ");
  lcd.setCursor(0, 2);
  lcd.print("NEXT: " +  String(ENTER_INPUT) + " | BACK: " + String(BACK_BUTTON) + "   ");
  lcd.setCursor(0, 1);
}

void start_tare(){
  Wire.beginTransmission(9);
  Wire.write('t');
  Wire.endTransmission();
}

void send_file(){
  file_num = parameter_values[0];
  String signal = "f" + parameter_values[0];
  Wire.beginTransmission(9);
  Wire.write(signal.c_str());
  Wire.endTransmission();
}

void send_markers(){
  String signal = "m" + parameter_values[3];
  Wire.beginTransmission(9);
  Wire.write(signal.c_str());
  Wire.endTransmission();
}

void start_testing(){
  Serial.println("Starting: Test Num: " + file_num + " | Increment: " + String(throttleIncrement));
  start_motor = true;
  Wire.beginTransmission(9);
  Wire.write('b');
  Wire.endTransmission();
}

void end_testing(){
  Wire.beginTransmission(9);
  Wire.write('e');
  Wire.endTransmission();
  setup();
}

void throttle_up(){
  if(millis() >= prev_interval_timestamp + INCREMENT_TIME && cycle_length >= MAX_THROTTLE){
    Serial.println("DONE THROTTLING");
    done_throttling = true;
  }
  else if(millis() >= prev_interval_timestamp + INCREMENT_TIME){
    Serial.println(cycle_length);
    lcd.setCursor(0, 3);
    lcd.print("THROTTLE:" + String(currthrottle) + " ");
    for(int i = cycle_length; i <= min(cycle_length + pwm_increment, MAX_THROTTLE); i++){
      esc.writeMicroseconds(i);
      delay(THROTTLE_UP_DELAY);
    }
    cycle_length = min(cycle_length + pwm_increment, MAX_THROTTLE);
    currthrottle += throttleIncrement;
    prev_interval_timestamp = millis();
  }
}

void throttle_down(){
  for(int i = cycle_length; i >= MIN_THROTTLE; i--){
    esc.writeMicroseconds(i);
    delay(THROTTLE_UP_DELAY);
  }
  end_testing();
}

void interrupt(){
  interrupted = true;
}

void check_interrupt(){
  if(interrupted){
    Serial.println("Manual Stop");
    interrupted = false;
    throttle_down();
  }
}

void setup_next_input(){
  parameter_values[parameter_index] = input;
  parameter_index++;
  if(parameter_index >= PARAMETER_NUM){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PRESS " + String(SEND_INPUT) + " TO START");
  }
  else{
    lcd_home();
  }
}

void setup_prev_input(){
  parameter_index--;
  lcd_home();
}

void setup_next_tare_input(){
  tare_values[tare_index] = input;
  tare_index++;
  if(tare_index >= TARE_NUM){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PRESS " + String(SEND_INPUT) + " TO ZERO");
  }
  else{
    tare_ui();
  }
}

void setup_prev_tare_input(){
  tare_index--;
  tare_ui();
}

bool all_entered(){
  for(int i = 0; i < PARAMETER_NUM; i++){
    if(parameter_values[i] == ""){
      return false;
    }
  }
  return true;
}

bool all_entered_tare(){
  for(int i = 0; i < TARE_NUM; i++){
    if(tare_values[i] == ""){
      return false;
    }
  }
  return true;
}

void send_torque(){
  String signal = "q" + tare_values[0];
  Wire.beginTransmission(9);
  Wire.write(signal.c_str());
  Wire.endTransmission();
}

void send_thrust(){
  String signal = "r" + tare_values[1];
  Wire.beginTransmission(9);
  Wire.write(signal.c_str());
  Wire.endTransmission();
}

void start_zero(){
  Wire.beginTransmission(9);
  Wire.write('z');
  Wire.endTransmission();
  tared = true;
  delay(5000);
}

void setup_tare_mode(){
  Serial.println("ENTERING TARE MODE");
  tare_index = 0;
  tared = false;
  tare_ui();
  start_tare();
}

void send_tare_inputs(){
    Serial.println("CALIBRATING...");
    lcd.setCursor(0, 1);
    lcd.print("CALIBRATING...");

    send_torque();
    delay(1000);

    send_thrust();
    delay(1000);

    start_zero();

    Serial.println("DONE CALIBRATING");
    Serial.println("EXITING TARE MODE");
    lcd_home(); //reset display to its normal "home" state
}

void send_inputs(){
  send_file();
  delay(100);

  MAX_THROTTLE = min(max(parameter_values[1].toInt(), 1000), 2000);

  throttleIncrement = parameter_values[2].toInt();
  pwm_increment = map(throttleIncrement, 0, 100, 0, MAX_THROTTLE - MIN_THROTTLE); //1% -> 99% written in terms of PWM cycle length, assuming a linear mapping

  send_markers();
  
  Serial.println("TEST PARAMETERS CONFIRMED");
  start_testing();
}

////////////////////////////////////////////////////////////////////////////////////////
//MAIN DRIVER CODE

void setup() {
  pinMode(INTERRUPT_PIN, INPUT_PULLUP); //set default switch position to HIGH
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), interrupt, FALLING); //when switch is pressed down

  for(int i = 0; i < PARAMETER_NUM; i++){
    parameter_values[i] = "";
  }
  input = "";
  parameter_index = 0;

  done_throttling = false;
  throttling_up = false;
  start_motor = false;
  cycle_length = MIN_THROTTLE;
  interrupted = false;
  
  // Set up the LCD display
  lcd.init();
  lcd.backlight();
  //Initialize Serial 
  Serial.begin(9600);
  Serial.println("Setting up");
  delay(3000); //account for delay on the slave for load cell initialization/taring
  //DO NOT LOAD THE TORQUE SENSOR UNTIL LCD SCREEN TURNS ON
  //LOADING THE LOAD CELL WHILE THE PROGRAM INITIALIZES IT WILL RESULT IN INACCURATE READINGS

  //Initialize I2C protocol (master)
  Wire.begin();

  //Initialize servo PWM and arm the ESC
  esc.attach(ESC_PIN); //set esc to pin
  esc.writeMicroseconds(MIN_THROTTLE); //minimum throttle; arm the esc
  delay(ARMING_DELAY);
  
  Serial.println("READY");
  if(!tared){
    setup_tare_mode();
  }
  else{
    lcd_home();
  }
}

void loop() {
  check_interrupt();
  char key = keypad.getKey();

  if(!tared){
    if(key){
      if(key >= '0' && key <= '9' && input.length() < 2){
        input += key;
        lcd.print(key);
      }   
      else if(key == ENTER_INPUT && input != ""){
        setup_next_tare_input();
      }
      else if(key == BACK_BUTTON){
        setup_prev_tare_input();
      }
      else if(key == SEND_INPUT && all_entered_tare()){ //the button to zero the values
        send_tare_inputs();
      }
    }
  }
  else{ //can only do everything else once sensors have been tared
    if(start_motor){
      throttle_up();
    }

    if(done_throttling){
      throttle_down();
    }

    //if a keystroke has been entered from the keypad
    if(key){
      if(key == BACK_BUTTON){
        setup_prev_input();
      }
      else if(key == ENTER_INPUT && input != ""){
        setup_next_input();
      }
      else if(key == SEND_INPUT && all_entered()){
        send_inputs();
      }
      else if(key >= '0' && key <= '9'){
        switch(parameter_index){
          case 0: //ENTERING TEST NUMBER/FILE INFO
            if(input.length() < 3){ 
              input += key;
              lcd.print(key);
            }
            break;
          case 1: //ENTERING MAX THROTTLE
            if(input.length() < 4){ 
              input += key;
              lcd.print(key);
            }
            break;
          case 2: //ENTERING INCREMENT
            if(input.length() < 2){ 
              input += key;
              lcd.print(key);
            }
            break;
          case 3: //ENTERING MARKERS
            if(input.length() < 2){ 
              input += key;
              lcd.print(key);
            }
            break;
        }
      }
      else if(!start_motor && key == TARE_MODE){
        setup_tare_mode();
      }
    }
  }
}