//cc-mode
// Juergens funkuhr module
/* progess notes: */
//    removed infrared parts //1/
//    no use of Timezone namespace //2/
//    no need to mae noise (tick) //3/
//    led brightness //4/
//    quartz-motor--control //5/
//    summer/winter motor control //6/
//    slow/fast serial print removed //7/
//    new code //new/
//    led30 ? //8/
/* end progress notes */
// based on:
//  Digital Dial Clock
//  http://www.brettoliver.org.uk
//
//  Copyright 2014 Brett Oliver
//  v13 uses Udo Klein's v3 DCF77 Library
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see http://www.gnu.org/licenses/
//
//
// Based on the DCF77 library by Udo Klein
// http://blog.blinkenlight.net/experiments/dcf77/dcf77-library/
// http://blog.blinkenlight.net/experiments/dcf77/simple-clock/

// The version string MUST fit in the box
//               12345678911234567890
//              +--------------------+
//              !                    !
#define VERSION "(c) 180322 dg9dw    "
//              !                    !
//              +--------------------+


// modules inside
# define USERTC 1

// to be most flexible we define a protocol version
//   a client should tolerate minor changes
//   a client may abort on major changes
#define MAJOR 2
#define MINOR 1

#include <dcf77.h>
#include <Wire.h>
//#include <LiquidCrystal_I2C.h>
#include <LCDMenu.h>

#ifdef USERTC
# include "RTClib.h"
#endif

using namespace Internal; //v3

#define GERMAN

#ifndef GERMAN
#  define ENGLISH
#  define DATESEP '/'
#else
#  define DATESEP '.'
#endif
// **********************
//const uint8_t lcd_i2c_id              = 0x3f;
const uint8_t lcd_i2c_id              = 0x27;
const uint8_t dcf77_analog_sample_pin = A0;
const uint8_t dcf77_sample_pin        = A0;
const uint8_t dcf77_inverted_samples  = 1;
const uint8_t dcf77_analog_samples    = 1;
const uint8_t dcf77_monitor_led       = 8;
const int     ldr                     = A2;
const int     ldrRef                  = A1;
// **********************
// set the LCD address to 0x3f for a 20 chars 4 line display
LCDMenu lcd(lcd_i2c_id, 20, 4); // Set the LCD I2C address

#ifdef USERTC
RTC_DS1307 rtc;
boolean rtcUpdateNeeded = true;
boolean rtcOK = false;
#endif

boolean fullDate;

uint8_t ledpin(const uint8_t led) { //v3
  return led; //v3
} //v3

// *********************

int extracount  = 0; // where quartz seconds needs to miss a pulse
int hourextra   = 0; // hour last miss pulse variable
int dayextra    = 0; // day last miss pulse variable
int monthextra  = 0; // month last miss pulse variable
int minuteextra = 0; // minute last miss pulse variable
int secondextra = 0; // second last miss pulse variable
int yearextra   = 0;
int secondsnow = 0; // previous second
int yearmiss   = 0;
int daymiss    = 0; // day last extra pulse variable
int monthmiss  = 0; // month last extra pulse variable
int secsmiss   = 0; // works out is seconds need to an extra pulse or miss a pulse
int misscount  = 0; // 1 ok 0 needs to miss a pulse 1 needs an extra pulse
int hourmiss   = 0; // hour last extra pulse variable
int minutemiss = 0; // minute last extra pulse variable
int           secondmiss = 0; // second last extra pulse variable
int           intensity = 0; // ldr readout turniing lcd on or off
unsigned int ldrValue;      // LDR value
unsigned int ldrRefValue;

# define LDRHYS   25  // hysteresis for display on/off
boolean ledOn = true;


int signalQual = 0; // computed once per minute and indicates how well
//                     the received signal matches the locally synthesized reference signal max 50
int monthval = 0;
int dayval   = 0;
int hourval  = 0;
int minsval  = 0;
int secsval  = 0;
int years    = 0;
int months   = 0;
int days     = 0;
int hours    = 0;
int minutes  = 0;
int seconds  = 0;

