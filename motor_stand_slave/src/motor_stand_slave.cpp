#include <Arduino.h>
#include <motor_stand_slave_definitions.h>

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

float zero_analog(float (*func)()){
  long start_time = millis();
  float average_raw = 0;
  float samples = 0;
  while(millis() < start_time + 2000){
    samples++;
    float reading = func();
    average_raw += reading;

    Serial.print(F("READING: "));
    Serial.print(reading);
    Serial.println(F(" KNOWN: 0"));   
    delay(5); 
  }
  average_raw = average_raw / samples;
  return average_raw;
}

void calibrate_hx711(HX711_ADC& load_cell, float known){
  long start_time = millis();
  float average_raw = 0;
  float samples = 0;
  while(millis() < start_time + 2000){
    if(load_cell.update()){
      samples++;
      float reading = load_cell.getData();
      average_raw += reading;

      Serial.print(F("READING: "));
      Serial.print(reading);
      Serial.print(F(" KNOWN: "));
      Serial.println(known);
    }
  }
  average_raw = average_raw / samples;
  load_cell.setCalFactor(average_raw / known);
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
  calibrate_hx711(TorqueSensor, KNOWN_TORQUE);
  Serial.println(F("Done calibrating torque sensor"));

  Serial.println(F("Calibrating thrust sensor"));
  calibrate_hx711(ThrustSensor, KNOWN_THRUST);
  Serial.println(F("Done Calibrating thrust sensor"));

  Serial.println(F("Zeroing the airspeed sensor"));
  zeroVoltage = zero_analog([]() {return analogRead(AIRSPEED_PIN) * (Vcc / 1023);});
  Serial.println(F("Done zeroing airspeed sensor"));

  Serial.println(F("Zeroing the current sensor"));
  ZERO_CURRENT_VOLTAGE = zero_analog([]() {return analogRead(CURRENT_PIN) * (Vcc / 1023);});
  Serial.println(F("Done zeroing current sensor"));

  Serial.println(F("Done calibrating sensors"));
}

extern int __heap_start, *__brkval;
int free_memory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
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
  Serial.print(F("Free RAM (in bytes): "));
  Serial.println(free_memory());

  init_LoadCell(); //initialze the load cell

  //Initialize SD card; If no file is attached or something else goes wrong, 
  //the code put itself in an infinite loop
  if (!SD.begin(SD_PIN)) {
    Serial.println(F("Failed to initialize SD card"));
    while(1); //infinite loop to prevent further looping by loop()
  }
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
        //RPM SENSOR READING AND CALCULATION; Can increase precision by adding more markers
        if(millis() >= prev_second + 1000){
          RPM = (objects / MARKERS) * 60.0;
          objects = 0;
          prev_second = millis();
        }
        
        if(TorqueSensor.update() && ThrustSensor.update()){
          //CURRENT/VOLTAGE SENSOR READING
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

          //THRUST AND TORQUE SENSOR READINGS
          float torque_data = TorqueSensor.getData();  
          float thrust_data = ThrustSensor.getData();  

          //RATE LIMIT THE WRITING TO AVOID OVERLOADING AND KEEP CONSISTENT DATAPOINTS
          if(millis() > last_serial_timestamp + SERIAL_PRINT_INTERVAL){     
            last_serial_timestamp = millis();

            Serial.print(F("Current: ")); Serial.print(current);
            Serial.print(F(" | Voltage: ")); Serial.print(voltage);
            Serial.print(F(" | Torque: ")); Serial.print(torque_data);
            Serial.print(F("| Thrust: ")); Serial.print(thrust_data);
            Serial.print(F(" | RPM: ")); Serial.print(RPM);
            Serial.print(F(" | AIRSPEED: ")); Serial.println(airspeed);

            data_file.print(current); data_file.print(", "); 
            data_file.print(voltage); data_file.print(", ");
            data_file.print(torque_data); data_file.print(", ");
            data_file.print(thrust_data); data_file.print(", ");
            data_file.print(RPM); data_file.print(", ");
            data_file.println(airspeed);
            data_file.flush();
          }
        }

        //increment a rotation counter when the tachometer sees a marker pass by)
        increment();
      }
      else if(stop){ //If the signal to stop testing is recieved from master, close the file
        data_file.close();
        setup();
      }
    }
  }
}