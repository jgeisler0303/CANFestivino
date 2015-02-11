/*
This file is part of CanFestival, a library implementing CanOpen Stack.

Copyright (C): Edouard TISSERANT and Francis DUPIN
AVR Port: Andreas GLAUSER and Peter CHRISTEN

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/******************************************************************************
Project description:
Test projekt for a DS 401 slave, running on Atmel's STK500 with AT90CAN128
Short description:
  PORTA:	Inputs (Keys, low active)
  PORTB:	Outputs (LEDs, low active)
  PORTC:	Node ID (1 BCD switch, low active)

******************************************************************************/

#include <avr/io.h>
#include "hardware.h"
#include "canfestival.h"
#include "mcp_can.h"
#include "ObjDict.h"
#include "ds401.h"
#include "timerscfg.h"
#include "can.h"
#include "Arduino.h"
#include "SPI.h"


unsigned char Flag_Recv = 0;
unsigned char len = 0;
unsigned char buf[8];
char str[20];
int res;

MCP_CAN CAN(7);


volatile unsigned char timer_interrupt = 0;		// Set if timer interrupt elapsed
unsigned char inputs;

// CAN
unsigned char nodeID;
unsigned char digital_input[1] = {0};
unsigned char digital_output[1] = {0};

static Message m = Message_Initializer;		// contain a CAN message

void sys_init();

unsigned char canInit(unsigned int bitrate) {
  if(bitrate==500)
    return CAN.begin(CAN_500KBPS);
  else
    return 0;
}

unsigned char canSend(CAN_PORT notused, Message *m) {
  Serial.print("snd");
  Serial.println(m->cob_id, HEX);
  return CAN.sendMsgBuf(m->cob_id, 0, m->rtr, m->len, m->data);
}


unsigned char canReceive(Message *m) {
    if(CAN_MSGAVAIL == CAN.checkReceive())            // check if data coming
    {
      //      readMsgBufID(&(m->cob_id), &(m->len), m->data);
      CAN.readMsgBuf(&(m->len), m->data);
      m->cob_id= CAN.getCanId();
      m->rtr= 0; // m_nRtr;
      return 1;
    } else
      return 0;
}


unsigned char canChangeBaudRate_driver( CAN_HANDLE fd, char* baud) {
  return 1;
}

static unsigned long lWaitMillis;

void setup() { 
  sys_init();                // Initialize system
  Serial.begin(9600);
  int res= canInit(CAN_BAUDRATE);         		// Initialize the CANopen bus
        Serial.print("CAN init ");
        Serial.println(res);
  initTimer();                                 	// Start timer for the CANopen stack
  nodeID = 23;				// Read node ID first
  setNodeId (&ObjDict_Data, nodeID);
  setState(&ObjDict_Data, Initialisation);	// Init the state
  lWaitMillis = millis() + 1000;  // initial setup
}

void loop() {
    if(coreTimerTrigger) {
      coreTimerTrigger= 0;
      TimeDispatch();                               // Call the time handler of the stack to adapt the elapsed time
      Serial.print('.');
    }

    if( (long)( millis() - lWaitMillis ) >= 0)
    {
      lWaitMillis += 1000;  // do it again 1 second later
      digital_input[0] = get_inputs();
      digital_input_handler(&ObjDict_Data, digital_input, sizeof(digital_input));
      digital_output_handler(&ObjDict_Data, digital_output, sizeof(digital_output));
      set_outputs(digital_output[0]);
      Serial.println("1s");
    }
    // a message was received pass it to the CANstack
    if (canReceive(&m))	{		// a message reveived
      Serial.print("rcv");
      Serial.println(m.cob_id, HEX);
     canDispatch(&ObjDict_Data, &m);         // process it
    }
}

void sys_init()
/******************************************************************************
Initialize the relays, the main states and the modbus protocol stack.
INPUT	LOCK_STATES *lock_states
OUTPUT	void
******************************************************************************/
{
  PORTC= INPUT_MASK;	                        // Inputs (Keys, low active) with pullup
  DDRC= 0x00;		                // 
  
  PORTD|= OUTPUT_MASK;	                        // Outputs (LEDs, low active) all 1
  DDRD|= OUTPUT_MASK;		                // 

  #ifdef WD_SLEEP		// Watchdog and Sleep
  wdt_reset();
  wdt_enable(WDTO_15MS);   	// Watchdogtimer start with 16 ms timeout
  #endif			// Watchdog and Sleep
}