#ifdef GERMAN
// lcd german specials ä=\xe1 ö=\xef ü=\xf5 ß=\e2
static char monthnames[][4] = {
  "Jan",
  "Feb",
  "M\xe1r",
  "Apr",
  "Mai",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Okt",
  "Nov",
  "Dez"
};
static char wdaynames[][11] = {
  //  "MONTAG",  "DIENSTAG","MITTWOCH","DONNERSTAG","FREITAG", "SAMSTAG","SONNTAG"
  "Montag",
  "Dienstag",
  "Mittwoch",
  "Donnerstag",
  "Freitag",
  "Samstag",
  "Sonntag"
};
#else
static char wdaynames[][10] = {
  "Monday",
  "Tuesday",
  "Wednesday", "Thursday",
  "Friday", "Saturday",
  "Sunday"
};
#endif

const LCDField line0     = {  0, 0, 20};
const LCDField timePos   = {  0, 0,  9};
const LCDField secPos    = {  6, 0,  3};
const LCDField datePos   = {  9, 0,  8};
const LCDField wdayPos   = {  0, 1, 13};
const LCDField TZPos     = { 16, 1,  4};
const LCDField StatusPos = {  0, 2, 20};
const LCDField OMsgPos   = {  0, 3, 16};
const LCDField DcfStPos  = { 13, 3,  7};

uint8_t sample_input_pin() {
  const uint8_t sampled_data =
#if defined(__AVR__)
    dcf77_inverted_samples ^ (dcf77_analog_samples ? (analogRead(dcf77_analog_sample_pin) > 200)
                              : digitalRead(dcf77_sample_pin));
#else
    dcf77_inverted_samples ^ digitalRead(dcf77_sample_pin);
#endif

  digitalWrite(ledpin(dcf77_monitor_led), ledOn ? sampled_data : 0);
  return sampled_data;
}

void SerialSpace() {
  Serial.print(' ');
}

void SerialSemicolon() {
  Serial.print(';');
}

void paddedPrint(BCD::bcd_t n) {
  Serial.print(n.digit.hi);
  Serial.print(n.digit.lo);
}

void LCDpaddedPrint(BCD::bcd_t n) {
  lcd.print(n.digit.hi);
  lcd.print(n.digit.lo);
}

void PrintSecondTelegram(Clock::time_t n) {
    int secondsToUTC;
    DateTime dt (BCD::bcd_to_int(n.year),
                 BCD::bcd_to_int(n.month),
                 BCD::bcd_to_int(n.day),
                 BCD::bcd_to_int(n.hour),
                 BCD::bcd_to_int(n.minute),
                 BCD::bcd_to_int(n.second));
    secondsToUTC = n.uses_summertime ? 7200 : 3600;
    Serial.print('d');
    Serial.println(dt.unixtime() - secondsToUTC );
}

// **********************
#ifdef GERMAN
char * getMonthName(int mon) {
  return (monthnames[mon - 1]);
}
//new/
#endif

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

void LCDshowWeekday(int d) {
  lcd.printFilled(wdayPos,wdaynames[d - 1]);
}

void segIntensity(int intensity) {
  if (intensity < 1000) lcd.space();
  if (intensity < 100)  lcd.space();
  if (intensity < 10)   lcd.zero();
  lcd.print(intensity);
}

void leadzero(int val) {
  if ( val < 10)
    lcd.print('0');
  lcd.print(val);
}

void intensityValue() {
  intensity = ldrValue;
  if ( ldrValue > ldrRefValue ) {
    lcd.backlight();
    ledOn = true;
  } else {
    if ( (ldrValue + LDRHYS) < ldrRefValue ) {
      lcd.noBacklight();
      ledOn = false;
    }
  }
}

void signalmatch() {
  signalQual = DCF77_Clock::get_prediction_match();

  if ( signalQual == 255 || signalQual == 0 ) {
    signalQual = 0;
  } else {
    signalQual <<= 1 ;
  }
  lcd.Menpos(OMsgPos);
#ifdef GERMAN
  lcd.Print(OMsgPos,F("Signal: "));
#else
  lcd.Print(PMsgPos,F("Sig Match "));
#endif
  lcd.print(signalQual);
  lcd.print('%');
#ifdef GERMAN
  lcd.print(F("   "));
#endif
}

void precision() {
#ifdef GERMAN
  lcd.Print(OMsgPos,F("Akuranz "));
#else
  lcd.Print(OMagPos,F("Acuracy "));
#endif
  lcd.print(DCF77_Frequency_Control::get_confirmed_precision());
  lcd.print(F(" Hz"));
}

void freqadj() {
  lcd.Menpos(OMsgPos);
  lcd.print(16000000L - Internal::Generic_1_kHz_Generator::read_adjustment());
  lcd.print(F(" Hz "));
}

