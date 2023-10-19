#include <Arduino.h>
#include <pgmspace.h>
#include "pic1b.h"
#include "fonts.h"
#include "jadwal_shalat.h"

#define CANVAS_WIDTH (128)
#define CANVAS_HEIGHT (16)

//#include "pics.h"
//#include "audio.h"
#define AUD D0
//#define MINI_MODE

// RTC MODE:
#define RTC_I2C

// DISPLAY MODE: choose only one
#define HUB15
//#define HUB75

bool use_rtc = true;
bool beeping = false;
#ifdef RTC_I2C
  #include <Wire.h>
  #include <RtcDS1307.h>
  RtcDS1307<TwoWire> Rtc(Wire);
  void setBeep(bool beep){
    if(use_rtc){
      if(beep!=beeping){
        beeping = beep;
        if(beep){
          Rtc.SetSquareWavePin(DS1307SquareWaveOut_4kHz); 
        }else{
          Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 
        }
      }
    }
    if(beep!=beeping){
      beeping = beep;
      if(beep){
        pinMode(3,OUTPUT);
      }else{
        pinMode(3,INPUT);
      }
    }
    digitalWrite(D0,beep);
  };
#else
  #include <RtcDS1302.h>
  ThreeWire myWire(SDA,SCL,D0); // IO, SCLK, CE
  RtcDS1302<ThreeWire> Rtc(myWire);
  void setBeep(bool beep){
    pinMode(3,OUTPUT);
    digitalWrite(3,beep);
  };
#endif

#ifdef HUB75
  #include "myspihub75.h"
  MySPIHub75 display(CANVAS_WIDTH,CANVAS_HEIGHT);
#endif
#ifdef HUB15
  #include "myspihub15.h"
  MySPIHub15 display = MySPIHub15(CANVAS_WIDTH,CANVAS_HEIGHT);
#endif

#include "timekeeper.h"
#include "ota3.h"

static float sint[256];
struct tm timeinfo;
static time_t lastTime = 0;
static time_t lastTimePlus = 0;
static int32_t lastMillis = 0;
static int32_t lastMillis1 = 0;
static int32_t lastMillis2 = 0;
static int32_t lastMicros1 = 0;
static int32_t lastTimeMillis = 0;
static uint64_t now_millis = 0;
static uint8_t pixelBrightnessValue = 255;
void pixelBrightness(uint8_t v){
  pixelBrightnessValue = v;
}
void pixel(int x, int y, int c, int d=255){
  int m = (c*(pixelBrightnessValue+1))>>8;
  if(x<32) display.blendPixel(x,y,display.color888(0,m,0), d);
  else     display.blendPixel(x,y,display.color888(m,m,m), d);
}

template <typename T> void canvas_pic1b(const T *img, int ox=0, int oy=0, int d=255){
  for(int x=0, i=0; x<128; x++)
    for(int y=0; y<sizeof(T)*8; y+=8)
      for(int k=0, d=pgm_read_byte((uint8_t*)img + i++); k<8; k++)
        pixel(ox+x,oy+y+k,((d>>(k))&1)?255:0,255);
}


/////////////////////////////////////
// To Hijri conversion
/////////////////////////////////////
struct hijri_date{
  uint8_t day;
  uint8_t month;
  uint16_t year; 
};

static hijri_date hijrinfo;

struct hijri_date convertToHijri(time_t masehi){
  struct tm timeinfo = *localtime(&masehi);
  int hr = timeinfo.tm_mday;
  int bln = timeinfo.tm_mon+1;
  int thn = timeinfo.tm_year+1900;
  struct hijri_date hijri;
  /*
  * Author: FahmyDoank (Fahmi al-Amri)
  *  THIS SOURCE CODE IS FREE FOR USE IN
  *  THE NAME OF ALLAH SUBHANAHU WA TA'ALA
  */
  
  int zjd, zl, zn, zj, bulan, hari, tahun;
  if ((thn > 1582) || ((thn == 1582) && (bln > 10)) || ((thn == 1582) && (bln == 10) && (hr > 14))) {
    zjd=((1461 * (thn + 4800 + ((bln - 14) / 12))) / 4) + ((367 * (bln - 2 - 12 * (((bln - 14) / 12)))) / 12) - ((3 * (((thn + 4900 + ((bln - 14) / 12)) / 100))) / 4) + hr - 32075;
  } else {
    zjd= 367 * thn - ((7 * (thn + 5001 + ((bln - 9) / 7))) / 4) + ((275 * bln) / 9) + hr + 1729777;
  }

  zl=zjd - 1948440 + 10632;
  zn=((zl - 1) / 10631);
  zl=zl - 10631 * zn + 354;
  zj=(((10985 - zl) / 5316)) * (((50 * zl) / 17719)) + ((zl / 5670)) * (((43 * zl) / 15238));
  zl=zl-(((30 - zj) / 15)) * (((17719 * zj) / 50))-((zj / 16)) * (((15238 * zj) / 43)) + 29;
  bulan=((24 * zl) / 709);
  hari=zl-((709 * bulan) / 24);
  tahun=30 * zn + zj - 30;

  // output-nya
  hijri.day = hari;
  hijri.month = bulan;
  hijri.year = tahun;

  return hijri;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BEGIN RTC STUFFS


static volatile int salah_idx;
static volatile int salah_time;
static volatile int salah_next;
static volatile int now_time;

void update_time(){
  int newTime;
  if (use_rtc){
    RtcDateTime now = Rtc.GetDateTime();
    newTime = now.Unix64Time();
  }else{
    newTime = (lastTime+(millis()-lastMillis)/1000);
  }
  if(lastTimePlus != newTime){
    lastTimeMillis = millis();
    lastTimePlus = newTime;
  }
  timeinfo = *localtime(&lastTimePlus);
  hijrinfo = convertToHijri(lastTimePlus);
  now_time = timeinfo.tm_hour*60*60+timeinfo.tm_min*60+timeinfo.tm_sec;
  now_millis = millis()-lastTimeMillis;
  update_salah_time();
}

int scanforrtc() {
  Serial.print("This program is compiled since: ");
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

#ifdef RTC_I2C
  int error;
  int address = 0x68;
  int mmm = millis();
  Wire.begin();

  Serial.println("Detecting for RTC...");
  // do this for only 2 secs
  while (millis()-mmm < 2000) { 
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.println("RTC is found!");
      Rtc.Begin();
      if (!Rtc.IsDateTimeValid())  {
        Serial.println("RTC lost confidence in the DateTime!");
        // Common Causes:
        //    1) first time you ran and the device wasn't running yet
        //    2) the battery on the device is low or even missing
        Rtc.SetDateTime(compiled);
      }
      if (!Rtc.GetIsRunning()) {
          Serial.println("RTC was not actively running, starting now");
          Rtc.SetIsRunning(true);
      }
      return Rtc.IsDateTimeValid();
    }
    else if (error == 4) {
      Serial.println("Fail to detect RTC!");
      delay(100);
    }
    interruptDisplayRowFirst();
  }
#else
  Rtc.Begin();
  Serial.println();

