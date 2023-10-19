#pragma once

#ifdef ESP32
  #define GPPOR (*(uint32_t *)0x3ff44004)
  #define GPPOS (*(uint32_t *)0x3ff44008)
  #define GPPOC (*(uint32_t *)0x3ff4400C)
#endif
#ifdef ESP8266
  #define GPPOR (*(uint32_t *)0x60000300)
  #define GPPOS (*(uint32_t *)0x60000304)
  #define GPPOC (*(uint32_t *)0x60000308)
#endif

class ParallelPin{
  private:
    uint32_t lookup[256];
    char pins;

  public:
    ParallelPin(
      char p0 = -1, char p1 = -1,
      char p2 = -1, char p3 = -1,
      char p4 = -1, char p5 = -1,
      char p6 = -1, char p7 = -1
    ){
      for(int k=0; k<256; k++){
        lookup[k] =
          (k>>0&(p0!=-1))<<p0|
          (k>>1&(p1!=-1))<<p1|
          (k>>2&(p2!=-1))<<p2|
          (k>>3&(p3!=-1))<<p3|
          (k>>4&(p4!=-1))<<p4|
          (k>>5&(p5!=-1))<<p5|
          (k>>6&(p6!=-1))<<p6|
          (k>>7&(p7!=-1))<<p7;
      }
    }
    
    void setPins(char k){
      //GPPOC = lookup[255];
      //GPPOS = lookup[k];
      GPPOR = GPPOR & ~lookup[255] | lookup[k];
      pins = k;
    }
    
    uint8_t getPins(){
      return pins;
    }
};