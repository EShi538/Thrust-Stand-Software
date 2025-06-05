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

bool tared = false; //only reset taring status after the entire system has been unpowered/restarted
bool sending; 
bool choosing;
const int TARE_NUM = 2;
const String tare_names[] = {"KNOWN TORQUE:", "KNOWN THRUST:"};
String tare_values[TARE_NUM];
int tare_index;

String input;

////////////////////////////////////////////////////////////////////////////////////////
//KEYBOARD INITIALIZATION (4x4 Membrane keypad)

const byte ROWS = 4; // rows
const byte COLS = 4; // columns

//Non-digit keybindings
const char BACK_BUTTON = 'D';
const char ENTER_INPUT = '#';
const char SEND_INPUT = '*';
 
// Define the keymap
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

//Define pins
byte rowPins[ROWS] = {9, 10, 11, 12};
byte colPins[COLS] = {5, 6, 7, 8};

//Initialize the keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

////////////////////////////////////////////////////////////////////////////////////////
//THROTTLE LOGIC DEFINITIONS
//(For a HARGRAVE MICRODRIVE ESC, accepted PWM frequencies range from 50Hz to 499 Hz

Servo esc; 

const int MIN_THROTTLE = 1000;
int MAX_THROTTLE;

const int ESC_PIN = 3;
const int INCREMENT_TIME = 4000;
const int THROTTLE_UP_DELAY = 10;

int throttleIncrement;
int currthrottle;
int pwm_increment;
int cycle_length;

bool start_motor; 
bool done_throttling;
bool throttling_up;

unsigned long prev_interval_timestamp;

////////////////////////////////////////////////////////////////////////////////////////
//MANUAL OVERRIDE DEFINITIONS

const int INTERRUPT_PIN = 2;
bool interrupted;