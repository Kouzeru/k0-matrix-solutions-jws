// Written by Kouzerumatsukite,
// driver was written since 9 Nov 2022
// reworked a bit since 17 Mar 2023
#include <sigma_delta.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include "parallelpin.h"

#define CANVAS_WIDTH (32)
#define CANVAS_HEIGHT (16)
#define MAX_COLOR_DEPTH (16)

#define OEN D6
#define LAT D8
#define RSA D3
#define RSB D4
#define RSC D2

ParallelPin ppin(RSA,RSB,-1,-1,LAT,OEN);

uint32_t subframeInterval = 0x4000;
volatile bool planeframe = false;
uint8_t brightnessled = 255;
uint8_t brightnessval = 255;
uint8_t bitextend = 4;
uint8_t bitlimit = MAX_COLOR_DEPTH-1;

uint32_t bufferDisp[2][MAX_COLOR_DEPTH][CANVAS_HEIGHT/4][CANVAS_WIDTH/8];  // 2x12 planes flushing display buffer
const uint32_t bufferPlaneSize = CANVAS_HEIGHT*CANVAS_WIDTH/8;

const uint16_t lumConvTab[]={ 
        0,    27,    56,    84,   113,   141,   170,   198,   227,   255,   284,   312,   340,   369,   397,   426, 
      454,   483,   511,   540,   568,   597,   626,   657,   688,   720,   754,   788,   824,   860,   898,   936, 
      976,  1017,  1059,  1102,  1146,  1191,  1238,  1286,  1335,  1385,  1436,  1489,  1543,  1598,  1655,  1713, 
     1772,  1833,  1895,  1958,  2023,  2089,  2156,  2225,  2296,  2368,  2441,  2516,  2592,  2670,  2750,  2831, 
     2914,  2998,  3084,  3171,  3260,  3351,  3443,  3537,  3633,  3731,  3830,  3931,  4034,  4138,  4245,  4353, 
     4463,  4574,  4688,  4803,  4921,  5040,  5161,  5284,  5409,  5536,  5665,  5796,  5929,  6064,  6201,  6340, 
     6482,  6625,  6770,  6917,  7067,  7219,  7372,  7528,  7687,  7847,  8010,  8174,  8341,  8511,  8682,  8856, 
     9032,  9211,  9392,  9575,  9761,  9949, 10139, 10332, 10527, 10725, 10925, 11127, 11332, 11540, 11750, 11963, 
    12178, 12395, 12616, 12839, 13064, 13292, 13523, 13757, 13993, 14231, 14473, 14717, 14964, 15214, 15466, 15722, 
    15980, 16240, 16504, 16771, 17040, 17312, 17587, 17865, 18146, 18430, 18717, 19006, 19299, 19595, 19894, 20195, 
    20500, 20808, 21119, 21433, 21750, 22070, 22393, 22720, 23049, 23382, 23718, 24057, 24400, 24745, 25094, 25446, 
    25802, 26160, 26522, 26888, 27256, 27628, 28004, 28382, 28765, 29150, 29539, 29932, 30328, 30727, 31130, 31536, 
    31946, 32360, 32777, 33197, 33622, 34049, 34481, 34916, 35354, 35797, 36243, 36692, 37146, 37603, 38064, 38528, 
    38996, 39469, 39945, 40424, 40908, 41395, 41886, 42382, 42881, 43383, 43890, 44401, 44916, 45434, 45957, 46484, 
    47014, 47549, 48088, 48630, 49177, 49728, 50283, 50842, 51406, 51973, 52545, 53120, 53700, 54284, 54873, 55465, 
    56062, 56663, 57269, 57878, 58492, 59111, 59733, 60360, 60992, 61627, 62268, 62912, 63561, 64215, 64873, 65535
};

volatile bool refreshing = false;
volatile bool bufferflip = false;

