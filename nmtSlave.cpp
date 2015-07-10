/*
 This file is part of CanFestival, a library implementing CanOpen
 Stack.

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
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 USA
 */
/*!
 ** @file   nmtSlave.c
 ** @author Edouard TISSERANT and Francis DUPIN
 ** @date   Tue Jun  5 08:50:53 2007
 **
 ** @brief
 **
 **
 */
#include "nmtSlave.h"
#include "states.h"
#include "sysdep.h"

/*!
 ** put the slave in the state wanted by the master
 **
 ** @param d
 ** @param m
 **/
void proceedNMTstateChange(Message *m) {
    if (ObjDict_Data.nodeState == Pre_operational || ObjDict_Data.nodeState == Operational || ObjDict_Data.nodeState == Stopped) {

        MSG_WAR(0x3400, "NMT received. for node :  ", (*m).data[1]);

        /* Check if this NMT-message is for this node */
        /* byte 1 = 0 : all the nodes are concerned (broadcast) */

        if (((*m).data[1] == 0) || ((*m).data[1] == ObjDict_bDeviceNodeId)) {

            switch ((*m).data[0]) { /* command specifier (cs) */
                case NMT_Start_Node:
                    if ((ObjDict_Data.nodeState == Pre_operational) || (ObjDict_Data.nodeState == Stopped))
                        setState(Operational);
                    break;

                case NMT_Stop_Node:
                    if (ObjDict_Data.nodeState == Pre_operational || ObjDict_Data.nodeState == Operational)
                        setState(Stopped);
                    break;

                case NMT_Enter_PreOperational:
                    if (ObjDict_Data.nodeState == Operational || ObjDict_Data.nodeState == Stopped)
                        setState(Pre_operational);
                    break;

                case NMT_Reset_Node:
//          if(ObjDict_Data.NMT_Slave_Node_Reset_Callback != NULL)
//             ObjDict_Data.NMT_Slave_Node_Reset_Callback();
                    setState(Initialisation);
                    break;

                case NMT_Reset_Comunication: {
//                    UNS8 currentNodeId = getNodeId();

//             if(ObjDict_Data.NMT_Slave_Communications_Reset_Callback != NULL)
//                ObjDict_Data.NMT_Slave_Communications_Reset_Callback();
#ifdef CO_ENABLE_LSS
                    // LSS changes NodeId here in case lss_transfer.nodeID doesn't 
                    // match current getNodeId()
                    if(currentNodeId!=ObjDict_Data.lss_transfer.nodeID)
                    currentNodeId = ObjDict_Data.lss_transfer.nodeID;
#endif
#ifdef CO_ENABLE_CHANGE_NODE_ID
                    // clear old NodeId to make SetNodeId reinitializing
                    // SDO, EMCY and other COB Ids
                    *ObjDict_Data.bDeviceNodeId = 0xFF;

                    setNodeId(currentNodeId);
#endif
                }
                    setState(Initialisation);
                    break;

            }/* end switch */

        }/* end if( ( (*m).data[1] == 0 ) || ( (*m).data[1] ==
         bDeviceNodeId ) ) */
    }
}

/*!
 **
 **
 ** @param d
 **
 ** @return
 **/
UNS8 slaveSendBootUp() {
    Message m;

#ifdef CO_ENABLE_LSS
    if(*ObjDict_Data.bDeviceNodeId==0xFF)return 0;
#endif

    MSG_WAR(0x3407, "Send a Boot-Up msg ", 0);

    /* message configuration */
    {
        UNS16 tmp = NODE_GUARD << 7 | ObjDict_bDeviceNodeId;
        m.cob_id = UNS16_LE(tmp);
    }
    m.rtr = NOT_A_REQUEST;
    m.len = 1;
    m.data[0] = 0x00;

    return canSend(&m);
}

