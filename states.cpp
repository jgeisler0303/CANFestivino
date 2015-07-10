/*
 This file is part of CanFestival, a library implementing CanOpen Stack.

 Copyright (C): Edouard TISSERANT and Francis DUPIN

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
/*!
 ** @file   states.c
 ** @author Edouard TISSERANT and Francis DUPIN
 ** @date   Tue Jun  5 09:32:32 2007
 **
 ** @brief
 **
 **
 */

#include "data.h"
#include "sysdep.h"

/** Prototypes for internals functions */
/*!                                                                                                
 **
 **
 ** @param d
 ** @param newCommunicationState
 **/
void switchCommunicationState(s_state_communication *newCommunicationState);

/*!                                                                                                
 **
 **
 ** @param d
 **
 ** @return
 **/
e_nodeState getState() {
    return ObjDict_Data.nodeState;
}

/*!                                                                                                
 **
 **
 ** @param d
 ** @param m
 **/
void canDispatch(Message *m) {
    UNS16 cob_id = UNS16_LE(m->cob_id);
    switch (cob_id >> 7) {
        case SYNC: /* can be a SYNC or a EMCY message */
            if (cob_id == 0x080) /* SYNC */
            {
                if (ObjDict_Data.CurrentCommunicationState.csSYNC)
                    proceedSYNC();
            } else /* EMCY */
            if (ObjDict_Data.CurrentCommunicationState.csEmergency)
                proceedEMCY(m);
            break;
            /* case TIME_STAMP: */
        case PDO1tx:
        case PDO1rx:
        case PDO2tx:
        case PDO2rx:
        case PDO3tx:
        case PDO3rx:
        case PDO4tx:
        case PDO4rx:
            if (ObjDict_Data.CurrentCommunicationState.csPDO)
                proceedPDO(m);
            break;
        case SDOtx:
        case SDOrx:
            if (ObjDict_Data.CurrentCommunicationState.csSDO)
                proceedSDO(m);
            break;
        case NODE_GUARD:
            if (ObjDict_Data.CurrentCommunicationState.csLifeGuard)
                proceedNODE_GUARD(m);
            break;
        case NMT:
            // if (ObjDict_iam_a_slave) {
            proceedNMTstateChange(m);
            // }
            break;
#ifdef CO_ENABLE_LSS
            case LSS:
            if (!ObjDict_Data.CurrentCommunicationState.csLSS)break;
            if ((*(ObjDict_iam_a_slave)) && cob_id==MLSS_ADRESS)
            {
                proceedLSS_Slave(m);
            }
            else if(!(*(ObjDict_iam_a_slave)) && cob_id==SLSS_ADRESS)
            {
                proceedLSS_Master(m);
            }
            break;
#endif
    }
}

