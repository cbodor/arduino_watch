#include <Arduino.h>
void setup(void);
void loop(void);
void sleep(void);
void mainscreen(void);
void auxscreen1(void);
void auxscreen2(void);
void auxscreen3(void);
void idle(void);

/*
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.


 TODO
 - Automatic mode for torch
 - Battery charge meter and display
 - Button debounce? (not sure if this is needed)
 - Fix free mem diag screen

 Modules
 - IR blaster to turn off TVs I dont like? http://www.arcfn.com/2009/08/multi-protocol-infrared-remote-library.html
 - EMF and temp sensing glove
 - geiger counter


 */

#define VERSION "4.41"


#include <uOLED.h>
#include <colors.h>
#include <SPI.h>
#include <LSM303.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <Wire.h>


const int  cs=10;                      // Chip select for clock

const int buttonPinLeft = 7;
const int buttonPinLight = 2;
const int buttonPinRight = 5;

int buttonStateLeft = 0;
int buttonStateLight = 0;
int buttonStateRight = 0;

int currentStateUp = 0;
int previousStateUp = 0;
int currentStateDown = 0;
int previousStateDown = 0;

int state = LOW;                       // The current state of the output pin
int reading;                           // The current reading from the input pin
int previous = HIGH;                   // The previous reading from the input pin

const int photocellPin = 0;            // The cell and 10K pulldown are connected to a0
int photocellReading = 1024;           // The analog reading from the sensor divider

const int lightled = 9;

uOLED uoled;                           // Create an instance of the uOLED class
LSM303 compass;                        // Create a compass instance

long interval_Screen_Saver = 30000;    // 30 secs screen saver - shut display down / off
unsigned long startTime_Screen_Saver;

int menu = 0;

unsigned long start_hold;
boolean allow = false;
int OnDelay = 500;                     // Time to hold down torch button before activation

const int maxmenu = 3;                 // Maximum number of menu items, this needs to be set for loop to work

// Boot color Scheme
const int boottext = WHITESMOKE;
const int bootstatus = MIDNIGHTBLUE;

// Color Schemes Day or Night
const int colorheadingday = ORANGE;
const int colorhighlightday = MIDNIGHTBLUE;
const int colortextday = WHITESMOKE;
const int colordataday = MIDNIGHTBLUE;

const int colorheadingnight = RED;
const int colorhighlightnight = RED;
const int colortextnight = RED;
const int colordatanight = RED;

const int colorbackground = BLACK;

int colorheading;
int colorhighlight;
int colortext;
int colordata;

byte adcsra, mcucr1, mcucr2;

int TimeDate[7];

void setup()
{
  EICRA = 0x00;                        // Configure INT0 to trigger on low level
  Serial.begin(115200);

  uoled.begin(8,256000, &Serial);      // with the reset line connected to pin 8 and a serial speed of 256000 
  uoled.SetContrast(5);

  //*************************************************************/
  // Display Boot information and initial device configuration
  //*************************************************************/

  uoled.Text(5,5,LARGE_FONT,boottext,"Booting.",1);
  uoled.Text(8,3,LARGE_FONT,bootstatus,".",1);
  delay(500);

  Wire.begin();
  compass.init();
  compass.enableDefault();

  // Compass Calibration
  compass.m_max.x = +412;
  compass.m_max.y = +491;
  compass.m_max.z = +590;
  compass.m_min.x = -747;
  compass.m_min.y = -532;
  compass.m_min.z = -526;

  uoled.Text(8,3,LARGE_FONT,bootstatus,"o",1);
  delay(500);

  pinMode(buttonPinLeft, INPUT_PULLUP);
  pinMode(buttonPinLight, INPUT_PULLUP);
  pinMode(buttonPinRight, INPUT_PULLUP);
  pinMode(lightled, OUTPUT);
  pinMode(cs,OUTPUT);             // Chip select

  SPI.begin();                    // Start the SPI library:
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE1);     // Both mode 1 & 3 should work
  digitalWrite(cs, LOW);          // Set control register
  SPI.transfer(0x8E);
  SPI.transfer(0x60);             // 60= disable Osciallator and Battery SQ wave @1hz, temp compensation, Alarms disabled
  digitalWrite(cs, HIGH);

  uoled.Text(8,3,LARGE_FONT,bootstatus,"O",1);
  delay(500);

  //day(1-31), month(1-12), year(0-99), hour(0-23), minute(0-59), second(0-59)
  //setTimeDate(18,11,12,14,23,00);

  startTime_Screen_Saver = millis();
  randomSeed(analogRead(0));

  uoled.SetBackColor(colorbackground);
}

