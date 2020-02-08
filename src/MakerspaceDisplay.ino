/* DESCRIPTION
  ====================
/* started on 11DEC2017 - uploaded on 06.01.2018 by Dieter

  Commands to Raspi --->
  'OK'  - message received

  Commands from Raspi <--- <0,command>
  'GDT' - Get Displayed Text
  'DAN' - Display Animation on
  'DAF' - Display Animation ofF
  'INI' - Initialize all new
  'L1N' - Light 1 oN
  'L1F' - Light 1 ofF
  'L2N' - Light 2 oN
  'L2F' - Light 2 ofF

    <--- <1,Text,0,0,0,0> show text in (line - 1) until maxLines

  last change: 07.02.2020 by Michael Muehl
  changed: reduse bus communication nearly zero form display,
           and POR; and Version
*/
#define Version "2.0.x" // (Test =1.0.0 ==> 2.0.0)

#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// PIN Assignments
// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define CLK_PIN   13  // blau    -> Bulgin 3
#define DATA_PIN  11  // grün    -> Bulgin 1
#define CS_PIN    10  // violett -> Bulgin 2
#define LED_SW1   A2  // A2 auf 2 LED Display SW1
#define LED_SW2   A3  // A3 auf 3 [not used]  SW2

// Hardware SPI connection
#define MAX_DEVICES 16
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// DEFINES
#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))
#define SPEED_TIME  25
#define PAUSE_TIME  1000

// VARIABLES
const byte numChars = 50;
const byte maxLines = 6;

String inStr = "";      // a string to hold incoming data

uint8_t L2D = 6;        // Lines to Display
char receivedChars[numChars];
char input[numChars];
char tempChars[numChars]; // temporary array for use when parsing
// variables to hold the parsed data
char message[numChars] = {0};
uint16_t line = 0;
uint16_t pause = 0;
uint16_t dspeed = 0;
uint16_t curText;
uint16_t inFX;
uint16_t outFX;
boolean newData = false;
boolean animation_enable = true;

// Global data
char lmessage[maxLines][numChars]=
{
  "MAKERSPACE",
  "Wiesbaden e.V.",
  "> > > > > >"
};

typedef struct
{
  uint16_t speed;  // speed multiplier of library default
  uint16_t pause;  // pause multiplier for library default
  uint16_t inFX;   // pause multiplier for library default
  uint16_t outFX;  // pause multiplier for library default
} sCatalog;

sCatalog catalog[maxLines]
{
  {50, 1000, 6, 6 },
  {50, 1000, 3, 3 },
  {25, 0, 1, 1 },
  {0, 0, 0, 0 },
  {0, 0, 0, 0 },
  {0, 0, 0, 0 }
};

textEffect_t effect[] =
{
  PA_PRINT,
  PA_SCROLL_RIGHT,
  PA_SCROLL_LEFT,
  PA_SCROLL_UP_RIGHT,
  PA_SCROLL_UP_LEFT,
  PA_SCROLL_DOWN_RIGHT,
  PA_SCROLL_DOWN_LEFT,
  PA_SCROLL_UP,
  PA_SCROLL_DOWN,
  PA_GROW_UP,
  PA_GROW_DOWN,
  PA_OPENING,
  PA_CLOSING,
  PA_OPENING_CURSOR,
  PA_WIPE,
  PA_WIPE_CURSOR,
  PA_MESH,
  PA_RANDOM
};

// ======>  SET UP AREA <=====
void setup() {
  //init Serial port
  Serial.begin(115200);
  inStr.reserve(numChars); // reserve for instr serial input

  // initialize:
  P.begin();
  P.setInvert(false);

  // IO MODES
  pinMode(LED_SW1,OUTPUT);
  pinMode(LED_SW2,OUTPUT);

  // Set default values
  digitalWrite(LED_SW1,LOW);
  digitalWrite(LED_SW2,LOW);
  Serial.println("ms_displ;POR;V" + String(Version));
}
// Setup End ------------------------------------

// FUNCTIONS ------------------------------------
void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static int ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();
    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) ndx = numChars - 1;
      } else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    } else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    parseData();
    processParsedData();
    newData = false;
  }
}

void processParsedData() {
  inStr = message;
  if (line == 0) { // Normal commands <0,command,0,0,0,0>
    if (inStr.substring(0,3) == "GDT") { // Get Displayed Text
      getTextLines();
    }

    if (inStr.substring(0,3) == "DAN") { // Display oN
      animation_enable = true;
    }

    if (inStr.substring(0,3) == "DAF") { // Display ofF
      animation_enable = false;
      P.displayClear();
    }

    if (inStr.substring(0,3) == "INI") { // Initialize all new
      asm volatile ("jmp 0");
    }

    if (inStr.substring(0,3) == "L1N") { // Turn LED Light 1 oN
      digitalWrite(LED_SW1,HIGH);
    }

    if (inStr.substring(0,3) == "L1F") { // Turn LED Light 1 ofF
      digitalWrite(LED_SW1,LOW);
    }

    if (inStr.substring(0,3) == "L2N") { // Turn LED Light 2 oN
      digitalWrite(LED_SW2,HIGH);
    }

    if (inStr.substring(0,3) == "L2F") { // Turn LED Light 2 ofF
      digitalWrite(LED_SW2,LOW);
    }

  } else {                       // Display commands
    uint8_t p = line-1;
    inStr.toCharArray(lmessage[p], numChars);
    catalog[p].inFX = inFX;
    catalog[p].outFX = outFX;
    catalog[p].speed = dspeed;
    catalog[p].pause = pause;
  }
}

void parseData() {      // split the data into its parts
  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(tempChars,",");  // get the first part - the string
  line = atoi(strtokIndx);

  strtokIndx = strtok(NULL, ",");
  strcpy(message, strtokIndx);

  strtokIndx = strtok(NULL, ",");      // this continues where the previous call left off
  dspeed = atoi(strtokIndx);           // convert this part to an integer

  strtokIndx = strtok(NULL, ",");
  pause = atoi(strtokIndx);     // convert this part to an integer

  strtokIndx = strtok(NULL, ",");      // this continues where the previous call left off
  inFX = atoi(strtokIndx);    // convert this part to an integer

  strtokIndx = strtok(NULL, ",");      // this continues where the previous call left off
  outFX = atoi(strtokIndx);    // convert this part to an integer
}

void getTextLines() {
  for (uint8_t i=0; i<L2D; i++) {
    Serial.print("L"); Serial.print(i+1); Serial.print(";");
    Serial.print(lmessage[i]); Serial.print(";");
    Serial.print(catalog[i].speed); Serial.print(";");
    Serial.print(catalog[i].pause); Serial.print(";");
    Serial.print(catalog[i].inFX); Serial.print(";");
    Serial.println(catalog[i].outFX);
  }
}

void loop() {
  recvWithStartEndMarkers(); // look for any commands
  if (animation_enable == true) {
    P.displayReset();
    P.displayClear();
    textPosition_t just;
    just = PA_CENTER;
    for (uint8_t i=0; i<L2D; i++) {
      P.displayText(lmessage[i], just, catalog[i].speed, catalog[i].pause, effect[catalog[i].inFX], effect[catalog[i].outFX]);
      while (!P.displayAnimate())
              ; // animates and returns true when an animation is completed
      if(animation_enable == false) return;
      delay(catalog[i].pause);
      recvWithStartEndMarkers(); // look for any commands
    }
  }
}
