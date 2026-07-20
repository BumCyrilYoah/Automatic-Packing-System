/*
=========================================================
AUTOMATIC GATE SYSTEM
ESP32 + FreeRTOS VERSION

Evolution from Arduino FSM version

Features:
- FreeRTOS multitasking
- Ultrasonic object detection
- Servo gate control
- IR object counter
- Buzzer feedback
- I2C LCD display

=========================================================
*/


#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>



//=========================================================
// PIN DEFINITIONS
//=========================================================

#define TRIG_PIN       5

#define ECHO_PIN       18

#define IR_PIN         19

#define SERVO_PIN      21

#define BUZZER_PIN     23



// I2C LCD

#define SDA_PIN        4

#define SCL_PIN        15




//=========================================================
// OBJECTS
//=========================================================

LiquidCrystal_I2C lcd(0x27,16,2);


Servo gateServo;




//=========================================================
// SYSTEM SETTINGS
//=========================================================

#define MAX_CAPACITY 10


#define TRIGGER_DISTANCE 30



// Servo angles

#define GATE_OPEN  0

#define GATE_CLOSE 90




// Gate open duration

#define GATE_TIME 20000




//=========================================================
// FREE RTOS OBJECTS
//=========================================================


// Task handles

TaskHandle_t ultrasonicTaskHandle;

TaskHandle_t gateTaskHandle;

TaskHandle_t irTaskHandle;

TaskHandle_t lcdTaskHandle;




// Queue for distance data

QueueHandle_t distanceQueue;



// Queue for IR events

QueueHandle_t irQueue;





//=========================================================
// SYSTEM STATES
//=========================================================

enum GateState
{

    IDLE,

    OPENING,

    OPENED,

    FULL

};


GateState gateState = IDLE;




//=========================================================
// GLOBAL VARIABLES
//=========================================================


volatile int objectCount = 0;


bool parkingFull = false;


bool gateOpened = false;



unsigned long gateOpenTime = 0;



// IR protection

bool objectDetected = false;


unsigned long lastIRTime = 0;


const unsigned long IR_DELAY = 1500;





//=========================================================
// FUNCTION PROTOTYPES
//=========================================================

void ultrasonicTask(void *parameter);

void gateControlTask(void *parameter);

void irCounterTask(void *parameter);

void lcdTask(void *parameter);


long readDistance();

void openGate();

void closeGate();

void beep();





//=========================================================
// SETUP
//=========================================================

void setup()
{

    Serial.begin(115200);



    // Sensor pins

    pinMode(TRIG_PIN,OUTPUT);

    pinMode(ECHO_PIN,INPUT);



    pinMode(IR_PIN,INPUT);



    pinMode(BUZZER_PIN,OUTPUT);

    digitalWrite(BUZZER_PIN,LOW);



    // Servo

    gateServo.attach(SERVO_PIN);


    gateServo.write(GATE_CLOSE);




    // LCD

    Wire.begin(SDA_PIN,SCL_PIN);


    lcd.init();

    lcd.backlight();



    lcd.setCursor(0,0);

    lcd.print("FreeRTOS Gate");

    lcd.setCursor(0,1);

    lcd.print("Starting...");


    delay(2000);

    lcd.clear();





    //=====================================================
    // CREATE QUEUES
    //=====================================================


    distanceQueue = xQueueCreate(
                         5,
                         sizeof(int)
                         );


    irQueue = xQueueCreate(
                    5,
                    sizeof(bool)
                    );






    //=====================================================
    // CREATE TASKS
    //=====================================================


    xTaskCreatePinnedToCore(

        ultrasonicTask,
        "Ultrasonic",
        2048,
        NULL,
        1,
        &ultrasonicTaskHandle,
        0
    );



    xTaskCreatePinnedToCore(

        gateControlTask,
        "Gate Control",
        2048,
        NULL,
        2,
        &gateTaskHandle,
        1
    );



    xTaskCreatePinnedToCore(

        irCounterTask,
        "IR Counter",
        2048,
        NULL,
        2,
        &irTaskHandle,
        1
    );



    xTaskCreatePinnedToCore(

        lcdTask,
        "LCD Display",
        2048,
        NULL,
        1,
        &lcdTaskHandle,
        0
    );

}




//=========================================================
// LOOP
//=========================================================

void loop()
{

    // FreeRTOS handles everything

    vTaskDelay(portMAX_DELAY);

}

//=========================================================
// ULTRASONIC TASK
//
// Reads distance continuously
// Sends distance values to Gate Task
//=========================================================

void ultrasonicTask(void *parameter)
{

    int distance;


    while(1)
    {

        distance = readDistance();



        // Send distance to queue

        xQueueSend(
            distanceQueue,
            &distance,
            portMAX_DELAY
        );



        // Run every 100ms

        vTaskDelay(100 / portTICK_PERIOD_MS);

    }

}






