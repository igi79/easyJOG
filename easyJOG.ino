#include "U8glib.h"
#include "pushButton.h"
#include <EEPROM.h> 
#include "EncoderTool.h"
using namespace EncoderTool;

#define STEPMM 2133.333
#define DISTANCE_ADDR    0 //4 - float
#define FEEDMOVE_ADDR    4  //4 - int
#define FEEDRAPID_ADDR    6 //4 - int
#define AUTOMAT_ADDR    8 //2 - int
#define INCREMENT_ADDR    10 //4 - float
#define JOGINCREMENT_ADDR    14 //4 - float

pushButton Aswitch(6); //stop/zero
pushButton Bswitch(7); //+/-
pushButton Cswitch(8); //play
pushButton Dswitch(9); //mode
pushButton Eswitch(4); //setup
pushButton jogswitch(5);

U8GLIB_ST7920_128X64_1X u8g(10);
Encoder encoder; 

unsigned long lastBlink;
boolean blinkOn;
int8_t clicks = 0;
int absclicks = 0;
int encclicks;
String stat = "-"; 
float wpos[3] = {0,0,0};
float mpos[3] = {0,0,0};
float dpos[3] = {0,0,0};
float wmoffset[3] = {0,0,0};
float dest[3] = {0,0,0};
//char axes[3] = {'X','Y','Z'};
char A = 'X';
uint8_t a = 0;
unsigned int feedJog = 100;
unsigned int feedMove = 50;
unsigned int feedMovePot = 50;
unsigned int feedRapid = 100;
unsigned int feedCurrent = 100;
String ov;

#define AMODES 5
const char *autoMode[AMODES] = {"Manual", "Switch", "Auto < >>", "Auto < >", "Remote"};

uint8_t automat = 2;
uint8_t runbyremote = 0;
/*
 * 0 - jog
 * 1 - jogIncrement
 * 2 - feedJog
 * 3 - feedMove
 * 4 - feedRapid
 * 5 - distance
 * 6 - increment
 * 7 - automat
 * 8 - run / hold - feed override
 * 9 - blocked / remote
 */
unsigned int wheelMode = 0; 
/*
 * 0 - run rapid
 * 1 - run move
 */
char workMode = '1';
char nextWorkMode;

int sw;

float jogIncrement = 1.0;
float increment = 1.0;
float distance = 10.0;
float nextDistance;
boolean nextSet = false;

uint8_t progressA = 0;
uint8_t progressB = 0;

void Get_Settings(){
  
  distance = readFloatParam(DISTANCE_ADDR);
  if (distance > 9999 || distance < -9999 || isnan(distance)){
    writeFloatParam(10,DISTANCE_ADDR);
    distance = 10.0;
  }

  feedMove = readParam(FEEDMOVE_ADDR);
  if (feedMove > 9999 || feedMove < 0 || isnan(feedMove)){
    writeParam(10,FEEDMOVE_ADDR);
    feedMove = 50;
  }
  feedCurrent = feedMove;

  feedRapid = readParam(FEEDRAPID_ADDR);
  if (feedRapid > 9999 || feedRapid < 0 || isnan(feedRapid)){
    writeParam(100,FEEDRAPID_ADDR);
    feedRapid = 100;
  }  

  automat = readParam(AUTOMAT_ADDR);
  if (automat > 4 || automat < 0 || isnan(automat)){
    writeParam(0,AUTOMAT_ADDR);
    automat = 0;
  }  

  increment = readFloatParam(INCREMENT_ADDR);
  if (increment > 1000 || increment < 0.01 || isnan(increment)){
    writeFloatParam(1,INCREMENT_ADDR);
    increment = 1;
  }  

  jogIncrement = readFloatParam(JOGINCREMENT_ADDR);
  if (jogIncrement > 1000 || jogIncrement < 0.01 || isnan(jogIncrement)){
    writeFloatParam(1,JOGINCREMENT_ADDR);
    jogIncrement = 1;
  }  

}

