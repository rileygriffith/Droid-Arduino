// =======================================================================================
//                     PS3 Starting Sketch for Notre Dame Droid Class
// =======================================================================================
//                          Last Revised Date: 02/06/2021
//                             Revised By: Prof McLaughlin
// =======================================================================================
// ---------------------------------------------------------------------------------------
//                          Libraries
// ---------------------------------------------------------------------------------------
#include <PS3BT.h>
#include <usbhub.h>
#include <Sabertooth.h>
#include <Adafruit_TLC5947.h>
#include <MP3Trigger.h>
#include <Servo.h> 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NewPing.h>
#include <MP3Trigger.h>
#include <Adafruit_TLC5947.h>


// ---------------------------------------------------------------------------------------
//                       Sound Data Ports
// ---------------------------------------------------------------------------------------
#define clock 5
#define data 6
#define latch 4

// ---------------------------------------------------------------------------------------
//                       Debug - Verbose Flags
// ---------------------------------------------------------------------------------------
#define SHADOW_DEBUG       //uncomment this for console DEBUG output

// ---------------------------------------------------------------------------------------
//                 Setup for USB, Bluetooth Dongle, & PS3 Controller
// ---------------------------------------------------------------------------------------
USB Usb;
BTD Btd(&Usb);
PS3BT *PS3Controller=new PS3BT(&Btd);

// Declare MP3 Trigger
MP3Trigger MP3Trigger;

int oldTurnNum = 0;
int oldFootDriveSpeed = 0;
byte driveDeadBandRange = 10;
#define SABERTOOTH_ADDR 128
Sabertooth *ST=new Sabertooth(SABERTOOTH_ADDR, Serial1);

Adafruit_TLC5947 LEDControl = Adafruit_TLC5947(1, clock, data, latch);
int ledMaxBright = 4000;

// ---------------------------------------------------------------------------------------
//    Output String for Serial Monitor Output
// ---------------------------------------------------------------------------------------
char output[300];

// ---------------------------------------------------------------------------------------
//    Deadzone range for joystick to be considered at nuetral
// ---------------------------------------------------------------------------------------
byte joystickDeadZoneRange = 15;

// ---------------------------------------------------------------------------------------
//    Used for PS3 Fault Detection
// ---------------------------------------------------------------------------------------
uint32_t msgLagTime = 0;
uint32_t lastMsgTime = 0;
uint32_t currentTime = 0;
uint32_t lastLoopTime = 0;
int badPS3Data = 0;

boolean isPS3ControllerInitialized = false;
boolean mainControllerConnected = false;
boolean WaitingforReconnect = false;
boolean isFootMotorStopped = true;
boolean isMoving = false;
boolean scaleVolume = false;

// ---------------------------------------------------------------------------------------
//    Used for PS3 Controller Click Management
// ---------------------------------------------------------------------------------------
long previousMillis = millis();
boolean extraClicks = false;

// ---------------------------------------------------------------------------------------
//    Used for Pin 13 Main Loop Blinker
// ---------------------------------------------------------------------------------------
long blinkMillis = millis();
boolean blinkOn = false;

// ---------------------------------------------------------------------------------------
//    Byte variables to reference each motor
// ---------------------------------------------------------------------------------------
byte rightMotor = 1;
byte leftMotor = 2;

// ---------------------------------------------------------------------------------------
//    Throw out 5 out every 6 inputs on drive
// ---------------------------------------------------------------------------------------
int counter = 0;

// ---------------------------------------------------------------------------------------
//    Screen stuff
// ---------------------------------------------------------------------------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------------------------------------------------------------------------------
//    Sonar stuff
// ---------------------------------------------------------------------------------------
#define TRIGGER_PIN_FRONT 12
#define ECHO_PIN_FRONT 11
#define TRIGGER_PIN_LEFT 10
#define ECHO_PIN_LEFT 9
#define MAX_DISTANCE 400
#define SONAR_NUM 2
#define PING_INTERVAL 100

NewPing sonarFront(TRIGGER_PIN_FRONT, ECHO_PIN_FRONT, MAX_DISTANCE);
NewPing sonarLeft(TRIGGER_PIN_LEFT, ECHO_PIN_LEFT, MAX_DISTANCE);

