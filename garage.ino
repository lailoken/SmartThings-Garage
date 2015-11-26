/**
 *  Arduino Garage
 *
 *  Author: Marius Piedallu van Wyk
 *  Date: 2014-07-27
 */

//*****************************************************************************
#include <SoftwareSerial.h>
#include <SmartThings.h>

#define DO_BACK_DOOR

//*****************************************************************************
// Pin Definitions
//*****************************************************************************
#define PIN_LED              13
#define PIN_THING_RX          3
#define PIN_THING_TX          2
#define PIN_BACK_DOOR_CONTACT 4     // input
#define PIN_RIGHT             6     // out
#define PIN_RIGHT_CONTACT     7     // input
#define PIN_LEFT_CONTACT      8     // input
#define PIN_LEFT              9     // out

#define OPEN                  0     // HIGH
#define CLOSED                1     // LOW
#define UNKNOWN               2     // --- reset / force update

#define PUSH_DELAY            1200  // milliseconds to keep the button "pushed"

//*****************************************************************************
// Global Variables
//*****************************************************************************
SmartThingsCallout_t messageCallout;    // call out function forward decalaration
SmartThings smartthing(PIN_THING_RX, PIN_THING_TX, messageCallout);  // constructor

int leftClosed  = UNKNOWN;
int rightClosed = UNKNOWN;
int backClosed  = UNKNOWN;

int anyOpen     = UNKNOWN;

bool isDebugEnabled=false;    // enable or disable debug in this example
int stateLED;           // state to track last set value of LED
int stateNetwork;       // state of the network

//*****************************************************************************
// Local Functions
//*****************************************************************************
void pushLeft()
{
  smartthing.shieldSetLED(0, 0, 2); // blue
  digitalWrite(PIN_LEFT,LOW);
  delay(PUSH_DELAY);
  digitalWrite(PIN_LEFT,HIGH);
  smartthing.shieldSetLED(0, 0, 0); // off
}

//*****************************************************************************
void pushRight()
{
  smartthing.shieldSetLED(0, 2, 0); // green
  digitalWrite(PIN_RIGHT,LOW);
  delay(PUSH_DELAY);
  digitalWrite(PIN_RIGHT,HIGH);
  smartthing.shieldSetLED(0, 0, 0); // off
}

int isClosed(int pin)
{
  // LOW  -> closed
  // HIGH -> open
  return (digitalRead(pin) == LOW)?CLOSED:OPEN;
}

void updateDoorState()
{
  int newState;
  char* msg = NULL;

  newState = isClosed(PIN_LEFT_CONTACT);
  if (leftClosed != newState)
  {
    leftClosed = newState;
    if(leftClosed == CLOSED) msg = "leftDoor closed";
    else                     msg = "leftDoor open";

    smartthing.send(msg);
    if(isDebugEnabled) Serial.println(msg);
    return; // only one message per loop
  }

  newState = isClosed(PIN_RIGHT_CONTACT);
  if (rightClosed != newState)
  {
    rightClosed = newState;
    if(rightClosed == CLOSED) msg = "rightDoor closed";
    else                      msg = "rightDoor open";

    smartthing.send(msg);
    if(isDebugEnabled) Serial.println(msg);
    return; // only one message per loop
  }

#ifdef DO_BACK_DOOR
  newState = isClosed(PIN_BACK_DOOR_CONTACT);
  if (backClosed != newState)
  {
    backClosed = newState;
    if(backClosed == CLOSED) msg = "backDoor closed";
    else                     msg = "backDoor open";

    smartthing.send(msg);
    if(isDebugEnabled) Serial.println(msg);
    return; // only one message per loop
  }
#endif

  newState = CLOSED;

#ifdef DO_BACK_DOOR
  if(rightClosed == OPEN || leftClosed == OPEN || backClosed == OPEN) newState = OPEN;
#else
  if(rightClosed == OPEN || leftClosed == OPEN) newState = OPEN;
#endif

  if(anyOpen != newState)
  {
    anyOpen = newState;
    if(anyOpen == CLOSED) msg = "anyDoor closed";
    else                  msg = "anyDoor open";

    smartthing.send(msg);
    if(isDebugEnabled) Serial.println(msg);
    return; // only one message per loop
  }
}

