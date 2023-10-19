#include <Adafruit_GFX.h>
#include <SPI.h>
//#pragma GCC optimize ("O2")

//#define DOUBLE_BUFFER
#define CANVAS_WIDTH (128)
#define CANVAS_HEIGHT (32)
#define MAX_COLOR_DEPTH (12)

#ifdef ESP8266
 #include <sigma_delta.h>
  #define OEN D4
  #define LAT D8
  #define RSA D6
  #define RSB D1
  #define RSC D2
  #define RSD D3
#elif ESP32
  #define OEN 19
  #define LAT 5
  #define RSA 15
  #define RSB 4
  #define RSC 16
  #define RSD 17

  hw_timer_t *timer = NULL;
  //TaskHandle_t TaskSafeFlush = NULL;
  TaskHandle_t TaskMonitor = NULL;
#endif

uint32_t subframeInterval = 0x4000;
uint8_t brightnessled = 255;
uint8_t brightnessval = 255;
uint8_t bitextend = 4;
uint8_t bitlimit = MAX_COLOR_DEPTH-1;
// 0 : 25.00%
// 1 : 40.00%
// 2 : 57.14%
// 3 : 72.72%
// 4 : 84.21%

#ifdef DOUBLE_BUFFER
  #define SCREEN_BUFFERS 2
  #define DOUBLE_BUFFER_ENABLED true
#else
  #define SCREEN_BUFFERS 1
  #define DOUBLE_BUFFER_ENABLED false
#endif

uint32_t bufferDisp[SCREEN_BUFFERS][MAX_COLOR_DEPTH][CANVAS_HEIGHT/2][24];  // 2x12 planes flushing display buffer
const uint32_t bufferPlaneSize = CANVAS_HEIGHT*24*4/2;

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
volatile bool planeframe = false;
volatile uint32_t framecountrend = 0;
volatile uint32_t framecountrefr = 0;

char planemaps[] = { 0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4 };

#include "parallelpin.h"
ParallelPin pp(RSA,RSB,RSC,RSD,LAT,OEN);

int flushrow=0;
int flushplane=0,lastflushplane=0;
void IRAM_ATTR displayMatrixFlush(int bitlimit, int bitextend, bool mode=false){
  int f, p, nn, br;
  if(mode){
    flushplane++;
  }else{
    f = flushrow++;
    if( flushrow >= 16 ){
      flushrow = 0;
      flushplane++;
    }
  }
  nn = (1<<bitextend)-1;
  if ( flushplane >= nn+8 )  flushplane = p = 0;
  if ( flushplane >= nn   )  p = bitextend+flushplane-nn;
  else  p = (flushplane&15)==15?planemaps[flushplane>>4]+4:planemaps[flushplane&15]  ;
  if( p > bitlimit )  flushplane = p = 0;
  if ( flushplane < nn )  br = brightnessled;
  else  br = (brightnessled+1)>>(flushplane-nn+1);
  if(mode){
    if ( flushplane < lastflushplane )
      flushrow++;
    if( flushrow >= 16 ) flushrow = 0;
    f = flushrow;
  }
  lastflushplane = flushplane;
  if( flushrow==0 && flushplane == 0 ){
    framecountrend++;
    if(refreshing){
      bufferflip = !bufferflip;
      refreshing = false;
    }
  }
  
  // 0 1 2 3 4 5 6 7
  // 0|1 2 3 4 5 6 7 8
  // 0 0 1|2 3 4 5 6 7 8 9
  // 0 0 0 0 1 1 2|3 4 5 6 7 8 9 A
  // 0 0 0 0 0 0 0 0 1 1 1 1 2 2 3|4 5 6 7 8 9 A B
  // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 2 2 2 2 3 3 4|5 6 7 8 9 A B C
  SPI.writeBytes((uint8_t*)bufferDisp[bufferflip][p][f], 96);
  #ifdef ESP8266
    sigmaDeltaDetachPin(OEN);
  #elif ESP32
    ledcDetachPin(OEN);
  #endif
  pp.setPins(f|0b110000);
  pp.setPins(f|0b100000);
  //digitalWrite(OEN,HIGH);
  //digitalWrite(LAT,HIGH);
  //digitalWrite(RSD,(f>>3&1));
  //digitalWrite(RSC,(f>>2&1));
  //digitalWrite(RSB,(f>>1&1));
  //digitalWrite(RSA,(f>>0&1));
  //digitalWrite(LAT,LOW);

#ifdef ESP8266
  sigmaDeltaWrite(0,255-br);
  sigmaDeltaAttachPin(OEN,0);
#elif ESP32
  ledcWrite(0,255-br);
  ledcAttachPin(OEN,0);
#endif

  return;
}

