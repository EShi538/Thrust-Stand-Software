#include <Arduino.h>
#include <Wire.h>
#include <SD.h>
#include <HX711_ADC.h>   
                          
///////////////////////////////////////////////////////////////////////////////////////
// DEFINITIONS

const int TORQUE_DOUT_PIN = 5;    // mcu > hx711 data out pin
const int TORQUE_SCK_PIN = 6;   // mcu > hx711 serial clock pin

const int CURRENT_PIN = A2;
const int VOLTAGE_PIN = A3;
const int CURRENT_SENSITIVITY = 0.020;
const int CURRENT_SENSITIVITY_NEW =  0.022;
const int ZERO_CURRENT_VOLTAGE = 0.22;

const int KNOWN_TORQUE = 1; //change this to update zeroing settings
//TODO: Use i2C to send knwon torque parameter form master to the slave
// by adding a 'q' character before the other numbers in the signal to send
// this way, the slave knows that it is recieving a knwon torque parameter signal 
// by seeing that the signal starts iwth a q character

// CALIBRATION DEFINITIONS

#define MANUAL_TORQUE_CALIBRATION_VALUE 1.0     // manual torque calibration value (float)

#define SERIAL_PRINT_INTERVAL 300.0      // interval between each printed value

///////////////////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
///////////////////////////////////////////////////////////////////////////////////////

HX711_ADC TorqueSensor(TORQUE_DOUT_PIN, TORQUE_SCK_PIN);

///////////////////////////////////////////////////////////////////////////////////////
// VARIABLE INSTANTIATIONS
///////////////////////////////////////////////////////////////////////////////////////

// GENERAL VARIABLES
unsigned long last_serial_timestamp = 0;

// CALIBRATION VARIABLES
boolean bool_tare_next;

float thrust_calibration_value;
float torque_calibration_value;

///////////////////////////////////////////////////////////////////////////////////////
//INPUT AND WRITING DEFINITIONS

const int SD_PIN = 10; //change this to change the SD card pin number
String signal;

File data_file; 
bool reading_on;
bool stop;
bool new_file_created;
bool zero;
bool taring;

////////////////////////////////////////////////////////////////////////////////////////
//LOAD CELL READING DEFINITIONS

const int RPM_PIN = 2;
const float MARKERS = 1;
float RPM;
float objects;
long prev_second;
bool see_object;

////////////////////////////////////////////////////////////////////////////////////////
//HELPER FUNCTIONS

//called when a signal is sent from master
void receiveEvent(int bytes){ 
  signal = "";
  while (Wire.available()) {  // Loop through all available data
    char c = Wire.read();  // Read a byte
    signal += c;  // Append to the string
  }

  if(signal == "b"){ // START data collection
    reading_on = true;
  }
  else if(signal == "e"){ // STOP data collection
    stop = true;
    reading_on = false;
  }
  else if(signal == "t"){
    taring = true;
  }
  else if(signal == "z"){
    zero = true;
  }
  else{ //write to file
    new_file_created = true;
  }
}

void count(){
  see_object = true;
}

void increment(){
  if(see_object){
    objects++;
    see_object = false;
  }
}

// Initializes Load Cell
void init_LoadCell () {
  Serial.println("Initializing the HX711 . . .");

  TorqueSensor.begin();
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  TorqueSensor.start(2000, _tare); //tare for 2 seconds
  if (TorqueSensor.getTareTimeoutFlag() || TorqueSensor.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }

  Serial.println("Done initializing HX711");
}

void calibrate(){
  Serial.println("Calibrating torque sensor");
  if(TorqueSensor.update()){
    float raw = TorqueSensor.getData();
    TorqueSensor.setCalFactor(raw / KNOWN_TORQUE);
  }
  Serial.println("Done calibrating torque sensor");
}

////////////////////////////////////////////////////////////////////////////////////////
//MAIN DRIVER CODE

void setup(){
  signal = "";
  reading_on = false;
  stop = false;
  new_file_created = false;
  zero = false;
  taring = false;
  RPM = 0;

  pinMode(CURRENT_PIN, INPUT);
  pinMode(VOLTAGE_PIN, INPUT);

  pinMode(RPM_PIN, INPUT);
  see_object = false;
  attachInterrupt(digitalPinToInterrupt(RPM_PIN), count, FALLING);

  //Initialize I2C protocol (slave)
  Wire.begin(9); //Slave arduino set to address 9
  Wire.onReceive(receiveEvent);

  //Initialize Serial
  Serial.begin(57600);
  Serial.println("Setting up");

  init_LoadCell(); //initialze the load cell

  //Initialize SD card; If no file is attached or something else goes wrong, 
  //the code put itself in an infinite loop
  if (!SD.begin(SD_PIN)) {
    Serial.println("Failed to initialize SD card");
    while(1); //infinite loop to prevent further looping by loop()
  }
}

void loop(){
  if(taring){
    if(zero){ 
      calibrate();
      zero = false;
      taring = false;
    }
  }
  else{
    if(new_file_created){ //create a new file
      String file_name = "Test_" + signal + ".csv";
      data_file = SD.open(file_name, FILE_WRITE); //create the file
      data_file.println("Current, Raw Current, Voltage, Raw Voltage, Torque, RPM"); //set up csv headers
      new_file_created = false;
    }

    if(data_file){
      if(reading_on){ //if a file exists and data logging/testing is turned on
        //calculate RPM; Can increase precision by adding more markers
        if(millis() >= prev_second + 1000){
          RPM = (objects / MARKERS) * 60.0;
          objects = 0;
          prev_second = millis();
        }
        
        if(TorqueSensor.update()){
          //read in current and voltage
          int current_value_in = analogRead(CURRENT_PIN);
          int voltage_value_in = analogRead(VOLTAGE_PIN);

          // Calculate the voltage, this is for a 5V Arduino, is that what we have?
          float voltage = (21 * voltage_value_in);

          // Calculate current
          float current_voltage = current_value_in * (5.0 / 1024.0);
          float current = (current_voltage - ZERO_CURRENT_VOLTAGE) / CURRENT_SENSITIVITY;
          
          if(millis() > last_serial_timestamp + SERIAL_PRINT_INTERVAL){     
            float torque_data = TorqueSensor.getData();    
            last_serial_timestamp = millis();

            Serial.println("Current: " + String(current, 4) + " | Voltage: " + String(voltage, 4) + " | Torque: " +  String(torque_data) + " | RPM: " + String(RPM));
            
            data_file.println(String(current) + ", " + String(current_value_in) + ", " + String(voltage) + ", " + String(voltage_value_in) + ", " + String(torque_data) + ", " + String(RPM)); 
            data_file.flush();
          }
        }
        //increment a rotation counter if the tachometer reads LOW (it sees something pass)
        increment();
      }
      else if(stop){ //If the signal to stop testing is recieved from master, close the file
        data_file.close();
        setup();
      }
    }
  }
}