unsigned int pingSpeed = 200;
unsigned long pingTimer1;
unsigned long pingTimer2;

boolean autonomousMode = false;

unsigned long scaleTimer;
unsigned long soundTimer;
int soundDuration;
int songNum;
int volume;


#define NUM_PREV_VALUES 7
double previousValuesFront[NUM_PREV_VALUES] = {MAX_DISTANCE};
double previousValuesLeft[NUM_PREV_VALUES] = {MAX_DISTANCE};

boolean randomSoundState;
boolean scaleVolumeMode;

boolean isMovingForward;
unsigned long movingForwardTimer;


// ROUTINE ONE VARIABLES
int BEEP_SONG_NUM = 1;
int EXPLODE_SONG_NUM = 2;
int NUM_LIGHTS = 1;
int driveDirection = 1;
unsigned long rt1Timer;
unsigned long explosionTimer;
unsigned long SONG_LENGTH[2] = {390, 840};
unsigned long songTimer;
int rt1PingInterval = 4000;
boolean routineOne = false;
boolean playingBeep = false;
boolean redLights = false;

// Light blink variables
int blinkInterval = 300;
unsigned long blinkTimer = 0;
boolean lightsOn = false;

// Light scroll variables
int scrollInterval = 50;
int lightNum = 0;
unsigned long scrollTimer = 0;
boolean backward = false;

// =======================================================================================
//                                 Main Program
// =======================================================================================
// =======================================================================================
//                          Initialize - Setup Function
// =======================================================================================
void setup()
{
    //Debug Serial for use with USB Debugging
    Serial.begin(115200);
    while (!Serial);

    // Sabertooth
    Serial1.begin(9600);
    ST->autobaud();
    ST->setTimeout(400);
    ST->setDeadband(driveDeadBandRange);

    
    if (Usb.Init() == -1)
    {
        Serial.print(F("\r\nOSC did not start"));
        while (1); //halt
    }

    strcpy(output, "");
    
    Serial.print(F("\r\nBluetooth Library Started"));
    
    //Setup for PS3 Controller
    PS3Controller->attachOnInit(onInitPS3Controller); // onInitPS3Controller is called upon a new connection

    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    // Sound setup
    MP3Trigger.setup(&Serial2);
    Serial2.begin(MP3Trigger::serialRate());

    LEDControl.begin();
    
    // Display setup
    display.begin(SSD1306_SWITCHCAPVCC, 0X3C);
    display.display();
    delay(2000);
    display.clearDisplay();
    soundTimer = millis() + 2000;

    // Sonar setup
    pingTimer1 = millis();
    pingTimer2 = millis() + PING_INTERVAL;
}

// =======================================================================================
//           Main Program Loop - This is the recurring check loop for entire sketch
// =======================================================================================
void loop()
{   
    // Make sure the Bluetooth Dongle is working - skip main loop if not
    if ( !readUSB() )
    {
        //We have a fault condition that we want to ensure that we do NOT process any controller data
        printOutput(output);
        return;
    }

    // Check and output PS3 Controller inputs
    checkController();
    
    // Ignore extra inputs from the PS3 Controller
    if (extraClicks){
        if ((previousMillis + 500) < millis())
        {
            extraClicks = false;
        }
    }

    // Control the Main Loop Blinker
    if ((blinkMillis + 1000) < millis()) {
        if (blinkOn) {
            digitalWrite(13, LOW);
            blinkOn = false;
        } else {
            digitalWrite(13, HIGH);
            blinkOn = true;
        }
        blinkMillis = millis();
    }
    
    // Sonar loop
//    if(millis() >= pingTimer1){
//        pingTimer1 += pingSpeed;
//        sonarFront.ping_timer(echoCheckFront);
//    }
    if(millis() >= pingTimer2){
        pingTimer2 += pingSpeed;
        sonarLeft.ping_timer(echoCheckLeft);
    }

//    alternateLights();
//    scrollLights();

    // Routines loop
    if (routineOne) {
      rt1();
    }
    
    MP3Trigger.update();

    if(!routineOne and !autonomousMode){
        backAndForth();
    }

    // Print debug output
    printOutput(output);
}

