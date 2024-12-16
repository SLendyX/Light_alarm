/*
 * IRremote: IRsendDemo - demonstrates sending IR codes with IRsend
 * An IR LED must be connected to Arduino PWM pin 3.
 * Version 0.1 July, 2009
 * Copyright 2009 Ken Shirriff
 * http://arcfn.com
 */

#include <Wire.h>
#include <DS3231.h>
#include <IRremote.h>
#define ON 0x1FEB847
#define OFF 0x1FE38C7               
#define DIMM 0x1FEA05F
#define BRIGHT 0x1FE48B7
#define COOL 0x1FE58A7
#define WARM 0x1FE7887
#define DELAY 0



IRsend irsend;
int led = 7, setHour = A0, setMinutes = A1, start = A2, timeSync = A3;
int buttons[] = {setHour, setMinutes, start, timeSync};
int buttonsLength = sizeof(buttons)/sizeof(int);

//7segment display
int A = 0, B = 1, C = 2, D = 12, E = 4, F = 5, G = 6;
int segments[] = {A, B, C, D, E, F, G};
int D1 = 8, D2 = 9, D3 = 10, D4 = 11;
int anodes[] = {D1, D2, D3, D4};

//clock

DS3231 clock;
RTCDateTime dt;

// On the Zero and others we switch explicitly to SerialUSB
#if defined(ARDUINO_ARCH_SAMD)
#define Serial SerialUSB
#endif

void setup() {
  for(int i=0; i<sizeof(segments)/sizeof(int); i++)
    pinMode(segments[i], OUTPUT);

  for(int i=0; i<sizeof(anodes)/sizeof(int); i++)
    pinMode(anodes[i], OUTPUT);

  initializeDisplay(segments, anodes);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(led, OUTPUT);  

  for(int i=0; i<buttonsLength; i++)
    pinMode(buttons[i], INPUT);

  // Serial.begin(9600);
  
#if defined(__AVR_ATmega32U4__)
  while (!Serial); //delay for Leonardo, but this loops forever for Maple Serial
#endif
#if defined(SERIAL_USB) || defined(SERIAL_PORT_USBVIRTUAL)
  delay(2000); // To be able to connect Serial monitor after reset and before first printout
#endif
    // Just to know which program is running on my Arduino
  Serial.println(F("START " __FILE__ " from " __DATE__));
  Serial.print(F("Ready to send IR signals at pin "));
  Serial.println(IR_SEND_PIN);

  Serial.println("Initialize RTC module");

  clock.begin();
  // Manual (YYYY, MM, DD, HH, II, SS
  // clock.setDateTime(2016, 12, 9, 11, 46, 00);
  
  // clock.setDateTime(__DATE__, __TIME__); 
}

int minutes = 0, hour = 0;
bool hasBeenPressed = false, startCount = false;
unsigned long initialPress;

void loop() {
  dt = clock.getDateTime();

  hour%=24, minutes%=60; 

  printClock(hour, minutes);

  if(!hasBeenPressed){
    if(incrementTime(hour, minutes)){
    }else if(digitalRead(start)){
      startCountDown();
    }else if(digitalRead(timeSync)){
      while(digitalRead(timeSync));
      delay(50);
      int clockHour = dt.hour, clockMinute = dt.minute;
      while(!digitalRead(timeSync)){
        if(!hasBeenPressed){
          incrementTime(clockHour, clockMinute);
        }else if(!digitalRead(setMinutes) && !digitalRead(setHour)){
          hasBeenPressed = false;
        }
        printClock(clockHour, clockMinute);
      }

      initialPress = millis();
      while(digitalRead(timeSync) && millis() - initialPress < 500);
      if(millis() - initialPress >=500)
        clockHour = dt.hour, clockMinute = dt.minute;
      hasBeenPressed = true;
      printClock(clockHour, clockMinute);
      clock.setDateTime(dt.year, dt.month, dt.day, clockHour, clockMinute, 0); 

      delay(50);
    }
    printClock(hour, minutes);
  }else if(!digitalRead(setMinutes) && !digitalRead(setHour) && !digitalRead(start) && !digitalRead(timeSync))
    hasBeenPressed = false;

  printClock(hour, minutes);
}

