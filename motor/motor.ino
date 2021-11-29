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

int oldTurnNum = 0;
int oldFootDriveSpeed = 0;
byte driveDeadBandRange = 10;
#define SABERTOOTH_ADDR 128
Sabertooth *ST=new Sabertooth(SABERTOOTH_ADDR, Serial);

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
//    Throw out 5 out every 6 inputs
// ---------------------------------------------------------------------------------------
int counter = 0;

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

NewPing sonar1(TRIGGER_PIN_FRONT, ECHO_PIN_FRONT, MAX_DISTANCE);
NewPing sonar2(TRIGGER_PIN_LEFT, ECHO_PIN_LEFT, MAX_DISTANCE);

unsigned int pingSpeed = 200;
unsigned long pingTimer1;
unsigned long pingTimer2;

boolean autonomousMode = false;
boolean turning = false;
double turnThreshold;

#define NUM_PREV_VALUES 7
double previousValuesFront[NUM_PREV_VALUES] = {MAX_DISTANCE};
double previousValuesLeft[NUM_PREV_VALUES] = {MAX_DISTANCE};

// =======================================================================================
//                                 Main Program
// =======================================================================================
// =======================================================================================
//                          Initialize - Setup Function
// =======================================================================================
void setup()
{
    //Debug Serial for use with USB Debugging
    Serial.begin(9600);
    ST->autobaud();
    ST->setTimeout(200);
    ST->setDeadband(driveDeadBandRange);
    while (!Serial);
    
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
    if (millis() >= pingTimer2) {
      pingTimer2 += pingSpeed;
      sonar2.ping_timer(echoCheck2);
    }
    if (millis() >= pingTimer1) {
      pingTimer1 += pingSpeed;
      sonar1.ping_timer(echoCheck1);
    }
    
    printOutput(output);
}

void processFrontSensor(bool moveForward){
    if(moveForward){
        // Simulate controller movement
        turning = false;
        Serial.print("Moving droid forward");
        moveDroid(0, -90);
    }else{ // Begin right turn if forward movement has stopped
        turning = true;
        Serial.print("Turning droid");

        ST->motor(rightMotor, 0);
        ST->motor(leftMotor, 50);
    }
}

void echoCheck1(){
    double ping_distance_front;
    if(sonar1.check_timer()){
//        Serial.print("Ping1: ");
        ping_distance_front = (sonar1.ping_result/US_ROUNDTRIP_CM) * 0.39370079;
//        Serial.print(ping_distance_front);
//        Serial.println("in");
        if(autonomousMode){
            // Push back all previous values
            double frontAverage = 0;
            for(int i = NUM_PREV_VALUES-1; i >= 1; i--){
                previousValuesFront[i] = previousValuesFront[i-1];
                Serial.print(previousValuesFront[i]);
                Serial.print(" ");
                frontAverage += previousValuesFront[i];
            }
            // Add new value
            previousValuesFront[0] = ping_distance_front;
            frontAverage += ping_distance_front;
            frontAverage = frontAverage/NUM_PREV_VALUES;
            Serial.print("Average: ");
            Serial.print(frontAverage);
            Serial.println("in");
            // Set the threshold based on the droid state
            if(turning){
                turnThreshold = 36;
            }else{
                turnThreshold = 15;
            }
            // Process new average
            if(frontAverage < turnThreshold){
                processFrontSensor(false);
            }else{
                processFrontSensor(true);
            }
        }
    }
}

void echoCheck2(){
    double ping_distance_left;
    if(sonar2.check_timer()){
//        Serial.print("Ping2: ");
        ping_distance_left = (sonar2.ping_result/US_ROUNDTRIP_CM) * 0.39370079;
//        Serial.print(ping_distance_left);
//        Serial.println("in");
//        if(autonomousMode){
//            // Push back all previous values
//            double leftAverage = 0;
//            for(int i = NUM_PREV_VALUES-1; i >= 1; i--){
//                previousValuesLeft[i] = previousValuesLeft[i-1];
//                Serial.print(previousValuesLeft[i]);
//                Serial.print(" ");
//                leftAverage += previousValuesLeft[i];
//            }
//            // Add new value
//            previousValuesLeft[0] = ping_distance_left;
//            leftAverage += ping_distance_left;
//            leftAverage = leftAverage/NUM_PREV_VALUES;
//            Serial.print("Average: ");
//            Serial.print(leftAverage);
//            Serial.println("in");
//            // If leftAverage is greater than 4 inches we want to get closer to the wall
//            if(leftAverage < 4){
//                processFrontSensor(false);
//            }else{
//                processFrontSensor(true);
//            }
//        }
    }
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
            //ST->turn(50);
            //ST->drive(5);
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
            
//            #ifdef SHADOW_DEBUG
//                strcat(output, "RIGHT Joystick Y Value: ");
//                strcat(output, yString);
//                strcat(output, "\n");
//                strcat(output, "RIGHT Joystick X Value: ");
//                strcat(output, xString);
//                strcat(output, "\r\n");
//            #endif

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
    
        #ifdef SHADOW_DEBUG
            strcat(output, "setting drive power to: ");
            strcat(output, drivePower);
            strcat(output, "\r\n");
        #endif
        ST->drive(y);
        if(x < joystickDeadZoneRange and x > joystickDeadZoneRange * -1){
            ST->turn(0);
        }
    }

    if(x > joystickDeadZoneRange or x < joystickDeadZoneRange * -1){
        char turnPower[6];
        itoa(x/3, turnPower, 10);
    
        #ifdef SHADOW_DEBUG
            strcat(output, "setting turn power to: ");
            strcat(output, turnPower);
            strcat(output, "\r\n");
        #endif

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