void loop()
{
  //*************************************************************/
  // Initial main loop items
  //*************************************************************/

  if ((millis() - startTime_Screen_Saver) > interval_Screen_Saver)
  {
    sleep();
    menu = 0;
  }

  //*************************************************************/
  // Color theme based on light levels
  //*************************************************************/

  if (photocellReading < 200) {
    colorheading = colorheadingnight;
    colorhighlight = colorhighlightnight;
    colortext = colortextnight;
    colordata = colordatanight;
  }
  else {
    colorheading = colorheadingday;
    colorhighlight = colorhighlightday;
    colortext = colortextday;
    colordata = colordataday;
  }

  //*************************************************************/
  // Display Section
  //*************************************************************/

  switch (menu)
  {
  case 0:
    mainscreen();
    break;
  case 1:
    auxscreen1();
    break;
  case 2:
    auxscreen2();
    break;
  case 3:
    auxscreen3();
    break;
  }

  //*************************************************************/
  // Button Navigation Section
  //*************************************************************/

  buttonStateLeft = digitalRead(buttonPinLeft);
  if (buttonStateLeft == HIGH) {
    currentStateUp = 1;
  }
  else {
    currentStateUp = 0;
  }

  if(currentStateUp != previousStateUp) {
    if(currentStateUp == 1) {
      if(menu == maxmenu ) {
        menu = 0;
        uoled.Cls();
        idle();
      }
      else {
        menu++;
        uoled.Cls();
        idle();
      }
    }
  }


  buttonStateRight = digitalRead(buttonPinRight);
  if (buttonStateRight == HIGH) {
    currentStateDown = 1;
  }
  else {
    currentStateDown = 0;
  }

  if(currentStateDown != previousStateDown) {
    if(currentStateDown == 1) {
      if(menu == 0 ) {
        menu = 3;
        uoled.Cls();
        idle();
      }
      else {
        menu--;
        uoled.Cls();
        idle();
      }
    }
  }


  previousStateUp = currentStateUp;
  previousStateDown = currentStateDown;


  //*************************************************************/
  // LED torch Code
  //*************************************************************/

  buttonStateLight = digitalRead(buttonPinLight);


  if ( buttonStateLight == LOW && previous == HIGH) {
    start_hold = millis();
    allow = true;
  }

  if (allow == true && buttonStateLight == LOW && previous == LOW) {
    if ((millis() - start_hold) >= OnDelay) {
      state = !state;
      allow = false;
      idle();
    }
  }

  previous = buttonStateLight;
  digitalWrite(lightled, state);
}

//*************************************************************/
// Functions
//*************************************************************/

int setTimeDate(int d, int mo, int y, int h, int mi, int s){
  int TimeDate [7]={
    s,mi,h,0,d,mo,y                                                                                            };
  for(int i=0; i<=6;i++){
    if(i==3)
      i++;
    int b= TimeDate[i]/10;
    int a= TimeDate[i]-b*10;
    if(i==2){
      if (b==2)
        b=B00000010;
      else if (b==1)
        b=B00000001;
    }
    TimeDate[i]= a+(b<<4);

    digitalWrite(cs, LOW);
    SPI.transfer(i+0x80);
    SPI.transfer(TimeDate[i]);
    digitalWrite(cs, HIGH);
  }
}

void getTimeDate()
{
  for(int i=0; i<=6;i++) {
    if(i==3)
      i++;
    digitalWrite(cs, LOW);
    SPI.transfer(i+0x00);
    unsigned int n = SPI.transfer(0x00);
    digitalWrite(cs, HIGH);
    int a=n & B00001111;
    if(i==2) {
      int b=(n & B00110000)>>4;   // 24 hour mode
      if(b==B00000010)
        b=20;
      else if(b==B00000001)
        b=10;
      TimeDate[i]=a+b;
    }
    else if(i==4){
      int b=(n & B00110000)>>4;
      TimeDate[i]=a+b*10;
    }
    else if(i==5){
      int b=(n & B00010000)>>4;
      TimeDate[i]=a+b*10;
    }
    else if(i==6){
      int b=(n & B11110000)>>4;
      TimeDate[i]=a+b*10;
    }
    else {
      int b=(n & B01110000)>>4;
      TimeDate[i]=a+b*10;
    }
  }
}

