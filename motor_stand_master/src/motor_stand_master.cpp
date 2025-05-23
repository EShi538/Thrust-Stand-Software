#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

////////////////////////////////////////////////////////////////////////////////////////
//LCD I2C address: 0x27

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

////////////////////////////////////////////////////////////////////////////////////////
//KEYPAD AND I/O DEFINITIONS

//KEYBOARD INPUT (4x4 Membrane keypad):
const byte ROWS = 4; // rows
const byte COLS = 4; // columns

//change these values to change keymapping for starting data collection, stopping data collection, entering test number, and sending test number
const char START_INPUT = 'A'; 
const char CONFIRM_TARE = 'B';
const char ENTER_INPUT = '#';
const char SEND_INPUT = '*';
 
// Define the keymap
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {10, 11, 12, 13};
byte colPins[COLS] = {6, 7, 8, 9};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
String file_num;
bool input_done;
bool file_created;

////////////////////////////////////////////////////////////////////////////////////////
//THROTTLE LOGIC DEFINITIONS
/*
For HARGRAVE MICRODRIVE LP. Accepted PWM frequencies are from 50Hz to 499 Hz
*/
Servo esc; 

const int ESC_PIN = 3;
const int MIN_THROTTLE = 1000;
const int MAX_THROTTLE = 2000;
const int ARMING_DELAY = 2000;
const int INCREMENT_TIME = 4000;

String increment_input;
int throttleIncrement;
int pwm_increment;
int cycle_length;

bool increment_done;
bool start_motor; //tells the program to start throttling up the motor in increments
bool done_throttling;
bool throttling_up;
bool interrupted;
long prev_interval_timestamp;

const int THROTTLE_UP_DELAY = 10;

const int INTERRUPT_PIN = 2;
const int ARM_PIN = 1;

////////////////////////////////////////////////////////////////////////////////////////

bool tared = false; //only reset taring status after the entire system has been unpowered/restarted

////////////////////////////////////////////////////////////////////////////////////////
//HELPER FUNCTIONS

void lcd_home(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Test #:");
  lcd.setCursor(0, 1);
  lcd.print("Test Status:OFF ");
  lcd.setCursor(0, 2);
  lcd.print("File Status:NO FILE");
  lcd.setCursor(0, 3);
  lcd.print("Increment:");
  lcd.setCursor(7, 0);
}

void tare_ui(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("V:"); 
  lcd.setCursor(0, 1);
  lcd.print("I:");
  lcd.setCursor(0, 2);
  lcd.print("T:");
  lcd.setCursor(0, 3);
  lcd.print("R:");

  Wire.beginTransmission(9);
  Wire.write('t');
  Wire.endTransmission();
}

void send_file(){
  lcd.setCursor(12, 2);
  lcd.print("EXISTS ");
  file_created = true;
  Wire.beginTransmission(9);
  Wire.write(file_num.c_str());
  Wire.endTransmission();
}

void start_testing(){
  Serial.println("Starting: Test Num: " + file_num + " | Increment: " + String(throttleIncrement));
  lcd.setCursor(12, 1);
  lcd.print("ON   ");
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
    for(int i = cycle_length; i <= min(cycle_length + pwm_increment, MAX_THROTTLE); i++){
      esc.writeMicroseconds(i);
      delay(THROTTLE_UP_DELAY);
    }
    cycle_length = min(cycle_length + pwm_increment, MAX_THROTTLE);
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

////////////////////////////////////////////////////////////////////////////////////////
//MAIN DRIVER CODE

void setup() {
  file_num = "";
  input_done = false;
  file_created = false;

  increment_input = "";
  done_throttling = false;
  throttling_up = false;
  start_motor = false;
  increment_done = false;
  cycle_length = MIN_THROTTLE;
  interrupted = false;
  
  //Initialize Serial 
  Serial.begin(9600);
  Serial.println("Setting up");
  delay(3000); //account for delay on the slave for load cell initialization/taring
  //DO NOT LOAD THE TORQUE SENSOR UNTIL LCD SCREEN TURNS ON
  //LOADING THE LOAD CELL WHILE THE PROGRAM INITIALIZES IT WILL RESULT IN INACCURATE READINGS

  pinMode(INTERRUPT_PIN, INPUT_PULLUP); //set default switch position to HIGH
  pinMode(ARM_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), interrupt, FALLING); //when switch is pressed down

  // Set up the LCD display
  lcd.init();
  lcd.clear();
  lcd.backlight();
  if(tared){
    lcd_home();
  }
  else{
    tare_ui();
  }

  //Initialize I2C protocol (master)
  Wire.begin();

  //Initialize servo PWM and arm the ESC
  esc.attach(ESC_PIN); //set esc to pin
  esc.writeMicroseconds(MIN_THROTTLE); //minimum throttle; arm the esc
  delay(ARMING_DELAY);
}

void loop() {
  check_interrupt();
  char key = keypad.getKey();

  if(!tared){
    //TODO: get torque/RPM data from slave with Wire.request
    if(key && key == CONFIRM_TARE){ //the button to zero the values
      Wire.beginTransmission(9);
      Wire.write('z');
      Wire.endTransmission();
      tared = true;
      lcd_home(); //reset display to its normal "home" state
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
      //If user is still entering test number (AKA test number NOT confirmed yet)
      if(!input_done){
        if(key >= '0' && key <= '9' && file_num.length() < 3){ //if a digit is being entered and the test number is less or equal to 3 digits long, add digit to test number
          file_num += key;
          lcd.print(key);
        }
        else if(key == ENTER_INPUT){ //If the user confirms the test number, no further digits can be typed in; The user is only able to send files, start testing, and stop testing from now on
          lcd.print(ENTER_INPUT);
          input_done = true; 
        }
      }
      //if test number has been finalized (entered in) by the user
      //this ensures starting, stopping, and creating a logging file can only be done if the user confirmed the test number
      else {
        if(!file_created){ //if a file hasn't been created/sent to the slave yet...
          if(key == SEND_INPUT){ //send test number to the data collection arduino
              send_file();
          }
        }
        //This ensures that data recording can only commence once the file has been created
        else {
          if(!increment_done){
            lcd.setCursor(10 + increment_input.length(), 3);
            if(key >= '0' && key <= '9' && increment_input.length() < 2){ //set the throttle increment. Keep it below 100%
              if(!(increment_input.length() == 0 && key == '0')){ //making sure no goofy things with starting with zero
                lcd.print(key);
                increment_input += key;
              }
            }
            else if(key == ENTER_INPUT){
              lcd.print(ENTER_INPUT);
              throttleIncrement = increment_input.toInt(); //set the throttleIncrement to what the user inputted and display it on the info screen
              pwm_increment = map(throttleIncrement, 0, 100, 0, MAX_THROTTLE - MIN_THROTTLE); //1% -> 99% written in terms of PWM cycle length, assuming a linear mapping
              increment_done = true;
            }
          }
          else if(key == START_INPUT){ //tell data collection to start by sending 'b' for begin
            start_testing();
          }
        }      
      }
    }
  }
}