void PrintRTCTelegram() {
  DateTime n = rtc.now();
  Serial.print('r');
  Serial.println(n.unixtime());
}

void lcdRTCTime() {
  DateTime n = rtc.now();

  lcd.blank(StatusPos);
  lcd.Menpos(StatusPos);
  if ( n.hour() < 10 ) lcd.zero();
  lcd.print(n.hour());
  lcd.print(':');
  if ( n.minute() < 10 ) lcd.zero();
  lcd.print(n.minute());
  lcd.print(':');
  if ( n.second() < 10 ) lcd.zero();
  lcd.print(n.second());
  lcd.space();
  if ( n.day() < 10 ) lcd.space();
  lcd.print(n.day());
  lcd.print('.');
  if ( n.month() < 10 ) lcd.zero();
  lcd.print(n.month());
  lcd.print('.');
  lcd.space();
  lcd.print(n.year());
}

void SyncDCF() {
  // Wait till clock is synced, depending on the signal quality this may take
  // rather long. About 5 minutes with a good signal, 30 minutes or longer
  // with a bad signal

  static uint8_t count = 0;
  static uint8_t minutesCount = 0;
  bool displayClearFlag = false;
  for (uint8_t state = Clock::useless; //v3 Mod
       state == Clock::useless || state == Clock::dirty; //v3 Mod
       state = DCF77_Clock::get_clock_state()) {
    // wait for next sec
    Clock::time_t now; //v3 mod
    DCF77_Clock::get_current_time(now);

    // render one dot per second while initializing
    if ( !(count & 0x0f)  ) {
      Serial.print(F("0DCF init "));
      Serial.print(minutesCount);
      Serial.print(F(" m "));
      Serial.print(count%60);
      Serial.println(F(" s"));
    }
    lcd.setCursor(11, 3);
    leadzero(minutesCount);
    lcd.print(F(":"));
    lcd.setCursor(14, 3); //Start at character 0 on line 0
    leadzero(count%60);
    ++count;
    if ( displayClearFlag ) {
      lcdRTCTime();
      PrintRTCTelegram();
    }
    if (!(count % 60))
    {
      count = 0;
      ++minutesCount;
      if ( (minutesCount == 2) && !displayClearFlag ) {
        lcd.blank(line0);
        lcd.blank(StatusPos);
        displayClearFlag = true;
      }
    }
  }
    //    ***************
#ifdef GERMAN
  Serial.print(F("IInit: "));
  Serial.print(minutesCount);
  Serial.print(F(  ":"));
  Serial.print(count);
  Serial.println(F(" Sekunden."));
#else
  Serial.print(F("I"));
  Serial.print(minutesCount);
  Serial.print(F(":"));
  Serial.print(count);
  Serial.println(F(" init."));
#endif

}