bool incrementTime(int& hour, int& minutes){
  if(digitalRead(setMinutes)){
      minutes = (minutes+1)%60, hasBeenPressed = true;
      Serial.print("setMinutes: ");
      Serial.println(minutes);
      initialPress = millis();

      while(digitalRead(setMinutes) && millis() - initialPress < 750)
        printClock(hour, minutes);

      if(millis() - initialPress >= 750){
        initialPress = millis();
        while(digitalRead(setMinutes)){
          if(millis() - initialPress >= 150)
            minutes = (minutes+1)%60, initialPress = millis(); 
          printClock(hour, minutes); 
        }
      }
      return true;
    }
    else if(digitalRead(setHour)){
      hour = (hour+1)%24, hasBeenPressed = true;
      Serial.print("setHour: ");
      Serial.println(hour);

      while(digitalRead(setHour) && millis() - initialPress < 750)
        printClock(hour, minutes);

      if(millis() - initialPress >= 750){
        initialPress = millis();
        while(digitalRead(setHour)){
          if(millis() - initialPress >= 250)
            hour = (hour+1)%24, initialPress = millis(); 
          printClock(hour, minutes); 
        }
      }
      return true;
    }

    return false;
}


void startCountDown(){
  initialPress = millis();
  while(digitalRead(start))
    if(millis()-initialPress >= 500){
      hasBeenPressed = true;
      Serial.println("reseting....");
      hour = minutes = 0;
      return;
    }

  digitalWrite(led, LOW);

  int totalTime = hour*60 + minutes;
  Serial.print("totalTime: ");
  Serial.println(totalTime);
  delay(50);

  for(int i=0; i<2; i++){
    sendIRsignal(DIMM);
    sendIRsignal(WARM);
  }

  sendIRsignal(OFF);

  while(dt.hour != hour || dt.minute != minutes){
    dt = clock.getDateTime();

    if(digitalRead(start)){
      initialPress = millis();
      while(digitalRead(start))
        if(millis()-initialPress >= 500){
          Serial.println("reseting....");
          hasBeenPressed = true;
          hour = minutes = 0;
          return;
        }
    }
  }

  sendIRsignal(ON);

  for(int i=0; i<2; i++){
    sendIRsignal(BRIGHT);
    sendIRsignal(COOL);
  }

  digitalWrite(led, HIGH);
  hour = minutes = 0;
}

void sendIRsignal(unsigned long signal){
  irsend.sendNEC(0xFFFFFFF, 32);
  for(int i=0; i<10; i++){
      irsend.sendNEC(signal, 32);
      delay(50);
    }
}

void printClock(int hour, int minutes){
  printHour(hour);
  printMinutes(minutes);
}

//printNumbers
void printMinutes(int number){
  int firstDigit = number / 10;
  int secondDigit = number % 10;

  selectDigit(2);
  printDigit(firstDigit);
  delay(DELAY);
  null();

  selectDigit(1);
  printDigit(secondDigit);
  delay(DELAY);
  null();
}

void printHour(int number){
  int firstDigit = number / 10;
  int secondDigit = number % 10;

  selectDigit(4);
  printDigit(firstDigit);
  delay(DELAY);
  null();


  selectDigit(3);
  printDigit(secondDigit);
  delay(DELAY);
  null();
  }

//select digit
void selectDigit(int digitNumber){
  switch(digitNumber){
    case 1:
      digit1();
      break;
    case 2:
      digit2();
      break;
    case 3:
      digit3();
      break;
    case 4:
      digit4();
      break;
    default:
      break;
  }
}

//printDigit

bool printDigit(int digit){
  switch(digit){
    case 0:
      zero();
      return true;
    case 1:
      one();
      return true;
    case 2:
      two();
      return true;
    case 3:
      three();
      return true;
    case 4:
      four();
      return true;
    case 5:
      five();
      return true;
    case 6:
      six();
      return true;
    case 7:
      seven();
      return true;
    case 8:
      eight();
      return true;
    case 9:
      nine();
      return true;
    default:
      return false;
  }
}


