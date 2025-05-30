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

const int SERIAL_PRINT_INTERVAL = 300;      // interval between each printed value to not overload the serial monitor
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

////////////////////////////////////////////////////////////////////////////////////////
//HELPER FUNCTIONS

//called when a signal is sent from master
void receiveEvent(int bytes){ 
  signal = "";
  char type = Wire.read(); //get first character 
  while (Wire.available()) {  // Loop through all available data
    char c = Wire.read();  // Read a byte
    signal += c;  // Append to the string
  }

  if(type == 'f'){ //write to flie
    new_file_created = true;
  }
  else if(type == 'm'){ //set marker
    marker_sent = true;
  }
  else if(type == 'q'){ //torque
    torque_signal = signal;
  }
  else if(type == 'r'){ //thrust
    thrust_signal = signal;
  }
  else if(type == 'z'){ //zero
    zero = true;
  }
  else if(type == 'b'){ // START data collection
    reading_on = true;
  }
  else if(type == 'e'){ // STOP data collection
    stop = true;
    reading_on = false;
  }
  else if(type == 't'){ // turn on taring mode
    taring = true;
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
  Serial.println(F("Initializing the HX711 . . ."));

  TorqueSensor.begin();
  ThrustSensor.begin();
  
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  TorqueSensor.start(2000, _tare); //tare for 2 seconds
  ThrustSensor.start(2000, _tare);

  if (TorqueSensor.getTareTimeoutFlag() || TorqueSensor.getSignalTimeoutFlag()) {
    Serial.println(F("Torque Sensor Timeout, check MCU>HX711 wiring and pin designations"));
    while (1);
  }
  
  if (ThrustSensor.getTareTimeoutFlag() || ThrustSensor.getSignalTimeoutFlag()) {
    Serial.println(F("Thrust Sensor Timeout, check MCU>HX711 wiring and pin designations"));
    while (1);
  }

  Serial.println(F("Done initializing HX711"));
}

void calibrate(){
  Serial.println(F("Calibrating sensors"));
  Serial.println(F("Calibrating torque sensor"));
  long start_time = millis();
  float average_raw = 0;
  float samples = 0;
  while(millis() < start_time + 2000){
    if(TorqueSensor.update()){
      samples++;
      float torque = TorqueSensor.getData();
      average_raw += torque;

      Serial.print(F("READING: "));
      Serial.print(torque);
      Serial.print(F(" KNOWN: "));
      Serial.println(KNOWN_TORQUE);
    }
  }
  average_raw = average_raw / samples;
  TorqueSensor.setCalFactor(average_raw / KNOWN_TORQUE);
  Serial.println(F("Done calibrating torque sensor"));

  Serial.println(F("Calibrating thrust sensor"));
  start_time = millis();
  average_raw = 0;
  samples = 0;
  while(millis() < start_time + 2000){
    if(ThrustSensor.update()){
      samples++;
      float thrust = ThrustSensor.getData();
      average_raw += thrust;

      Serial.print(F("READING: "));
      Serial.print(thrust);
      Serial.print(F(" KNOWN: "));
      Serial.println(KNOWN_THRUST);
    }
  }
  average_raw = average_raw / samples;
  ThrustSensor.setCalFactor(average_raw / KNOWN_THRUST);
  Serial.println(F("Done Calibrating thrust sensor"));

  Serial.println(F("Zeroing the airspeed sensor"));
  start_time = millis();
  average_raw = 0;
  samples = 0;
  while(millis() < start_time + 2000){
    samples++;
    float airspeed_voltage = analogRead(AIRSPEED_PIN) * (Vcc / 1023);
    average_raw += airspeed_voltage;
    
    Serial.print(F("READING: "));
    Serial.print(airspeed_voltage);
    Serial.println(F(" KNOWN: 0"));
    delay(5);
  }
  average_raw = average_raw / samples;
  zeroVoltage = average_raw;
  Serial.println(F("Done zeroing airspeed sensor"));

  Serial.println(F("Zeroing the current sensor"));
  start_time = millis();
  average_raw = 0;
  samples = 0;
  while(millis() < start_time + 2000){
    samples++;
    float current_voltage = analogRead(CURRENT_PIN) * (Vcc / 1023);
    average_raw += current_voltage ;
    
    Serial.print(F("READING: "));
    Serial.print(current_voltage);
    Serial.println(F(" KNOWN: 0"));
    delay(5);
  }
  average_raw = average_raw / samples;
  ZERO_CURRENT_VOLTAGE = average_raw;
  Serial.println(F("Done zeroing current sensor"));

  Serial.println(F("Done calibrating sensors"));
}

////////////////////////////////////////////////////////////////////////////////////////
//MAIN DRIVER CODE

void setup(){
  signal = "";
  reading_on = false;
  stop = false;
  new_file_created = false;
  marker_sent = false;
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
  Serial.println(F("Setting up"));

  init_LoadCell(); //initialze the load cell

  //Initialize SD card; If no file is attached or something else goes wrong, 
  //the code put itself in an infinite loop
  if (!SD.begin(SD_PIN)) {
    Serial.println(F("Failed to initialize SD card"));
    while(1); //infinite loop to prevent further looping by loop()
  }
}

extern int __heap_start, *__brkval;
int free_memory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void loop(){
  if(taring){
    if(zero){ 
      KNOWN_TORQUE = torque_signal.toInt();
      KNOWN_THRUST = thrust_signal.toInt();
      calibrate();
      zero = false;
      taring = false;
    }
  }
  else{
    if(new_file_created){ //create a new file
      String file_name = "Test_" + signal + ".csv";
      data_file = SD.open(file_name, FILE_WRITE); //create the file
      data_file.println("Current, Voltage, Torque, Thrust, RPM, Airspeed"); //set up csv headers
      new_file_created = false;
    }
    else if(marker_sent){
      MARKERS = signal.toInt();
      marker_sent = false;
    }

    if(data_file){
      if(reading_on){ //if a file exists and data logging/testing is turned on
        //RPM SENSOR READING
        //calculate RPM; Can increase precision by adding more markers
        if(millis() >= prev_second + 1000){
          RPM = (objects / MARKERS) * 60.0;
          objects = 0;
          prev_second = millis();
        }
        
        if(TorqueSensor.update() && ThrustSensor.update()){
          //CURRENT/VOLTAGE SENSOR READING
          //read in current and voltage
          int current_value_in = analogRead(CURRENT_PIN);
          int voltage_value_in = analogRead(VOLTAGE_PIN);          

          float voltage = (21 * voltage_value_in);

          float current_voltage = current_value_in * (Vcc / 1023.0);
          float current = (current_voltage - ZERO_CURRENT_VOLTAGE) / CURRENT_SENSITIVITY;

          //AIRSPEED SENSOR READING
          int raw = analogRead(AIRSPEED_PIN);
          float airspeed_voltage = raw * (Vcc / 1023.0);

          float pressure_kPa = (airspeed_voltage - zeroVoltage) / sensitivity; // Convert voltage to differential pressure in kPa
          float pressure_Pa = pressure_kPa * 1000.0; // Convert kPa to Pascals

          float airspeed = 0.0;          
          if (pressure_Pa > 0) {
            airspeed = sqrt((2.0 * pressure_Pa) / airDensity); // Compute airspeed using Bernoulli equation
          }

          if(millis() > last_serial_timestamp + SERIAL_PRINT_INTERVAL){     
            float torque_data = TorqueSensor.getData();  
            float thrust_data = ThrustSensor.getData();  
            last_serial_timestamp = millis();

            Serial.print(F("Current: "));
            Serial.print(current);
            Serial.print(F(" | Voltage: ")); 
            Serial.print(voltage);
            Serial.print(F(" | Torque: "));
            Serial.print(torque_data);
            Serial.print(F("| Thrust: "));
            Serial.print(thrust_data);
            Serial.print(F(" | RPM: "));
            Serial.print(RPM);
            Serial.print(F(" | AIRSPEED: "));
            Serial.println(airspeed);

            data_file.print(current);
            data_file.print(", "); 
            data_file.print(voltage);
            data_file.print(", ");
            data_file.print(torque_data);
            data_file.print(", ");
            data_file.print(thrust_data);
            data_file.print(", ");
            data_file.print(RPM);
            data_file.print(", ");
            data_file.println(airspeed);
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