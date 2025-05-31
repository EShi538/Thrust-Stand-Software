#include <Arduino.h>
#include <Wire.h>
#include <SD.h>
#include <HX711_ADC.h>   
 
///////////////////////////////////////////////////////////////////////////////////////
//AIRSPEED SENSOR DEFINITIONS

const int AIRSPEED_PIN = A0;          // Analog pin connected to sensor output
const float Vcc = 5.0;             // MODIFY THIS VALUE TO MATCH WITH 5V PIN VOLTAGE
float zeroVoltage = 2.7;     // MODIFY THIS VALUE TO CORRESPOND TO VOLTAGE WITHOUT ANY AIRFLOW
const float sensitivity = 1;     // Sensor sensitivity in V/kPa
const float airDensity = 1.2;    // Air density at sea level in kg/m^3

///////////////////////////////////////////////////////////////////////////////////////
//THRUST SENSOR DEFINITIONS

const int THRUST_DOUT_PIN = 3;
const int THRUST_SCK_PIN = 4;

String thrust_signal;
float KNOWN_THRUST;

HX711_ADC ThrustSensor(THRUST_DOUT_PIN, THRUST_SCK_PIN);

///////////////////////////////////////////////////////////////////////////////////////
// TORQUE SENSOR DEFINITIONS

const int TORQUE_DOUT_PIN = 5;    // mcu > hx711 data out pin
const int TORQUE_SCK_PIN = 6;   // mcu > hx711 serial clock pin

String torque_signal;
float KNOWN_TORQUE; //for taring

HX711_ADC TorqueSensor(TORQUE_DOUT_PIN, TORQUE_SCK_PIN);

////////////////////////////////////////////////////////////////////////////////////////
//RPM/TACHOMETER SENSOR DEFINITIONS

const int RPM_PIN = 2;
float MARKERS = 1;
float RPM;
float objects;
long prev_second;
bool see_object;

///////////////////////////////////////////////////////////////////////////////////////
// CURRENT AND VOLTAGE SENSOR DEFINITIONS

const int CURRENT_PIN = A2;
const int VOLTAGE_PIN = A3;
const float CURRENT_SENSITIVITY = 0.020;
float ZERO_CURRENT_VOLTAGE = 0.22;

///////////////////////////////////////////////////////////////////////////////////////
// TIMING VARIABLE DEFINITIONS (FOR TRACKING)

const int SERIAL_PRINT_INTERVAL = 100;      // interval between each printed value to not overload the serial monitor
unsigned long last_serial_timestamp = 0;

///////////////////////////////////////////////////////////////////////////////////////
//PARAMETER I2C RECIEVER AND SD CARD WRITING DEFINITIONS

const int SD_PIN = 10; //change this to change the SD card pin number

String signal;
File data_file; 
bool reading_on;
bool stop;
bool new_file_created;
bool marker_sent;
bool zero;
bool taring;

///////////////////////////////////////////////////////////////////////////////////////