  if (!Rtc.IsDateTimeValid())  {
    Serial.println("RTC lost confidence in the DateTime!");
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected())
  {
      Serial.println("RTC was write protected, enabling writing now");
      Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning()) {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
  }
  if(Rtc.IsDateTimeValid()){
    return true;
  }
#endif
  return false;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// JADWAL WAKTU SHALAT
////////////////////////////////////////////////////////////////////////////////////////////////////

const char* prayTimesNameFull_normal[] = { "Imsak", "Shubuh", "Syuruq",  "Dhuha", "Dzuhur", "Ashar", "Maghrib",  "Isya'", };
const char* prayTimesNameFull_jumat[] = { "Imsak", "Shubuh", "Syuruq",  "Dhuha", "Jum'at", "Ashar", "Maghrib",  "Isya'", };
const char* prayTimesNameShort_normal[] = { "Imsak", "Subuh", "Syruq", "Dhuha", "Zuhur", "Ashar", "Magrb", "Isya'", };
const char* prayTimesNameShort_jumat[] = { "Imsak", "Subuh", "Syruq", "Dhuha", "Jumat", "Ashar", "Magrb", "Isya'", };
const char** prayTimesNameFull = prayTimesNameFull_normal;
const char** prayTimesNameShort = prayTimesNameShort_normal;

int get_salah_time(int mm, int dd, int tt){
  int w = pgm_read_word(jadwal_shalat[mm]+dd*8+(tt%8));
  return ((((w>>12)&15)*600+((w>>8)&15)*60+((w>>4)&15)*10+(w&15)))*60;
}

int remainder(int i, int n){
  return (i % n + n) % n;
}

void update_salah_time(){
  if((timeinfo.tm_wday == 5) && (millis()%6000<3000)){
    prayTimesNameFull = prayTimesNameFull_jumat;
    prayTimesNameShort = prayTimesNameShort_jumat;
  }else{
    prayTimesNameFull = prayTimesNameFull_normal;
    prayTimesNameShort = prayTimesNameShort_normal;
  }
  int dd = timeinfo.tm_mday-1;
  int mm = timeinfo.tm_mon;
  int salah_min_sec = 60*60*24;
  int salah_min_idx = -1;
  for (int k=0; k<8; k++){
    int time_salah = get_salah_time(mm,dd,k);
    int time_salah_diff = remainder(now_time-time_salah, 60*60*24);
    if( time_salah_diff  < salah_min_sec ){
      salah_min_idx = k;
      salah_min_sec = time_salah_diff;
    }
  }
  salah_idx = salah_min_idx;
  salah_time = get_salah_time(mm,dd,salah_idx);
  salah_next = get_salah_time(mm,dd,salah_idx+1);
}

/////////////////////////////////////
// TIME DATE CLOCK DRAW ROUTINE
/////////////////////////////////////
const char* namaHari[] = { "Ahad", "Senin", "Selasa", "Rabu", "Kamis", "Jum'at", "Sabtu", };
const char* namaBulanPend[] = { "Jan", "Feb", "Mar", "Apr", "Mei", "Jun", "Jul", "Agu", "Sep", "Okt", "Nov", "Des", };
const char* namaBulan[] = { "Januari", "Februari", "Maret", "April", "Mei", "Juni", "Juli", "Agustus", "September", "Oktober", "November", "Desember", };
const char *cbulan[] = { "Muharram","Safar","Rabiul Awal","Rabiul Akhir", "Jumadil Awal","Jumadil Akhir","Rajab", "Sya'ban","Ramadhan","Syawwal","Dzulkaidah","Dzulhijjah" };

void drawClock(int x, int y, tm t, bool c){
  char buf[20];
  font_load(&font_kouzu5x8);

  float z, tt = (float)(millis()%4000)/4000*PI*2;
  sprintf(buf,"%02d",t.tm_hour);
  z = (sinf((float)(x+0)/32+tt)+1)/2*0.10+0.90; z *= z; z *= z;
  font_text(buf, x+ 0, y, c?z*255:0);

  sprintf(buf,"%02d",t.tm_min );
  z = (sinf((float)(x+15)/32+tt)+1)/2*0.10+0.9; z *= z; z *= z;
  font_text(buf, x+15, y, c?z*255:0);

  //sprintf(buf,"%02d",t.tm_sec );
  //z = (sinf((float)(x+30)/32+tt)+1)/2*0.25+0.75; z *= z; z *= z;
  //drawbigfont(buf, x+30, y, c?z*255:0);

  float a = (cosf((float)(now_millis%1000)/1000.f*2.f*PI)+1.f)/2.f; a *= a*255;
  font_text(":", x+11, y, a*c);
  //drawbigfont(":", x+25, y, a*c);
}

void drawClockPlain(int x, int y, tm t, int c){
  char buf[20]; font_load(&font_kouzu5x8);
  sprintf(buf,"%02d",t.tm_hour); font_text(buf, x+ 0, y, c);
  sprintf(buf,"%02d",t.tm_min ); font_text(buf, x+15, y, c);
  if( now_millis < 500 ) { font_text(":", x+11, y, c); }
}

void drawDate(int x, int y, tm t, int c){
  char buf[20]; font_load(&font_kouzu3x5);
  if(t.tm_mday<10) sprintf(buf,"%01d",t.tm_mday);
  else             sprintf(buf,"%02d",t.tm_mday);
  font_text(buf, x+ 0, y, c);
  font_text(namaBulanPend[t.tm_mon], x+10, y, c);
  sprintf(buf,"%02d",t.tm_year%100);
  font_text(buf, x+24, y, c);
}

void drawDatePlain(int x, int y, tm t, int c){
  char buf[20]; font_load(&font_kouzu3x5);
  sprintf(buf,"%02d/%02d/%02d",t.tm_mday,t.tm_mon+1,t.tm_year%100);
  font_text(buf, x+ 0, y, c);
}

void drawMiniClock(int ox, int oy){
  char tt = (millis()%10240)/40;
  for(char y=0;y<16;y++)
    for(char x=0;x<32;x++){
      float k = (sint[(x+y-tt)*4&255]+1.f)/2.f; k *= k; k *= k;
      pixel(ox+x,oy+y,k*4.f,255);
     }
  char scry = (millis()>>8)%96;
  drawClock(ox+3, oy+8, timeinfo, 255 );

  font_load(&font_kouzu3x5);
  if (millis()%4000>2000){
    font_text(namaHari[timeinfo.tm_wday], ox+0, oy+0, 255 );
  }else{
    drawDate(ox+0, oy+0, timeinfo, 255 );
  }

  double t = (double)millis()/1000;

  for(int x=0;x<32;x++){
    float k = (sinf((float)x/32*2*PI+(float)(millis()%1000)/1000*2*PI)+1)/2;
    pixel(ox+x,6,k*k*255);
  }
}

void drawMiniClockArabPlain(int ox, int oy, int t){
  if(t%8000<6000){
    font_load(&font_arab5x8);
    if(now_millis%1000<500)
      sprintf(buff,"%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min);
    else
      sprintf(buff,"%02d %02d",timeinfo.tm_hour,timeinfo.tm_min);
    font_text(buff,ox+16-font_width(buff)/2,oy+1,255);

    if (t%8000>2000){
      if (t%8000>4000){
        font_load(&font_arab3x5);
        sprintf(buff,"%02d/%02d/%02d",hijrinfo.day,hijrinfo.month,hijrinfo.year%100);
        font_text(buff, ox+16-font_width(buff)/2, oy+10, 255 );
      }else{
        font_load(&font_arabhari);
        sprintf(buff,"%c",timeinfo.tm_wday+0x20);
        font_text(buff, ox+16-font_width(buff)/2, oy+10, 255 );
      }
    }else{
      font_load(&font_arab3x5);
      sprintf(buff,"%02d    %02d",hijrinfo.day,hijrinfo.year%100);
      font_text(buff, ox+16-font_width(buff)/2, oy+10, 255 );
      font_load(&font_arabbulan);
      sprintf(buff,"%c",hijrinfo.month+0x20-1);
      font_text(buff, ox+16-font_width(buff)/2, oy+10, 255 );
    }
  }else{
    drawClockPlain(3+ox,  1+oy, timeinfo, 255 );
    drawDatePlain( 0+ox, 10+oy, timeinfo, 255 );
  }
}

void drawMiniClockArab(int ox, int oy){
  char buff[20];
  char tt = (millis()%10240)/40;
  for(char y=0;y<16;y++)
    for(char x=0;x<32;x++){
      float k = (sint[(x+y-tt)*4&255]+1.f)/2.f; k *= k; k *= k;
      pixel(ox+x,oy+y,k*4.f,255);
     }
  font_load(&font_arab5x8);
  if(now_millis%1000<500)
    sprintf(buff,"%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min);
  else
    sprintf(buff,"%02d %02d",timeinfo.tm_hour,timeinfo.tm_min);
  font_text(buff,ox+16-font_width(buff)/2,oy+8,255);

  if (millis()%4000>2000){
    font_load(&font_arabhari);
    sprintf(buff,"%c",timeinfo.tm_wday+0x20);
    font_text(buff, ox+16-font_width(buff)/2, oy+0, 255 );
  }else{
    font_load(&font_arab3x5);
    sprintf(buff,"%02d    %02d",hijrinfo.day,hijrinfo.year%100);
    font_text(buff, ox+16-font_width(buff)/2, oy+0, 255 );
    font_load(&font_arabbulan);
    sprintf(buff,"%c",hijrinfo.month-1+0x20);
    font_text(buff, ox+16-font_width(buff)/2, oy+0, 255 );
  }

  double t = (double)millis()/1000;

  for(int x=0;x<32;x++){
    float k = (sinf((float)x/32*2*PI+(float)(millis()%1000)/1000*2*PI)+1)/2;
    pixel(ox+x,6,k*k*255);
  }
}
void drawSmallClock(int x, int y, tm t, int c){
  char buff[32];
  if(now_millis%1000<500)
    sprintf(buff,"%02d:%02d:%02d",t.tm_hour,t.tm_min,t.tm_sec);
  else
    sprintf(buff,"%02d %02d %02d",t.tm_hour,t.tm_min,t.tm_sec);
  font_load(&font_kouzu3x5);
  font_text(buff, x+ 0, y, c);
}

int drawMiniRunning(int ox, int oy, int32_t t, const char *s){
  char tt = (millis()%10240)/40;
  //for(char y=0;y<16;y++)
  //  for(char x=0;x<64;x++){
  //    float k = (sint[(x+y-tt)*4&255]+1.f)/2.f; k *= k;
  //    pixel(ox+x,oy+y,k*4.f);
  //   }

  char scry = (millis()>>8)%96;

  if(millis()%4000 > 2000){
    drawSmallClock(ox,oy,timeinfo,255);
  }else{
    drawDate(ox,oy,timeinfo,255);
  }

  //char* testString = "\"Sungguh, shalat itu adalah kewajiban yang ditentukan waktunya atas orang-orang yang beriman.\" (QS. An Nisa: 103).";
  //struct hijri_date hijr;
  //char hari[16];
  //if(timeinfo.tm_hour<18){
  //  sprintf(hari,"%s",namaHari[(timeinfo.tm_wday)%7]);
  //  hijr = convertToHijri(lastTimePlus);
  //}else{
  //  sprintf(hari,"Malam %s",namaHari[(timeinfo.tm_wday+1)%7]);
  //  hijr = convertToHijri(lastTimePlus+60*60*12);
  //}
  //char testString[128];
  //sprintf(testString,"%s, %d %s %d       Selamat datang di Pondok Pesantren Subulussalam, Karatungan, Limpasu.",
  //sprintf(testString,"%s, %d %s %d       10 Dzulhijjah 1444H - Selamat Hari Raya Idul Adha 1444H              ",
  //sprintf(testString,"%s, %d %s %d - %s, %s -  \"Sebaik-baik manusia adalah yang paling bermanfaat bagi manusia lainnya).\" ***",
  //sprintf(testString,"%s, %d %s %04d - %s %d %s %04dH -  Selamat datang di Pondok Pesantren Subulussalam, Karatungan, Limpasu. ***",
  //namaHari[timeinfo.tm_wday], timeinfo.tm_mday, namaBulan[timeinfo.tm_mon], timeinfo.tm_year+1900,
  //hari,hijr.day,cbulan[hijr.month-1],hijr.year
  //);
  //char* testString = ;

  for(int x=0;x<64;x++){
    float k = (sinf((float)x/64*2*PI+(float)(millis()%2000)/2000*2*PI)+1)/2;
    pixel(ox+x,6,k*k*255);
  }
  int xx = t*40/1000;
  font_load(&font_kouzuMx8,2);
  return (((ox+32)*2-xx)+font_text(s, (ox+32)*2-xx, 8))/2;
}

/////////////////////////////////////
// SCREEN DRAW ROUTINE
/////////////////////////////////////

float clamp(float x){ return max(min(x,1.f),0.f); }
float ease_in(float x){ return 1.f-(1.f-x)*(1.f-x); }
float ease_out(float x){ return x*x; }
float ease_inout(float x){ return 3.f*x*x - 2.f*x*x*x; }

void display_debugmode(int mode, int value){
  update_time();
  display.fillScreen(display.color888(0,0,0));
  char buff[20];
#ifdef MINI_MODE
  sprintf(buff,"%s",WiFi.localIP().toString().c_str());
  buff[7] = 0;
  font_load(&font_kouzu3x5);
  font_text(buff,0+1,1,255);
  font_text(buff+8,32-font_width(buff+8),7,255);
#else
  drawClockPlain(3, 1, timeinfo, 255 );
  drawDatePlain(0, 10, timeinfo, 255 );
  int progressbarwidth = 80;
  int progressbarX = 32+48-progressbarwidth/2;
  int progressbarprog = value*progressbarwidth/100;
  switch(mode){
    case 0: // Title and IP
      sprintf(buff,"LANGGAR AL-MUKARRAM");
      font_load(&font_kouzuNx7);
      font_text(buff,32+48-font_width(buff)/2,1,255);
      sprintf(buff,"%s",WiFi.localIP().toString().c_str());
      font_load(&font_kouzu3x5);
      font_text(buff,32+48-font_width(buff)/2,9,255);
      break;
    case 1: // Starting OTA...
      sprintf(buff,"Starting OTA...");
      font_load(&font_kouzuNx7);
      font_text(buff,32+48-font_width(buff)/2,4,255);
      break;
    case 2: // Downloading %d\%%
      sprintf(buff,"Downloading %d%%",value);
      font_load(&font_kouzuNx7);
      font_text(buff,32+48-font_width(buff)/2,1,255);
      display.drawRect(progressbarX-1, 9,progressbarwidth+2,6,display.color888(255,255,255));
      display.fillRect(progressbarX  ,10,progressbarprog   ,4,display.color888(255,255,255));
      break;
    case 3: // OTA Complete! Restarting
      sprintf(buff,value<100?"OTA Fail!":"OTA Complete!");
      font_load(&font_kouzuNx6);
      font_text(buff,32+48-font_width(buff)/2,1,255);
      sprintf(buff,"restarting...");
      font_load(&font_kouzuNx6);
      font_text(buff,32+48-font_width(buff)/2,9,255);
      break;
    default: break;
    
  }
#endif
  display.setDepth(1);
  display.display(true);
  display.setDepth(0);
}

void display_demo_judul(int x, int y, int t) {
  int s = (millis()*32/1000)%8;
  int allt = t<0?(1.-clamp(-(float)t*0.001-1.))*255:clamp((float)t*0.001-1.)*255;
  for(int k=s;k<96;k+=8){ pixel(x+32+k ,y  ,allt); pixel(x+32+95-k,15  +y,allt); }
  for(int k=s;k<16;k+=8){ pixel(x+32+95,y+k,allt); pixel(x+32     ,15-k+y,allt); }
  //  . . . . . . . . . . . . //
  //  -       LANGGAR       - //
  //  -     AL-MUKARRAM     - //
  //  ' ' ' ' ' ' ' ' ' ' ' ' //
  static char textA[] = "LANGGAR";
  static char textB[] = "AL-MUKARRAM";
  font_load(&font_kouzu5x6);
  font_string_motion(
    font_drawarg(textA,x+32+48-6*sizeof(textA)/2,2,255,255),
    t<0?font_motionarg(
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -ease_out(clamp(t))*16; },
      [](float t)->float{ return -(clamp(t))*255.; },
      [](float t)->float{ return 0.f; },
      0.2f, (float)-t*0.001
    ):font_motionarg(
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -ease_out(clamp(1.-t))*16; },
      [](float t)->float{ return -(clamp(1.-t))*255; },
      [](float t)->float{ return 0.f; },
      0.2f, (float)t*0.001
    )
  );
  font_load(&font_kouzu4x5);
  font_string_motion(
    font_drawarg(textB,x+32+48-5*sizeof(textB)/2,9,255,255),
    t<0?font_motionarg(
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return ease_out(clamp(t))*16; },
      [](float t)->float{ return -(clamp(t))*255; },
      [](float t)->float{ return 0.f; },
      0.1f, (float)-t*0.001-.5f
    ):font_motionarg(
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return ease_out(clamp(1.-t))*16; },
      [](float t)->float{ return -(clamp(1.-t))*255; },
      [](float t)->float{ return 0.f; },
      0.1f, (float)t*0.001-.5f
    )
  );
}

