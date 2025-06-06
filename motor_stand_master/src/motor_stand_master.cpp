#include <Arduino.h>
#include <motor_stand_master_definitions.h>

////////////////////////////////////////////////////////////////////////////////////////
//HELPER FUNCTIONS:

void lcd_home(){
  input = "";
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(parameter_names[parameter_index] + parameter_values[parameter_index]);
  lcd.setCursor(0, 2);
  lcd.print("NEXT: " +  String(ENTER_INPUT) + " | BACK: " + String(BACK_BUTTON));
  lcd.setCursor(0, 3);
  lcd.print("THROTTLE:OFF");
  lcd.setCursor(0, 1);
}

void tare_ui(){
  input = "";
  lcd.clear();
  lcd.print(tare_names[tare_index] + tare_values[tare_index]);
  lcd.setCursor(0, 2);
  lcd.print("NEXT: " +  String(ENTER_INPUT) + "             ");
  lcd.setCursor(0, 3);
  if(tare_index == 0){
    lcd.print("UNITS: N.mm");
  }
  else{
    lcd.print("UNITS: N");
  }
  lcd.setCursor(0, 1);
}

void send_ui(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PRESS " + String(SEND_INPUT) + " TO TARE");
  if(tare_index == 2){
    lcd.setCursor(0, 1);
    lcd.print("ANALOG SENSORS");
  }
  lcd.setCursor(0, 3);
  lcd.print("BACK: " + String(BACK_BUTTON));
  lcd.setCursor(0, 1);
}

void send_parameters(String type, String value){
  String signal = type + value;
  int length = signal.length();
  Serial.println("SENDING: " + signal);
  Wire.beginTransmission(9);
  Wire.write(signal.c_str(), length);
  Wire.endTransmission();
}

void start_testing(){
  lcd.setCursor(0, 0);
  lcd.print("                    ");
  lcd.setCursor(0, 0);
  lcd.print("RUNNING TEST");
  Serial.println("Starting: Test Num: " + parameter_values[0] + " | Increment: " + String(throttleIncrement));
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

void send_inputs(){
  send_parameters("f", parameter_values[0]);
  delay(100);

  int max_throttle_input = min(max(parameter_values[1].toInt(), 0), 100);
  MAX_THROTTLE = map(max_throttle_input, 0, 100, 1000, 2000);

  throttleIncrement = min(parameter_values[2].toInt(), 100);
  pwm_increment = map(throttleIncrement, 0, 100, 0, MAX_THROTTLE - MIN_THROTTLE); //1% -> 99% written in terms of PWM cycle length, assuming a linear mapping
  
  send_parameters("m", parameter_values[3]);

  INCREMENT_TIME = parameter_values[4].toInt() * 1000;
  Serial.println("TEST PARAMETERS CONFIRMED");
  Serial.println("MAX THROTTLE" + String(MAX_THROTTLE));
  Serial.println("INCREMENT" + String(pwm_increment));
  Serial.println("INCREMENT TIME" + String(INCREMENT_TIME));

  start_testing();
}

////////////////////////////////////////////////////////////////////////////////////////
//MAIN DRIVER CODE:

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
  currthrottle = 0;
  
  // Set up the LCD display
  lcd.init();
  lcd.backlight();
  //Initialize Serial 
  Serial.begin(9600);
  Serial.println("Setting up");
  lcd.print("LOADING...");

  //Initialize I2C protocol (master)
  Wire.begin();

  //Initialize servo PWM and arm the ESC
  esc.attach(ESC_PIN); //set esc to pin
  esc.writeMicroseconds(MIN_THROTTLE); //minimum throttle; arm the esc

  while(1){
    Wire.requestFrom(9, 1);
    if(Wire.read() == 1){
      break;
    }
    delay(100);
  }

  lcd.setCursor(0, 0);
  lcd.print("USE PREVIOUS TARE?");
  lcd.setCursor(0, 3);
  lcd.print("YES: A | NO: B");
  choosing = true;
  tared = false;

  Serial.println("READY");
}

void loop() {
  check_interrupt();
  char key = keypad.getKey();
  
  if(!tared){
    if(key){
      if(!choosing){
        if(!sending){
          if(key >= '0' && key <= '9' && input.length() < 9){
            input += key;
            lcd.print(key);
          }   
          else if(key == ENTER_INPUT && input != "" && tare_index < TARE_NUM){
            tare_values[tare_index] = input;
            input = "";
            send_ui();
            sending = true;
          }
        }
        else{
          if(key == BACK_BUTTON && tare_index != 2){
            tare_ui();
            sending = false;
          }
          else if(key == SEND_INPUT){ //the button to zero the values
            lcd.setCursor(0, 1);
            lcd.print("CALIBRATING...");
            if(tare_index == 0){
              send_parameters("q", tare_values[tare_index]); //tell slave to tare torque
              while(1){
                Wire.requestFrom(9, 1);
                if(Wire.read() == 1){
                  break;
                }
                delay(100);
              }
              tare_index++;
              tare_ui();
              sending = false;
            }
            else if(tare_index == 1){
              send_parameters("r", tare_values[tare_index]); //tell slave to tare thrust
              while(1){
                Wire.requestFrom(9, 1);
                if(Wire.read() == 1){
                  break;
                }
                delay(100);
              }
              tare_index++;
              send_ui();
            }
            else if(tare_index == 2){ //tell slave to tare analog sensors
              Wire.beginTransmission(9);
              Wire.write('a');
              Wire.endTransmission();
              while(1){
                Wire.requestFrom(9, 1);
                if(Wire.read() == 1){
                  break;
                }
                delay(100);
              }
              lcd_home();
              tared = true;
              sending = false;
            }
          }
        }
      }
      else{
        if(key == 'A'){
          Wire.beginTransmission(9);
          Wire.write('p');
          Wire.endTransmission();
          delay(100);
          choosing = false;
          tared = true;
          lcd_home();
        }
        else if(key == 'B'){
          tare_index = 0;
          sending = false;
          choosing = false;
          tare_ui();
        }
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
      if(key == BACK_BUTTON && parameter_index > 0){
        setup_prev_input();
      }
      else if(key == ENTER_INPUT && input != "" && parameter_index < PARAMETER_NUM){
        setup_next_input();
      }
      else if(key == SEND_INPUT && parameter_index == PARAMETER_NUM){
        send_inputs();
      }
      else if(key >= '0' && key <= '9'){
        if(input.length() < 3){ 
          input += key;
          lcd.print(key);
        }
      }
    }
  }
}