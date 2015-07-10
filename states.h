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

/** @defgroup statemachine State Machine
 *  @ingroup userapi
 */

#ifndef __states_h__
#define __states_h__

#include "applicfg.h"
#include "CO_can.h"

/* The nodes states 
 * -----------------
 * values are choosen so, that they can be sent directly
 * for heartbeat messages...
 * Must be coded on 7 bits only
 * */
/* Should not be modified */
enum enum_nodeState {
    Initialisation = 0x00,
    Disconnected = 0x01,
    Connecting = 0x02,
    Preparing = 0x02,
    Stopped = 0x04,
    Operational = 0x05,
    Pre_operational = 0x7F,
    Unknown_state = 0x0F
}__attribute__ ((packed));

typedef enum enum_nodeState e_nodeState;

typedef struct {
    INTEGER8 csBoot_Up :1;INTEGER8 csSDO :1;INTEGER8 csEmergency :1;INTEGER8 csSYNC :1;INTEGER8 csLifeGuard :1;INTEGER8 csPDO :1;INTEGER8 csLSS :1;
} s_state_communication;

// TODO: ugly circle include
#include "data.h"

/** 
 * @brief Function that user app can overload
 * @ingroup statemachine
 */
typedef void (*initialisation_t)();
typedef void (*preOperational_t)();
typedef void (*operational_t)();
typedef void (*stopped_t)();

/** 
 * @ingroup statemachine
 * @brief Function that user app can overload
 * @param *d Pointer on a CAN object data structure
 */
// void _initialisation();
/** 
 * @ingroup statemachine
 * @brief Function that user app can overload
 * @param *d Pointer on a CAN object data structure
 */
// void _preOperational();
/**
 * @ingroup statemachine 
 * @brief Function that user app can overload
 * @param *d Pointer on a CAN object data structure
 */
// void _operational();
/** 
 * @ingroup statemachine
 * @brief Function that user app can overload
 * @param *d Pointer on a CAN object data structure
 */
// void _stopped();

/************************* prototypes ******************************/

/** 
 * @brief Called by driver/app when receiving messages
 * @param *d Pointer on a CAN object data structure
 * @param *m Pointer on a CAN message structure
 */
void canDispatch(Message *m);

/** 
 * @ingroup statemachine
 * @brief Returns the state of the node
 * @param *d Pointer on a CAN object data structure
 * @return The node state
 */
e_nodeState getState();

/** 
 * @ingroup statemachine
 * @brief Change the state of the node 
 * @param *d Pointer on a CAN object data structure
 * @param newState The state to assign
 * @return 
 */
UNS8 setState(e_nodeState newState);

/**
 * @ingroup statemachine 
 * @brief Returns the nodId 
 * @param *d Pointer on a CAN object data structure
 * @return
 */
UNS8 getNodeId();

#ifdef CO_ENABLE_CHANGE_NODE_ID
/** 
 * @ingroup statemachine
 * @brief Define the node ID. Initialize the object dictionary
 * @param *d Pointer on a CAN object data structure
 * @param nodeId The node ID to assign
 */
void setNodeId (UNS8 nodeId);
#endif

/** 
 * @brief Some stuff to do when the node enter in pre-operational mode
 * @param *d Pointer on a CAN object data structure
 */
void initPreOperationalMode();

#endif