void echoCheckFront(){
    if(sonarFront.check_timer()){
        if(autonomousMode){
            double ping_distance_front = (sonarFront.ping_result/US_ROUNDTRIP_CM) * 0.39370079;
            double frontAverage = 0;
            for(int i = NUM_PREV_VALUES-1; i >= 1; i--){
                previousValuesFront[i] = previousValuesFront[i-1];
                frontAverage += previousValuesFront[i];
            }
            // Add new value
            previousValuesFront[0] = ping_distance_front;
            frontAverage += ping_distance_front;
            frontAverage = frontAverage/NUM_PREV_VALUES;
            Serial.print("Front: ");
            Serial.println(frontAverage);
            
            if(frontAverage < 5){
                Serial.print("Stopping\r\n");
                blinkLights();
                ST->drive(0);
                ST->turn(0);
            }else{
                Serial.print("Going forward\r\n");
                backAndForth();
                ST->drive(50);
                ST->turn(0);
            }
        }
    }
}

void echoCheckLeft(){
    if(sonarLeft.check_timer()){
        Serial.println("Inside echoCheckLeft");
        if(autonomousMode){
            double ping_distance_left = (sonarLeft.ping_result/US_ROUNDTRIP_CM) * 0.39370079;
            double leftAverage = 0;
            for(int i = NUM_PREV_VALUES-1; i >= 1; i--){
                previousValuesLeft[i] = previousValuesLeft[i-1];
                leftAverage += previousValuesLeft[i];
            }
            // Add new value
            previousValuesLeft[0] = ping_distance_left;
            leftAverage += ping_distance_left;
            leftAverage = leftAverage/NUM_PREV_VALUES;
            Serial.print("Left: ");
            Serial.println(leftAverage);

            if(leftAverage < 5){
                blinkLights();
            }else{
                backAndForth();
            }
        }
    }
}



void rt1() {
  if(rt1PingInterval < 500) {
    strcat(output, "Explode!\r\n");
    playExplosion();
    routineOne = false;
    LEDControl.write();
    return;
  }
  if (millis() > rt1Timer) {
    strcat(output, "change direction!\r\n");
    driveDirection = driveDirection * -1;
    playBeep();
    redLights = redLights ^ true;
    
    rt1PingInterval -= 500;
    rt1Timer = millis() + rt1PingInterval;
  }
  else {
    strcat(output, "drive!\r\n");
    ST->turn(0);
    ST->drive(40 * driveDirection);
    if(!redLights){
        turnOnWhite();
        LEDControl.write();
    }else{
        turnOnRed();
        LEDControl.write();
    }
  }
//  if(driveDirection == 1) {
//    for(float i = 0; i < 12; i++) {
//      if (i % 2 == 0) {
//        LEDControl.setPWM(i, ledMaxBright);
//      }
//      else {
//        LEDControl.setPWM(i, 0);
//      }
//    }
//  }
//  else {
//    for (float i = 0; i < 12; i++) {
//      if (i % 2 != 0) {
//        LEDControl.setPWM(i, ledMaxBright);
//      }
//      else {
//        LEDControl.setPWM(i, 0);
//      }
//    }      
//  }
}

// Lighting functions
// Scrolls through all lights
void scrollLights(){
    if(millis() > scrollTimer){
        LEDControl.setPWM(lightNum, ledMaxBright);
        for(int i = 0; i < 12; i++){
            if(i != lightNum){
                LEDControl.setPWM(i, 0);
            }
        }
        scrollTimer = millis() + scrollInterval;
        lightNum += 1;
        if(lightNum > 11) lightNum = 0;
    }
    LEDControl.write();
}

void backAndForth(){
    if(millis() > scrollTimer){
        LEDControl.setPWM(lightNum, ledMaxBright);
        for(int i = 0; i < 12; i++){
            if(i != lightNum){
                LEDControl.setPWM(i, 0);
            }
        }
        scrollTimer = millis() + scrollInterval;
        if(!backward){
            lightNum += 1;
            if(lightNum > 11){
                lightNum = 10;
                backward = true;
            }
        }else{
            lightNum -= 1;
            if(lightNum < 0){
                lightNum = 1;
                backward = false;
            }
        }
    }
    LEDControl.write();
}

