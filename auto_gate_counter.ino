/*
=========================================================
AUTOMATIC GATE SYSTEM
FSM VERSION WITH BUZZER
=========================================================

Board:
Arduino UNO

Components:
- HC-SR04 Ultrasonic Sensor
- IR Obstacle Sensor
- Servo Motor
- I2C LCD 16x2
- Buzzer

Operation:
1. Ultrasonic detects approaching object.
2. Gate opens (0 degree).
3. Gate remains open for 20 seconds.
4. Gate closes automatically (90 degree).
5. IR sensor counts object passage once.
6. Buzzer beeps only when IR detects object crossing.
7. LCD displays parking status.

=========================================================
*/


#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>


//=========================================================
// PIN DEFINITIONS
//=========================================================

#define TRIG_PIN     9
#define ECHO_PIN     8

#define IR_PIN       2

#define SERVO_PIN    6

#define BUZZER_PIN   7



//=========================================================
// LCD
//=========================================================

LiquidCrystal_I2C lcd(0x27,16,2);



//=========================================================
// SERVO OBJECT
//=========================================================

Servo gateServo;



//=========================================================
// SYSTEM SETTINGS
//=========================================================

const int MAX_CAPACITY = 10;


// Distance to activate gate
const int TRIGGER_DISTANCE = 30;


// Servo positions
// Adjust if your mechanical setup requires
const int GATE_OPEN  = 0;
const int GATE_CLOSE = 90;



// Ultrasonic checking period
const unsigned long ULTRASONIC_PERIOD = 80;


// IR debounce
const unsigned long IR_DELAY = 1500;


// Gate open duration
const unsigned long GATE_OPEN_DURATION = 4000;




//=========================================================
// FSM STATES
//=========================================================

enum GateState
{
    IDLE,
    GATE_OPENED,
    PARKING_FULL
};


GateState state = IDLE;




//=========================================================
// VARIABLES
//=========================================================

int objectCount = 0;


bool parkingFull = false;


bool gateOpened = false;


// Prevent multiple counting
bool objectAlreadyCounted = false;



// Timing variables

unsigned long previousUltrasonic = 0;


unsigned long lastIRDetection = 0;


// Stores the moment gate opens
unsigned long gateOpenTime = 0;



//=========================================================
// LCD MEMORY
//=========================================================

int lastCount = -1;

bool lastFullState = false;




//=========================================================
// FUNCTION PROTOTYPES
//=========================================================

long getDistance();

void openGate();

void closeGate();

void checkIR();

void beep();

void updateLCD(bool force);





//=========================================================
// SETUP
//=========================================================

void setup()
{

    pinMode(TRIG_PIN,OUTPUT);

    pinMode(ECHO_PIN,INPUT);


    pinMode(IR_PIN,INPUT);


    pinMode(BUZZER_PIN,OUTPUT);

    digitalWrite(BUZZER_PIN,LOW);



    gateServo.attach(SERVO_PIN);


    // Start with gate closed

    gateServo.write(GATE_CLOSE);


    lcd.init();

    lcd.backlight();


    lcd.clear();


    lcd.setCursor(0,0);

    lcd.print("Automatic Gate");


    lcd.setCursor(0,1);

    lcd.print("Initializing");


    delay(1500);


    lcd.clear();


    updateLCD(true);

}






//=========================================================
// MAIN LOOP
//=========================================================

void loop()
{

    unsigned long now = millis();



    // Check ultrasonic periodically

    if(now - previousUltrasonic >= ULTRASONIC_PERIOD)
    {

        previousUltrasonic = now;



        long distance = getDistance();



        switch(state)
        {



        //=================================================
        // IDLE STATE
        //=================================================

        case IDLE:



            if(parkingFull)
            {

                state = PARKING_FULL;

            }



            else if(distance <= TRIGGER_DISTANCE)
            {

                openGate();


                state = GATE_OPENED;

            }


            break;






        //=================================================
        // GATE OPEN STATE
        //=================================================

        case GATE_OPENED:



            // Check if object crossed IR

            checkIR();



            // Close after 20 seconds

            if(millis() - gateOpenTime >= GATE_OPEN_DURATION)
            {

                closeGate();


                objectAlreadyCounted = false;


                state = IDLE;

            }


            break;






        //=================================================
        // PARKING FULL STATE
        //=================================================

        case PARKING_FULL:



            closeGate();


            break;



        }



        updateLCD(false);

    }

}  