String readTime() {
  String temp;
  String ampm;
  int hh_24, hh_12;

  getTimeDate();

  hh_24=TimeDate[2];

  if (hh_24==0) hh_12=12;
  else hh_12=hh_24%12;

  String hh = String( hh_12 );
  if (hh.length()==1) {
    hh = "0"+hh;
  }

  String mm = String( TimeDate[1] );
  if (mm.length()==1) {
    mm = "0"+mm;
  }

  if (hh_24 < 12) {
    ampm = "AM";
  }
  else {
    ampm = "PM";
  }

  temp.concat( hh );
  temp.concat( ":" );
  temp.concat( mm );
  temp.concat( " " );
  temp.concat( ampm );

  return(temp);
}

String readDate(){
  String temp;

  getTimeDate();

  String DD = String( TimeDate[4] );
  if (DD.length()==1) {
    DD = "0"+DD;
  }

  String MM = String( TimeDate[5] );
  if (MM.length()==1) {
    MM = "0"+MM;
  }

  String YYYY = String( TimeDate[6] );

  if (YYYY.length()==2) {
    YYYY = "20"+YYYY;
  }

  temp.concat( YYYY );
  temp.concat( "-" );
  temp.concat( MM );
  temp.concat( "-" );
  temp.concat( DD );

  return(temp);

}

float readTemp()
{
  float rv;
  uint8_t temp_msb, temp_lsb;
  int8_t nint;

  digitalWrite(cs, LOW);
  SPI.transfer(0x11);                       // Temperature register MSB address
  temp_msb = SPI.transfer(0x00);
  digitalWrite(cs, HIGH);
  digitalWrite(cs, LOW);
  SPI.transfer(0x12);                       // Temperature register MSB address
  temp_lsb = SPI.transfer(0x00) >> 6;
  digitalWrite(cs, HIGH);

  if ((temp_msb & B10000000) != 0)
    nint = temp_msb | ~((1 << 8) - 1);      // If negative get two's complement
  else
    nint = temp_msb;

  rv = 0.25 * temp_lsb + nint;

  return rv;
}

int readCompass() {

  compass.read();

  float heading = compass.heading((LSM303::vector){
    0,-1,0                                                                          }
  );

  return heading;
}

String Way()
{
  int previousMillisCompass = 0;
  int intervalCompass = 1000;
  String temp;
  unsigned int currentMillisCompass = millis();

  if(currentMillisCompass - previousMillisCompass > intervalCompass) {
    previousMillisCompass = currentMillisCompass;

    int heading;
    heading = readCompass();

    if (heading <= 0 || heading >= 360) {
      temp = "XX";
    }
    else {
      if ((heading >= 337)||(heading < 22)) {
        temp = " N";
      }
      else
        if ((heading >= 22)&&(heading < 67)) {
        temp = "NE";
      }
      else
        if ((heading >= 67)&&(heading < 112)) {
          temp = " E";
        }
        else
          if ((heading >= 112)&&(heading < 157)) {
          temp = "SE";
        }
        else
          if ((heading >= 157)&&(heading < 202)) {
            temp = " S";
          }
          else
            if ((heading >= 202)&&(heading < 247)) {
              temp = "SW";
            }
            else
              if ((heading >= 247)&&(heading < 292)) {
                temp = " W";
              }
              else
                if ((heading >= 292)&&(heading < 337)) {
                  temp = "NW";
                }
                else
                  temp = "  ";
    }
  }
  return temp;
}


String pvcell()
{
  String temp;
  photocellReading = analogRead(photocellPin);
  if (photocellReading < 10) {
    temp = "  Dark";
  }
  else if (photocellReading < 200) {
    temp = "   Dim";
  }
  else if (photocellReading < 500) {
    temp = " Light";
  }
  else if (photocellReading < 800) {
    temp = "Bright";
  }
  else {
    temp = " Blind";
  }

  int val = photocellReading;
  val = map(val, 0, 1023, 1, 15);
  uoled.SetContrast(val);

  return temp;

}

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

double getTemp(void)
{
  unsigned int wADC;
  double t;
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));  // Set the internal reference and mux.
  ADCSRA |= _BV(ADEN);                            // Enable the ADC
  delay(20);                                      // Wait for voltages to become stable.
  ADCSRA |= _BV(ADSC);                            // Start the ADC
  while (bit_is_set(ADCSRA,ADSC));                // Detect end-of-conversion
  wADC = ADCW;                                    // Reading register "ADCW" takes care of how to read ADCL and ADCH.
  t = (wADC-310) / 1.22;                          // The offset of 324.31 could be wrong. It is just an indication.
  return (t);                                     // The returned temperature is in degrees Celcius.
}