void display_demo_alamat(int x, int y, int t){
  //                          //
  //  LANGGAR AL-MUKARRAM     //
  //  Telaga Sari Birayang    //
  //                          //

  static char textA[] = "LANGGAR AL-MUKARRAM";
  static char textB[] = "Telaga Sari Birayang";
  font_load(&font_kouzuNx7);
  //font_text(textA,x+32+48-font_width(textA)/2,1,255);
  font_string_motion(
    font_drawarg(textA, x+32+48-font_width(textA)/2, y+1, 255),
    t<0?font_motionarg(
      [](float t)->float{ return -ease_out(clamp(t))*128; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -(clamp(t))*255.f; },
      0.05f, (float)-t*0.001-0.f
    ):font_motionarg(
      [](float t)->float{ return ease_out(clamp(1.-t))*128; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -(1.-clamp(t))*255.f; },
      0.05f, (float)t*0.001-0.f
    )
  );

  font_load(&font_kouzuNx5);
  //font_text(textB,x+32+48-font_width(textB)/2,9,255);
  font_string_motion(
    font_drawarg(textB, x+32+48-font_width(textB)/2, y+9, 255),
    t<0?font_motionarg(
      [](float t)->float{ return -ease_out(clamp(t))*128; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -(clamp(t))*255.f; },
      0.05f, (float)-t*0.001-0.5f
    ):font_motionarg(
      [](float t)->float{ return ease_out(clamp(1.-t))*128; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -(1.-clamp(t))*255.f; },
      0.05f, (float)t*0.001-0.5f
    )
  );

  display.fillRect(0,0,32,16,display.color888(0,0,0));
  drawClockPlain(x+3, y+1, timeinfo, 255 );
  drawDatePlain(x+0, y+10, timeinfo, 255 );
}