uint32_t framecount = 0;
uint32_t framecountrefr = 0;
char planemaps[] = { 
  0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,6,
  0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,7,
  0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,6,
  0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,8,
};
//char planemaps[] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,4, };
//char planemaps[] = { 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4, }
char zeros[] = {
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
};
int flushcount;
int planecount;
int rowcount;
void IRAM_ATTR displayMatrixFlush(int bitlimit, int bitextend, bool mode=false){
  flushcount++;
  digitalWrite(3,(flushcount>>1)&1);
  int s = (1<<bitextend)-1;
  int br = planecount>=s?(brightnessled+1)>>(planecount+1-s):brightnessled;
  int pl = planecount>=s?bitextend+(planecount-s):planemaps[planecount];
  if( pl > bitlimit ){
    char zero = 0;
    SPI.writeBytes((uint8_t*)zeros,(CANVAS_WIDTH/8)*(CANVAS_HEIGHT/4));
    sigmaDeltaDetachPin(OEN);
    digitalWrite(OEN,LOW);
  }else{
    SPI.writeBytes((uint8_t*)bufferDisp[bufferflip][pl][rowcount], (CANVAS_WIDTH/8)*(CANVAS_HEIGHT/4));
    sigmaDeltaDetachPin(OEN);
    digitalWrite(OEN,LOW);
    ppin.setPins(rowcount&3|0b010000);
    ppin.setPins(rowcount&3|0b000000);
    //digitalWrite(OEN,LOW);
    //digitalWrite(RSB,(rowcount>>1&1));
    //digitalWrite(RSA,(rowcount>>0&1));
    //digitalWrite(LAT,HIGH);
    //digitalWrite(LAT,LOW);
    sigmaDeltaWrite(0,br);
    sigmaDeltaAttachPin(OEN,0);
  }

  if(mode){
    planecount++;
    pl = planecount>=s?bitextend+(planecount-s):planemaps[planecount];
    if( pl > bitlimit ){
      planecount = 0;
      rowcount++;
      if(rowcount >= 4){
        rowcount = 0;
        if(refreshing){
          bufferflip = !bufferflip;
          refreshing = false;
          framecount++;
        }
      }
    }
  }else{
    rowcount++;
    if( rowcount >= 4 ){
      rowcount = 0;
      planecount++;
      pl = planecount>=s?bitextend+(planecount-s):planemaps[planecount];
      if( pl > bitlimit ){
        planecount = 0;
        if(refreshing){
          bufferflip = !bufferflip;
          refreshing = false;
          framecount++;
        }
      }
    }
  }

  return;
}

volatile int32_t flushesPerFrame = 0;
volatile uint32_t delayval = subframeInterval;

uint64_t lastCycle;

void IRAM_ATTR interruptDisplayRow(){
  timer0_write(lastCycle+=delayval);
  displayMatrixFlush(bitlimit, bitextend, planeframe);
  flushesPerFrame++;
}
void interruptDisplayRowFirst(){
  //flushcount &= 3;
  displayMatrixFlush(0,1);
}

char demoselect = 0;

class MySPIHub15 : public GFXcanvas8 {
public:
  MySPIHub15(uint16_t w, uint16_t h);
  void updateImage();
  void begin();
  void initdisplay();
  void startdisplay();
  void display(bool);
  void setBrightness(uint8_t v);
  void setBrightness8(uint8_t v);
  void setDepth(uint8_t v);
  void blendPixel(int x, int y, uint8_t c, uint8_t d);
  uint8_t getBrightness8();
  uint32_t color888(uint8_t r, uint8_t g, uint8_t b);
  int _depth = 16;
};

MySPIHub15::MySPIHub15(uint16_t w, uint16_t h)
    : GFXcanvas8(w, h) {}
    
void MySPIHub15::initdisplay(){
  SPI.begin();
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

  pinMode(OEN, OUTPUT);
  pinMode(LAT, OUTPUT);
  pinMode(RSA, OUTPUT);
  pinMode(RSB, OUTPUT);

  this->fillScreen(0);
  this->display(true);

#ifdef ESP8266
  sigmaDeltaSetup(0, 312500/8);
  sigmaDeltaAttachPin(OEN, 0);
#elif ESP32
  ledcSetup(0, 40000000/(1<<8), 8);
  ledcAttachPin(OEN, 0);
#endif
  Serial.println("Display initialized");
}

void MySPIHub15::startdisplay(){
  delayval = subframeInterval;
#ifdef ESP8266
  timer0_isr_init();
  //timer1_enable(TIM_DIV1,TIM_EDGE,TIM_LOOP);
  timer0_attachInterrupt(interruptDisplayRow);
  lastCycle = ESP.getCycleCount();
  timer0_write(lastCycle+=delayval); // 40fps, 640fps
  //timer1_write(delayval); // 40fps, 640fps
#elif ESP32
  timer = timerBegin(0, 2, true);
  timerAttachInterrupt(timer, &interruptDisplayRow, true);
  timerAlarmWrite(timer, delayval/2, true);
  timerAlarmEnable(timer);
  xTaskCreate(\
    TaskMonitorCode, /* Function to implement the task */\
    "Task1", /* Name of the task */\
    2000,  /* Stack size in words */\
    NULL,  /* Task input parameter */\
    4,  /* Priority of the task */\
    &TaskMonitor,  /* Task handle. */\
  );
#endif
  Serial.println("Display started");
}