unsigned long sketchSz(void)
{
  extern int _etext;
  extern int _edata;

  return ((unsigned long)(&_etext) + ((unsigned long)(&_edata) - 256L));
}

int Accel()
{
  int temp;
  compass.read();

  /*
   Serial.print("A ");
   Serial.print("X: ");
   Serial.print((int)compass.a.x);
   Serial.print(" Y: ");
   Serial.print((int)compass.a.y);
   Serial.print(" Z: ");
   Serial.print((int)compass.a.z);

   Serial.print(" M ");
   Serial.print("X: ");
   Serial.print((int)compass.m.x);
   Serial.print(" Y: ");
   Serial.print((int)compass.m.y);
   Serial.print(" Z: ");
   Serial.println((int)compass.m.z);
   */

  //temp.concat("x:");
  temp = (int)compass.m.x;
  //temp.concat( " y:" );
  //temp.concat( (int)compass.m.y );
  //temp.concat( "z" );
  //temp.concat( (int)compass.m.z );

  return temp;
}

void idle()
{
  startTime_Screen_Saver = millis();  // Reset idle counter
}

void sleep()
{
  for(int x=15; x > 0; x--) {
    uoled.SetContrast(x);
    delay(50);
  }

  uoled.SetDisplayState (0x00);       // Display off
  uoled.DisplayControl(0x00, 0x00);   // Backlight off
  uoled.Cls();

  sleep_enable();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  EIMSK |= _BV(INT0);                 // Enable INT0
  adcsra = ADCSRA;                    // Save the ADC Control and Status Register A
  ADCSRA = 0;                         // Disable ADC
  cli();
  mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);  // Turn off the brown-out detector
  mcucr2 = mcucr1 & ~_BV(BODSE);
  MCUCR = mcucr1;
  MCUCR = mcucr2;
  sei();                              // Ensure interrupts enabled so we can wake up again
  sleep_cpu();                        // Go to sleep
  sleep_disable();                    // Wake up here
  ADCSRA = adcsra;                    // Restore ADCSRA

  startTime_Screen_Saver = millis();  // Reset idle counter

  uoled.SetDisplayState (0x01);       // Display on - no redraw necessary
  uoled.DisplayControl(0x00, 0x01);   // Backlight on
  uoled.SetContrast(0);

  for(int x=0; x < 15; x++) {
    uoled.SetContrast(x);
    delay(50);
  }

}

//*************************************************************/
// Main Display
//*************************************************************/

void mainscreen() {

  char time[20];                  // Variable for Time display
  char date[20];                  // Variable for Date display
  char temp[10];                  // Variable for Temperature display
  char go[3];                     // Variable for Compass display
  char lux[7];                    // Variable for Light Level display

  readTime().toCharArray(time,20);
  readDate().toCharArray(date,20);
  itoa(readTemp(), temp, 10);
  Way().toCharArray(go,3);
  pvcell().toCharArray(lux,7);

  // void Text(char col, char row, char font, int color, char *Text, char transparent);
  uoled.Text(6,0,LARGE_FONT,colorheading,"Main",1);
  uoled.Line (0, 14, 128, 14, colortext);
  uoled.Line (0, 15, 128, 15, colortext);
  uoled.TextGraphic(0,19,LARGE_FONT,colorhighlight,2,2,time,1);
  uoled.Line (0, 45, 128, 45, colortext);
  uoled.Line (0, 46, 128, 46, colortext);

  uoled.Text(0,5,LARGE_FONT,colortext,"Temperature: ",1);
  uoled.Text(14,5,LARGE_FONT,colordata,temp,1);

  uoled.Text(0,6,LARGE_FONT,colortext,"Direction:   ",1);
  uoled.Text(14,6,LARGE_FONT,colordata,go,1);

  uoled.Text(0,7,LARGE_FONT,colortext,"Light:    ",1);
  uoled.Text(10,7,LARGE_FONT,colordata,lux,1);

  uoled.Text(3,9,LARGE_FONT,colordata,date,1);
}

//*************************************************************/
// Secondary Screen 1
//*************************************************************/