uint32_t delayval;
uint32_t lastCycle;

void IRAM_ATTR interruptDisplayRow(){
  displayMatrixFlush(bitlimit,bitextend,planeframe);
#ifdef ESP8266
  //timer0_write(ESP.getCycleCount()+delayval);
  timer0_write(lastCycle+=delayval);
#endif
#ifdef ESP32
  timerAlarmWrite(timer, delayval/2, true);
#endif
}
void interruptDisplayRowFirst(){
  displayMatrixFlush(0,1);
}
/*
void TaskSafeFlush(void *param){
  bitextend =- 1;
  while(true){
    displayMatrixFlush();
    vTaskDelay(1);
  }
}
*/
uint32_t lastframecount;
uint32_t demoselect;
void TaskMonitorCode(void *param){
  while(true){
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
            demoselect = number&255;
            Serial.print("Demo select: ");
            Serial.println(demoselect);
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
          case 0xC: 
            break;
          case 0xD: 
            sigmaDeltaSetPrescaler(number); 
            Serial.print("sigmadelta prescaler: ");
            Serial.println(number&255);
            break;
          case 0xE: 
            brightnessval = number; 
            Serial.print("brightness value: ");
            Serial.println(number&255);
            break;
          case 0xF:
            brightnessled = number; 
            Serial.print("brightness sigma: ");
            Serial.println(number&255);
            break;
        }
      }else{
        delayval = number;
        Serial.print("interval: ");
        Serial.println(delayval);
        lastCycle = ESP.getCycleCount();
        timer0_write(lastCycle += delayval);
      }
    }
    if((int32_t)(ESP.getCycleCount()-lastCycle) > 0x100000){
      Serial.println("Interrupt late! ");
      lastCycle = ESP.getCycleCount();
      timer0_write(lastCycle += 0x100000);
    }
    return;
  }
}

class MySPIHub75 : public GFXcanvas8 {
public:
  MySPIHub75(uint16_t w, uint16_t h);
  void updateImage();
  void begin();
  void initdisplay();
  void startdisplay();
  void display(bool);
  void setDepth(uint8_t v);
  void setBrightness(uint8_t v);
  void setBrightness8(uint8_t v);
  void blendPixel(int x, int y, uint16_t c, uint8_t d);
  //void drawPixel(int x, int y, int c){
  //  x -= 96;
  //  GFXcanvas8::drawPixel(x*2,y*2,c);
  //  //GFXcanvas8::drawPixel(x*2+1,y*2,c);
  //  GFXcanvas8::drawPixel(x*2,y*2+1,c);
  //  //GFXcanvas8::drawPixel(x*2+1,y*2+1,c);
  //};
  uint8_t getBrightness8();
  uint32_t color888(uint8_t r, uint8_t g, uint8_t b);
  int _depth = MAX_COLOR_DEPTH;
};

MySPIHub75::MySPIHub75(uint16_t w, uint16_t h)
    : GFXcanvas8(w, h) {}
    
void MySPIHub75::initdisplay(){
  SPI.begin();
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  
  pinMode(OEN, OUTPUT);
  pinMode(LAT, OUTPUT);
  pinMode(RSA, OUTPUT);
  pinMode(RSB, OUTPUT);
  pinMode(RSC, OUTPUT);
  pinMode(RSD, OUTPUT);

  this->fillScreen(0);
  this->display(true);

#ifdef ESP8266
  sigmaDeltaSetup(0, 312500/8);
  sigmaDeltaAttachPin(OEN, 0);
#elif ESP32
  ledcSetup(0, 40000000/(1<<8), 8);
  ledcAttachPin(OEN, 0);
#endif

  flushplane = flushrow = 0;

  Serial.println("Display initialized");
}


