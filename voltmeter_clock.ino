//*drive 2 milliamperometer to show minutes and hours
//*use a DS1307 to remember time when switched off
//*use a photoresistor to receive commands
//*drive leds when in dark light
//*drive a speaker for alarm sound
//
//*commands from photoresistor OR serial port:
//*0 reset alarm
//*hhmm set alarm
//*8xxx set hour calibration (full scale for 12 hours)
//*9xxx set minute calibration(full scale for 60 minutes)
//*yymmddhhmm yyyymmddhhmmss set date and time

#include <avr/wdt.h>
#include <Wire.h>
#include "RTClib.h"
#include "LightTalk.h"
#include "PCM.h"
#include "bell.h"
#include <EEPROM.h>


//A4-->SDA rtc module
//A5-->SCL rtc module
#define Hpin 6 //drive a milliamperometer for hour. use a resistor to set full scale
#define Mpin 5 //drive a milliamperometer for minute. use a resistor to set full scale
//do not use other pins because pwm sound disable the timer after playing
#define ledPin 13//arduino internal led
#define lightPin 4//drive a led during dark hour according to photoresistor readout
#define photoresistPin 2//pullup to drive a 5k photoresistor
//this pin requires interrupt so do not use another one
#define speakerPin 11//drive a speaker for bell sound
//this pin requires a separate timer so do not use another one


#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "="  VALUE(var)



//                        pin             timing maxlen bitperdigit
LightTalk  LT = LightTalk(photoresistPin, 100,   15,    4);//light talk library


RTC_DS1307 RTC; //rtc library


int iH = 0; //meter position. not the actual time
int iM = 0;

int iHour = 0; //the present time
int iMinute = 0;

int iHourAlarm = -1; //the present ALARM time
int iMinuteAlarm = -1;


//number of blink to be flashed
int iLightBlinker = 0;
//number of ding to be played
int iAlarmDing = 0;

//full scale calibration for H and M
int iCalH = 170;
int iCalM = 170;

int iVCCRef;


char cWdCounter = 0; //counter used to check all steps before resetting watchdog.


//void(* resetFunc) (void) = 0; //declare reset function @ address 0
void resetFunc()
{
  //wait for watchdog
  for (;;);
}

long readVcc() {
  long result; // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA, ADSC));
  result = ADCL;
  result |= ADCH << 8;
  result = 1100000L / result; // Back-calculate AVcc in mV
  return result;
}







void printShortDateTimeToString(DateTime * DT, char*dest)
{
  sprintf(dest, "%02d/%02d/%02d %02d:%02d:%02d",
          DT->day(), DT->month(), DT->year(),
          DT->hour(), DT->minute(), DT->second());


}


void showDateTime()
{
  DateTime now = RTC.now();
  char str[40];
  printShortDateTimeToString(&now, str);
  Serial.println(str);
}



void getAlarmTime()
{
  byte h = EEPROM.read(8);//RTC.readRAM(8);
  byte m = EEPROM.read(9);//RTC.readRAM(9);
  byte checksum = EEPROM.read(10);//RTC.readRAM(10);
  Serial.print("GET alarm=");
  Serial.print(h);
  Serial.print(":");
  Serial.print(m);
  Serial.print(" Check=");
  Serial.print(checksum);
  if (checksum != (h + m))
  {
    EEPROM.write(8, 100); //RTC.writeRAM(8, 100);//100=do nothing
    EEPROM.write(9, 0); //RTC.writeRAM(9, 0);
    EEPROM.write(10, 100); //RTC.writeRAM(10, 100);
    Serial.println(" KO");
  }
  else {
    Serial.println(" OK");
    if (h != 100) {
      iHourAlarm = h;
      iMinuteAlarm = m;
    }
    else
    {
      iHourAlarm = -1;
      iMinuteAlarm = -1;
    }
  }

}


void setAlarmTime(int h , int m)
{
  EEPROM.write(8, h); //RTC.writeRAM(8, h);
  EEPROM.write(9, m); //RTC.writeRAM(9, m);
  EEPROM.write(10, h + m); //RTC.writeRAM(10, h + m);
}









void getCalibration()
{
  int iRef;
  byte h = EEPROM.read(11);//RTC.readRAM(11);
  byte m = EEPROM.read(12);//RTC.readRAM(12);
  byte checksum = h + m;

  iRef = (unsigned int)EEPROM.read(14) + (((unsigned int)EEPROM.read(15)) << 8);
  if (iRef < 4500 || iRef > 5500)
  {
    iRef = iVCCRef;
  }


  Serial.print("GET calibration=");
  Serial.print(h);
  Serial.print(",");
  Serial.print(m);
  Serial.print(" Check=");
  Serial.print(checksum);
  if (checksum != EEPROM.read(13))//RTC.readRAM(13))
  {
    Serial.println(" KO");
    h = iCalH;
    m = iCalM;
    checksum = h + m;
    EEPROM.write(11, h); //RTC.writeRAM(11, h);
    EEPROM.write(12, m); //RTC.writeRAM(12, m);
    EEPROM.write(13, checksum); //RTC.writeRAM(13, checksum); //do not care about overflow...
  }
  else {
    Serial.println(" OK");
    iCalH = ((long)h * (long)iRef) / iVCCRef;
    iCalM = ((long)m * (long)iRef) / iVCCRef;
  }

}