void display_jadwal_shalat_horizontal(int x, int y){
  // Imsak Subuh Suruq Dhuha Zuhur Ashar Mgrib Isya  
  // xx:xx xx:xx xx:xx xx:xx xx:xx xx:xx xx:xx xx:xx
  char buff[20];
  int dd = timeinfo.tm_mday-1;
  int mm = timeinfo.tm_mon;
  for (int k=0; k<8; k++){
    int w = get_salah_time(mm,dd,k);
    bool blink = millis()%1000<500;
    bool insalahtime = remainder(now_time-salah_time,60*60*24)>60*20;
    int cst = (k==salah_idx)?((blink||insalahtime)*191+64):128;
    int csn = (k==salah_idx)?((blink||insalahtime)*191+64):128;
    sprintf(buff,"%02d:%02d",(w/60/60)%24,(w/60)%60);
    font_load(&font_kouzuNx8); font_text(buff,k*32+3+x,8,csn);
    font_load(&font_kouzuNx6); font_text(prayTimesNameFull[(k%8)],k*32+3+x,0,cst);
  }
}

void draw_hourminsecpanel(){
  // Panel 0: draw hour, min, sec
  bool blink = (now_millis)%1000<500;
  font_load(&font_kouzu6x16); sprintf(buff,"%02d",timeinfo.tm_hour); font_text(buff,0,0);
  font_load(&font_kouzu6x8); sprintf(buff,"%02d",timeinfo.tm_min); font_text(buff,18,0);
  font_load(&font_kouzu6x7); sprintf(buff,"%02d",timeinfo.tm_sec); font_text(buff,18,9);
  font_load(&font_kouzu6x16); font_text(blink?":":" ",12,0);
}

void draw_salahpanel(int t, int f){
  const char* text0 = "Waktu";
  const char* text2 = prayTimesNameShort[salah_idx];
  const char* text1 = "Jelang";
  const char* text3 = prayTimesNameFull[(salah_idx+1)%8];
  char buff[32];
  int a0, a1;
  // Panel 0: draw hour, min, sec
  draw_hourminsecpanel();
  bool blink = (now_millis)%1000<500;
  // Panel 1: current salah time
  float tf = (float)t*0.001;
  float ff = (float)f*0.001;
  a0 = ease_out(1.f-clamp(tf     )+clamp(ff     ))*16;
  a1 = ease_out(1.f-clamp(tf-0.25)+clamp(ff-0.25))*16;
  font_load(&font_kouzuNx6); font_text(text0,32*1+16-font_width(text0)/2,0-a0); // Waktu
  font_load(&font_kouzuNx8); font_text(text2,32*1+16-font_width(text2)/2,8+a1); // Magrb
  // Panel 2: incoming next salah time remaining
  int r = remainder(salah_next-now_time,60*60*24);
  if(r<60*60){
    sprintf(buff,blink?"%02d:%02d":"%02d %02d",r/60,r%60);
  }else{
    sprintf(buff,blink?"%d:%02d:%02d":"%d %02d %02d",r/60/60,(r/60)%60,r%60);
  }
  a0 = ease_out(1.f-clamp(tf-.5 )+clamp(ff-.5 ))*16;
  a1 = ease_out(1.f-clamp(tf-.75)+clamp(ff-.75))*16;
  font_load(&font_kouzuNx6); font_text(text1,32*2+16-font_width(text1)/2,0-a0); // Jelang
  font_load(&font_kouzuNx6); font_text(buff,32*2+16-font_width(buff)/2,9+a1);
  
  // Panel 3: incoming next salah time and name
  sprintf(buff,"%02d:%02d",(salah_next/60/60)%24,(salah_next/60)%60);
  a0 = ease_out(1.f-clamp(tf-1.  )+clamp(ff-1.  ))*16;
  a1 = ease_out(1.f-clamp(tf-1.25)+clamp(ff-1.25))*16;
  font_load(&font_kouzuNx6); font_text(text3,32*3+16-font_width(text3)/2,0-a0);
  font_load(&font_kouzuNx8); font_text(buff,32*3+16-font_width(buff)/2,8+a1);
}

void display_semua_tanggal_waktu(int x, int y, int t){
  char buf[32];

  // tanggal masehi
  font_load(&font_kouzuNx6);
  sprintf(buf,"%d  %s  %04d",timeinfo.tm_mday,namaBulan[timeinfo.tm_mon],timeinfo.tm_year+1900);
  font_string_motion(
    font_drawarg(buf, x+32+48-font_width(buf)/2, y+0, 255),
    t<0?font_motionarg(
      [](float t)->float{ return ease_out(clamp(t))*128; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -(clamp(t))*255.f; },
      0.05f, (float)-t*0.001-0.f
    ):font_motionarg(
      [](float t)->float{ return -ease_out(clamp(1.-t))*128; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -(1.-clamp(t))*255.f; },
      0.05f, (float)t*0.001-0.f
    )
  );

  // tanggal hijriah
  font_load(&font_kouzuNx6);
  sprintf(buf,"%d  %s  %04d",hijrinfo.day,cbulan[hijrinfo.month-1],hijrinfo.year);
  font_string_motion(
    font_drawarg(buf, x+32+48-font_width(buf)/2, y+8, 255),
    t<0?font_motionarg(
      [](float t)->float{ return ease_out(clamp(t))*128; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -(clamp(t))*255.f; },
      0.05f, (float)-t*0.001-0.5f
    ):font_motionarg(
      [](float t)->float{ return -ease_out(clamp(1.-t))*128; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return 0.f; },
      [](float t)->float{ return -(1.-clamp(t))*255.f; },
      0.05f, (float)t*0.001-0.5f
    )
  );

  display.fillRect(0,0,32,16,display.color888(0,0,0));
  font_load(&font_kouzuNx6);
  font_text(namaHari[timeinfo.tm_wday], x+16-font_width(namaHari[timeinfo.tm_wday])/2, y+0, 255 );
  drawClock(x+3, y+7, timeinfo, 255 );
  //sprintf(buf,"%d",timeinfo.tm_mday); // tanggal
  //font_text(buf, x+46-font_width(buf), y+0, 255);
  //sprintf(buf,"%s",namaBulan[timeinfo.tm_mon]); // bulan
  //font_text(buf, x+77-font_width(buf)/2, y+0, 255);
  //sprintf(buf,"%04d",timeinfo.tm_year+1900); // tahun
  //font_text(buf, x+128-font_width(buf), y+0, 255);
  //sprintf(buf,"%d",hijrinfo.day); // tanggal
  //font_text(buf, x+46-font_width(buf), y+8, 255);
  //sprintf(buf,"%s",cbulan[hijrinfo.month-1]); // bulan
  //font_text(buf, x+77-font_width(buf)/2, y+8, 255);
  //sprintf(buf,"%04d",hijrinfo.year); // tahun
  //font_text(buf, x+128-font_width(buf), y+8, 255);
}

int display_running_judul(const char *s, int ox, int oy){
  font_load(&font_segoeui8p);
  font_text(s,ox,oy+1,255);
  return ox+font_width(s);
}

/// MAKE AND DRAW PARTICLES
struct Particles { float x; float y; float z; float w; };
struct Particles particles[64];

float randf(){
  return (float)rand()/(float)(RAND_MAX);
}

void init_particles(){
  for(int i=0;i<64;i++){
    particles[i].x = randf()*128.f;
    particles[i].y = randf()*64.f;
    particles[i].z = randf()*2.f;
    particles[i].w = 1.f+randf()*63.f;
  }
}

void draw_particles(int tt){
  float t = (float)(tt&131071)/10000.f;
  for(int i=0;i<64;i++){
    struct Particles *p = &particles[i];
    float x = fmodf(fmodf( p->x + (cosf(t)-t/3.f)*p->z*100.0f, 128.0f)+128.0f,128.0f);
    float y = fmodf(fmodf( p->y + (sinf(t)+t/3.f)*p->z*100.0f, 32.0f)+32.0f,32.0f);
    float xd = x-floor(x), xs = 1-xd;
    float yd = y-floor(y), ys = 1-yd;
    float c00 = xs*ys;
    float c10 = xd*ys;
    float c01 = xs*yd;
    float c11 = xd*yd;
 
    pixel((int)x+0,(int)y+0,(int)(p->w*c00*c00));
    pixel((int)x+1,(int)y+0,(int)(p->w*c10*c10));
    pixel((int)x+0,(int)y+1,(int)(p->w*c01*c01));
    pixel((int)x+1,(int)y+1,(int)(p->w*c11*c11));
  }
}

//////////////////////////////////////////