void draw(void) {
  
  u8g.setDefaultForegroundColor();
  
  //debug
  // u8g.setFont(u8g_font_profont11r);
  // u8g.setPrintPos(0, 10);
  // u8g.print(wpos[0]);
  // u8g.setPrintPos(64, 10);
  // u8g.print(wheelMode);
  // u8g.setPrintPos(64, 20);
  // u8g.print(feedCurrent);

  //position
  u8g.setFont(u8g_font_helvR24n);
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();
  u8g.setPrintPos(0,0);
  u8g.print(wpos[0]);

  u8g.setFont(u8g_font_profont11r);
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();  

  u8g.drawBox(progressA,26,progressB,2);
  
  //designated position
  u8g.setPrintPos(0, 29);
  u8g.print("G:");
  u8g.print(dpos[0]);
  //status
  u8g.setPrintPos(64, 29);
  u8g.print(stat);

  u8g.setPrintPos(0, 38);
  if(!(blinkOn && wheelMode == 5))u8g.print("X");
  u8g.setPrintPos(6, 38);
  u8g.print(distance);
  u8g.setPrintPos(64, 38);
  if(!(blinkOn && (wheelMode == 3 || wheelMode == 4)))u8g.print("F");
  u8g.setPrintPos(70, 38);
  if(automat == 4 && !(wheelMode == 3 || wheelMode == 4)){
    u8g.print(feedMovePot);
  } else u8g.print(workMode == '0' ? feedRapid : feedMove);
  //override
  if(workMode == '1'){
    u8g.print("(");
    u8g.print(ov);
    u8g.print(")"); 
  }
  //mode
  u8g.setPrintPos(0, 47);
  u8g.print(workMode == '0' ? "Rapid" : "Move");  
  //u8g.print(abs(dpos[0] - wpos[0]));
  //automatic mode
  u8g.setPrintPos(64, 47);
  if(!(blinkOn && wheelMode == 7))u8g.print(autoMode[automat]);

  //bottom line
  u8g.setFont(u8g_font_chikitar);
  //jog increment
  u8g.setPrintPos(0, 64);
  if(!(blinkOn && wheelMode == 1))u8g.print("JOG inc: ");
  u8g.setPrintPos(36, 64);
  u8g.print(jogIncrement);
  //distance increment
  u8g.setPrintPos(64, 64);
  if(!(blinkOn && wheelMode == 6))u8g.print("X inc: ");
  u8g.setPrintPos(100, 64);
  u8g.print(increment);
}

void setup(void) {
  clicks = 0;
  encoder.begin(2, 3);
  u8g.setColorIndex(1);
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  Serial.begin(115200);
  softReset();
  resetMpos();
  setStepMm();
  Get_Settings();
  nextDistance = distance;
  nextWorkMode = workMode;
}

void resetMpos(){
  readPosition();
  wmoffset[0] = mpos[0];
  wmoffset[1] = mpos[1];
  wmoffset[2] = mpos[2];
}

void updateClicks(){
  if (encoder.valueChanged()){
    encclicks = encoder.getValue();
    clicks = absclicks - encclicks;
    absclicks = encclicks;
  } else clicks = 0;
}