void turnOnAll(){
    LEDControl.setPWM(0, ledMaxBright);
    LEDControl.setPWM(1, ledMaxBright);
    LEDControl.setPWM(2, ledMaxBright);
    LEDControl.setPWM(3, ledMaxBright);
    LEDControl.setPWM(4, ledMaxBright);
    LEDControl.setPWM(5, ledMaxBright);
    LEDControl.setPWM(6, ledMaxBright);
    LEDControl.setPWM(7, ledMaxBright);
    LEDControl.setPWM(8, ledMaxBright);
    LEDControl.setPWM(9, ledMaxBright);
    LEDControl.setPWM(10, ledMaxBright);
    LEDControl.setPWM(11, ledMaxBright);
}

void turnOffAll(){
    LEDControl.setPWM(0, 0);
    LEDControl.setPWM(1, 0);
    LEDControl.setPWM(2, 0);
    LEDControl.setPWM(3, 0);
    LEDControl.setPWM(4, 0);
    LEDControl.setPWM(5, 0);
    LEDControl.setPWM(6, 0);
    LEDControl.setPWM(7, 0);
    LEDControl.setPWM(8, 0);
    LEDControl.setPWM(9, 0);
    LEDControl.setPWM(10, 0);
    LEDControl.setPWM(11, 0);
}

void turnOnRed(){
    LEDControl.setPWM(0, 0);
    LEDControl.setPWM(1, ledMaxBright);
    LEDControl.setPWM(2, 0);
    LEDControl.setPWM(3, ledMaxBright);
    LEDControl.setPWM(4, 0);
    LEDControl.setPWM(5, ledMaxBright);
    LEDControl.setPWM(6, 0);
    LEDControl.setPWM(7, ledMaxBright);
    LEDControl.setPWM(8, 0);
    LEDControl.setPWM(9, ledMaxBright);
    LEDControl.setPWM(10, 0);
    LEDControl.setPWM(11, ledMaxBright);
}

void turnOnWhite(){
    LEDControl.setPWM(0, ledMaxBright);
    LEDControl.setPWM(1, 0);
    LEDControl.setPWM(2, ledMaxBright);
    LEDControl.setPWM(3, 0);
    LEDControl.setPWM(4, ledMaxBright);
    LEDControl.setPWM(5, 0);
    LEDControl.setPWM(6, ledMaxBright);
    LEDControl.setPWM(7, 0);
    LEDControl.setPWM(8, ledMaxBright);
    LEDControl.setPWM(9, 0);
    LEDControl.setPWM(10, ledMaxBright);
    LEDControl.setPWM(11, 0);
}

// Turns lights on/off every 500 ms
void alternateLights(){
    // Turn lights on initially, then set time to turn off
    if(millis() > blinkTimer && !lightsOn){
        turnOnWhite();
        blinkTimer = millis() + blinkInterval;
        lightsOn = true;
    }else if(millis() > blinkTimer && lightsOn){
        turnOnRed();
        blinkTimer = millis() + blinkInterval;
        lightsOn = false;
    }
    LEDControl.write();
}

void blinkLights(){
    // Turn lights on initially, then set time to turn off
    if(millis() > blinkTimer && !lightsOn){
        turnOnAll();
        blinkTimer = millis() + blinkInterval;
        lightsOn = true;
    }else if(millis() > blinkTimer && lightsOn){
        turnOffAll();
        blinkTimer = millis() + blinkInterval;
        lightsOn = false;
    }
    LEDControl.write();
}

void playBeep() {
  MP3Trigger.trigger(BEEP_SONG_NUM);
}

void playExplosion() {
  MP3Trigger.trigger(EXPLODE_SONG_NUM);
}