void setup() {
  lcd.begin();
  lcd.noBacklight();
  lcd.setCursor(0, 0); //Start at character 0 on line 0
  lcd.print(F("Arduino dcf ntp gate"));
  //    ***************
  Serial.begin(57600);
  //Serial.begin(19200);
  Serial.print(F("IJuergens DCF77 Master: "));
  Serial.println(F(VERSION));

#ifdef GERMAN
  Serial.println(F("Ibasiert auf:  DCF77  Dial  Clock "));
  Serial.println(F("I(c) Brett Oliver 2014 http://www.brettoliver.org.uk"));
  Serial.println(F("Iverwendet die DCF77 library von Udo Klein"));
  Serial.println(F("I(c) www.blinkenlight.net"));
#else
  Serial.println(F("Ibased on: DCF77  Dial  Clock "));
  Serial.println(F("I(c) Brett Oliver 2014 http://www.brettoliver.org.uk"));
  Serial.println(F("Ibased on the DCF77 library by Udo Klein"));
  Serial.println(F("I(c) www.blinkenlight.net"));
#endif

  Serial.println(F("Icompile time parameters"));
  Serial.print(F("IInverted Mode:      "));
  Serial.println(dcf77_inverted_samples);
  Serial.print(F("IAnalog Mode:        "));
  Serial.println(dcf77_analog_samples);
  Serial.print(F("Ianalog sample pin:  "));
  Serial.println(dcf77_analog_sample_pin);
  Serial.print(F("Idigital sample pin: "));
  Serial.println(dcf77_sample_pin);
  Serial.print(F("IMonitor pin:        "));
  Serial.println(dcf77_monitor_led);
  Serial.print(F("ILDR pin:            "));
  Serial.println(ldr);
  Serial.print(F("IBrightnes poti pin: "));
  Serial.println(ldrRef);
  Serial.print(F("ILCD using i2c addr: 0x"));
  Serial.println(lcd_i2c_id, HEX);
#ifdef USERTC
  rtcOK = true;
  Serial.println(F("IDS1307 RTC usinig its std i2c addr 0x68"));
  if (! rtc.begin()) {
    Serial.println(F("IDS1307: RTC not found"));
    rtcOK=false;
  }
  if ( !rtc.isrunning() ) {
    Serial.println(F("IDS1307 is not running"));
    rtcOK = false;
  }
  if ( rtcOK ) {
    Serial.println(F("IDS1307 ready and running."));
  }
#endif
  Serial.print('M');
  Serial.println(MAJOR);
  Serial.print('m');
  Serial.println(MINOR);
#ifdef GERMAN
  Serial.println(F("IDCF77 Init..."));
#else
  Serial.println(F("Ini1tializing..."));
#endif
  delay(3000);
  lcd.Print(StatusPos,F(VERSION));
  lcd.backlight();
  delay(2000);

  lcd.setCursor(0, 3);
#ifdef GERMAN
  lcd.print(F("starte..."));
#else
  lcd.print(F("init  ..."));
#endif

  pinMode(dcf77_sample_pin, INPUT);
  digitalWrite(dcf77_sample_pin, HIGH);

  pinMode(ledpin(dcf77_monitor_led), OUTPUT); //v3
  // *******************

  DCF77_Clock::setup();
  DCF77_Clock::set_input_provider(sample_input_pin);

  SyncDCF();
  Serial.println(F("Iready."));
  lcd.blank(OMsgPos);
}


