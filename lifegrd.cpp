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
 ** @file   lifegrd.c
 ** @author Edouard TISSERANT
 ** @date   Mon Jun  4 17:19:24 2007
 **
 ** @brief
 **
 **
 */

#include "data.h"
#include "lifegrd.h"
#include "sysdep.h"

// e_nodeState getNodeState (UNS8 nodeId)
// {
//   e_nodeState networkNodeState = Unknown_state;
//   #if NMT_MAX_NODE_ID>0
//   if(nodeId < NMT_MAX_NODE_ID)
//     networkNodeState = ObjDict_Data.NMTable[nodeId];
//   #endif
//   return networkNodeState;
// }

/*! 
 ** The Consumer Timer Callback
 **
 ** @param d
 ** @param id
 * @ingroup heartbeato
 **/
void ConsumerHeartbeatAlarm(UNS32 id) {
#ifdef CO_ENABLE_CONSUMER_HEART_BEAT
    UNS8 nodeId = (UNS8)(((ConsumerHeartbeatEntries[id]) & (UNS32)0x00FF0000) >> (UNS8)16);
    /*MSG_WAR(0x00, "ConsumerHearbeatAlarm", 0x00);*/

    /* timer have been notified and is now free (non periodic)*/
    /* -> avoid deleting re-assigned timer if message is received too late*/
    ObjDict_Data.ConsumerHeartBeatTimers[id]=TIMER_NONE;

    /* set node state */
    ObjDict_Data.NMTable[nodeId] = Disconnected;
    /*! call heartbeat error with NodeId */
    (*ObjDict_Data.heartbeatError)(nodeId);
#endif
}

void proceedNODE_GUARD(Message* m) {
#ifdef CO_ENABLE_NODE_GUARD
    UNS8 nodeId = (UNS8) GET_NODE_ID((*m));

    if((m->rtr == 1) )
    /*!
     ** Notice that only the master can have sent this
     ** node guarding request
     */
    {
        /*!
         ** Receiving a NMT NodeGuarding (request of the state by the
         ** master)
         ** Only answer to the NMT NodeGuarding request, the master is
         ** not checked (not implemented)
         */
        if (nodeId == *ObjDict_Data.bDeviceNodeId )
        {
            Message msg;
            UNS16 tmp = *ObjDict_Data.bDeviceNodeId + 0x700;
            msg.cob_id = UNS16_LE(tmp);
            msg.len = (UNS8)0x01;
            msg.rtr = 0;
            msg.data[0] = ObjDict_Data.nodeState;
            if (ObjDict_Data.toggle)
            {
                msg.data[0] |= 0x80;
                ObjDict_Data.toggle = 0;
            }
            else
            ObjDict_Data.toggle = 1;
            /* send the nodeguard response. */
            MSG_WAR(0x3130, "Sending NMT Nodeguard to master, state: ", ObjDict_Data.nodeState);
            canSend(&msg );
        }

    } else { /* Not a request CAN */
        /* The state is stored on 7 bit */
        e_nodeState newNodeState = (e_nodeState) ((*m).data[0] & 0x7F);

        MSG_WAR(0x3110, "Received NMT nodeId : ", nodeId);

        /*!
         ** Record node response for node guarding service
         */
        ObjDict_Data.nodeGuardStatus[nodeId] = *ObjDict_Data.LifeTimeFactor;

        if (ObjDict_Data.NMTable[nodeId] != newNodeState)
        {
            (*ObjDict_Data.post_SlaveStateChange)(nodeId, newNodeState);
            /* the slave's state receievd is stored in the NMTable */
            ObjDict_Data.NMTable[nodeId] = newNodeState;
        }

        /* Boot-Up frame reception */
        if ( ObjDict_Data.NMTable[nodeId] == Initialisation)
        {
            /*
             ** The device send the boot-up message (Initialisation)
             ** to indicate the master that it is entered in
             ** pre_operational mode
             */
            MSG_WAR(0x3100, "The NMT is a bootup from node : ", nodeId);
            /* call post SlaveBootup with NodeId */
            (*ObjDict_Data.post_SlaveBootup)(nodeId);
        }

        if( ObjDict_Data.NMTable[nodeId] != Unknown_state ) {
            UNS8 index, ConsumerHeartBeat_nodeId;
            for( index = (UNS8)0x00; index < ConsumerHeartbeatCount; index++ )
            {
                ConsumerHeartBeat_nodeId = (UNS8)( ((ConsumerHeartbeatEntries[index]) & (UNS32)0x00FF0000) >> (UNS8)16 );
                if ( nodeId == ConsumerHeartBeat_nodeId )
                {
                    TIMEVAL time = ( (ConsumerHeartbeatEntries[index]) & (UNS32)0x0000FFFF );
                    /* Renew alarm for next heartbeat. */
                    DelAlarm(ObjDict_Data.ConsumerHeartBeatTimers[index]);
                    ObjDict_Data.ConsumerHeartBeatTimers[index] = SetAlarm(index, &ConsumerHeartbeatAlarm, MS_TO_TIMEVAL(time), 0);
                }
            }
        }
    }
#endif
}