// =======================================================================================
//          Check Controller Function to show all PS3 Controller inputs are Working
// =======================================================================================
void checkController()
{
       if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(UP) && !extraClicks)
     {              
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: UP Selected.\r\n");
            #endif
            
            previousMillis = millis();
            extraClicks = true;
            
     }
  
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(DOWN) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: DOWN Selected.\r\n");
            #endif                     
            
            previousMillis = millis();
            extraClicks = true;
       
     }

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(LEFT) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: LEFT Selected.\r\n");
            #endif  
            
            previousMillis = millis();
            extraClicks = true;

     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(RIGHT) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
                     
     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(CIRCLE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: CIRCLE Selected.\r\n");
            #endif      
            
            previousMillis = millis();
            extraClicks = true;
           
     }

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(CROSS) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: CROSS Selected. Autonomous mode starting\r\n");
            #endif

            Serial.print("Starting autonomous mode\r\n");

            autonomousMode = true;
            previousMillis = millis();
            extraClicks = true;
              
     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(TRIANGLE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: TRIANGLE Selected.\r\n");
            #endif       
            previousMillis = millis();
            extraClicks = true;
              
     }
     

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(SQUARE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: SQUARE Selected. Autonomous mode ending\r\n");
            #endif

            Serial.print("Stopping autonomous mode\r\n");
            
            autonomousMode = false;
            previousMillis = millis();
            extraClicks = true;
              
     }
     
     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(L1))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: LEFT 1 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(L2))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: LEFT 2 Selected.\r\n");
            #endif       
            routineOne = true;            
            rt1Timer = millis() + rt1PingInterval;
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(R1))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT 1 Selected.\r\n");
            #endif       

            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(R2))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT 2 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(SELECT))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: SELECT Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(START))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: START Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(PS))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: PS Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(LeftHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(LeftHatX)-128) > joystickDeadZoneRange)))
     { 
            
            int currentValueY = PS3Controller->getAnalogHat(LeftHatY) - 128;
            int currentValueX = PS3Controller->getAnalogHat(LeftHatX) - 128;
            
            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);
          
            #ifdef SHADOW_DEBUG
                strcat(output, "LEFT Joystick Y Value: ");
                strcat(output, yString);
                strcat(output, "\n");
                strcat(output, "LEFT Joystick X Value: ");
                strcat(output, xString);
                strcat(output, "\r\n");
            #endif

            previousMillis = millis();
            extraClicks = true;
            
     }

     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(RightHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(RightHatX)-128) > joystickDeadZoneRange)))
     {
            int currentValueX = PS3Controller->getAnalogHat(RightHatX) - 128;
            int currentValueY = PS3Controller->getAnalogHat(RightHatY) - 128;

            if(counter % 6 == 0 && !autonomousMode){
              moveDroid(currentValueX, currentValueY);
            }
            counter += 1;

            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);

            previousMillis = millis();
            extraClicks = true;
     }
}

void moveDroid(int x, int y){
    // X, Y values range from -128 to 128
    // Down is y = 128, up is y = -128
    // Right is x = 128, left is y = -128

    if(y > joystickDeadZoneRange or y < joystickDeadZoneRange * -1){
        y = (y/2) * -1;
        char drivePower[6];
        itoa(y, drivePower, 10);

        Serial.print("Drive power set to :");
        Serial.print(drivePower);
        Serial.print("\r\n");
        
        ST->drive(y);
        if(x < joystickDeadZoneRange and x > joystickDeadZoneRange * -1){
            ST->turn(0);
        }
    }

    if(x > joystickDeadZoneRange or x < joystickDeadZoneRange * -1){
        char turnPower[6];
        itoa(x/3, turnPower, 10);

        Serial.print("Turn power set to :");
        Serial.print(turnPower);
        Serial.print("\r\n");

        ST->turn(x/3);
        if(y < joystickDeadZoneRange and y > joystickDeadZoneRange * -1){
            ST->drive(50);
        }
    }

}

// =======================================================================================
//           PPS3 Controller Device Mgt Functions
// =======================================================================================
// =======================================================================================
//           Initialize the PS3 Controller Trying to Connect
// =======================================================================================
void onInitPS3Controller()
{
    PS3Controller->setLedOn(LED1);
    isPS3ControllerInitialized = true;
    badPS3Data = 0;

    mainControllerConnected = true;
    WaitingforReconnect = true;

    #ifdef SHADOW_DEBUG
       strcat(output, "\r\nWe have the controller connected.\r\n");
       Serial.print("\r\nDongle Address: ");
       String dongle_address = String(Btd.my_bdaddr[5], HEX) + ":" + String(Btd.my_bdaddr[4], HEX) + ":" + String(Btd.my_bdaddr[3], HEX) + ":" + String(Btd.my_bdaddr[2], HEX) + ":" + String(Btd.my_bdaddr[1], HEX) + ":" + String(Btd.my_bdaddr[0], HEX);
       Serial.println(dongle_address);
    #endif
}