int hadist_harian_idx = 0;
const char* hadist_harian[] = {
	"\"Barangsiapa bersuci di rumahnya, kemudian berjalan ke salah satu rumah Allah (masjid) untuk melaksanakan kewajiban yang Allah tetapkan, maka kedua langkahnya yang satu menghapus kesalahan dan satunya lagi meninggikan derajat,\" - ( HR. Imam Muslim )",
	"\"Shalat berjamaah lebih utama 27 derajat dibanding shalat sendirian,\" - ( HR. Imam Muslim )",
	"\"Kebersihan itu sebagian dari iman,\" ( HR. Muslim dan Ahmad )",
	"\"Sesungguhnya Allah Ta'ala itu baik (dan) menyukai kebaikan, bersih (dan) menyukai kebersihan, mulia (dan) menyukai kemuliaan, bagus (dan) menyukai kebagusan. Oleh sebab itu, bersihkanlah lingkunganmu,\" ( HR. At- Tirmidzi )",
	"\"Bahwsanya Rasulullah saw. bersabda, 'Ketika seorang laki-laki sedang berjalan di jalan, ia menemukan dahan berduri, maka ia mengambilnya (karena mengganggunya). Lalu Allah Swt. berterima kasih kepadanya dan mengampuni dosanya',\" ( HR. Bukhari )",
  "\"Tuntutlah ilmu dari buaian sampai ke liang lahat,\" (Ibnu Abdil Barr).",
  "\"Menuntut ilmu wajib bagi tiap muslim dan Muslimah,\" (HR. Ibnu Majah).",
  "\"Sebaik-baik kalian adalah orang yang mempelajari Al-Qur'an dan mengajarkannya,\" (HR. Bukhari no. 5027).",
  "\"Sesungguhnya amal itu tergantung niatnya,\" (HR. Bukhari, no. 1 dan Muslim, no. 1907).",
  "\"Barang siapa tidak menyayangi, tidak akan disayangi,\" (HR Muslim).",
  "\"Setiap kebaikan adalah sedekah,\" (HR Al Bukhari dan Muslim).",
  "\"Salat adalah tiang agama,\" (HR. Tirmidzi dan Ibnu Majah).",
  "\"Agama adalah nasihat,\" (HR. Muslim).",
  "\"Orang yang pintar membaca Al-Qur'an akan tinggal bersama Jibril,\" (HR. Bukhari Muslim).",
  "\"Surga itu ada dibawah telapak kaki Ibu,\" (diriwayatkan oleh An-Nasa'i, Ibnu Majah, Ahmad, dan disahihkan oleh Al-Hakim).",
  "\"Senyum engkau dihadapan saudaramu adalah sedekah,\" (HR at-Tirmidzi (no. 1956), Ibnu Hibban (no. 474 dan 529).",
  "\"Sebagian dari kebaikan Islam, seseorang meninggalkan sesuatu yang tidak berguna,\" (HR. Tirmidzi).",
  "\"Tidak sempurna iman seseorang, sehingga dia mencintai saudaranya seperti mencintai dirinya sendiri,\" (HR. Bukhari, no. 13 dan Muslim, no. 45).",
  "\"Kebanyakan dosa anak-anak Adam itu ada pada lisannya,\" (HR ath-Thabraniy, Abu asy-Syaikh dan Ibnu Asakir)."
};

int quran_shalat_idx = 0;
const char* quran_shalat[] = {
  "\"Tegakkanlah sholat, tunaikanlah zakat, dan rukuklah beserta orang-orang yang rukuk. \"(Q.S Al-Baqarah: 43)",
  "\"Mohonlah pertolongan (kepada Allah) dengan sabar dan sholat. Sesungguhnya (sholat) itu benar-benar berat, kecuali bagi orang-orang yang khusyuk.\" (Q.S Al-Baqarah: 45)",
  "\"Peliharalah semua sholat (fardu) dan salat Wustho. Berdirilah karena Allah (dalam sholat) dengan khusyu.\"  (Q.S Al-Baqarah: 238)",
  "\"Apabila kamu telah menyelesaikan sholat, berdzikirlah kepada Allah (mengingat dan menyebut-Nya), baik ketika kamu berdiri, duduk, maupun berbaring. Apabila kamu telah merasa aman, laksanakanlah sholat itu (dengan sempurna). Sesungguhnya sholat itu merupakan kewajiban yang waktunya telah ditentukan atas orang-orang mukmin.\" (Q.S An-Nisa:103)",
  "\"Dirikanlah sholat pada kedua ujung hari (pagi dan petang) dan pada bagian-bagian malam. Sesungguhnya perbuatan-perbuatan baik menghapus kesalahan-kesalahan. Itu adalah peringatan bagi orang-orang yang selalu mengingat (Allah).\" Hud:114",
  "\"Sungguh, Allah menyukai orang-orang yang tobat dan menyukai orang-orang yang menyucikan diri.\" - (Q.S Al-Baqarah: 222)",
};

int draw_jadwalsholatrunning(int t, const char *s, int e){
  char buff[16];
  char buf2[16];

  int dd = timeinfo.tm_mday-1;
  int mm = timeinfo.tm_mon;
  int cb = max(255-255*e/1000,0);

  font_load(&font_kouzuNx8);
  int ox = 144-(t*30/1000);
  ox += font_string(font_drawarg(s,ox,8,255,cb));


  int k = (t/6000)%8;
  int l = t%6000;
  int w = get_salah_time(mm,dd,k);
  const char *shalat = prayTimesNameFull[(k%8)];
  sprintf(buff,"%02d:%02d",(w/60/60)%24,(w/60)%60);
  sprintf(buf2,"%s %02d:%02d",shalat,(w/60/60)%24,(w/60)%60);
  float lf = (float)l*0.001f;
  int m = (l<5000?ease_out(1.f-clamp(lf)):ease_out(clamp(lf-5.)))*96.f;
  int n = ox<0?ease_inout(clamp((float)-ox/15.f))*5:0;
  if(ox<-20){
    n -= 1;
    font_load(&font_kouzu5x8);
  }else{
    font_load(&font_kouzu5x6);
  }
  font_string(font_drawarg(shalat,32+48-font_width(buf2)/2-m,n,255,cb));
  font_string(font_drawarg(buff,32+48+font_width(buf2)/2-font_width(buff)+m,n,255,cb));

  return ox;
}

void draw_currentsalahtime(int x, int y, int d){
  const char* text0 = "Waktu";
  const char* text2 = prayTimesNameShort[salah_idx];
  font_load(&font_kouzuNx6); font_text(text0,x+16-font_width(text0)/2,0+y, 255, d); // Waktu
  font_load(&font_kouzuNx8); font_text(text2,x+16-font_width(text2)/2,8+y, 255, d); // Magrb
}