//=========================================================
// ULTRASONIC SENSOR FUNCTION
//=========================================================

long getDistance()
{

    digitalWrite(TRIG_PIN, LOW);

    delayMicroseconds(2);


    digitalWrite(TRIG_PIN, HIGH);

    delayMicroseconds(10);


    digitalWrite(TRIG_PIN, LOW);



    long duration = pulseIn(ECHO_PIN, HIGH, 25000);



    // No object detected

    if(duration == 0)
    {
        return 999;
    }



    long distance = duration * 0.0343 / 2;


    return distance;

}






//=========================================================
// OPEN GATE FUNCTION
//=========================================================

void openGate()
{

    if(gateOpened)
        return;



    // Rotate servo to opening position

    gateServo.write(GATE_OPEN);



    gateOpened = true;



    // Start 20 second timer

    gateOpenTime = millis();

}






//=========================================================
// CLOSE GATE FUNCTION
//=========================================================

void closeGate()
{

    if(!gateOpened)
        return;



    // Rotate servo to closing position

    gateServo.write(GATE_CLOSE);



    gateOpened = false;

}








//=========================================================
// IR SENSOR FUNCTION
//
// Counts only one object
// Beeps only when object passes IR
//=========================================================


void checkIR()
{

    unsigned long now = millis();



    /*
       Most IR obstacle sensors:
       
       LOW  = object detected
       HIGH = clear

       If your sensor works opposite,
       replace LOW with HIGH.
    */



    if(digitalRead(IR_PIN) == LOW)
    {


        // Already counted this object

        if(objectAlreadyCounted)
        {
            return;
        }




        // Debounce protection

        if(now - lastIRDetection < IR_DELAY)
        {
            return;
        }




        lastIRDetection = now;



        // Lock this object

        objectAlreadyCounted = true;




        //=============================================
        // OBJECT HAS PASSED IR SENSOR
        // NOW WE BEEP
        //=============================================


        beep();




        // Increase parking count

        if(objectCount < MAX_CAPACITY)
        {

            objectCount++;



            if(objectCount >= MAX_CAPACITY)
            {

                parkingFull = true;


                state = PARKING_FULL;


                closeGate();

            }

        }

    }




    else
    {

        /*
          IR beam is free again.

          This allows the next object
          to be counted.
        */


        objectAlreadyCounted = false;

    }

}








//=========================================================
// BUZZER FUNCTION
//
// Beeps ONLY when IR detects passage
//=========================================================

void beep()
{

    tone(BUZZER_PIN,1000,150);

}

//=========================================================
// LCD UPDATE FUNCTION
//=========================================================

void updateLCD(bool force)
{

    // Avoid unnecessary LCD refreshing

    if(!force)
    {

        if(lastCount == objectCount &&
           lastFullState == parkingFull)
        {
            return;
        }

    }



    // Save current values

    lastCount = objectCount;

    lastFullState = parkingFull;



    lcd.clear();



    //=====================================================
    // FIRST LINE
    //=====================================================


    lcd.setCursor(0,0);


    lcd.print("Inside:");

    lcd.print(objectCount);

    lcd.print("/");

    lcd.print(MAX_CAPACITY);





    //=====================================================
    // SECOND LINE
    //=====================================================


    lcd.setCursor(0,1);



    switch(state)
    {


        case IDLE:

            lcd.print("READY          ");

            break;



        case GATE_OPENED:

            lcd.print("GATE OPEN      ");

            break;



        case PARKING_FULL:

            lcd.print("PARKING FULL   ");

            break;


    }

}



//=========================================================
// END OF PROGRAM
//=========================================================