void controller(void) {

  if(stat.equals("Idle")){
    if(wheelMode == 8){
      wheelMode = 0;
    }
    if(automat == 0){
      feedCurrent = workMode == '0' ? feedRapid : feedMove;
    }
    if(automat == 1){
      workMode = digitalRead(A3) == HIGH ? '0' : '1';
      distance = digitalRead(A2) == HIGH ? abs(distance) : -abs(distance);
      feedCurrent = workMode == '0' ? feedRapid : feedMove;
    }
    if(automat == 2 && nextSet){
      distance = nextDistance;
      workMode = nextWorkMode;
      feedCurrent = workMode == '0' ? feedRapid : feedMove;
      nextSet = false;
    }
    if(automat == 3 && nextSet){
      distance = nextDistance;
      nextSet = false;
    }
  }

  if(stat.equals("Run")){
    if(distance < 0){
      progressA = 0;
      progressB = 128 * (dpos[0] - wpos[0])/distance;  
    } else {
      progressA = 128 - (128 * (dpos[0] - wpos[0])/distance);
      progressB = 128;
    }
    
  }
  
  updateClicks();
  if(clicks != 0){
    switch(wheelMode){
      case 0:
        if(stat.equals("Idle") || stat.equals("Jog")){
          jog(clicks);
        }
        break;
      case 1:
        jogIncrement = setIncrement(jogIncrement, clicks);
        break;
      case 2:
        feedJog += clicks;
        break;
      case 3:
        feedMove += clicks;
        break;
      case 4:
        feedRapid += clicks;
        break;                
      case 5:
        distance += (float)clicks * (float)increment;
        break;
      case 6:
        increment = setIncrement(increment, clicks);
        break;
      case 7:
        automat = setAutomat(clicks);
        break;
      case 8:
        setOverride(clicks);
        break;
    }   
  }

  if(automat == 4){
    workMode = '1';
    if(stat.equals("Idle")){
      unsigned int sensorValueA0 = analogRead(A1);
      feedMovePot = map(sensorValueA0, 0, 1020, 0, feedMove);
      feedCurrent = feedMovePot;
    }

    unsigned int sensorValueA1 = analogRead(A0);
    int8_t rmt = 0;
    if(sensorValueA1 > 590 && sensorValueA1 < 610) rmt = 1;
    if(sensorValueA1 > 755 && sensorValueA1 < 775) rmt = 2;
    if(sensorValueA1 > 90 && sensorValueA1 < 110) rmt = 3;
    if(sensorValueA1 > 170 && sensorValueA1 < 190) rmt = 4;

    switch(rmt){
      case 0:
        //if(stat.equals("Jog"))stopJog();
        if(stat.equals("Run") && runbyremote > 0) gmoveHold();
        if(stat.equals("Idle") && runbyremote > 0) runbyremote = 0;
        break;
      case 1:
        if(runbyremote > 0){
          runbyremote = 0;
          softReset();
        }
        if(stat.equals("Idle") || (stat.equals("Jog") && abs(dpos[0] - wpos[0]) < jogIncrement)){
          jog(-1);
        }
        break;
      case 2:
        if(runbyremote > 0){
          runbyremote = 0;
          softReset();
        }
        if(stat.equals("Idle") || (stat.equals("Jog") && abs(dpos[0] - wpos[0]) < jogIncrement)){
          jog(1);
        }
        break;
      case 3:
        if(stat.equals("Idle") && runbyremote == 0){
          distance = -1*abs(distance);
          gmove();
          runbyremote = 1;
          wheelMode = 8;
        }
        if(stat.equals("Hold:0") && runbyremote == 1){
          gmoveResume();
        }
        if(stat.equals("Hold:0") && runbyremote == 2){
          softReset();
          distance = -1*abs(distance);
          gmove();
          runbyremote = 1;

        }
        break;
      case 4:
        if(stat.equals("Idle") && runbyremote == 0){
          distance = abs(distance);
          gmove();
          runbyremote = 2;
          wheelMode = 8;
        }
        if(stat.equals("Hold:0") && runbyremote == 2){
          gmoveResume();
        }
        if(stat.equals("Hold:0") && runbyremote == 1){
          softReset();
          distance = abs(distance);
          gmove();
          runbyremote = 2;
        }
        break;
    }
  }

  /* STOP ZERO */
  if(Aswitch.wasPressed()){
    wheelMode = 0;
    if(stat.equals("Hold:0")){
      softReset();
    } else if(stat.equals("Jog")){
      stopJog();
    } else if(stat.equals("Idle")){
      resetMpos();
      resetDpos();
    } else if(stat.equals("Alarm")){
      killLock();
    } else if(stat.equals("Run")){
      gmoveHold();
    }
    
   }

   /* DIRECTION +/- */
   if(Bswitch.wasPressed() && automat != 1 && stat.equals("Idle")){
    distance = -distance;
    wheelMode = 0;
   }

   /* RUN GCODE */
   if(Cswitch.wasPressed()){
    wheelMode = 0;
    if(stat.equals("Idle")){
      gmove();
      wheelMode = 8;
      if(automat == 2){
        switch(workMode){
          case '0':
            nextWorkMode = '1';
            break;
          case '1':
            nextWorkMode = '0';
            break;
        }
        nextDistance = -distance;
        nextSet = true;
      }
      if(automat == 3){
        nextDistance = -distance;
        nextSet = true;
      }
    }
    if(stat.equals("Jog")){
      stopJog();
    }    
    if(stat.equals("Run")){
      gmoveHold();
    }    
    if(stat.equals("Hold:0")){
      gmoveResume();
    }    
   }

   /* MOVE / RAPID */
   if(Dswitch.wasPressed() && automat != 1 && automat != 4 && stat.equals("Idle")){
    switch(workMode){
      case '0':
        workMode = '1';
        feedCurrent = feedMove;
        break;
      case '1':
        workMode = '0';
        feedCurrent = feedRapid;
        break;
    }
   }

   /* SETUP */
   if(Eswitch.wasPressed()){
    switch(wheelMode){
      case 0:
        wheelMode = 5;
        break;
      case 5:
        writeFloatParam(distance, DISTANCE_ADDR);
        wheelMode = workMode == '0' ? 4 : 3;
        break;
      case 4:
        writeParam((int)feedRapid, FEEDRAPID_ADDR);
        setMaxRate();
        wheelMode = 7;
        break;
      case 3:
        writeParam((int)feedMove, FEEDMOVE_ADDR);
        wheelMode = 7;
        break;        
      default:
        writeParam(automat, AUTOMAT_ADDR);
        wheelMode = 0;
        break;        
    }
   }

   /* ENCODER BUTTON */
   if(jogswitch.wasPressed()){
    switch(wheelMode){
      case 0:
        wheelMode = 1;
        break;
      case 1:
        writeFloatParam(jogIncrement, JOGINCREMENT_ADDR);
        wheelMode = 0;
        break;        
      case 5:
        wheelMode = 6;
        break;
      case 6:
        wheelMode = 5;
        writeFloatParam(increment, INCREMENT_ADDR);
        break;        
      case 8:
        resetOverride();
        break;        
    }
   }
}