int demo_now = -3;
int demo_last = -999;
int demo_arg0, demo_arg1, demo_arg2, demo_arg3;
int demo_arg4, demo_arg5, demo_arg6, demo_arg7;
const char* text_selamat_datang = "Selamat Datang di Langgar Al-Mukarram, Telaga Sari Birayang.";
void draw_main(int32_t t){
  char buff[256];
  char hari[16];
  struct hijri_date hijr;

  uint8_t waktu_highlight = 0;
  bool waktu_tiba = false;
  int tt, x;
  float tf;
  int nowsalahdiff = remainder(now_time-salah_time,60*60*24)*1000+now_millis;
  char salahAlarmDuration[] = { 10, 15, 2, 2, 10, 7, 4, 5, };  // Menit2 alarm bunyi
  if ( ( nowsalahdiff/1000 < 60*30 ) && ( ( 0b11110010 >> salah_idx ) & 1 ) ) {
    if ( nowsalahdiff/1000 < 60*salahAlarmDuration[salah_idx] ) {
      //  Waktu
      // %shalat%
      tf = (float)(nowsalahdiff%2000)*0.001;
      if((nowsalahdiff%2000)<1000){
        waktu_highlight = 255*ease_inout(clamp(tf*2.f));
        setBeep(true);
      }else{
        waktu_highlight = 255*ease_inout(1.-clamp((tf-1.f)*2.f));
        setBeep(false);
      }
      waktu_tiba = true;
    }else{
      setBeep(false);
      // Shalat %shalat%
      tf = (float)(nowsalahdiff%4000)*0.001;
      draw_particles(millis());
      font_load(&font_kouzuNx7);
      sprintf(buff,"Shalat %s",prayTimesNameFull[salah_idx]);
      font_string_motion(
        font_drawarg(buff,32+48-font_width(buff)/2,4,255,255),
        font_motionarg(
          [](float t)->float{ return 0.f; },
          [](float t)->float{ return 0.f; },
          [](float t)->float{ return -192.f*(1.f-cos(t*3.1416f*0.5f))*0.5f; },
          [](float t)->float{ return 0.f; },
          0.1,tf
        )
      );
      drawClockPlain(0+3, 0+1, timeinfo, 64+192*(1.f-cosf((tf+2.f/3.f)*3.1416f))*0.5f);
      drawDatePlain(0+0, 0+10, timeinfo, 64+192*(1.f-cosf((tf+4.f/3.f)*3.1416f))*0.5f);
      return;
    }
  }else if( ( salah_next-now_time < 60*15 ) && ( ( 0b01111001 >> salah_idx ) & 1 ) ){
    // Jelang %waktu_shalat%
    //  %waktu sisa jelang% 
    tf = (float)(nowsalahdiff%8000)*0.001;
    if((nowsalahdiff%8000)>5000){
      waktu_highlight = 255*ease_inout(clamp((tf-5.f)*2.f));
      setBeep(((nowsalahdiff%8000)>7500));
    }else{
      waktu_highlight = 255*ease_inout(1.-clamp((tf)*2.f));
      setBeep(false);
    }
  }else{
    setBeep(false);
  }
  while(1){
    font_drawfunc(&pixel);
    pixelBrightness(255-((waktu_highlight*224)>>8));
    // IMPORTANT REMINDER OF SALAH for 8 MINUTES

    switch(demo_now){
      case -3:
        if (demo_now != demo_last){ demo_last = demo_now; demo_arg0 = millis(); }
        if(millis()-demo_arg0 > 1000){ // last for 5 secs
          if(millis()-demo_arg0 > 2000) demo_now++;
          x = ease_inout(clamp((float)(millis()-demo_arg0-1000)*0.001))*16;
          canvas_pic1b(pic1b_intro,0,-x);
          canvas_pic1b(pic1b_assalam,0,16-x);
        }else{
          canvas_pic1b(pic1b_intro,0,0);
        }
        break;
      case -2:
        if (demo_now != demo_last){ demo_last = demo_now; demo_arg0 = millis(); }
        if(millis()-demo_arg0 > 3000){ // last for 5 secs
          if(millis()-demo_arg0 > 4000) demo_now++;
          x = ease_inout(clamp((float)(millis()-demo_arg0-3000)*0.001))*128;
          canvas_pic1b(pic1b_assalam,x,0);
          canvas_pic1b(pic1b_bismillah,x-128,0);
        }else{
          canvas_pic1b(pic1b_assalam,x,0);
        }
        break;
      case -1:
        if (demo_now != demo_last){ demo_last = demo_now; demo_arg0 = millis(); }
        if(millis()-demo_arg0 > 3000){ // last for 5 secs
          if(millis()-demo_arg0 > 4000) demo_now++;
          x = ease_inout(clamp((float)(millis()-demo_arg0-3000)*0.001))*16;
          draw_particles(millis());
          drawMiniClock(0,0);
          display.fillRect(0,x,128,16+x,display.color888(0,0,0));
          canvas_pic1b(pic1b_bismillah,0,x);
        }else{
          canvas_pic1b(pic1b_bismillah,0,0);
        }
        break;
      case 0:  // JUDUL
        if (demo_now != demo_last){
          demo_last = demo_now;
          demo_arg0 = millis();
        }
        draw_particles(millis());
        drawMiniClock(0,0);
        if(millis()-demo_arg0 > 5000){ // last for 5 secs
          if(millis()-demo_arg0 > 7500) demo_now++;
          display_demo_judul(0,0,-(millis()-demo_arg0-5000)); 
        }else{
          display_demo_judul(0,0,millis()-demo_arg0); 
        }
        break;
      case 1:  // ALAMAT
        if (demo_now != demo_last){
          demo_last = demo_now;
          demo_arg0 = millis();
        }
        draw_particles(millis());

        if(millis()-demo_arg0 > 5000){ // last for 5 secs
          if(millis()-demo_arg0 > 7500) demo_now++;
          display_demo_alamat(0,0,-(millis()-demo_arg0-5000));
        }else{
          display_demo_alamat(0,0,millis()-demo_arg0);
        }
        break;
      case 2:  // SELAMAT DATANG
        if (demo_now != demo_last){
          demo_last = demo_now;
          demo_arg0 = millis();
        }
        tt = millis()-demo_arg0;
        x = 384-tt*100/1000;

        draw_particles(millis());
        x = display_running_judul(text_selamat_datang,x,0);
        drawMiniClock(0,0);


        if(x < 96){ // last till text end
          demo_now++;
        }
        break;
      case 3: // TANGGAL MASEHI HIJRIAH
        if (demo_now != demo_last){
          demo_last = demo_now;
          demo_arg0 = millis();
        }
        draw_particles(millis());

        if(millis()-demo_arg0 > 5000){ // last for 5 secs
          if(millis()-demo_arg0 > 7500) demo_now++;
          display_semua_tanggal_waktu(0,0,-(millis()-demo_arg0-5000));
        }else{
          display_semua_tanggal_waktu(0,0,millis()-demo_arg0);
        }
        break;
      case 4: // JADWAL WAKTU SHALAT
        if (demo_now != demo_last){
          demo_last = demo_now;
          demo_arg0 = millis();
          demo_arg1 = quran_shalat_idx++%(sizeof(quran_shalat)/sizeof(char *));
          demo_arg2 = false;
        }
        draw_particles(millis());
        if(demo_arg2){
          if(millis()-demo_arg2>1000){
            demo_now++;
          }
          draw_jadwalsholatrunning(millis()-demo_arg0,quran_shalat[demo_arg1],millis()-demo_arg2);
        }else{
          x = draw_jadwalsholatrunning(millis()-demo_arg0,quran_shalat[demo_arg1],0);
          if((x<32) && ((millis()-demo_arg0)/6000>=8)){
            demo_arg2 = millis()|1;
            if(x<12){demo_now++;}
          }
        }
        display.fillRect(0,0,32,16,display.color888(0,0,0));
        drawMiniClock(0,0);
        break;
      case 5:
        if (demo_now != demo_last){  demo_last = demo_now;  demo_arg0 = millis(); }
        draw_particles(millis());

        if(millis()-demo_arg0 > 15000){ 
          if(millis()-demo_arg0 > 17500)
            demo_now++;
          draw_salahpanel(millis()-demo_arg0,millis()-demo_arg0-15000); 
        }else{
          draw_salahpanel(millis()-demo_arg0,0); }
        break;
      case 6:
        if (demo_now != demo_last){
          demo_last = demo_now;
          demo_arg0 = millis();
        }
        tt = millis()-demo_arg0;
        x = 128-(tt*16/1000);
        draw_particles(millis());
        display.drawFastHLine(x+32*(salah_idx+0)+0,0,2,display.color888(64,64,64));
        display.drawFastVLine(x+32*(salah_idx+0)+0,0,16,display.color888(64,64,64));
        display.drawFastHLine(x+32*(salah_idx+0)+0,15,2,display.color888(64,64,64));
        display.drawFastHLine(x+32*(salah_idx+1)-2,0,2,display.color888(64,64,64));
        display.drawFastVLine(x+32*(salah_idx+1)-1,0,16,display.color888(64,64,64));
        display.drawFastHLine(x+32*(salah_idx+1)-2,15,2,display.color888(64,64,64));
        display_jadwal_shalat_horizontal(x,0);
        display.fillRect(0,0,32,16,display.color888(0,0,0));
        for(int k=0; k<8; k++){

        }
        drawClockPlain(0+3, 0+1, timeinfo, 255 );
        drawDatePlain(0+0, 0+10, timeinfo, 255 );

        if(x < 32-256){ // last till text end
          demo_now++;
        }
        break;
      case 7:
        if (demo_now != demo_last){
          demo_last = demo_now;
          demo_arg0 = millis();
          demo_arg1 = hadist_harian_idx++%(sizeof(hadist_harian)/sizeof(char *));
        }
        tt = millis()-demo_arg0;
        x = 384-tt*100/1000;
        draw_particles(millis());
        
        if(salah_idx<6){
          sprintf(hari,"%s",namaHari[(timeinfo.tm_wday)%7]);
          hijr = convertToHijri(lastTimePlus);
        }else{
          sprintf(hari,"Malam %s",namaHari[(timeinfo.tm_wday+1)%7]);
          hijr = convertToHijri(lastTimePlus+60*60*24);
        }
        sprintf(buff,"%s, %d %s %d - %s, %d %s %04dH - %s",
        namaHari[timeinfo.tm_wday], timeinfo.tm_mday, namaBulan[timeinfo.tm_mon], timeinfo.tm_year+1900,
        hari,hijr.day,cbulan[hijr.month-1],hijr.year,hadist_harian[demo_arg1]);

        x = display_running_judul(buff,384-tt*100/1000,0);
        drawMiniClock(0,0);


        if(x < 96){ // last till text end
          demo_now++;
        }
        break;
      break;
      default: demo_now = 0; continue;
    }
    if(waktu_highlight>0){
      pixelBrightness(255);
      if(waktu_tiba){
        font_load(&font_kouzu5x8);
        sprintf(buff,"Waktu %s",prayTimesNameFull[salah_idx]);
        font_string(font_drawarg(buff,32+48-font_width(buff)/2-1,4-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2-1,4+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2+1,4-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2+1,4+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2+0,4+0,255,waktu_highlight));
      }else{
        int r = remainder(salah_next-now_time,60*60*24);

        font_load(&font_kouzuNx6);
        sprintf(buff,"Jelang %s",prayTimesNameFull[(salah_idx+1)%8]);
        font_string(font_drawarg(buff,32+48-font_width(buff)/2-1,1+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2-1,1-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2+1,1+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2+1,1-1,  0,waktu_highlight));

        font_load(&font_kouzu5x6);
        sprintf(buff,now_millis%1000<500?"%d:%02d":"%d %02d",r/60,r%60);
        font_string(font_drawarg(buff,32+48-font_width(buff)/2-1,9+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2-1,9-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2+1,9+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,32+48-font_width(buff)/2+1,9-1,  0,waktu_highlight));

        font_load(&font_kouzuNx6);
        sprintf(buff,"Jelang %s",prayTimesNameFull[(salah_idx+1)%8]);
        font_string(font_drawarg(buff,32+48-font_width(buff)/2+0,1+0,255,waktu_highlight));
        font_load(&font_kouzu5x6);
        sprintf(buff,now_millis%1000<500?"%d:%02d":"%d %02d",r/60,r%60);
        font_string(font_drawarg(buff,32+48-font_width(buff)/2+0,9+0,255,waktu_highlight));

      }
    }
    return;
  }
}