void MySPIHub75::startdisplay(){
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

void MySPIHub75::begin(){
  this->initdisplay();
  this->startdisplay();
}

void MySPIHub75::display(bool flush=false) {
  while ( refreshing & !flush ) { delay(0); }
  bool b = ( bufferflip ^ !flush ) & DOUBLE_BUFFER_ENABLED;
  //uint16_t* bufferf = this->getBuffer();
  uint8_t* bufferf = this->getBuffer();
  uint32_t colpixlL,colpixlR,colpixlG,colpixlB;
  for (char y = 0; y < _height; y++) {
    for (char x = 0; x < _width; x += 8) {
      uint8_t* colbyte = (uint8_t*)&bufferDisp[b][0][y&15][0];
      uint8_t* colbyte_origin = colbyte+(x>>3&7)+(!(y>>4)+(x>>6)*2)*24;
      for (char z = 0; z < 8; z++) {
        colbyte   = colbyte_origin;
        colpixlL  = bufferf[_width-x-z-1+(_height-y-1)*_width];
        //colpixlL  = getRawPixel(x+z,y);
        colpixlR  = lumConvTab[colpixlL];
        colpixlG  = lumConvTab[colpixlL];
        colpixlB  = lumConvTab[colpixlL];
        /*
        colpixlR  = ( ( colpixlL >> 11 ) << 3 ) & 255 ;
        colpixlG  = ( ( colpixlL >> 5  ) << 2 ) & 255 ;
        colpixlB  = ( ( colpixlL >> 0  ) << 3 ) & 255 ;
        colpixlR  = lumConvTab[colpixlR|(colpixlR>>5)];
        colpixlG  = lumConvTab[colpixlG|(colpixlG>>6)];
        colpixlB  = lumConvTab[colpixlB|(colpixlB>>5)];
        */
        colpixlR *= brightnessval + 1 ;
        colpixlG *= brightnessval + 1 ;
        colpixlB *= brightnessval + 1 ;
        for (char i = 0; i < _depth; i++){
          uint32_t masker = ~(1<<(7-z));
          *(colbyte+ 0) = (*(colbyte+ 0)&masker)|((colpixlB>>(23-i)&1)<<(7-z));
          *(colbyte+ 8) = (*(colbyte+ 8)&masker)|((colpixlG>>(23-i)&1)<<(7-z));
          *(colbyte+16) = (*(colbyte+16)&masker)|((colpixlR>>(23-i)&1)<<(7-z));
          colbyte += bufferPlaneSize;
        }
      }
    }
  }
  refreshing = !flush & DOUBLE_BUFFER_ENABLED;
  framecountrefr++;
}

void MySPIHub75::setBrightness(uint8_t v){
  brightnessled = v;
}

void MySPIHub75::setBrightness8(uint8_t v){
  brightnessval = v;
}

uint8_t MySPIHub75::getBrightness8(){
  return brightnessval;
}

void MySPIHub75::setDepth(uint8_t v){
  if( v ) _depth = v;
  else _depth = 8;
}

void MySPIHub75::blendPixel(int x, int y, uint16_t c, uint8_t d){
  if( d < 255 ){
    int n = getPixel( x, y );
    /*
    int r = ( ( c >> 11 & 0x1F ) * ( d + 1 ) + ( n >> 11 & 0x1F ) * ( 255 - d ) ) >> 8;
    int g = ( ( c >> 5  & 0x3F ) * ( d + 1 ) + ( n >> 5  & 0x3F ) * ( 255 - d ) ) >> 8;
    int b = ( ( c >> 0  & 0x1F ) * ( d + 1 ) + ( n >> 0  & 0x1F ) * ( 255 - d ) ) >> 8;
    c = r << 11 | g << 5 | b;
    */
    c = ( ( c ) * ( d + 1 ) + ( n ) * ( 255 - d ) ) >> 8;
  }
  drawPixel( x, y, c );
}

uint32_t MySPIHub75::color888(uint8_t r, uint8_t g, uint8_t b){
  //return ( ( r >> 3 ) << 11 ) | ( ( g >> 2 ) << 5 ) | ( b >> 3 ) ;
  return g;
}

//void MySPIHub75::beginSafe(){
//  xTaskCreate(TaskSafeFlushCode,"Task",1024,NULL,0,&TaskSafeFlush);
//}