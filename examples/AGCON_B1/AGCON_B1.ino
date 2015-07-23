// Example CANopen device with four DHT sensors, two switch outputs and three up/down outputs via MCP23s17 port expander
#include "DHT22.h"
#include <Mcp23s17.h> // use library from my repo!

#include "canfestival.h"
#include "ObjDict.h"
// These all need to be included because canfestival needs them
#include <avr/io.h>
#include "Arduino.h"
#include <SPI.h>
#include "mcp_can.h"
#include "Timer.h"
#include "digitalWriteFast.h"
#include "BlinkPattern.h"

CO<3, 4> co;

const uint16_t CANopenDevErr_DHTErrorCode= 0xFF00;
const uint16_t CANopenMonErr_UnderVoltage= 0x8300;

#define DHT22_PIN1 A0
#define DHT22_PIN2 A1
#define DHT22_PIN3 A2
#define DHT22_PIN4 A3

// Setup a DHT22 instance
DHT22 myDHT22[4]= {DHT22(DHT22_PIN1), DHT22(DHT22_PIN2), DHT22(DHT22_PIN3), DHT22(DHT22_PIN4) };

const float VoltageScale= 0.015702;
boolean underVoltageState= false;

UNS32 readDHTCallback(const subindex * OD_entry, UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess) {
  static DHT22_ERROR_t last_error[4];
  
  if(writeAccess) return 0;
  
  byte sensorNo= bSubindex-1;
  
  DHT22_ERROR_t errorCode = myDHT22[sensorNo].readData();
  if(errorCode==DHT_ERROR_TOOQUICK) errorCode= last_error[sensorNo];
  
  if(errorCode!=last_error[sensorNo]) {
    EMCY_errorRecovered(CANopenDevErr_DHTErrorCode | last_error[sensorNo] | sensorNo<<4);
    last_error[sensorNo]= DHT_ERROR_NONE;
  }
  
  if(errorCode==DHT_ERROR_NONE) {
    DHT22_Temp[sensorNo]= myDHT22[sensorNo].getTemperatureC();
    DHT22_Humi[sensorNo]= myDHT22[sensorNo].getHumidity();
    ObjDict_PDO_status[ObjDict_Data.currentPDO].event_trigger= 1;
  } else {
    if(last_error[sensorNo]==DHT_ERROR_NONE) {
      last_error[sensorNo]= errorCode;
      EMCY_setError(CANopenDevErr_DHTErrorCode | last_error[sensorNo] | sensorNo<<4, CANopenErrReg_Manufacturer, 0);
    }
    DHT22_Temp[sensorNo]= -100.0;
    DHT22_Humi[sensorNo]= -100.0;
    ObjDict_PDO_status[ObjDict_Data.currentPDO].event_trigger= 0;
  }
  return 0;
}

UNS32 readVoltageCallback(const subindex * OD_entry, UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess) {
flashGreen();
if(!writeAccess)
      ObjDict_PDO_status[ObjDict_Data.currentPDO].event_trigger= 1;

  return 0;
}

#define MCP23S17_SLAVE_SELECT_PIN  10 //arduino   <->   SPI Slave Select           -> CS  (Pin 11 on MCP23S17 DIP)
MCP23S17 Mcp23s17 = MCP23S17( MCP23S17_SLAVE_SELECT_PIN, true );

#define SWITCH_ON(no) Mcp23s17.digitalWrite(no+8, 1)
#define SWITCH_OFF(no) Mcp23s17.digitalWrite(no+8, 0)
#define SWITCH_STATE(no) Mcp23s17.digitalRead(no)

TIMER_HANDLE switchTimeOutHandle[2]= {TIMER_NONE, TIMER_NONE};

void switchTimeOutAlarm(UNS8 switchNo) {
  switchTimeOutHandle[switchNo]= TIMER_NONE;
  Switch[switchNo]= 0;
  SWITCH_OFF(switchNo);
  ObjDict_PDO_status[5].event_trigger= 1;
  sendOnePDOevent(5);
}

UNS32 writeSwitchCallback(const subindex * OD_entry, UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess) {
    byte switchNo= bSubindex-1;
    
    if(!writeAccess) return 0;

    if(Switch[switchNo]) {
      if(Switch_Timeout!=0) {
        switchTimeOutHandle[switchNo]= DelAlarm(switchTimeOutHandle[switchNo]);
        switchTimeOutHandle[switchNo]= SetAlarm(switchNo, switchTimeOutAlarm, MS_TO_TIMEVAL((uint32_t)Switch_Timeout*1000), 0);
      }
      SWITCH_ON(switchNo);
    } else {
      switchTimeOutHandle[switchNo]= DelAlarm(switchTimeOutHandle[switchNo]);
      SWITCH_OFF(switchNo);
    }
    return 0;
}