// =======================================================================================
//           Determine if we are having connection problems with the PS3 Controller
// =======================================================================================
boolean criticalFaultDetect()
{
    if (PS3Controller->PS3Connected)
    {
        
        currentTime = millis();
        lastMsgTime = PS3Controller->getLastMessageTime();
        msgLagTime = currentTime - lastMsgTime;            
        
        if (WaitingforReconnect)
        {
            
            if (msgLagTime < 200)
            {
             
                WaitingforReconnect = false; 
            
            }
            
            lastMsgTime = currentTime;
            
        } 
        
        if ( currentTime >= lastMsgTime)
        {
              msgLagTime = currentTime - lastMsgTime;
              
        } else
        {

             msgLagTime = 0;
        }
        
        if (msgLagTime > 300 && !isFootMotorStopped)
        {
            #ifdef SHADOW_DEBUG
              strcat(output, "It has been 300ms since we heard from the PS3 Controller\r\n");
              strcat(output, "Shut down motors and watching for a new PS3 message\r\n");
            #endif
            
//          You would stop all motors here
            isFootMotorStopped = true;
        }
        
        if ( msgLagTime > 10000 )
        {
            #ifdef SHADOW_DEBUG
              strcat(output, "It has been 10s since we heard from Controller\r\n");
              strcat(output, "\r\nDisconnecting the controller.\r\n");
            #endif
            
//          You would stop all motors here
            isFootMotorStopped = true;
            
            PS3Controller->disconnect();
            WaitingforReconnect = true;
            return true;
        }

        //Check PS3 Signal Data
        if(!PS3Controller->getStatus(Plugged) && !PS3Controller->getStatus(Unplugged))
        {
            //We don't have good data from the controller.
            //Wait 15ms - try again
            delay(15);
            Usb.Task();   
            lastMsgTime = PS3Controller->getLastMessageTime();
            
            if(!PS3Controller->getStatus(Plugged) && !PS3Controller->getStatus(Unplugged))
            {
                badPS3Data++;
                #ifdef SHADOW_DEBUG
                    strcat(output, "\r\n**Invalid data from PS3 Controller. - Resetting Data**\r\n");
                #endif
                return true;
            }
        }
        else if (badPS3Data > 0)
        {

            badPS3Data = 0;
        }
        
        if ( badPS3Data > 10 )
        {
            #ifdef SHADOW_DEBUG
                strcat(output, "Too much bad data coming from the PS3 Controller\r\n");
                strcat(output, "Disconnecting the controller and stop motors.\r\n");
            #endif
            
//          You would stop all motors here
            isFootMotorStopped = true;
            
            PS3Controller->disconnect();
            WaitingforReconnect = true;
            return true;
        }
    }
    else if (!isFootMotorStopped)
    {
        #ifdef SHADOW_DEBUG      
            strcat(output, "No PS3 controller was found\r\n");
            strcat(output, "Shuting down motors and watching for a new PS3 message\r\n");
        #endif
        
//      You would stop all motors here
        isFootMotorStopped = true;
        
        WaitingforReconnect = true;
        return true;
    }
    
    return false;
}

// =======================================================================================
//           USB Read Function - Supports Main Program Loop
// =======================================================================================
boolean readUSB()
{
  
     Usb.Task();
     
    //The more devices we have connected to the USB or BlueTooth, the more often Usb.Task need to be called to eliminate latency.
    if (PS3Controller->PS3Connected) 
    {
        if (criticalFaultDetect())
        {
            //We have a fault condition that we want to ensure that we do NOT process any controller data!!!
            printOutput(output);
            return false;
        }
        
    } else if (!isFootMotorStopped)
    {
        #ifdef SHADOW_DEBUG      
            strcat(output, "No controller was found\r\n");
            strcat(output, "Shuting down motors, and watching for a new PS3 foot message\r\n");
        #endif
        
//      You would stop all motors here
        isFootMotorStopped = true;
        
        WaitingforReconnect = true;
    }
    
    return true;
}

// =======================================================================================
//          Print Output Function
// =======================================================================================

void printOutput(const char *value)
{
    if ((strcmp(value, "") != 0))
    {
        if (Serial) Serial.println(value);
        strcpy(output, ""); // Reset output string
    }
}
