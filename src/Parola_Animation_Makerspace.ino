#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define MAX_DEVICES 16
#define CLK_PIN   13  // blau -> Bulgin 3
#define DATA_PIN  11  // grün -> Bulgin 1
#define CS_PIN    10  // violett -> Bulgin 2
#define LED_SW    A2  // A2 auf 2 SSR1-1
#define L2_SW     A3  // A3 auf 3 SSR2-1 zum schalten

// Hardware SPI connection
MD_Parola P = MD_Parola(CS_PIN, MAX_DEVICES);
// Arbitrary output pins
// MD_Parola P = MD_Parola(DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))
#define SPEED_TIME  25
#define PAUSE_TIME  1000
const byte numChars = 50;
const byte maxLines = 6;

String inStr = "";      // a string to hold incoming data

uint8_t L2D = 6;     // Lines to Display
char receivedChars[numChars];
char input[numChars];
char tempChars[numChars];        // temporary array for use when parsing
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
  uint16_t speed;   // speed multiplier of library default
  uint16_t pause;   // pause multiplier for library default
  uint16_t inFX;   // pause multiplier for library default
  uint16_t outFX;   // pause multiplier for library default
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

void setup()  {
  Serial.begin(115200);

  P.begin();
  P.setInvert(false);
  inStr.reserve(numChars); // reserve for instr serial input
  pinMode(LED_SW,OUTPUT);
  digitalWrite(LED_SW,LOW);

/*
   for (uint8_t i=0; i<L2D; i++)
   {
   catalog[i].speed *= P.getSpeed();
   catalog[i].pause *= 500;
   }
*/
}

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
        if (ndx >= numChars) {
                ndx = numChars - 1;
        }
      }
      else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
  if (newData == true) {
          strcpy(tempChars, receivedChars);
          parseData();
          // showParsedData();
          processParsedData();
          newData = false;
  }
}

void processParsedData() {
  inStr = message;
  if(line == 0) // Normal commands <0,command,0,0,0,0>
  {
    if (inStr.substring(0,4) == "STOP") { //
            animation_enable = false;
            P.displayClear();
            Serial.println("CMD;STOP;COMPLETED");
    }

    if (inStr.substring(0,5) == "START") { //
            animation_enable = true;
            Serial.println("CMD;START;COMPLETED");
    }

    if (inStr.substring(0,5) == "INIT") { // Clear all Text Buffer
            asm volatile ("jmp 0");
            Serial.println("CMD;INIT;COMPLETED");
    }

    if (inStr.substring(0,3) == "LON") { // Turn LED Lighting on
            digitalWrite(LED_SW,HIGH);
            Serial.println("CMD;LON;COMPLETED");
    }

    if (inStr.substring(0,4) == "LOFF") { // Turn LED Lighting off
            digitalWrite(LED_SW,LOW);
            Serial.println("CMD;LOFF;COMPLETED");
    }

    if (inStr.substring(0,4) == "L2ON") { // Turn LED Lighting on
            digitalWrite(L2_SW,HIGH);
            Serial.println("CMD;L2ON;COMPLETED");
    }

    if (inStr.substring(0,5) == "L2OFF") { // Turn LED Lighting off
            digitalWrite(L2_SW,LOW);
            Serial.println("CMD;L2OFF;COMPLETED");
    }

  }
  else                        // Display commands
  {
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

void showParsedData() {
  Serial.println(tempChars);
  Serial.print("Line ");
  Serial.println(line);
  Serial.print("Effect In ");
  Serial.println(inFX);
  Serial.print("Effect Out ");
  Serial.println(outFX);
  Serial.print("Message ");
  Serial.println(message);
  Serial.print("speed ");
  Serial.println(dspeed);
  Serial.print("Pause ");
  Serial.println(pause);
}

void loop() {
  recvWithStartEndMarkers(); // look for any commands
  if(animation_enable == true) {
    P.displayReset();
    P.displayClear();
    textPosition_t just;
    just = PA_CENTER;
    for (uint8_t i=0; i<L2D; i++)
    {
      Serial.print("L"); Serial.print(i+1); Serial.print(";");
      Serial.print(lmessage[i]); Serial.print(";");
      Serial.print(catalog[i].speed); Serial.print(";");
      Serial.print(catalog[i].pause); Serial.print(";");
      Serial.print(catalog[i].inFX); Serial.print(";");
      Serial.println(catalog[i].outFX);

      P.displayText(lmessage[i], just, catalog[i].speed, catalog[i].pause, effect[catalog[i].inFX], effect[catalog[i].outFX]);
      while (!P.displayAnimate())
              ; // animates and returns true when an animation is completed
      if(animation_enable == false) return;
      delay(catalog[i].pause);
      recvWithStartEndMarkers(); // look for any commands
    }
  }
}
