//Basic code is from https://github.com/zaidpirwani/DiffMotorVelocityControl

#define DEBUG 0
#define MAGICADDRESS 7

#include <math.h>
#include <EEPROM.h>

// For encoders
#include <Encoder.h>
Encoder encL(3,5);
Encoder encR(2,4);
long encoderL=0;
long encoderR=0;

// For Motor Control
#include "MotorLib.h"
MotorLib motorL(6, 7, 9);
MotorLib motorR(12, 13, 10);
// Struct to hold motor parameters
struct motorParams {
  double kp;
  double ki;
  double kd;
};
motorParams motorPIDL;
motorParams motorPIDR;

// For PID for motors
#include <PID_v1.h>
double vel1 = 0, vel2 = 0;
double spd1, spd2;
double pwm1, pwm2;
PID pidL(&spd1, &pwm1, &vel1, 1,0,0, DIRECT);
PID pidR(&spd2, &pwm2, &vel2, 1,0,0, DIRECT);
boolean pidActive= false;

// Extra Information
const float wheel_radius = 3.35;  // in cm
const float circumference = 2 * M_PI * wheel_radius;
const float tickDistance = (float)circumference/1500.0;

// For incoming Serial String
String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

unsigned long previousMillis = 0;
const long interval = 10;

float totalDistance = 0;

void setup() {
  Serial.begin(115200);
  inputString.reserve(100);
  
  encL.write(0);
  encR.write(0);
  motorL.setDir(FORWARD);
  motorR.setDir(FORWARD);

  initEEPROM();
  
  // initalizing PID
  pidL.SetMode(MANUAL);       // PID CONTROL OFF
  pidR.SetMode(MANUAL);
  pidL.SetTunings(motorPIDL.kp, motorPIDL.ki, motorPIDL.kd);
  pidR.SetTunings(motorPIDR.kp, motorPIDR.ki, motorPIDR.kd);
  pidL.SetSampleTime(interval);      // Sample time for PID
  pidR.SetSampleTime(interval);
  pidL.SetOutputLimits(0,255);  // min/max PWM
  pidR.SetOutputLimits(0,255);

}

void loop() {

  if (stringComplete) {
    interpretSerialData();
    stringComplete = false;
    inputString = "";
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    long encCurr1 = encL.read();
    long encCurr2 = encR.read();
    encoderL += encCurr1;
    encoderR += encCurr2;
    resetEncoders();

    float distance1 = (float)encCurr1*tickDistance;
    spd1 = (float)distance1*(1000.0/interval);
    float distance2 = (float)encCurr2*tickDistance;
    spd2 = (float)distance2*(1000.0/interval);

    totalDistance += (distance1 + distance2)/2;    //total Distance
    
/*    debugCount++;
    if(DEBUG && (debugCount%10==0)){
//      Serial.print(encCurr1);
//      Serial.print(", ");
//      Serial.print(distance1);
//      Serial.print(", ");
      Serial.print(spd1);
      Serial.print(", ");
//      Serial.print(vel1);
//      Serial.print(", ");
      Serial.print(vel1-spd1);
      Serial.print(", ");
      Serial.print(pwm1);
      Serial.print(" | ");
//      Serial.print(encCurr2);
//      Serial.print(", ");
//      Serial.print(distance2);
//      Serial.print(", ");
      Serial.print(spd2);
      Serial.print(", ");
//      Serial.print(vel2);
//      Serial.print(", ");
      Serial.print(vel2-spd2);
      Serial.print(", ");
      Serial.print(pwm2);
      Serial.println();
    }*/
  }
  pidL.Compute();
  pidR.Compute();
  if (totalDistance < 75){
  if(pidActive){
    if(vel1>0) motorL.setPWM(pwm1);
    else motorL.setPWM(0);
    if(vel2>0) motorR.setPWM(pwm2);
    else motorR.setPWM(0);
  }
 }
 else {
    motorL.setDir(BRAKE);
    motorR.setDir(BRAKE);
    motorL.setPWM(0);
    motorR.setPWM(0);
  }   
  
}

void(* resetFunc) (void) = 0;//declare reset function at address 0