void MySPIHub15::begin(){
  this->initdisplay();
  this->startdisplay();
}

void MySPIHub15::display(bool flush=false) {
  while( refreshing & !flush ){ delay(0); }
  uint8_t* bufferf = this->getBuffer();
  bool b = bufferflip^!flush;
  for (char y = 0; y < CANVAS_HEIGHT; y++) {
    for (char x = 0; x < CANVAS_WIDTH; x += 8) {
      uint32_t colpixl = 0;
      uint8_t* colbyte = (uint8_t*)&bufferDisp[b][0][y&3][x>>3];
      uint8_t* colbyte_origin = colbyte+((y>>2)^3);
      for (char z = 0; z < 8; z++) {
        colbyte = colbyte_origin;
        colpixl   = bufferf[y*_width+x+z];
        colpixl   = lumConvTab[colpixl&255];
        colpixl  *= brightnessval+1;
        colpixl >>= 8;
        for (char i = 0; i < _depth; i++){
          char shift = ( 7 - z );
          bool pixel = ( colpixl >> (15-i) & 1 );
          *colbyte &= ~(1 << shift );
          *colbyte |= !pixel << shift;
          colbyte += bufferPlaneSize;
        }
      }
    }
  }
  refreshing = !flush;
  framecountrefr++;
}

void MySPIHub15::setBrightness(uint8_t v){
  brightnessled = v;
}

void MySPIHub15::setBrightness8(uint8_t v){
  brightnessval = v;
}

uint8_t MySPIHub15::getBrightness8(){
  return brightnessval;
}

void MySPIHub15::setDepth(uint8_t v){
  if( v ) _depth = v;
  else _depth = 16;
}

void MySPIHub15::blendPixel(int x, int y, uint8_t c, uint8_t d){
  if(d<255){
    int n = getPixel( x, y );
    int o = ( c * ( d + 1 ) + n * ( 256 - d ) ) >> 8;
    drawPixel(x, y, o);
  }else{
    drawPixel(x, y, c);
  }
}

void TaskMonitorCode(void *param){
  if (Serial.available() > 0) { // This thing is just for command receiver
    // read the incoming byte:
    Serial.setTimeout(100);
    //read until timeout
    String teststr = Serial.readString();
    // remove any \r \n whitespace at the end of the String
    teststr.trim();
    const char* teststrchar = teststr.c_str();
    int32_t number = (int32_t)strtol(teststrchar, NULL, 16);
    if(number<0x1000){
      switch(number>>8){
        case 0x0: // demo selector
          Serial.print("demo select: ");
          Serial.println(number&255);
          demoselect = number&255;
        break;
        case 0x1: break;
        case 0x2: break;
        case 0x3: break;
        case 0x4: break;
        case 0x5: break;
        case 0x6: break;
        case 0x7: break;
        case 0x8: break;
        case 0x9: break;
        case 0xA:
          bitlimit = (((number>>4)-1)%MAX_COLOR_DEPTH);
          bitextend = (number)&15;
          Serial.print("bitdepth extend: ");
          Serial.println(bitextend);
          Serial.print("bitdepth limit: ");
          Serial.println(bitlimit);
          break;
        case 0xB: 
          planeframe = (number&255)>0;
          Serial.print("planeframe: ");
          Serial.println(planeframe);
          break;
        case 0xD: 
          sigmaDeltaSetPrescaler(number); 
          Serial.print("sigmadelta prescaler: ");
          Serial.println(number&255);
        break;
        case 0xE: 
          brightnessval = number; 
          Serial.print("brightness sigma: ");
          Serial.println(number&255);
          break;
        case 0xF:
          brightnessled = number; 
          Serial.print("brightness value: ");
          Serial.println(number&255);
          break;
      }
    }else{
      delayval = number;
      Serial.print("interval: ");
      Serial.println(delayval);
    }
  }
}
uint32_t MySPIHub15::color888(uint8_t r, uint8_t g, uint8_t b){return g;}