void auxscreen1() {
  pvcell();
  uoled.Text(5,0,LARGE_FONT,colorheading,"Compass",1);
  uoled.Line (0, 14, 128, 14, colortext);
  uoled.Line (0, 15, 128, 15, colortext);

  float Pi = 3.14159;
  float compasssize = 40.0;
  float x_compass = 64.0;
  float y_compass = 70.0;

  float heading;
  heading = readCompass();

  int x = x_compass - 0.8 * compasssize * sin(Pi * float(-heading)/180.0);
  int y = y_compass + 0.8 * compasssize * cos(Pi * float(-heading)/180.0);

  delay(10);
  uoled.Circle(x_compass,y_compass, 0.85 * compasssize, colorbackground,1);
  uoled.Circle(x_compass,y_compass, compasssize, colordata,0);
  uoled.Line(x_compass, y_compass, x, y, colorheading);

}
//*************************************************************/
// Secondary Screen 2
//*************************************************************/

void auxscreen2() {
  pvcell();
  uoled.Text(4,0,LARGE_FONT,colorheading,"Lady Luck",1);
  uoled.Line (0, 14, 128, 14, colortext);
  uoled.Line (0, 15, 128, 15, colortext);
  uoled.Text(0,2,LARGE_FONT,colortext,"Coin Flip:",1);

  int hort = random(0,2);
  int flipped = Accel();
  int dice1 = random(1, 7);
  int dice2 = random(1, 7);

  if (flipped >= 200) {

    if (hort == 1) {
      uoled.Text(11,2,LARGE_FONT,colordata,"Heads",1);
    }
    else
      uoled.Text(11,2,LARGE_FONT,colordata,"Tails",1);
  }

  // void Rectangle (char x1, char y1, char x2, char y2, int color, char filled);
  // void Circle (char x, char y, char radius, int color, char filled);

  uoled.Rectangle(5, 60, 55, 110, colortext, 0);
  uoled.Rectangle(70, 60, 120, 110, colortext, 0);

  if (flipped >= 200) {

    if (dice1 == 1){
      uoled.Rectangle(6, 61, 54, 108, colorbackground, 1);
      uoled.Circle(30, 85, 5, colordata, 2);   // Center
    }
    if (dice1 == 2){
      uoled.Rectangle(6, 61, 54, 108, colorbackground, 1);
      uoled.Circle(45, 70, 5, colordata, 2);   // Top right
      uoled.Circle(15, 100, 5, colordata, 2);  // Bottom left
    }
    if (dice1 == 3){
      uoled.Rectangle(6, 61, 54, 108, colorbackground, 1);
      uoled.Circle(45, 70, 5, colordata, 2);   // Top right
      uoled.Circle(30, 85, 5, colordata, 2);   // Center
      uoled.Circle(15, 100, 5, colordata, 2);  // Bottom left
    }
    if (dice1 == 4){
      uoled.Rectangle(6, 61, 54, 108, colorbackground, 1);
      uoled.Circle(15, 70, 5, colordata, 2);   // Top left
      uoled.Circle(15, 100, 5, colordata, 2);  // Bottom left
      uoled.Circle(45, 70, 5, colordata, 2);   // Top right
      uoled.Circle(45, 100, 5, colordata, 2);  // Bottom right
    }
    if (dice1 == 5){
      uoled.Rectangle(6, 61, 54, 108, colorbackground, 1);
      uoled.Circle(15, 70, 5, colordata, 2);   // Top left
      uoled.Circle(15, 100, 5, colordata, 2);  // Bottom left
      uoled.Circle(30, 85, 5, colordata, 2);   // Center
      uoled.Circle(45, 70, 5, colordata, 2);   // Top right
      uoled.Circle(45, 100, 5, colordata, 2);  // Bottom right
    }
    if (dice1 == 6){
      uoled.Rectangle(6, 61, 54, 108, colorbackground, 1);
      uoled.Circle(15, 70, 5, colordata, 2);   // Top left
      uoled.Circle(15, 100, 5, colordata, 2);  // Bottom left
      uoled.Circle(15, 85, 5, colordata, 2);   // Middle left
      uoled.Circle(45, 70, 5, colordata, 2);   // Top right
      uoled.Circle(45, 100, 5, colordata, 2);  // Bottom right
      uoled.Circle(45, 85, 5, colordata, 2);   // Middle right
    }
    if (dice2 == 1){
      uoled.Rectangle(71, 61, 119, 108, colorbackground, 1);
      uoled.Circle(95, 85, 5, colordata, 2);    // Center
    }
    if (dice2 == 2){
      uoled.Rectangle(71, 61, 119, 108, colorbackground, 1);
      uoled.Circle(110, 70, 5, colordata, 2);  // Top right
      uoled.Circle(80, 100, 5, colordata, 2);  // Bottom left
    }
    if (dice2 == 3){
      uoled.Rectangle(71, 61, 119, 108, colorbackground, 1);
      uoled.Circle(110, 70, 5, colordata, 2);   // Top right
      uoled.Circle(95, 85, 5, colordata, 2);    // Center
      uoled.Circle(80, 100, 5, colordata, 2);   // Bottom left
    }
    if (dice2 == 4){
      uoled.Rectangle(71, 61, 119, 108, colorbackground, 1);
      uoled.Circle(80, 70, 5, colordata, 2);    // Top left
      uoled.Circle(80, 100, 5, colordata, 2);  // Bottom left
      uoled.Circle(110, 70, 5, colordata, 2);   // Top right
      uoled.Circle(110, 100, 5, colordata, 2);  // Bottom right
    }
    if (dice2 == 5){
      uoled.Rectangle(71, 61, 119, 108, colorbackground, 1);
      uoled.Circle(80, 70, 5, colordata, 2);    // Top left
      uoled.Circle(80, 100, 5, colordata, 2);   // Bottom left
      uoled.Circle(95, 85, 5, colordata, 2);    // Center
      uoled.Circle(110, 70, 5, colordata, 2);   // Top right
      uoled.Circle(110, 100, 5, colordata, 2);  // Bottom right
    }
    if (dice2 == 6){
      uoled.Rectangle(71, 61, 119, 108, colorbackground, 1);
      uoled.Circle(80, 70, 5, colordata, 2);    // Top left
      uoled.Circle(80, 100, 5, colordata, 2);   // Bottom left
      uoled.Circle(80, 85, 5, colordata, 2);    // Middle left
      uoled.Circle(110, 70, 5, colordata, 2);   // Top right
      uoled.Circle(110, 100, 5, colordata, 2);  // Bottom right
      uoled.Circle(110, 85, 5, colordata, 2);   // Middle right
    }
  }

  // Dice 1
  //uoled.Circle(15, 70, 5, colordata, 2);   // Top left
  //uoled.Circle(15, 100, 5, colordata, 2);  // Bottom left
  //uoled.Circle(15, 85, 5, colordata, 2);   // Middle left
  //uoled.Circle(30, 85, 5, colordata, 2);   // Center
  //uoled.Circle(45, 70, 5, colordata, 2);   // Top right
  //uoled.Circle(45, 100, 5, colordata, 2);  // Bottom right
  //uoled.Circle(45, 85, 5, colordata, 2);   // Middle right

  // Dice 2
  //uoled.Circle(80, 70, 5, colordata, 2);   // Top left
  //uoled.Circle(80, 100, 5, colordata, 2);  // Bottom left
  //uoled.Circle(80, 85, 5, colordata, 2);   // Middle left
  //uoled.Circle(95, 85, 5, colordata, 2);   // Center
  //uoled.Circle(110, 70, 5, colordata, 2);  // Top right
  //uoled.Circle(110, 100, 5, colordata, 2); // Bottom right
  //uoled.Circle(110, 85, 5, colordata, 2);  // Middle right

}

