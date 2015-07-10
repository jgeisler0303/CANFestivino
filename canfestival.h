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

#ifndef __CAN_CANFESTIVAL__
#define __CAN_CANFESTIVAL__

#include "applicfg.h"
#include "data.h"
#include "CO_timer.h"
#include "objacces.h"
#include "digitalWriteFast.h"
#include "CO_ErrorState.h"


// ---------  to be called by user app ---------

template<const int redLEDPin, const int greenLEDPin> class CO {
public:
    CO() :
            m(Message_Initializer) {
    }
    ;
    void CO_Cycle();
    void CO_Init();

private:
    static void errorStateBlink(uint8_t dummy);

    Message m;
};

template<int redLEDPin, int greenLEDPin> void CO<redLEDPin, greenLEDPin>::CO_Init() {
    if(redLEDPin>0) pinModeFast(redLEDPin, OUTPUT);
    if(greenLEDPin>0) pinModeFast(greenLEDPin, OUTPUT);
    if(redLEDPin>0) digitalWriteFast(redLEDPin, 1);
    if(greenLEDPin>0) digitalWriteFast(greenLEDPin, 1);

    initCAN();

    setState(Initialisation);

    SetAlarm(0, &errorStateBlink, 62, 62);

    if(redLEDPin>0) digitalWriteFast(redLEDPin, 0);
    if(greenLEDPin>0) digitalWriteFast(greenLEDPin, 0);
}

template<int redLEDPin, int greenLEDPin> void CO<redLEDPin, greenLEDPin>::CO_Cycle() {
    TimeDispatch();

    if (CAN.checkReceive()) {
        CAN.readMsgBuf(&(m.len), m.data);
        m.cob_id = CAN.getCanId();
        m.rtr = 0; // m_nRtr;

        if (isRxNoError())
            flashGreen();
        canDispatch(&m);         // process it
    }

    uint8_t ef = CAN.errorFlag();
    if (ef & MCP_CAN::EFlg_TxWar)
        setTxErrorState(tx_warning);

    if (ef & MCP_CAN::EFlg_TxBusOff) {
        setTxErrorState(tx_bus_off);
    } else if (ef & MCP_CAN::EFlg_TxEP)
        setTxErrorState(tx_passive);
    else if (!isTxNoError()) {
        if (CAN.checkTransmit())
            resetTxErrorState();
    }
    updateTxErrorState();

    if (ef && MCP_CAN::EFlg_RxWar)
        setRxErrorState(rx_warning);

    if (ef & (MCP_CAN::EFlg_Rx1Ovr | MCP_CAN::EFlg_Rx0Ovr))
        setRxErrorState(rx_overflow);
    else if (ef && MCP_CAN::EFlg_RxEP)
        setRxErrorState(rx_passive);
    else if (greePatternStarted())
        resetRxErrorState();

    updateRxErrorState();
}

template<int redLEDPin, int greenLEDPin> void CO<redLEDPin, greenLEDPin>::errorStateBlink(uint8_t dummy) {
    if(nextRedBlinkState()) {
        if(redLEDPin>0) digitalWriteFast(redLEDPin, 1);
    } else {
        if(redLEDPin>0) digitalWriteFast(redLEDPin, 0);
    }

    if(nextGreenBlinkState()) {
        if(greenLEDPin>0) digitalWriteFast(greenLEDPin, 1);
    } else {
        if(greenLEDPin>0) digitalWriteFast(greenLEDPin, 0);
    }
}

#endif