void draw_mini(int t, int ox=0, int oy=0){
  font_drawfunc(&pixel);
  char buff[256];
  char hari[16];
  struct hijri_date hijr;

  uint8_t waktu_highlight = 0;
  bool waktu_tiba = false;
  int tt, x, y;
  float tf;
  int nowsalahdiff = remainder(now_time-salah_time,60*60*24)*1000+now_millis;
  char salahAlarmDuration[] = { 10, 15, 2, 2, 5, 5, 2, 2, };  // Menit2 alarm bunyi
  if ( ( nowsalahdiff/1000 < 60*30 ) && ( ( 0b11110010 >> salah_idx ) & 1 ) ) {
    if ( nowsalahdiff/1000 < 60*salahAlarmDuration[salah_idx] ) {
      //  Waktu
      // %shalat%
      tf = (float)(nowsalahdiff%2000)*0.001;
      if((nowsalahdiff%2000)<1000){
        setBeep(true);
        waktu_highlight = 255*ease_inout(clamp(tf*2.f));
      }else{
        setBeep(false);
        waktu_highlight = 255*ease_inout(1.-clamp((tf-1.f)*2.f));
      }
      waktu_tiba = true;
    }else{
      setBeep(false);
      // Shalat %shalat%
      tf = (float)(nowsalahdiff%4000)*0.001;
      draw_particles(millis());
      font_load(&font_kouzuNx7);
      sprintf(buff,"%s",(millis()%5000<2500)?"Shalat":prayTimesNameFull[salah_idx]);
      font_string_motion(
        font_drawarg(buff,ox+16-font_width(buff)/2,oy,255,255),
        font_motionarg(
          [](float t)->float{ return 0.f; },
          [](float t)->float{ return 0.f; },
          [](float t)->float{ return -192.f*(1.f-cos(t*3.1416f*0.5f))*0.5f; },
          [](float t)->float{ return 0.f; },
          0.1,tf
        )
      );
      drawClockPlain(ox+3, oy+8, timeinfo, 64+192*(1.f-cosf((tf+1.f)*3.1416f))*0.5f);
      return;
    }
  }else if( ( salah_next-now_time < 60*15 ) && ( ( 0b01111001 >> salah_idx ) & 1 ) ){
    setBeep(false);
    // Jelang %waktu_shalat%
    //  %waktu sisa jelang% 
    tf = (float)(nowsalahdiff%8000)*0.001;
    if((nowsalahdiff%8000)>5000){
      waktu_highlight = 255*ease_inout(clamp((tf-5.f)*2.f));
    }else{
      waktu_highlight = 255*ease_inout(1.-clamp((tf)*2.f));
    }
  }
  
  while(1){
    font_drawfunc(&pixel);
    pixelBrightness(255-((waktu_highlight*240)>>8));
    display.fillScreen(display.color888(0,0,0));
    switch(demo_now){
      case 0: // JAM TANGGAL
        if (demo_now != demo_last){ 
          demo_last = demo_now; 
          demo_arg0 = millis(); 
        }
        tt = millis()-demo_arg0;
        drawMiniClock(ox,oy);
        if(tt>15000) demo_now++;
        break;
      case 1: // SELAMAT DATANG DI
        if (demo_now != demo_last){
          demo_last = demo_now;
          demo_arg0 = millis();
        }
        tt = millis()-demo_arg0;
        draw_particles(millis());
        //x = drawMiniRunning(0,0,tt,"Selamat datang di Pondok Pesantren Subulussalam, Karatungan, Limpasu.");
        //x = drawMiniRunning(0,0,tt,"Selamat Datang di Panti Asuhan Amanah Birayang");
        x = drawMiniRunning(ox,oy,tt,"Selamat datang di Langgar An-Nuur, Birayang.");
        if(x<=0){ demo_now++; }
        break;
      case 2: // JAM TANGGAL
        if (demo_now != demo_last){ 
          demo_last = demo_now; 
          demo_arg0 = millis(); 
        }
        tt = millis()-demo_arg0;
        draw_particles(millis());
        drawClockPlain(ox+3, oy+1, timeinfo, 255 );
        drawDatePlain(ox+0, oy+10, timeinfo, 255 );
        if(tt>10000) demo_now++;
        break;
      case 3: // QURAN SHALAT
        if (demo_now != demo_last){
          demo_last = demo_now;
          demo_arg0 = millis();
          demo_arg1 = quran_shalat_idx++%(sizeof(quran_shalat)/sizeof(char *));
          demo_arg2 = false;
        }
        tt = millis()-demo_arg0;
        draw_particles(millis());
        x = drawMiniRunning(ox,oy,tt,quran_shalat[demo_arg1]);
        if(x<=0){ demo_now++; }
        break;
      case 4: // JAM TANGGAL
        if (demo_now != demo_last){ 
          demo_last = demo_now; 
          demo_arg0 = millis(); 
        }
        tt = millis()-demo_arg0;
        if(tt<5000)
          drawMiniClockArab(ox,oy);
        else
          drawMiniClock(ox,oy);
        if(tt>10000) demo_now++;
        break;
      case 5:{ // WAKTU SHALAT
        if (demo_now != demo_last){ 
          demo_last = demo_now; 
          demo_arg0 = millis(); 
        }
        tt = millis()-demo_arg0;
        int dd = timeinfo.tm_mday-1;
        int mm = timeinfo.tm_mon;
        int w;
        buff[0] = 0;
        for(int k=0;k<8;k++){
          w = get_salah_time(mm,dd,k);
          sprintf(buff,"%s %s %d:%02d  ",buff,prayTimesNameFull[k],(w/60)/60,(w/60)%60);
        }
        x = drawMiniRunning(ox,oy,tt,buff);
        if(x<=0){ demo_now++; }
        break;
      }
      case 6:  // JAM TANGGAL
        if (demo_now != demo_last){ 
          demo_last = demo_now; 
          demo_arg0 = millis(); 
        }
        tt = millis()-demo_arg0;
        drawMiniClockArabPlain(ox,oy,tt);
        if(tt>18000) demo_now++;
        break;
      case 7: // Hadits harian
        if (demo_now != demo_last){ 
          demo_last = demo_now; 
          demo_arg0 = millis();
          demo_arg1 = hadist_harian_idx++%(sizeof(hadist_harian)/sizeof(char *));
        }
        tt = millis()-demo_arg0;
        draw_particles(millis());
        
        if(salah_idx<6){
          sprintf(hari,"%s",namaHari[(timeinfo.tm_wday)%7]);
          hijr = convertToHijri(lastTimePlus);
        }else{
          sprintf(hari,"Malam %s",namaHari[(timeinfo.tm_wday+1)%7]);
          hijr = convertToHijri(lastTimePlus+60*60*24);
        }
        sprintf(buff,"%s, %d %s %d - %s, %d %s %04dH - %s",
        namaHari[timeinfo.tm_wday], timeinfo.tm_mday, namaBulan[timeinfo.tm_mon], timeinfo.tm_year+1900,
        hari,hijr.day,cbulan[hijr.month-1],hijr.year,hadist_harian[demo_arg1]);

        tt = millis()-demo_arg0;
        x = drawMiniRunning(ox,oy,tt,buff);
        if(x<=0) demo_now++;
        break;
      default:
        demo_now = 0;
        continue;
    }
    x = ox+16;
    y = 0;
    if(waktu_highlight>0){
      pixelBrightness(255);
      if(waktu_tiba){
        font_load(&font_kouzuNx6);
        sprintf(buff,"Waktu");
        font_string(font_drawarg(buff,x-font_width(buff)/2-1,y+1-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2-1,y+1+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+1,y+1-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+1,y+1+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+0,y+1+0,255,waktu_highlight));
        sprintf(buff,"%s",prayTimesNameFull[salah_idx]);
        font_string(font_drawarg(buff,x-font_width(buff)/2-1,y+8-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2-1,y+8+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+1,y+8-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+1,y+8+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+0,y+8+0,255,waktu_highlight));
      }else{
        int r = remainder(salah_next-now_time,60*60*24);

        font_load(&font_kouzuNx6);
        sprintf(buff,"%s",millis()%2000<1000?"Jelang":prayTimesNameFull[(salah_idx+1)%8]);
        font_string(font_drawarg(buff,x-font_width(buff)/2-1,y+1+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2-1,y+1-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+1,y+1+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+1,y+1-1,  0,waktu_highlight));

        font_load(&font_kouzu5x6);
        sprintf(buff,now_millis%1000<500?"%d:%02d":"%d %02d",r/60,r%60);
        font_string(font_drawarg(buff,x-font_width(buff)/2-1,y+9+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2-1,y+9-1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+1,y+9+1,  0,waktu_highlight));
        font_string(font_drawarg(buff,x-font_width(buff)/2+1,y+9-1,  0,waktu_highlight));

        font_load(&font_kouzuNx6);
        sprintf(buff,"%s",millis()%2000<1000?"Jelang":prayTimesNameFull[(salah_idx+1)%8]);
        font_string(font_drawarg(buff,x-font_width(buff)/2+0,y+1+0,255,waktu_highlight));
        font_load(&font_kouzu5x6);
        sprintf(buff,now_millis%1000<500?"%d:%02d":"%d %02d",r/60,r%60);
        font_string(font_drawarg(buff,x-font_width(buff)/2+0,y+9+0,255,waktu_highlight));

      }
    }
    return;
  }
}