//*************************************************************
// Secondary Screen 3
//*************************************************************

void auxscreen3() {
  pvcell();
  char mem[4];
  char inttemp[3];
  char prgsize[10];
  itoa(freeRam(), mem, 4);
  itoa(getTemp(), inttemp, 10);
  itoa(sketchSz(), prgsize, 10);

  uoled.Text(3,0,LARGE_FONT,colorheading,"Diagnostic",1);
  uoled.Line (0, 14, 128, 14, colortext);
  uoled.Line (0, 15, 128, 15, colortext);

  uoled.Text(0,2,LARGE_FONT,colortext,"F SRAM:",1);
  uoled.Text(8,2,LARGE_FONT,colordata,mem,1);

  uoled.Text(0,3,LARGE_FONT,colortext,"MCU Temp:",1);
  uoled.Text(10,3,LARGE_FONT,colordata,inttemp,1);

  uoled.Text(0,4,LARGE_FONT,colortext,"Size:",1);
  uoled.Text(6,4,LARGE_FONT,colordata,prgsize,1);

  uoled.Text(0,5,LARGE_FONT,colortext,"Version:",1);
  uoled.Text(9,5,LARGE_FONT,colordata,VERSION,1);
}

ISR(INT0_vect)
{
  EIMSK &= ~_BV(INT0);           // One interrupt to wake up only
}

