#define OPENER_OPEN(no) do { Mcp23s17.digitalWriteD(no<<1, 1); Mcp23s17.digitalWriteD((no<<1)+1, 0); Mcp23s17.applyDigitalWrite(); } while(0)
#define OPENER_CLOSE(no) do { Mcp23s17.digitalWriteD(no<<1, 0); Mcp23s17.digitalWriteD((no<<1)+1, 1); Mcp23s17.applyDigitalWrite(); } while(0)
#define OPENER_OFF(no) do { Mcp23s17.digitalWriteD(no<<1, 0); Mcp23s17.digitalWriteD((no<<1)+1, 0); Mcp23s17.applyDigitalWrite(); } while(0)
int8_t OPENER_STATE(byte no) {
  int8_t s= (Mcp23s17.digitalRead()>>(no<<1)) & 0x03;
  return (s & 0x01) - (s>>1);
}

TIMER_HANDLE openerTimeOutHandle[3]= {TIMER_NONE, TIMER_NONE, TIMER_NONE};

void openerTimeOutAlarm(UNS8 openerNo) {
  openerTimeOutHandle[openerNo]= TIMER_NONE;
  Opener[openerNo]= 0;
  OPENER_OFF(openerNo);
  ObjDict_PDO_status[5].event_trigger= 1;
  sendOnePDOevent(5);
}

UNS32 writeOpenerCallback(const subindex * OD_entry, UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess) {
    if(!writeAccess) return 0;

    byte openerNo= bSubindex-1;
    int8_t oldOpener= Opener[openerNo];
    
    if(Opener[openerNo]>1) Opener[openerNo]= 1;
    if(Opener[openerNo]<-1) Opener[openerNo]= -1;
      
    if(OPENER_STATE(openerNo)==0 && Opener[openerNo]!=0) {
      openerTimeOutHandle[openerNo]= DelAlarm(openerTimeOutHandle[openerNo]);
      openerTimeOutHandle[openerNo]= SetAlarm(openerNo, openerTimeOutAlarm, MS_TO_TIMEVAL((uint32_t)Opener_Timeout*1000), 0);
      
      if(Opener[openerNo]>0) {
        OPENER_OPEN(openerNo);
      } else {
        OPENER_CLOSE(openerNo);
      }
    } else {
      Opener[openerNo]= OPENER_STATE(openerNo);
      ObjDict_PDO_status[5].event_trigger= 1;
      sendOnePDOevent(5);      
    }
    
    if(oldOpener!=Opener[openerNo]) {
      ObjDict_PDO_status[5].event_trigger= 1;
      sendOnePDOevent(5);
    }
    
    return 0;
}

void setup() {
  co.CO_Init();

  analogReference(INTERNAL);
  
  RegisterSetODentryCallBack(0x2000, 1, readDHTCallback);
  RegisterSetODentryCallBack(0x2000, 2, readDHTCallback);
  RegisterSetODentryCallBack(0x2000, 3, readDHTCallback);
  RegisterSetODentryCallBack(0x2000, 4, readDHTCallback);
  
  RegisterSetODentryCallBack(0x2002, 1, writeSwitchCallback);
  RegisterSetODentryCallBack(0x2002, 2, writeSwitchCallback);
  
  RegisterSetODentryCallBack(0x2003, 1, writeOpenerCallback);
  RegisterSetODentryCallBack(0x2003, 2, writeOpenerCallback);
  RegisterSetODentryCallBack(0x2003, 3, writeOpenerCallback);

  RegisterSetODentryCallBack(0x200F, 0, readVoltageCallback);
  
  
  Mcp23s17.pinMode((uint16_t)0x033F);
  for(byte i= 0; i<16; i++)
    Mcp23s17.digitalWriteD(i, 0);
  Mcp23s17.applyDigitalWrite();  
}

void loop() {
  co.CO_Cycle();
  
  Main_Voltage= analogRead(A5) * VoltageScale;
  if(Main_Voltage<11.0) {
    underVoltageState= true;
    EMCY_setError(CANopenMonErr_UnderVoltage, CANopenErrReg_Voltage, 0);
  } else if(underVoltageState && Main_Voltage>11.7) {
    underVoltageState= false;
    EMCY_errorRecovered(CANopenMonErr_UnderVoltage);
  }    
}