#define StartOrStop(CommType, FuncStart, FuncStop) \
	if(newCommunicationState->CommType && ObjDict_Data.CurrentCommunicationState.CommType == 0){\
		MSG_WAR(0x9999,#FuncStart, 9999);\
		ObjDict_Data.CurrentCommunicationState.CommType = 1;\
		FuncStart;\
	}else if(!newCommunicationState->CommType && ObjDict_Data.CurrentCommunicationState.CommType == 1){\
		MSG_WAR(0x9999,#FuncStop, 9999);\
		ObjDict_Data.CurrentCommunicationState.CommType = 0;\
		FuncStop;\
	}
#define None

/*!                                                                                                
 **
 **
 ** @param d
 ** @param newCommunicationState
 **/
void switchCommunicationState(s_state_communication *newCommunicationState) {
#ifdef CO_ENABLE_LSS
    StartOrStop(csLSS, startLSS(), stopLSS())
#endif
    StartOrStop(csSDO, None, resetSDO())
    StartOrStop(csSYNC, startSYNC(), stopSYNC())
    StartOrStop(csLifeGuard, lifeGuardInit(), lifeGuardStop())
    StartOrStop(csEmergency, emergencyInit(), emergencyStop())
    StartOrStop(csPDO, PDOInit(), PDOStop())
    StartOrStop(csBoot_Up, None, slaveSendBootUp())
}

/*!                                                                                                
 **
 **
 ** @param d
 ** @param newState
 **
 ** @return
 **/
UNS8 setState(e_nodeState newState) {
    if (newState != ObjDict_Data.nodeState) {
        switch (newState) {
            case Initialisation: {
                s_state_communication newCommunicationState = { 1, 0, 0, 0, 0, 0, 0 };
                ObjDict_Data.nodeState = Initialisation;
                switchCommunicationState(&newCommunicationState);
                /* call user app init callback now. */
                /* ObjDict_Data.initialisation MUST NOT CALL SetState */
                // (*ObjDict_Data.initialisation)();
            }

                /* Automatic transition - No break statement ! */
                /* Transition from Initialisation to Pre_operational */
                /* is automatic as defined in DS301. */
                /* App don't have to call SetState(Pre_operational) */

            case Pre_operational: {

                s_state_communication newCommunicationState = { 0, 1, 1, 1, 1, 0, 1 };
                ObjDict_Data.nodeState = Pre_operational;
                switchCommunicationState(&newCommunicationState);
                // (*ObjDict_Data.preOperational)();
            }
                break;

            case Operational:
                if (ObjDict_Data.nodeState == Initialisation)
                    return 0xFF;
                {
                    s_state_communication newCommunicationState = { 0, 1, 1, 1, 1, 1, 0 };
                    ObjDict_Data.nodeState = Operational;
                    newState = Operational;
                    switchCommunicationState(&newCommunicationState);
                    // (*ObjDict_Data.operational)();
                }
                break;

            case Stopped:
                if (ObjDict_Data.nodeState == Initialisation)
                    return 0xFF;
                {
                    s_state_communication newCommunicationState = { 0, 0, 0, 0, 1, 0, 1 };
                    ObjDict_Data.nodeState = Stopped;
                    newState = Stopped;
                    switchCommunicationState(&newCommunicationState);
                    // (*ObjDict_Data.stopped)();
                }
                break;
            default:
                return 0xFF;

        }/* end switch case */

    }
    /* ObjDict_Data.nodeState contains the final state */
    /* may not be the requested state */
    return ObjDict_Data.nodeState;
}

/*!                                                                                                
 **
 **
 ** @param d
 **
 ** @return
 **/
UNS8 getNodeId() {
    return ObjDict_bDeviceNodeId;
}

#ifdef CO_ENABLE_CHANGE_NODE_ID
/*!                                                                                                
 **
 **
 ** @param d
 ** @param nodeId
 **/
void setNodeId(UNS8 nodeId) {
    const subindex *si;
    UNS8 si_size;
    ODCallback_t *callbacks;

#ifdef CO_ENABLE_LSS
    ObjDict_Data.lss_transfer.nodeID=nodeId;
    if(nodeId==0xFF) {
        *ObjDict_Data.bDeviceNodeId = nodeId;
        return;
    }
    else
#endif
    if (!(nodeId > 0 && nodeId <= 127)) {
        MSG_WAR(0x2D01, "Invalid NodeID",nodeId);
        return;
    }

    if (!(si= ObjDict_scanIndexOD(0x1200, &si_size, &callbacks))) {
        /* Adjust COB-ID Client->Server (rx) only id already set to default value or id not valid (id==0xFF)*/
        if ((*(UNS16*) si[1].pObject == 0x600 + *ObjDict_Data.bDeviceNodeId) || (*ObjDict_Data.bDeviceNodeId == 0xFF)) {
            /* cob_id_client = 0x600 + nodeId; */
            *(UNS16*) si[1].pObject = 0x600 + nodeId;
        }
        /* Adjust COB-ID Server -> Client (tx) only id already set to default value or id not valid (id==0xFF)*/
        if ((*(UNS16*) si[2].pObject == 0x580 + *ObjDict_Data.bDeviceNodeId) || (*ObjDict_Data.bDeviceNodeId == 0xFF)) {
            /* cob_id_server = 0x580 + nodeId; */
            *(UNS16*) si[2].pObject = 0x580 + nodeId;
        }
    }

    /*
     Initialize the server(s) SDO parameters
     Remember that only one SDO server is allowed, defined at index 0x1200

     Initialize the client(s) SDO parameters
     Nothing to initialize (no default values required by the DS 401)
     Initialize the receive PDO communication parameters. Only for 0x1400 to 0x1403
     */
    {
        UNS8 i = 0;
        UNS32 cobID[] = {0x200, 0x300, 0x400, 0x500};
        while ((si= ObjDict_scanIndexOD(0x1400+i, &si_size, &callbacks)) && (i < 4)) {
            if ((*(UNS16*) si[1].pObject
                            == cobID[i] + *ObjDict_Data.bDeviceNodeId)
                    || (*ObjDict_Data.bDeviceNodeId == 0xFF))
            *(UNS16*) si[1].pObject = cobID[i]
            + nodeId;
            i++;
        }
    }
    /* ** Initialize the transmit PDO communication parameters. Only for 0x1800 to 0x1803 */
    {
        UNS8 i = 0;
        UNS32 cobID[] = {0x180, 0x280, 0x380, 0x480};
        i = 0;
        while ((si= ObjDict_scanIndexOD(0x1800+i, &si_size, &callbacks)) && (i < 4)) {
            if ((*(UNS16*) si[1].pObject
                            == cobID[i] + *ObjDict_Data.bDeviceNodeId)
                    || (*ObjDict_Data.bDeviceNodeId == 0xFF))
            *(UNS16*) si[1].pObject = cobID[i]
            + nodeId;
            i++;
        }
    }

    /* Update EMCY COB-ID if already set to default*/
    if ((error_cobid == *ObjDict_Data.bDeviceNodeId + 0x80)
            || (*ObjDict_Data.bDeviceNodeId == 0xFF))
    error_cobid = nodeId + 0x80;

    /* bDeviceNodeId is defined in the object dictionary. */
    *ObjDict_Data.bDeviceNodeId = nodeId;
}
#endif

// void _initialisation() {}

// void _preOperational() {
// #ifdef CO_ENABLE_MASTER
// 	if (!(*(ObjDict_iam_a_slave))) {
// 		masterSendNMTstateChange(0, NMT_Reset_Node);
// 	}
// #endif
// }

// void _operational() {}

// void _stopped() {}
