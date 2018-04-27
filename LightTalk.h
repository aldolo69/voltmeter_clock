#ifndef LightTalk_h
#define LightTalk_h

#include "Arduino.h"




class LightTalk
{
  public:
    LightTalk(int interruptPin, int deltaTime, int bufferSize, int byteSize);
    int charInBuffer();
    char * getBuffer();
    void resetBuffer();
  private:

    static void Interrupt(void);//interrupt routine

    static byte interruptPin;// = 3;


    static unsigned int iDelta0Min;// = iDelta0 * 0.5; //100ms=0,200ms=1
    static unsigned int iDelta0Max;// = iDelta0 * 1.45;
    static unsigned int iDelta1Min;// = iDelta0 * 1.5; //100ms=0,200ms=1
    static unsigned int iDelta1Max;// = iDelta0 * 2.5;
    static char cTxtMax;// = 14;//buffer length
    static char cByteSize;// = 8; //4 or 8 bit per char. 4 for numbers only

    static   unsigned long lLastBlink;// = 0;//last interrupt timestamp
    static  char cBufferReady;// = 0; //1 when data is available
    static  char *cTxt;//[cTxtMax];//buffer for received char
    static  char cReceivedChar;//;//received chars in buffer
    static  int iIndex;// = 0; //index of the last received char

    static  char cStatus;// = 0; //0=trailing. wait for 9 zeroes and then 1
    //1=receiving. cByteSize bits and then 1 to keep receiving, 0 to skip to 2
    //2=here i've already received cByteSize of 0s, another 0 in status 1. wait for a couple of 0s to close message
    static  unsigned char cIncoming;// = 0;
    static  char cIncomingIndex;// = 0; //index in cIncoming byte to store next bit
    static  char cCounter;// = 0;
};

#endif