void interpretSerialData(void){
    int c1=1, c2=1;
    int val1=0, val2=0;
    
    switch(inputString[0]){
      case 'V':
        // COMMAND:  V,speed_motor_left,speed_motor_right\n
        c1 = inputString.indexOf(',')+1;
        c2 = inputString.indexOf(',',c1);
        val1 = inputString.substring(c1,c2).toInt();
        c1 = c2+1;
        val2 = inputString.substring(c1).toInt();
        if(val1<0) { motorL.setDir(BACKWARD); val1 = -val1; }
        else         motorL.setDir(FORWARD);
        if(val2<0) { motorR.setDir(BACKWARD); val2 = -val2; }
        else         motorR.setDir(FORWARD);
        vel1 = val1;
        vel2 = val2;
        if(DEBUG){
          Serial.print("Velocity 1 ");
          Serial.println(vel1);
          Serial.print("Velocity 2 ");
          Serial.println(vel2);
        }         
        
        if(vel1>0)
          pidL.SetMode(AUTOMATIC);
        else
          pidL.SetMode(MANUAL);
        if(vel2>0)
          pidR.SetMode(AUTOMATIC);
        else
          pidR.SetMode(MANUAL);
        pidActive= true;
        totalDistance = 0;
        break;
        
      case 'P':
        // COMMAND:  P,kp,ki,kd,1/2\n
        float p,i,d;
        c1 = inputString.indexOf(',')+1;
        c2 = inputString.indexOf(',',c1);
        p = inputString.substring(c1,c2).toFloat();
        c1 = c2+1;
        c2 = inputString.indexOf(',',c1);
        i = inputString.substring(c1,c2).toFloat();
        c1 = c2+1;
        c2 = inputString.indexOf(',',c1);
        d = inputString.substring(c1,c2).toFloat();
        c1 = c2+1;
        val1 = inputString.substring(c1).toInt();
        
        if(val1==1) {
          motorPIDL.kp = p;
          motorPIDL.ki = i;
          motorPIDL.kd = d;
          pidL.SetTunings(motorPIDL.kp, motorPIDL.ki, motorPIDL.kd);
          EEPROM.put((const int)MAGICADDRESS, motorPIDL);
        }
        else if(val1==2){
          motorPIDR.kp = p;
          motorPIDR.ki = i;
          motorPIDR.kd = d;
          pidR.SetTunings(motorPIDR.kp, motorPIDR.ki, motorPIDR.kd);
          EEPROM.put((const int)(MAGICADDRESS+sizeof(motorParams)), motorPIDR);
        }
        if (DEBUG){
        Serial.print("kp: ");
        Serial.println(p);
        Serial.print("ki: ");
        Serial.println(i);
        Serial.print("kd: ");
        Serial.println(d);
        Serial.println('h');
        }
        break;
      
      case 'L':
        // COMMAND:  L,pwm_motor_left, pwm_motor_right\n
        c1 = inputString.indexOf(',')+1;
        c2 = inputString.indexOf(',',c1);
        val1 = inputString.substring(c1,c2).toInt();
        c1 = inputString.indexOf(',',c2)+1;
        val2 = inputString.substring(c1).toInt();
        
        if(val1<0) { motorL.setDir(BACKWARD); val1 = -val1; }
        else         motorL.setDir(FORWARD);
        if(val2<0) { motorR.setDir(BACKWARD); val2 = -val2; }
        else         motorR.setDir(FORWARD);
        
        pidActive= false;
        pidL.SetMode(MANUAL);
        pidR.SetMode(MANUAL);
        pwm1 = val1;
        pwm2 = val2;
        motorL.setPWM(pwm1);
        motorR.setPWM(pwm2);
        if(DEBUG){
          Serial.print("PWM1: "); Serial.println(val1);        
          Serial.print("PWM2: "); Serial.println(val2);
        }
        break;
      
      case 'R':
        // COMMAND:  R\n
        Serial.print("r,");
        Serial.print(encoderL);
        Serial.print(',');
        Serial.print(encoderR);
        Serial.println();
        break;
      
      case 'X':
        // COMMAND: I\n
        resetEncoders();
        encoderL = 0;
        encoderR = 0;
        Serial.println('i');
        break;
      
      default:
        Serial.print("UNKNOWN COMMAND: ");
        Serial.println(inputString);
        break;
    }
}

void resetEncoders(void){
  encL.write(0);
  encR.write(0);
}

void initEEPROM(void){
  int checkEEPROM=0;
  EEPROM.get(0, checkEEPROM);
  if(checkEEPROM==MAGICADDRESS){
    if(DEBUG) Serial.println("Reading from EEPROM.");
    EEPROM.get((const int)MAGICADDRESS, motorPIDL);   //(address, data)
    EEPROM.get((const int)(MAGICADDRESS+sizeof(motorParams)), motorPIDR);
  }
  else{
    //set default values
    if(DEBUG) Serial.println("Setting Default Values.");
    EEPROM.put(0, MAGICADDRESS);
    motorPIDL.kp = 1.0;
    motorPIDL.ki = 0.0;
    motorPIDL.kd = 0.0;
    EEPROM.put((const int)MAGICADDRESS, motorPIDL);

    motorPIDR.kp = 1.0;
    motorPIDR.ki = 0.0;
    motorPIDR.kd = 0.0;
    EEPROM.put((const int)(MAGICADDRESS+sizeof(motorParams)), motorPIDR);
  }  
}


void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}