//digit functions
void zero(){
  digitalWrite(A, HIGH);
  digitalWrite(B, HIGH);
  digitalWrite(C, HIGH);
  digitalWrite(D, HIGH);
  digitalWrite(E, HIGH);
  digitalWrite(F, HIGH);
  digitalWrite(G, LOW);
}

void one(){
  digitalWrite(A, LOW);
  digitalWrite(B, HIGH);
  digitalWrite(C, HIGH);
  digitalWrite(D, LOW);
  digitalWrite(E, LOW);
  digitalWrite(F, LOW);
  digitalWrite(G, LOW);
}

void two(){
  digitalWrite(A, HIGH);
  digitalWrite(B, HIGH);
  digitalWrite(C, LOW);
  digitalWrite(D, HIGH);
  digitalWrite(E, HIGH);
  digitalWrite(F, LOW);
  digitalWrite(G, HIGH);
}

void three(){
  digitalWrite(A, HIGH);
  digitalWrite(B, HIGH);
  digitalWrite(C, HIGH);
  digitalWrite(D, HIGH);
  digitalWrite(E, LOW);
  digitalWrite(F, LOW);
  digitalWrite(G, HIGH);
}

void four(){
  digitalWrite(A, LOW);
  digitalWrite(B, HIGH);
  digitalWrite(C, HIGH);
  digitalWrite(D, LOW);
  digitalWrite(E, LOW);
  digitalWrite(F, HIGH);
  digitalWrite(G, HIGH);
}

void five(){
  digitalWrite(A, HIGH);
  digitalWrite(B, LOW);
  digitalWrite(C, HIGH);
  digitalWrite(D, HIGH);
  digitalWrite(E, LOW);
  digitalWrite(F, HIGH);
  digitalWrite(G, HIGH);
}

void six(){
  digitalWrite(A, HIGH);
  digitalWrite(B, LOW);
  digitalWrite(C, HIGH);
  digitalWrite(D, HIGH);
  digitalWrite(E, HIGH);
  digitalWrite(F, HIGH);
  digitalWrite(G, HIGH);
}

void seven(){
  digitalWrite(A, HIGH);
  digitalWrite(B, HIGH);
  digitalWrite(C, HIGH);
  digitalWrite(D, LOW);
  digitalWrite(E, LOW);
  digitalWrite(F, LOW);
  digitalWrite(G, LOW);
}

void eight(){
  digitalWrite(A, HIGH);
  digitalWrite(B, HIGH);
  digitalWrite(C, HIGH);
  digitalWrite(D, HIGH);
  digitalWrite(E, HIGH);
  digitalWrite(F, HIGH);
  digitalWrite(G, HIGH);
}

void nine(){
  digitalWrite(A, HIGH);
  digitalWrite(B, HIGH);
  digitalWrite(C, HIGH);
  digitalWrite(D, HIGH);
  digitalWrite(E, LOW);
  digitalWrite(F, HIGH);
  digitalWrite(G, HIGH);
}

void digit1(){
  digitalWrite(D1, LOW);
  digitalWrite(D2, HIGH);
  digitalWrite(D3, HIGH);
  digitalWrite(D4, HIGH);
}

void digit2(){
  digitalWrite(D1, HIGH);
  digitalWrite(D2, LOW);
  digitalWrite(D3, HIGH);
  digitalWrite(D4, HIGH);
}

void digit3(){
  digitalWrite(D1, HIGH);
  digitalWrite(D2, HIGH);
  digitalWrite(D3, LOW);
  digitalWrite(D4, HIGH);
}

void digit4(){
  digitalWrite(D1, HIGH);
  digitalWrite(D2, HIGH);
  digitalWrite(D3, HIGH);
  digitalWrite(D4, LOW);
}

void null(){
  for(int i=0; i<sizeof(segments)/sizeof(int);i++)
    digitalWrite(segments[i], LOW);
}

void initializeDisplay(int segments[], int anodes[]){
  for(int i=0; i<sizeof(segments)/sizeof(int);i++)
    digitalWrite(segments[i], LOW);
  for(int i=0; i<sizeof(anodes)/sizeof(int);i++)
    digitalWrite(anodes[i], HIGH);
}