void loop() {

  Clock::time_t now; //v3 mod

  //throw away things from serial line
  while ( Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'R':
      if ( rtcOK )
        rtcUpdateNeeded = false;
      break;
      default:
      break;
    }
  }

  DCF77_Clock::get_current_time(now);

  if (now.month.val > 0)
  {

    // ****************

#ifdef USERTC
    PrintRTCTelegram();
#endif
    PrintSecondTelegram(now);

    // ***********
    // get month & day values
    dayval = now.day.val, DEC;
    monthval = now.month.val, DEC;
    // Quartz clock driver
    // toggle Quartz drive 7 & 8 evey second

    if (secsmiss < 1 || seconds == 60)
    { //records time of extra sec second (quart motor needs to loose a second)
      extracount = extracount + 1; //increment extra count total (1sec needs to miss pulse)
      hourextra = hours;
      minuteextra = minutes;
      secondextra = seconds;
      yearextra = years;
      monthextra = months;
      dayextra = days;
    }


    if (secsmiss > 1)
    { //records time of miss second (quart motor needs to add a second)
      misscount = misscount + 1; //increment Miss count total (1sec has missed extra pulse)
      hourmiss = hours;
      minutemiss = minutes;
      secondmiss = seconds;
      yearmiss = years;
      monthmiss = months;
      daymiss = days;
    }

    secondsnow = seconds;

    if (hours == 6 && minutes == 10 && seconds == 01)
    { // resets miss second counter to 0 at 6:10:01
      misscount = 0;
      extracount = 0;
    }

    //???? }
    // Enable below to analize missed pulses on serial monitor
    // Serial.print(" ");
    // Serial.print("secsmiss ");
    // Serial.println(secsmiss);

    // end missing second pulse
    // ################################################################################

    /* serial freq display
      Serial.println(F("confirmed_precision [Hz], target_precision v,total_adjust [Hz], frequency [Hz]"));
      Serial.print(DCF77_Frequency_Control::get_confirmed_precision());
      Serial.print(F(", "));
      //Serial.print(DCF77_Frequency_Control::get_target_precision());
      Serial.print(F(", "));
      //      Serial.print(DCF77_1_Khz_Generator::read_adjustment());
      Serial.print(F(", "));
      //      Serial.print(16000000L - DCF77_1_Khz_Generator::read_adjustment());
      Serial.print(F(" Hz, "));
    */

    // ***************
    // signal quality
    /*
      signalQual = DCF77_Clock::get_prediction_match();
      if(signalQual == 255 || signalQual == 0 )
      {
      signalQual = 00;
      }
      else
      {
      signalQual = (signalQual * 2) -1;
      }
      Serial.print (" Signal Match ");
      Serial.print (signalQual);
      Serial.println ("%");
    */
    // signal quality
    // ***************

    lcd.Menpos(wdayPos);
    if ( (((seconds + 2) / 5)  < 3) != 0 )
    {
#ifdef GERMAN
      lcd.print(F("Hell   "));
#else
      lcd.print(F("Bright "));
#endif
      segIntensity(intensity);
    } else {
      LCDshowWeekday(now.weekday.digit.lo);
    }

    // #################################
    //LDR start

    ldrValue = analogRead(ldr);
    ldrRefValue = analogRead(ldrRef);

    //  Serial.print (" LDR Value ");
    //  Serial.print (ldrValue, DEC);
    //  Serial.print (" ");
    intensityValue();
    // Serial.print ("Intensity ");
    //   Serial.print (intensity);
    //   Serial.print (" ");
    //LDR finish
    //#################################


    // show state of dcf engine
    lcd.Menpos(DcfStPos);
    //lcd.print(" Wait  ");
    switch (DCF77_Clock::get_clock_state()) {
      // case DCF77::useless: Serial.print(F("useless ")); break;
      case Clock::useless: //v3 mod
#ifdef GERMAN
        lcd.print(F(" Fehler"));
#else
        lcd.print(F(" Fail  "));
#endif
        break;
      // case DCF77::dirty:   Serial.print(F("dirty: ")); break;
      //  case DCF77::synced:  Serial.print(F("synced: ")); break;
      // case DCF77::locked:  Serial.print(F("locked: ")); break;
      case Clock::dirty: //v3 Mod
#ifdef GERMAN
        lcd.print(F("ungenau"));
#else
        lcd.print(F(" Dirty "));
#endif
        break;
      case Clock::synced: //v3 Mod
#ifdef GERMAN
        lcd.print(F("   Sync"));
#else
        lcd.print(F(" Sync'd"));
#endif
        break;
      case Clock::locked:  //v3 Mod
#ifdef GERMAN
        lcd.print(F("   Fest"));
#else
        lcd.print(F(" Locked"));
#endif
        break;
    }
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


    // Get hours minutes and seconds variables
    hours   = BCD::bcd_to_int(now.hour);
    minutes = BCD::bcd_to_int(now.minute);
    seconds = BCD::bcd_to_int(now.second);
    years   = BCD::bcd_to_int(now.year);
    months  = BCD::bcd_to_int(now.month);
    days    = BCD::bcd_to_int(now.day);

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    // ****************
    if ( (seconds % 15) == 0 ) {
      fullDate = true;
    }
    if ( rtcUpdateNeeded || fullDate ) {
      lcd.Menpos(timePos);

      LCDpaddedPrint(now.hour);
      lcd.print(F(":"));
      LCDpaddedPrint(now.minute);
      lcd.print(F(":"));
      lcd.Menpos(datePos);
#ifdef GERMAN
      LCDpaddedPrint(now.day);
      lcd.print(DATESEP);
      lcd.print(getMonthName(months));
      lcd.space();
#else
      LCDpaddedPrint(now.month);
      lcd.print(DATESEP);
      LCDpaddedPrint(now.day);
      lcd.print(DATESEP);
#endif
      lcd.print(F("20"));
      LCDpaddedPrint(now.year);
    }
    lcd.Menpos(secPos);
    LCDpaddedPrint(now.second);
    lcd.space();
#ifdef USERTC
    if ( rtcOK && (rtcUpdateNeeded || (   (hours == 19) && (seconds == 22) ) ) ) {
      DateTime rtcdate;
      int secondsToUTC;

//      Serial.println(days);
      rtcdate = DateTime(2000+years,months,days,hours,minutes,seconds);
      if ( now.uses_summertime ) {
        secondsToUTC = 7200;
      } else {
        secondsToUTC = 3600;
      }
      rtc.adjust(DateTime(rtcdate.unixtime() - secondsToUTC));
      rtcUpdateNeeded = false;
      Serial.println(F("IRTC set"));
    }
#else
    rtcUpdateNeeded = false;
#endif

    // **************
    lcd.Menpos(TZPos);
    lcd.print((now.uses_summertime ? F("CEST") : F("CET")));
    // **************
  }

  // *******************
  // secsval = (now.second.val);
  // minsval = (now.minute.val);
  // hourval = (now.hour.val);

  // Serial.print ("Secs Val ");
  // Serial.print (secsval);
  // Serial.print (" Min Val ");
  // Serial.print (minsval);
  // Serial.print (" Hr Val ");
  // Serial.print (hourval);

  // Quality display on LCD -- beginn of row3

  if (seconds >= 0 && seconds <= 10)
  {
    signalmatch(); // Quality factor
    /*} else if (seconds == 11)
      {
        blankMen(OMsgPos); //Blanks row 3
      } else if (seconds == 12)
      {
        lcd.setCursor(0, 3);
        lcd.print("Quartz         ");
      } else if (seconds >= 13 && seconds <= 23) {
        precision();*/ //quartz confirmed and target precision
  } else if (seconds >= 13 && seconds <= 23) {
    freqadj();
  } else if (seconds == 24)
  {
    lcd.blank(OMsgPos); //Blanks row 3
  } else if (seconds >= 25 && seconds <= 35 )
  {
    lcd.Menpos(OMsgPos);
    signalmatch(); // Quality factor
  } else if (seconds == 36)
  {
    lcd.blank(OMsgPos); //Blanks row 3
  } else if (seconds == 37)
  {
    lcd.Menpos(OMsgPos);
    lcd.print(F("Quartz         "));
  } else if (seconds >= 38 && seconds <= 48)
  {
    precision(); //quartz confirmed and target precision
  } else if (seconds == 49)
  {
    lcd.blank(OMsgPos); //Blanks row 3
  } else if (seconds >= 50 && seconds <= 59 )
  {
    lcd.Menpos(OMsgPos);
    signalmatch(); // Quality factor
  }
  // End of Quality display on LCD
  // ======================================================================================
  // line 2 -- the big status line
  lcd.Menpos(StatusPos); //Start at character 0 on line 0
  switch ( seconds / 4 ) {
    case  0: //  0 -  3
      lcd.print(F("DCF77 J\xf5rgen Geinitz"));
      break;
    case  1: //  4 -  7
      lcd.print(F(" DCF77 (ntp) master "));
      //         12345678911234567892
      //lcd.print(" Brett Oliver v13.0 ");
      break;
    case  2: //  8 - 11
      lcd.print(F(VERSION));
      break;
    case  3: // 12 - 15
      // lcd.print("       Pulses       ");
#ifdef GERMAN
      lcd.print(F("Sekundentakt ok     "));
#else
      lcd.print(F("   1 Second Clocks  "));
#endif
      break;
    case  4: // 16 - 19
    case  5: // 20 - 23
    case  6: // 24 - 27
    case  7: // 28 - 31
    case  8: // 32 - 35
    case  9: // 36 - 39
       lcdRTCTime();
       fullDate = true;
       break;
    case 10: // 40 - 43
    case 11: // 44 - 47
      if ( now.leap_second_scheduled )
      {
#ifdef GERMAN
        lcd.print(F("Schaltsekunde kommt "));
#else
        lcd.print(F("Leap second schedule"));
#endif
        break;
      }
    // else no leap-second on the way - fall thru
    case 12: // 48 - 51
    case 13: // 52 - 55
    case 14: // 56 - 59
      if ( now.timezone_change_scheduled )
#ifdef GERMAN
        lcd.print(F("  Zeitumstellung!  "));
#else
        lcd.print(F("tmezone will change"));
#endif
      else
#ifdef GERMAN
        lcd.print((now.uses_summertime ? F("Sommerzeit          ") : F("Normalzeit (Winter) ")));
#else
        lcd.print((now.uses_summertime ? F("summertime          ") : F("normal time        ")));
      //      12345678911234567892
#endif
      break;
      /*    case 15: // 60 - 63
            if ( misscount > 1000 ) misscount = 0;
            if ( extracount > 1000 ) extracount = 0;
        #ifdef GERMAN
            lcd.print(F(" - ")); // miss pulse detected so extra 1 second motor pulse added
            lcd.print(misscount);
            lcdPrintSpace();
            lcd.print(F(" + "));
            lcd.print(extracount);
            lcd.print("  ");
        #else
            lcd.print(F("  Slow ")); // miss pulse detected so extra 1 second motor pulse added
            lcd.print(misscount);
            lcdPrintSpace();
            lcd.print(F("   Fast "));
            lcd.print(extracount);
        #endif
            break;*/
  }
}