void setCalibration(int h , int m)
{
  EEPROM.write(11, h); //RTC.writeRAM(11, h);
  EEPROM.write(12, m); //RTC.writeRAM(12, m);
  EEPROM.write(13, h + m); //RTC.writeRAM(13, h + m);

  EEPROM.write(14, iVCCRef);
  EEPROM.write(15, iVCCRef >> 8);


}









void muoviLancetta(int pin, int val, int *presentVal)
{
  if (val == *presentVal)
  {
    return;
  }
  if (val > *presentVal)
  {
    (*presentVal)++;
  }
  else  //if (val < *presentVal)
  {
    (*presentVal)--;
  }

  analogWrite(pin, 255 - (*presentVal));


}


void checkLight()
{

  cWdCounter++;

  if (iLightBlinker == 0)
  {
    if (digitalRead(photoresistPin))
    {
      pinMode(lightPin, OUTPUT);
      digitalWrite(lightPin, HIGH);
    }
    else
    {
      pinMode(lightPin, OUTPUT);
      digitalWrite(lightPin, LOW);
    }
  }
}


void checkDataReceiver()
{
  char *p = NULL;
  char *p1 = NULL;
  int iChar = 0;



  cWdCounter++;

  static int iCnt = 0;
  static char cBuff[20];

  if (Serial.available() > 0)
  {
    char c = Serial.read();
    Serial.println(c);
    if (c > ' ' || c == 13)
    {
      if (c == 13)
      {
        Serial.print("Caratteri ricevuti ");
        Serial.println(iCnt);

        iChar = iCnt;
        p = cBuff;
        iCnt = 0;
      }
      else {
        if (iCnt <= 14)
        {
          if (c >= '0' && c <= '9') c -= '0';
          cBuff[iCnt] = c;
          iCnt++;
        }
      }
    }
  }





  //  int iBlink;
  if (iChar == 0) iChar = LT.charInBuffer();


  if (  iChar != 0)
  {
    if (p == NULL)
    {
      p = LT.getBuffer();
    }


    //according to the lenght of the string received perform different operation
    if (iChar == 1) //reset alarm
    {
      Serial.println("RST alarm");
      iMinuteAlarm = iHourAlarm = -1;
      iLightBlinker = 8;
      setAlarmTime(100 , 0);

    }
    else if (iChar == 4) //set calibration 8/9 xxx where xxx=full scale value. usually 135. 0=hour,1=minutes
      //otherwhise set hhmm alarm
    {
      int iCal;
      int iVal;

      p1 = p;
      iCal = (*p1++);
      iVal = (*p1++) * 100;
      iVal += (*p1++) * 10;
      iVal += (*p1++) ;


      if (iCal == 8)
      {
        iCalH = iVal;
        setCalibration(iCalH, iCalM);
        iLightBlinker = 8;
        Serial.print("SET calibration=");
        Serial.print(iCal);
        Serial.print(",");
        Serial.println(iVal);
      } else if (iCal == 9)
      {

        iCalM = iVal;
        setCalibration(iCalH, iCalM);
        iLightBlinker = 8;
        Serial.print("SET calibration=");
        Serial.print(iCal);
        Serial.print(",");
        Serial.println(iVal);
      }
      else //set alarm
      {
        iHourAlarm = (*p++) * 10;
        iHourAlarm += (*p++);
        iMinuteAlarm = (*p++) * 10;
        iMinuteAlarm += (*p++);
        Serial.print("SET alarm=");
        Serial.print(iHourAlarm);
        Serial.print(":");
        Serial.println(iMinuteAlarm);
        setAlarmTime(iHourAlarm , iMinuteAlarm);
        iLightBlinker = 8;

      }

    }
    else if (iChar == 10 || iChar == 14) //set datetime yymmddhhmm yyyymmddhhmmss
    {
      int iYear;
      int iMonth;
      int iDay;
      int iSec = 30;
      if (iChar == 10)
      {
        iYear = 2000;
        iYear += (*p++) * 10;
        iYear += (*p++);
        iMonth = (*p++) * 10;
        iMonth += (*p++);
        iDay = (*p++) * 10;
        iDay += (*p++);
      }
      else
      {
        iYear = (*p++);
        iYear *= 1000;
        iYear += (*p++) * 100;
        iYear += (*p++) * 10;
        iYear += (*p++);
        iMonth = (*p++) * 10;
        iMonth += (*p++);
        iDay = (*p++) * 10;
        iDay += (*p++);
      }
      iHour = (*p++) * 10;
      iHour += (*p++);
      iMinute = (*p++) * 10;
      iMinute += (*p++);



      RTC.adjust(DateTime(iYear, iMonth, iDay, iHour, iMinute, iSec));

      Serial.print("SET time=");
      Serial.print(iDay);
      Serial.print("/");
      Serial.print(iMonth);
      Serial.print("/");
      Serial.print(iYear);
      Serial.print(" ");
      Serial.print(iHour);
      Serial.print(":");
      Serial.print(iMinute);
      Serial.print(":");
      Serial.println(iSec);


      iLightBlinker = 8;
    }




    LT.resetBuffer();
  }
}


