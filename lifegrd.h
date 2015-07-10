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

/** @defgroup heartbeato Heartbeat Object
 *  The heartbeat mechanism for a device is established through cyclically transmitting a message by a
 *	heartbeat producer. One or more devices in the network are aware of this heartbeat message. If the
 *	heartbeat cycle fails for the heartbeat producer the local application on the heartbeat consumer will be
 *	informed about that event.
 *  @ingroup comobj
 */

/** @defgroup nodeguardo Node-guarding Object
 *  The node-guarding mechanism for a device is established through cyclically polling all slaves by the NMT
 *    	master. If one polled slave does not respond during a specified time (LifeTime), the local application
 * 	will be informed about that event.<br>
 *	It is also possible for the slaves to monitor the node-guarding requests coming from the master to
 * 	determine, if the master operates in a right way
 *  @ingroup comobj
 *
 * @todo The implementation is very basic. The toggle bit of the nodes confirmation is not checked at the moment
 */

/**
 ** @file   lifegrd.h
 ** @author Markus WILDBOLZ
 ** @date   Mon Oct 01 14:44:36 CEST 2012 
 **
 ** @brief
 **
 **
 */

#ifndef __lifegrd_h__
#define __lifegrd_h__

#include "applicfg.h"
#include "states.h"

typedef void (*heartbeatError_t)(UNS8);
void _heartbeatError(UNS8 heartbeatID);

typedef void (*post_SlaveBootup_t)(UNS8);
void _post_SlaveBootup(UNS8 SlaveID);

typedef void (*post_SlaveStateChange_t)(UNS8, e_nodeState);
void _post_SlaveStateChange(UNS8 nodeId, e_nodeState newNodeState);

typedef void (*nodeguardError_t)(UNS8);
void _nodeguardError(UNS8 id);

#include "data.h"

/*************************************************************************
 * Functions
 *************************************************************************/
/** 
 * @brief Start node guarding with respect to 0x100C and 0x100D
 * in the object dictionary
 * 
 * @param *d Pointer on a CAN object data structure
 * @ingroup nodeguardo
 */
void nodeguardInit();

/** 
 * @brief Stop producing node guarding messages
 *
 * @param *d Pointer on a CAN object data structure
 * @ingroup nodeguardo
 */
void nodeguardStop();

/** 
 * @brief Start the life guarding service (heartbeat/node guarding).
 * This service handles NMT error control messages either by using
 * heartbeats and/or by using node guarding messages (defined via the
 * object dictionary)
 *
 * @param *d Pointer on a CAN object data structure
 */
void lifeGuardInit();

/** 
 * @brief Stop the life guarding service (heartbeat/node guarding).
 *
 * @param *d Pointer on a CAN object data structure
 */
void lifeGuardStop();

/** 
 * @ingroup statemachine
 * @brief To read the state of a node
 * This can be used by the master after having sent a life guard request,
 * of by any node if it is waiting for heartbeat.
 * @param *d Pointer on a CAN object data structure
 * @param nodeId Id of a node
 * @return e_nodeState State of the node corresponding to the nodeId
 */
e_nodeState getNodeState(UNS8 nodeId);

/** 
 * @brief Start heartbeat consumer and producer
 * with respect to 0x1016 and 0x1017
 * object dictionary entries
 * @param *d Pointer on a CAN object data structure
 * @ingroup heartbeato
 */
void heartbeatInit();

/** 
 * @brief Stop heartbeat consumer and producer
 * @param *d Pointer on a CAN object data structure
 * @ingroup heartbeato
 */
void heartbeatStop();

/** 
 * @brief This function is responsible to process a canopen-message which seams to be an NMT Error Control
 * Messages.
 * If a BootUp message is detected, it will return the nodeId of the Slave who booted up
 * @param *d Pointer on a CAN object data structure 
 * @param *m Pointer on the CAN-message which has to be analysed.
 * @ingroup nodeguardo
 */
void proceedNODE_GUARD(Message* m);

#endif /*__lifegrd_h__ */