//=========================================================
// GATE CONTROL TASK
//
// Controls the FSM of the gate
//=========================================================

void gateControlTask(void *parameter)
{


    int distance;



    while(1)
    {


        // Receive distance

        if(xQueueReceive(
              distanceQueue,
              &distance,
              portMAX_DELAY))
        {



            switch(gateState)
            {


                //-------------------------------------------------
                // IDLE STATE
                //-------------------------------------------------

                case IDLE:


                    if(parkingFull)
                    {

                        gateState = FULL;

                    }


                    else if(distance <= TRIGGER_DISTANCE)
                    {

                        openGate();


                        gateState = OPENED;


                        gateOpenTime = millis();

                    }


                    break;






                //-------------------------------------------------
                // OPENED STATE
                //-------------------------------------------------

                case OPENED:



                    /*
                    Gate stays open for 20 seconds
                    */


                    if(millis() - gateOpenTime >= GATE_TIME)
                    {

                        closeGate();


                        gateState = IDLE;

                    }



                    break;






                //-------------------------------------------------
                // FULL STATE
                //-------------------------------------------------

                case FULL:


                    closeGate();


                    break;



            }


        }



        vTaskDelay(20 / portTICK_PERIOD_MS);

    }

}








//=========================================================
// IR COUNTER TASK
//
// Counts objects crossing IR sensor
// Produces buzzer event
//=========================================================


void irCounterTask(void *parameter)
{


    bool lastState = HIGH;


    bool currentState;



    while(1)
    {


        currentState = digitalRead(IR_PIN);




        /*
        Detect transition:

        HIGH -> LOW

        Means object crossed sensor
        */



        if(lastState == HIGH &&
           currentState == LOW)
        {


            unsigned long now = millis();



            if(now - lastIRTime > IR_DELAY)
            {


                lastIRTime = now;



                if(objectCount < MAX_CAPACITY)
                {

                    objectCount++;


                    beep();



                    Serial.print("Object Count: ");

                    Serial.println(objectCount);



                    if(objectCount >= MAX_CAPACITY)
                    {

                        parkingFull = true;

                    }

                }


            }


        }




        lastState = currentState;



        vTaskDelay(50 / portTICK_PERIOD_MS);

    }

}







//=========================================================
// READ ULTRASONIC DISTANCE
//=========================================================


long readDistance()
{

    digitalWrite(TRIG_PIN,LOW);

    delayMicroseconds(2);



    digitalWrite(TRIG_PIN,HIGH);

    delayMicroseconds(10);



    digitalWrite(TRIG_PIN,LOW);



    long duration = pulseIn(
                        ECHO_PIN,
                        HIGH,
                        25000
                    );



    if(duration == 0)
    {
        return 999;
    }



    long distance = duration * 0.034 / 2;



    return distance;

}








//=========================================================
// SERVO OPEN
//=========================================================

void openGate()
{


    if(!gateOpened)
    {


        gateServo.write(GATE_OPEN);


        gateOpened = true;


        Serial.println("Gate Open");

    }

}







//=========================================================
// SERVO CLOSE
//=========================================================

void closeGate()
{


    if(gateOpened)
    {


        gateServo.write(GATE_CLOSE);


        gateOpened = false;


        Serial.println("Gate Closed");

    }

}







//=========================================================
// BUZZER
//
// Called ONLY when IR detects object
//=========================================================

void beep()
{

    tone(
        BUZZER_PIN,
        1000,
        150
    );

}
//=========================================================
// LCD TASK
//
// Updates parking information independently
//=========================================================

void lcdTask(void *parameter)
{

    int previousCount = -1;

    bool previousFull = false;


    while(1)
    {


        // Update only when information changes

        if(previousCount != objectCount ||
           previousFull != parkingFull)
        {


            previousCount = objectCount;

            previousFull = parkingFull;



            lcd.clear();



            // First line

            lcd.setCursor(0,0);

            lcd.print("Inside:");

            lcd.print(objectCount);

            lcd.print("/");

            lcd.print(MAX_CAPACITY);





            // Second line

            lcd.setCursor(0,1);



            if(parkingFull)
            {

                lcd.print("PARKING FULL");

            }

            else
            {

                switch(gateState)
                {


                    case IDLE:

                        lcd.print("READY        ");

                        break;



                    case OPENED:

                        lcd.print("GATE OPEN    ");

                        break;



                    case FULL:

                        lcd.print("FULL         ");

                        break;


                }

            }

        }




        // LCD refresh every 500ms

        vTaskDelay(500 / portTICK_PERIOD_MS);

    }

}