uint32_t frames, frameslast;
void monitor_fps(){
  int diff = millis() - lastMillis2;
  if( diff > 1000 ){
    lastMillis2 += 1000;
    Serial.print("FPS: ");
    Serial.println(frames-frameslast);
    frameslast = frames;
  }
  frames++;
}
/*
void drawpics(){
  display.fillScreen(display.color888(0,0,0));
  int t = (millis()/4000)%9;
  for(int y=0; y<16; y++){
    for(int x=0; x<128; x++){
      uint8_t c = pgm_read_byte(pics[t]+(y*128+x));
      uint8_t r = x<32?0:c;
      uint8_t g = x<32?c:c;
      uint8_t b = x<32?0:c;
      display.drawPixel(x,y,display.color888(r,g,b));
    }
  }
  for(int x=frames&31;x<128;x+=32){
    for(int y=16;y<32;y++){
      display.drawPixel(x,y,display.color888(255,0,0));
      display.drawPixel(127-x,y,display.color888(0,0,255));
    }
  }
}
*/

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[26];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}

// END RTC STUFFS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200); Serial.println("Started");
  pinMode(AUD,OUTPUT); digitalWrite(AUD,LOW);
  setBeep(LOW);
  for(int x=0;x<256;x++) sint[x] = sinf((float)x/256.f*2.f*PI);

  display.initdisplay();
  display.setRotation(0);
#ifdef MINI_MODE
  display.fillScreen(display.color888(0,0,0));
  char textA[] = "Mohon";
  char textB[]  = "tunggu";
  font_load(&font_kouzuNx5);
  font_text(textA,0+16-font_width(textA)/2,1);
  font_text(textB,0+16-font_width(textB)/2,9);
#else
  canvas_pic1b<uint16_t>(pic1b_intro);
#endif
  display.display(true);
  Serial.println(F("Display Initialized"));
  
  // RTC DETECT
  RtcDateTime now;
  if(scanforrtc()){
    now = Rtc.GetDateTime();
    lastTime = now.Unix64Time();
    use_rtc = true;
    Serial.println(F("RTC is indeed found!"));
  }else{
    pinMode(D1,OUTPUT);
    pinMode(D2,OUTPUT);
    use_rtc = false;
    Serial.println(F("RTC is not found, syncing thou WiFi instead"));
    lastTime = syncNTP(&interruptDisplayRowFirst);
  }

  // OTA DETECT
  if(OTA_connect(&interruptDisplayRowFirst)){
    lastTime = getEpochNTP();
    lastMillis = millis();
    lastMillis1= lastMillis;
    lastMillis2= lastMillis;

    RtcDateTime epoch; epoch.InitWithUnix64Time(lastTime);
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    if(use_rtc){
      if (compiled < epoch){
        Serial.print(F("Attempting to set the RTC time to "));
        printDateTime(epoch);
        Serial.println();
        if (!Rtc.GetIsRunning()){
          Serial.println(F("RTC wasnt running, running it..."));
          Rtc.SetIsRunning(true);
        }
        Rtc.SetDateTime(epoch);

        now = Rtc.GetDateTime();
        Serial.print(F("Time on the RTC: "));
        printDateTime(now);
        Serial.println();
        if(Rtc.IsDateTimeValid()){
          Serial.print(F("Keeping it. "));
          Serial.println();
          use_rtc = true;
        }else{
          Serial.print(F("Something's wrong, duh! Abandoning RTC..."));
          use_rtc = false;
          pinMode(D1,OUTPUT);
          pinMode(D2,OUTPUT);
        }
      }
    }
    int inStartOTA = false;
    int inProgressOTA = false;
    int inCompleteOTA = false;
    int inProgressPercent = 99;
    ElegantOTA_config(
      [&inStartOTA](){
        inStartOTA = 1;
      }, // onStart
      [&inProgressOTA,&inProgressPercent](size_t a, size_t b){
        int newProgress = a*100/b;
        if((inProgressPercent%10)>(newProgress%10))
          inProgressOTA = 1;
        inProgressPercent = newProgress;
      }, // onProgress
      [&inCompleteOTA,&inProgressPercent](bool complete){
        inCompleteOTA = 1;
        if(complete) inProgressPercent = 100;
      }  // onEnd
    );
    while(1){
      // WiFi download mode, while updating display
      if(inStartOTA){
        if(inProgressOTA){
          if(inCompleteOTA){
            if(inCompleteOTA==1){
              display_debugmode(3,inProgressPercent);
              inCompleteOTA++;
              lastMillis2 += millis();
            }else if (millis() - lastMillis2 > 500){
              ESP.restart();
            }
          }else if(inProgressOTA==1){
            display_debugmode(2,inProgressPercent);
            inProgressOTA++;
          }
        }else if(inStartOTA==1){
          display_debugmode(1,0);
          inStartOTA++;
        }
      }else if(-lastMillis2+millis()>=500){
        display_debugmode(0,0);
        lastMillis2 += 500;
      }
      if(-lastMillis1+millis()>0){
        interruptDisplayRowFirst();
        lastMillis1=millis();
        if ( WiFi.status() != WL_CONNECTED )
          break;
      }
      OTA_handle(nullptr);
      //loopaudio();
      esp_yield();
      //delay(1);
    }
  }
  lastMillis = millis(); //unchanging
  lastMillis1 = lastMillis;
  lastMicros1 = micros();
  lastMillis2 = lastMillis;

  display.println(WiFi.localIP());
  display.display(true);
  
  //timer1_enable(TIM_DIV1,TIM_EDGE,TIM_LOOP);
  //timer1_attachInterrupt(nothing);
  //timer1_write(160000); // 40fps, 640fps
  display.startdisplay();
  init_particles();
}

void drawDemo0(char offset){
  for(char y=0;y<16;y++)
    for(char x=0;x<32;x++)
      pixel(x,y,x+offset+(y>>1)*32);
}

void drawDemo1(int offset){
  for(char y=0;y<16;y+=4)
    for(char x=0;x<32;x++)
      pixel(x,y+((offset>>8)&3),offset&255);
}

void display_transition(){
  uint32_t tt = (millis()-lastMillis);
#ifdef MINI_MODE
  draw_mini(tt,32);
  draw_mini(tt,0);
#else
  draw_main(tt);
#endif
}

void loop() {
  update_time();
  monitor_fps();
  display.fillScreen(display.color888(0,0,0));
  //;
  //display.display();
  //display_transition();
  //ArduinoOTA.handle();
  switch(demoselect){
    case 0x00: display_transition(); break;        
    case 0x01: draw_particles(millis()); break;
    case 0x02: drawDemo1(frames); break;
    case 0x03: drawDemo1(-frames); break;
    case 0x04: drawDemo0(0); break;
    case 0x05: drawDemo0(16); break;
    case 0x06: drawDemo0(frames); break;
    case 0x07: drawDemo0(-frames); break;
    break;
  }
  display.display(true);
  TaskMonitorCode(NULL);
}
