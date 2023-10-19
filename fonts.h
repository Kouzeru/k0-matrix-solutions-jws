struct font_1b8p { const uint8_t *data; const uint16_t *pos; };
struct font_1b16p { const uint16_t *data; const uint16_t *pos; };
struct font_1b32p { const uint32_t *data; const uint16_t *pos; };
struct font_2b8p { const uint16_t *data; const uint16_t *pos; };
struct font_2b16p { const uint32_t *data; const uint16_t *pos; };
struct font_2b32p { const uint64_t *data; const uint16_t *pos; };

#include "font_segoeui8p.h"
#include "font_kouzu3x5.h"
#include "font_kouzu4x5.h"
#include "font_kouzu5x6.h"
#include "font_kouzu5x8.h"
#include "font_kouzu6x7.h"
#include "font_kouzu6x8.h"
#include "font_kouzu6x16.h"
#include "font_kouzuNx5.h"
#include "font_kouzuNx6.h"
#include "font_kouzuNx7.h"
#include "font_kouzuNx8.h"
#include "font_kouzuMx8.h"
#include "font_arab3x5.h"
#include "font_arab5x8.h"
#include "font_arabhari.h"
#include "font_arabbulan.h"

struct font_1b8p font_current;
int font_bytesize = 1;
int font_bitdepth = 1;
int font_bitmask = 0b1;
int font_unitvalue = 0xFF;
int font_xstrip = 1;

void font_load(const struct font_1b8p *font, int xstrip=1){
  font_current = *((struct font_1b8p*)font);
  font_bytesize = 1;
  font_bitdepth = 1;
  font_bitmask = 0b1;
  font_unitvalue = 0xFF;
  font_xstrip = xstrip;
}
void font_load(const struct font_1b16p *font, int xstrip=1){
  font_current = *((struct font_1b8p*)font);
  font_bytesize = 2;
  font_bitdepth = 1;
  font_bitmask = 0b1;
  font_unitvalue = 0xFF;
  font_xstrip = xstrip;
}
void font_load(const struct font_2b16p *font, int xstrip=3){
  font_current = *((struct font_1b8p*)font);
  font_bytesize = 4;
  font_bitdepth = 2;
  font_bitmask = 0b11;
  font_unitvalue = 0x55;
  font_xstrip = xstrip;
}

void pixel(int x, int y, int c, int d); // set Pixel
void (*font_pixel)(int, int, int, int) = &pixel; 

int font_draw(char ch, int ox, int oy, int c=255, int d=255){
  if( ch >= 32 && ch < 127 ){
    int p = pgm_read_word(&font_current.pos[ch-32]);
    int l = pgm_read_word(&font_current.pos[ch-31])-p;
    if((ox + l)/font_xstrip < 0) return l;
    if((ox + 0)/font_xstrip >= 128) return l;
    for(int j=0, m=p*font_bytesize; j<l; j++, ox++){
      if( ( ox % font_xstrip ) == 0 ){
        for(int z=0, n=0; z<font_bytesize; z++){
          int b = pgm_read_byte(&font_current.data[m++]);
          for(int k=0; k<8; k+=font_bitdepth, n++){
            int w = ( b >> k ) & font_bitmask;
            if( w ){
              w = font_unitvalue * w * (d+1) >> 8;
              (*font_pixel)(ox/font_xstrip,oy+n,c,w);
              //pixel(ox/font_xstrip,oy+n,c,w);
            }
          }
        }
      }
      else {
        m += font_bytesize;
      } 
    }
    return l;
  }else{
    return 0;
  }
}

void font_drawfunc(void (*func)(int,int,int,int)){
  font_pixel = func; 
}

int font_text(const char *s, int ox, int oy, int c=255, int d=255){
  int x = 0;
  for(int i=0, ch; ch = s[i]; i++){
    x += font_draw ( ch, ox+x, oy, c, d); 
    if( ( ox + x ) / font_xstrip >= 128) break;
  }
  return x;
}

struct font_drawargs{
  const char *string;
  int offset_x;
  int offset_y;
  int color_v;
  int blend_v;
};

struct font_drawargs font_drawarg(
  const char *string,
  int offset_x = 0,
  int offset_y = 0,
  int color_v = 255,
  int blend_v = 255
  ){
  struct font_drawargs args = { 
    string, offset_x, offset_y, color_v, blend_v
  }; 
  return args;
}

float font_zerof(float t){ return 0; }

struct font_motionargs {
  float (*t_func_x)(float); // easing function for x
  float (*t_func_y)(float); // easing function for y
  float (*t_func_c)(float); // easing function for c
  float (*t_func_d)(float); // easing function for d
  float t_interval; // interval interletters
  float t_start; // start timer
};

struct font_motionargs font_motionarg(
  float (*t_func_x)(float) = font_zerof,
  float (*t_func_y)(float) = font_zerof,
  float (*t_func_c)(float) = font_zerof,
  float (*t_func_d)(float) = font_zerof,
  float t_interval = 0.f,
  float t_start = 1.f
){
  struct font_motionargs args = {
    t_func_x, t_func_y, t_func_c, t_func_d, t_interval, t_start
  };
  return args;
}


int font_string(struct font_drawargs a){
  return font_text(a.string, a.offset_x, a.offset_y, a.color_v, a.blend_v);
}

int font_string_motion(struct font_drawargs a, struct font_motionargs b){
  int x = 0;
  for(int i=0, ch; ch = a.string[i]; i++){
    float ts = b.t_start-b.t_interval*i;
    int ox = a.offset_x + b.t_func_x(ts) + x;
    int oy = a.offset_y + b.t_func_y(ts);
    int c  = a.color_v + b.t_func_c(ts);
    int d  = a.blend_v + b.t_func_d(ts);
    x += font_draw ( ch, ox, oy, c, d);
  }
  return x;
}

int font_width(const char *s){
  int x = 0;
  for(int i=0, ch; ch = s[i]; i++){
    if( ch >= 32 && ch < 127 ){
      int p = pgm_read_word(&font_current.pos[ch-32]);
      x += pgm_read_word(&font_current.pos[ch-31])-p;
    }
  }
  return x;
}