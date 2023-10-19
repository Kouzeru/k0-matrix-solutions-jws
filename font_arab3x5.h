const uint8_t font_arab3x5_data[] PROGMEM = {
0b00000000, // ................// #0
0b00000000, // ................// #1
0b00000000, // ................// #2
0b00000000, // ................// #3

0b00000000, // ................// #4
0b00000000, // ................// #5

0b00000000, // ................// #6
0b00000000, // ................// #7

0b00000000, // ................// #8
0b00000000, // ................// #9

0b00000000, // ................// #10
0b00000000, // ................// #11

0b00000000, // ................// #12
0b00000000, // ................// #13

0b00000000, // ................// #14
0b00000000, // ................// #15

0b00000000, // ................// #16
0b00000000, // ................// #17

0b00000000, // ................// #18
0b00000000, // ................// #19

0b00000000, // ................// #20
0b00000000, // ................// #21

0b00000000, // ................// #22
0b00000000, // ................// #23

0b00000000, // ................// #24
0b00000000, // ................// #25

0b00000000, // ................// #26
0b00000000, // ................// #27

0b00000000, // ................// #28
0b00000000, // ................// #29

0b00000000, // ................// #30
0b00000000, // ................// #31

0b00001000, // ........##......// #32
0b00000100, // ..........##....// #33
0b00000010, // ............##..// #34
0b00000000, // ................// #35

0b00000000, // ................// #36
0b00001100, // ........####....// #37
0b00001100, // ........####....// #38
0b00000000, // ................// #39

0b00000000, // ................// #40
0b00011111, // ......##########// #41
0b00000000, // ................// #42
0b00000000, // ................// #43

0b00011111, // ......##########// #44
0b00000001, // ..............##// #45
0b00000001, // ..............##// #46
0b00000000, // ................// #47

0b00011110, // ......########..// #48
0b00000011, // ............####// #49
0b00000001, // ..............##// #50
0b00000000, // ................// #51

0b00011100, // ......######....// #52
0b00010111, // ......##..######// #53
0b00010101, // ......##..##..##// #54
0b00000000, // ................// #55

0b00011110, // ......########..// #56
0b00010011, // ......##....####// #57
0b00011110, // ......########..// #58
0b00000000, // ................// #59

0b00000001, // ..............##// #60
0b00000001, // ..............##// #61
0b00011111, // ......##########// #62
0b00000000, // ................// #63

0b00001111, // ........########// #64
0b00011000, // ......####......// #65
0b00001111, // ........########// #66
0b00000000, // ................// #67

0b00011110, // ......########..// #68
0b00000011, // ............####// #69
0b00011110, // ......########..// #70
0b00000000, // ................// #71

0b00000111, // ..........######// #72
0b00000101, // ..........##..##// #73
0b00011111, // ......##########// #74
0b00000000, // ................// #75

0b00000000, // ................// #76
0b00001010, // ........##..##..// #77
0b00000000, // ................// #78
0b00000000, // ................// #79

};

const uint16_t font_arab3x5_pos[] PROGMEM = {
0, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 
};

struct font_1b8p font_arab3x5 = {
  font_arab3x5_data,
  font_arab3x5_pos,
};