/*! The Producer Timer Callback
 **
 **
 ** @param d
 ** @param id
 * @ingroup heartbeato
 **/
void ProducerHeartbeatAlarm(UNS8 id) {
    if (ProducerHeartBeatTime) {
        Message msg;
        /* Time expired, the heartbeat must be sent immediately
         ** generate the correct node-id: this is done by the offset 1792
         ** (decimal) and additionaly
         ** the node-id of this device.
         */
        UNS16 tmp = (UNS16) ObjDict_bDeviceNodeId + 0x700;
//       UNS16 tmp = 0x724;
        msg.cob_id = UNS16_LE(tmp);
        msg.len = (UNS8) 0x01;
        msg.rtr = 0;
        msg.data[0] = ObjDict_Data.nodeState; /* No toggle for heartbeat !*/
        /* send the heartbeat */
        MSG_WAR(0x3130, "Producing heartbeat: ", ObjDict_Data.nodeState);
        canSend(&msg);

    } else {
        ObjDict_Data.ProducerHeartBeatTimer = DelAlarm(ObjDict_Data.ProducerHeartBeatTimer);
    }
}

/**
 * @brief The guardTime - Timer Callback.
 * 
 * This function is called every GuardTime (OD 0x100C) ms <br>
 * On every call, a NodeGuard-Request is sent to all nodes which have a
 * node-state not equal to "Unknown" (according to NMTable). If the node has
 * not responded within the lifetime, the nodeguardError function is called and
 * the status of this node is set to "Disconnected"
 *
 * @param d 	Pointer on a CAN object data structure 
 * @param id
 * @ingroup nodeguardo
 */
void GuardTimeAlarm(UNS32 id) {
#ifdef CO_ENABLE_NODE_GUARD
    if (*ObjDict_Data.GuardTime) {
        UNS8 i;

        MSG_WAR(0x00, "Producing nodeguard-requests: ", 0);

        for (i = 0; i < NMT_MAX_NODE_ID; i++) {
            /** Send node guard request to all nodes except this node, if the 
             * node state is not "Unknown_state"
             */
            if (ObjDict_Data.NMTable[i] != Unknown_state && i != *ObjDict_Data.bDeviceNodeId) {

                /** Check if the node has confirmed the guarding request within
                 * the LifeTime (GuardTime x LifeTimeFactor)
                 */
                if (ObjDict_Data.nodeGuardStatus[i] <= 0) {

                    MSG_WAR(0x00, "Node Guard alarm for nodeId : ", i);

                    // Call error-callback function
                    if (*ObjDict_Data.nodeguardError) {
                        (*ObjDict_Data.nodeguardError)(i);
                    }

                    // Mark node as disconnected
                    ObjDict_Data.NMTable[i] = Disconnected;

                }

                ObjDict_Data.nodeGuardStatus[i]--;

                masterSendNMTnodeguard(i);

            }
        }
    } else {
        ObjDict_Data.GuardTimeTimer = DelAlarm(ObjDict_Data.GuardTimeTimer);
    }
#endif
}

/**
 * This function is called, if index 0x100C or 0x100D is updated to
 * restart the node-guarding service with the new parameters
 *
 * @param d 	Pointer on a CAN object data structure 
 * @param unused_indextable
 * @param unused_bSubindex
 * @ingroup nodeguardo
 */
