
#include "LightTalk.h"



//static variables declaration
byte LightTalk::interruptPin;
unsigned int LightTalk::iDelta0Min;
unsigned int LightTalk::iDelta0Max;
unsigned int LightTalk::iDelta1Min;
unsigned int LightTalk::iDelta1Max;
char LightTalk::cTxtMax;
char LightTalk::cByteSize;
unsigned long LightTalk::lLastBlink;
char LightTalk::cBufferReady;
char *LightTalk::cTxt;
char LightTalk::cReceivedChar;
int LightTalk::iIndex;
char LightTalk::cStatus;
unsigned char LightTalk::cIncoming;
char LightTalk::cIncomingIndex;
char LightTalk::cCounter;







int LightTalk::charInBuffer() {
  if (cBufferReady == 0) return 0;
  return cReceivedChar;
}

char * LightTalk::getBuffer() {
  return cTxt;
}

void LightTalk::resetBuffer() {
  cStatus = 0;
  cCounter = 0;
  cIncoming = 0;//start from 0 char in input
  cIncomingIndex = 0;//start from first bit
  cReceivedChar = 0; //empty buffer
  cBufferReady = 0;
}

LightTalk::LightTalk(int intPin, int deltaTime, int bufferSize, int byteSize)
{

  lLastBlink = 0;

  interruptPin = intPin;

  iDelta0Min = deltaTime * 0.5;
  iDelta0Max = deltaTime * 1.45;
  iDelta1Min = deltaTime * 1.5;
  iDelta1Max = deltaTime * 2.5;
  cTxtMax = bufferSize;
  cByteSize = byteSize;

  cBufferReady  = 0;
  cTxt = (char*)malloc(bufferSize);
  cReceivedChar = 0;
  iIndex  = 0;

  cStatus  = 0;
  cIncoming  = 0;
  cIncomingIndex = 0;
  cCounter = 0;

  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), this->Interrupt , CHANGE);


}

//listen to bit stream
static void LightTalk::Interrupt(void) {

  unsigned long lNow = millis();
  unsigned long lDelta;
  unsigned char cReceived = 2; //2=unrecognized bit
  if ( lLastBlink == 0)
  {
    lLastBlink = lNow;
    return;
  }

  lDelta = lNow -  lLastBlink;
  lLastBlink = lNow;
  
  if (lDelta < 10)//ignore too short blink
  {
    //    Serial.write("small d\n");
    return;
  }

  if (lDelta > iDelta0Min && lDelta < iDelta0Max)
  {
    //  Serial.write("0\n");
    cReceived = 0;
  }
  if (lDelta > iDelta1Min && lDelta < iDelta1Max)
  {
    // Serial.write("1\n");
    cReceived = 1;
  }

  

  if (cReceived == 2)//unrecognized. go back to 0
  {
    cStatus = 0;
    cCounter = 0;
    return;
  }

  if (cStatus == 0)
  {
    if (cReceived == 0)
    { //sequence of 0s
      cCounter++;
      return;
    }
    if (cReceived == 1 && cCounter <= 8)
    { //error
      cCounter = 0;
      return;
      //     Serial.write(" 0 rec 1, few 0 -->0\n");

    }
    if (cReceived == 1 && cCounter > 8)
    { //1=start of transmission
      cStatus = 1;
      cCounter = 0;
      cIncoming = 0;//start from 0 char in input
      cIncomingIndex = 0;//start from first bit
      cReceivedChar = 0; //empty buffer
      cBufferReady = 0;
      //        Serial.write(" sot\n");
      return;
    }

  }

  if (cStatus == 1)
  { //receiving
    if (cIncomingIndex < cByteSize)
    {
      if (cReceived == 1) {
        cIncoming |= (1 << cIncomingIndex);
      } cIncomingIndex++;
    }
    cCounter++;
    if (cCounter > cByteSize)
    { //end of byte
      if (cReceived == 1)
      {
        if (cReceivedChar < cTxtMax)
        {
          //Serial.write(" rec=");
          //Serial.print(cIncoming);
          //Serial.write("\n");
          cTxt[cReceivedChar] = cIncoming;
          cReceivedChar++;
        }
        cIncoming = 0;


        
        cIncomingIndex = 0;
        cCounter = 0;
        return;
      }
      else
      { //error? maybe eot
        if (cIncoming  != 0)
        {
          cStatus = 0;
          cCounter = 0;
          //     Serial.write(" 0 && rec<>0 -->0\n");
          return;
        }
        //look for eot
        cStatus = 2;
      }
    }
  }
  if (cStatus == 2)
  {
    if (cReceived == 1)
    { //error!!!
      cStatus = 0;
      cCounter = 0;
      // Serial.write(" 2 rec 1-->0\n");
      return;
    }
    cCounter++;
    if (cCounter == 10)
    {
      //eot
      cStatus = 0;
      cCounter = 0;
      cBufferReady = 1;
      return;

    }
  }

}