float setIncrement(float increment, int8_t clicks){
  if(clicks > 0) {
      increment = increment * (float)(10 ^ (clicks - 1)); 
  }
  if(clicks < 0) {
      increment = increment / (float)(10 ^ (-1*clicks - 1)); 
  }        
  if(increment < 0.01){
    increment = 0.01;
  }
  if(increment > 100){
    increment = 100;
  }  
  return increment;
}

uint8_t setAutomat(int8_t clicks){
  int8_t a = automat;

  a += clicks;
  if(a >= AMODES){
    a = AMODES - 1;
  }
  if(a < 0){
    a = 0; 
  }  
  return a;
}


void setOverride(int8_t clicks){
  for(uint8_t i = 0; i < abs(clicks); i++){
   Serial.write(clicks > 0 ? 0x93 : 0x94); 
  }
}

void resetOverride(){
   Serial.write(0x90); 
}

void stopJog(){
  Serial.write(0x85);
  readPosition();
}

void jog(int8_t clicks){
  float jogDistance = (float)clicks * (float)jogIncrement;
  Serial.write("$J=G91");
  Serial.write(A);
  Serial.print(jogDistance);
  Serial.write('F');
  Serial.print(feedRapid);
  Serial.write('\n');
  Serial.find('o');
  dpos[a] += jogDistance;
}

void gmoveHold(){
  Serial.write('!');
  readPosition();
}

void gmoveResume(){
  Serial.write('~');
  readPosition();
}

void softReset(){
  matchDpos();
  Serial.write(0x18);
  readPosition();
}

void resetDpos(){
  dpos[0] = 0;
  dpos[1] = 0;
  dpos[2] = 0;
}

void matchDpos(){
  dpos[0] = wpos[0];
  dpos[1] = wpos[1];
  dpos[2] = wpos[2];
}

void killLock(){
  Serial.print("$X\n");
  readPosition();
}

void setStepMm(){
  Serial.write("$100=");
  Serial.print(STEPMM);
  Serial.write('\n');
  Serial.find('o');
}

void setMaxRate(){
  Serial.write("$110=");
  Serial.print(feedRapid);
  Serial.write('\n');
  Serial.find('o');
}

void gmove(){
  Serial.write("G91G");
  Serial.write(workMode);
  Serial.write(A);
  Serial.print(distance);
  Serial.write('F');
  Serial.print(feedCurrent);
  Serial.write('\n');
  Serial.find('o');
  dpos[a] += distance;
}


void readPosition(){
  Serial.write("?");
  do{ }while(Serial.available() == 0);
  Serial.find('<');
  stat = Serial.readStringUntil('|'); 
  Serial.readStringUntil(':');
  mpos[0] = Serial.readStringUntil(',').toFloat();
  mpos[1] = Serial.readStringUntil(',').toFloat();
  mpos[2] = Serial.readStringUntil('|').toFloat();
  String extras = Serial.readStringUntil('>');
  int8_t oAt = extras.indexOf('v');
  if(oAt >= 0){
    int8_t endAt = extras.indexOf(',', oAt);
    if(endAt >=0){
      ov = extras.substring(oAt + 2, endAt) + "%";
    }
  }
  if(stat.equals("Idle")){
    matchDpos();
  }
}

void calcWorldPosition(){
  wpos[0] = mpos[0] - wmoffset[0];
  wpos[1] = mpos[1] - wmoffset[1];
  wpos[2] = mpos[2] - wmoffset[2];
}

void writeParam(unsigned int value, unsigned int addr){ // Write menu entries to EEPROM
  unsigned int a = value/256;
  unsigned int b = value % 256;
  EEPROM.write(addr,a);
  EEPROM.write(addr+1,b);
}

unsigned int readParam(unsigned int addr){ // Read previous menu entries from EEPROM
  unsigned int a=EEPROM.read(addr);
  unsigned int b=EEPROM.read(addr+1);
  return a*256+b; 
}

void writeFloatParam(float value, unsigned int addr) {
  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(addr++, *p++);
  return;
}

static float readFloatParam(unsigned int addr) {
  float value;
  byte* p = (byte*)(void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(addr++);
  return value;
}

void loop(void) {
  unsigned long currentTime = millis();
  if(currentTime - lastBlink > 500){
    blinkOn = !blinkOn;
    lastBlink = currentTime;
  }
  calcWorldPosition();
  // picture loop
  u8g.firstPage();  
  do {
    draw();
  } while( u8g.nextPage() );

  readPosition(); 
  controller();
}