//*****************************************************************************
void setNetworkStateLED()
{
  SmartThingsNetworkState_t tempState = smartthing.shieldGetLastNetworkState();
  if (tempState != stateNetwork)
  {
    switch (tempState)
    {
      case STATE_NO_NETWORK:
        if (isDebugEnabled) Serial.println("NO_NETWORK");
        smartthing.shieldSetLED(2, 2, 0); // red
        break;
      case STATE_JOINING:
        if (isDebugEnabled) Serial.println("JOINING");
        smartthing.shieldSetLED(2, 2, 0); // yellow
        break;
      case STATE_JOINED:
        if (isDebugEnabled) Serial.println("JOINED");
        smartthing.shieldSetLED(0, 0, 0); // off

        // force report of current door state
        leftClosed  = UNKNOWN;
        rightClosed = UNKNOWN;
        backClosed  = UNKNOWN;
        anyOpen     = UNKNOWN;

        break;
      case STATE_JOINED_NOPARENT:
        if (isDebugEnabled) Serial.println("JOINED_NOPARENT");
        smartthing.shieldSetLED(2, 0, 2); // purple
        break;
      case STATE_LEAVING:
        if (isDebugEnabled) Serial.println("LEAVING");
        smartthing.shieldSetLED(2, 2, 0); // yellow
        break;
      default:
      case STATE_UNKNOWN:
        if (isDebugEnabled) Serial.println("UNKNOWN");
        smartthing.shieldSetLED(2, 0, 0); // red
        break;
    }
    stateNetwork = tempState;
  }
}

//*****************************************************************************
// API Functions    | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
//                  V V V V V V V V V V V V V V V V V V V V V V V V V V V V V V
//*****************************************************************************
void setup()
{
  // setup default state of global variables
  isDebugEnabled = true;
  stateLED = 0;                 // matches state of hardware pin set below
  stateNetwork = STATE_JOINED;  // set to joined to keep state off if off

  // setup hardware pins
  pinMode(PIN_LED, OUTPUT);     // define PIN_LED as an output
  pinMode(PIN_RIGHT, OUTPUT);
  pinMode(PIN_LEFT, OUTPUT);
  digitalWrite(PIN_RIGHT, HIGH);
  digitalWrite(PIN_LEFT, HIGH);
  digitalWrite(PIN_LED, LOW);   // set value to LOW (off) to match stateLED=0

  pinMode(PIN_LEFT_CONTACT, INPUT_PULLUP);
  pinMode(PIN_RIGHT_CONTACT, INPUT_PULLUP);
  pinMode(PIN_BACK_DOOR_CONTACT, INPUT_PULLUP);

  if(isDebugEnabled)
  { // setup debug serial port
    Serial.begin(9600);         // setup serial with a baud rate of 9600
    Serial.println("setup..");  // print out 'setup..' on start
  }
}

//*****************************************************************************
void loop()
{
  // run smartthing logic
  smartthing.run();

  // Check network connections (and send initial states on Join)
  setNetworkStateLED();

  if(stateNetwork == STATE_JOINED)
  {
    // Check the open/closed state of the doors
    updateDoorState();
  }
}

//*****************************************************************************
void messageCallout(String message)
{
  smartthing.shieldSetLED(2, 2, 2); // white

  // if debug is enabled print out the received message
  if(isDebugEnabled)
  {
    Serial.print("Received message: '");
    Serial.print(message);
    Serial.println("' ");
  }

  if(message.equals("pushLeft"))
  {
    pushLeft();
  }
  else if(message.equals("pushRight"))
  {
    pushRight();
  }

  smartthing.shieldSetLED(0, 0, 0); // off
}
