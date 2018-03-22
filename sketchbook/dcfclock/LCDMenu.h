#include <LiquidCrystal_I2C.h>

typedef struct LCDField_t {
  uint8_t x;
  uint8_t y;
  uint8_t len;
} LCDField ;


class LCDMenu : public LiquidCrystal_I2C {

 public:
 LCDMenu(const uint8_t& a, int b, int c) : LiquidCrystal_I2C(a,b,c) {};


 int Menpos(struct LCDField_t f) {
   LiquidCrystal_I2C::setCursor(f.x,f.y);
   return f.len;
 }

 void printFilled(LCDField f, char *s) {
   LiquidCrystal_I2C::setCursor(f.x,f.y);
   LiquidCrystal_I2C::print(s);
   for (int i=strlen(s); i < f.len-1; i++ ) {
       space();
   }
 }

 void blank(LCDField f) {
   LiquidCrystal_I2C::setCursor(f.x,f.y);
   for (int i = 0; i < f.len; i++)
      space();
 }

/* void Print(LCDField f, char *s) {
   LiquidCrystal_I2C::setCursor(f.x,f.y);
   LiquidCrystal_I2C::print(s);
 }*/


 void Print(LCDField f, String s) {
   LiquidCrystal_I2C::setCursor(f.x,f.y);
   LiquidCrystal_I2C::print(s);
 }

 void space() { LiquidCrystal_I2C::print(' '); }
 void zero() { LiquidCrystal_I2C::print('0'); }
 void colon() { LiquidCrystal_I2C::print(':'); }
  
};