void transmitTimeOverSerial()
{
  static long lUpdate = 0;
  long lNow = millis();

  cWdCounter++;


  if (lNow >= lUpdate)
  {
    lUpdate = lNow + 10000L;
    showDateTime();
  }

}


void updateMeters()
{
  static long lUpdate = 0;
  long lNow = millis();

  cWdCounter++;


  if (lNow >= lUpdate)
  {
    lUpdate = lNow + 100L;
    DateTime now;
    now = RTC.now();
    int h = now.hour();
    iHour = h;
    iMinute = now.minute();
    if (h > 12)
    {
      h = h - 12;
    }

    muoviLancetta(Hpin, (h * iCalH) / 12, &iH);
    muoviLancetta(Mpin, (iMinute * iCalM) / 60, &iM);

  }
}

void checkAlarm()
{
  static char cBlinker = 0;
  static char cAlarm = 0;

  cWdCounter++;


  if (iMinute != iMinuteAlarm || iHour != iHourAlarm)
  {
    cAlarm = 0;
    cBlinker = 0;
    return;
  }

  if (iMinute == iMinuteAlarm && iHour == iHourAlarm && cBlinker == 0)
  {
    cBlinker = 1;//switch on blinker
    iLightBlinker = 50;
  }

  if (iMinute == iMinuteAlarm && iHour == iHourAlarm && cAlarm == 0)
  {
    cAlarm = 1;//switch on alarm
    iAlarmDing = 3;
  }



}






void lightBlinker()
{
  static long lUpdate = 0;
  long lNow = millis();


  cWdCounter++;

  if (iLightBlinker == 0)
  {
    lUpdate = 0;
    return;
  }


  if (lNow >= lUpdate)
  {
    lUpdate = lNow + 500L;
    iLightBlinker--;

    if (iLightBlinker & 1)
    {
      pinMode(lightPin, OUTPUT);
      digitalWrite(lightPin, HIGH);
    }
    else
    {
      pinMode(lightPin, OUTPUT);
      digitalWrite(lightPin, LOW);
    }
  }
}



void AlarmDing()
{

  cWdCounter++;

  if (iAlarmDing != 0 && pcmPlaying() == 0)
  {
    iAlarmDing--;
    play_Bell();
  }


}



//read rtc. if value does not change-->stuck-->reset
void watchdog()
{
  static long lUpdate = 0;
  static char cSec = -1;

  cWdCounter++;

  long lNow = millis();

  if (lNow >= lUpdate)
  {
    //wdt_reset();


    lUpdate = lNow + 5000L;
    DateTime now = RTC.now();
    if (cSec < 0)
    {
      cSec = now.second();
      return;
    }
    if ( cSec == now.second())
    {
      resetFunc(); //call reset
    }
    cSec = now.second();
  }
}



void setup() {
  pinMode(Hpin, OUTPUT);
  analogWrite(Hpin, 255);
  pinMode(Mpin, OUTPUT);
  analogWrite(Mpin, 255);

  Wire.begin();
  RTC.begin();

  Serial.begin(9600);
  //RTC.adjust(DateTime(__DATE__, __TIME__));
  Serial.print("Vcc=");
  iVCCRef = readVcc();
  Serial.print( iVCCRef, DEC );
  Serial.println("mV");
  getAlarmTime();
  getCalibration();

 wdt_enable(WDTO_8S);

}
#pragma message(VAR_NAME_VALUE(__DATE__))
#pragma message(VAR_NAME_VALUE(__TIME__))



void loop() {
   
   
  cWdCounter = 0;
  lightBlinker();
  AlarmDing();
  checkLight();
  checkDataReceiver();
  transmitTimeOverSerial();
  updateMeters();
  checkAlarm();
  watchdog();
  if (cWdCounter == 8)
  {
    //all 8 steps done?
    wdt_reset();
    cWdCounter = 0;
  }
}