UNS32 OnNodeGuardUpdate(const subindex * OD_entry, UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess) {
#ifdef CO_ENABLE_NODE_GUARD
    nodeguardStop();
    nodeguardInit();
#endif
    return 0;
}

/*! This is called when Index 0x1017 is updated.
 **
 **
 ** @param d
 ** @param unused_indextable
 ** @param unused_bSubindex
 **
 ** @return
 * @ingroup heartbeato
 **/
UNS32 OnHeartbeatProducerUpdate(const subindex * OD_entry, UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess) {
    if (!writeAccess)
        return 0;

    heartbeatStop();
    heartbeatInit();
    return 0;
}

void heartbeatInit() {

//    UNS8 index; /* Index to scan the table of heartbeat consumers */
    RegisterSetODentryCallBack(0x1017, 0x00, &OnHeartbeatProducerUpdate);

    ObjDict_Data.toggle = 0;

#ifdef CO_ENABLE_CONSUMER_HEART_BEAT
    for( index = (UNS8)0x00; index < ConsumerHeartbeatCount; index++ )
    {
        TIMEVAL time = (UNS16) ( (ConsumerHeartbeatEntries[index]) & (UNS32)0x0000FFFF );
        if ( time )
        {
            ObjDict_Data->ConsumerHeartBeatTimers[index] = SetAlarm(index, &ConsumerHeartbeatAlarm, MS_TO_TIMEVAL(time), 0);
        }
    }
#endif
    if ( ProducerHeartBeatTime) {
        TIMEVAL time = ProducerHeartBeatTime;
        ObjDict_Data.ProducerHeartBeatTimer =
                SetAlarm(0, &ProducerHeartbeatAlarm, MS_TO_TIMEVAL(time), MS_TO_TIMEVAL(time));
    }
}

void nodeguardInit() {
#ifdef CO_ENABLE_NODE_GUARD

    RegisterSetODentryCallBack(0x100C, 0x00, &OnNodeGuardUpdate);
    RegisterSetODentryCallBack(0x100D, 0x00, &OnNodeGuardUpdate);

    if (*ObjDict_Data.GuardTime && *ObjDict_Data.LifeTimeFactor) {
        UNS8 i;

        TIMEVAL time = *ObjDict_Data.GuardTime;
        ObjDict_Data.GuardTimeTimer = SetAlarm(0, &GuardTimeAlarm, MS_TO_TIMEVAL(time), MS_TO_TIMEVAL(time));
        MSG_WAR(0x0, "GuardTime: ", time);

        for (i = 0; i < NMT_MAX_NODE_ID; i++) {
            /** Set initial value for the nodes */
            if (ObjDict_Data.NMTable[i] != Unknown_state && i != *ObjDict_Data.bDeviceNodeId) {
                ObjDict_Data.nodeGuardStatus[i] = *ObjDict_Data.LifeTimeFactor;
            }
        }

        MSG_WAR(0x0, "Timer for node-guarding startet", 0);
    }

#endif
}

void heartbeatStop() {
#ifdef CO_ENABLE_CONSUMER_HEART_BEAT
    UNS8 index;
    for( index = (UNS8)0x00; index < ConsumerHeartbeatCount; index++ )
    {
        ObjDict_Data->ConsumerHeartBeatTimers[index] = DelAlarm(ObjDict_Data->ConsumerHeartBeatTimers[index]);
    }
#endif
    ObjDict_Data.ProducerHeartBeatTimer = DelAlarm(ObjDict_Data.ProducerHeartBeatTimer);
}

void nodeguardStop() {
    ObjDict_Data.GuardTimeTimer = DelAlarm(ObjDict_Data.GuardTimeTimer);
}

void lifeGuardInit() {
    heartbeatInit();
    nodeguardInit();
}

void lifeGuardStop() {
    heartbeatStop();
    nodeguardStop();
}

void _heartbeatError(UNS8 heartbeatID) {
}
void _post_SlaveBootup(UNS8 SlaveID) {
}
void _post_SlaveStateChange(UNS8 nodeId, e_nodeState newNodeState) {
}
void _nodeguardError(UNS8 id) {
}

