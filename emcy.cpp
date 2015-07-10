/*
 This file is part of CanFestival, a library implementing CanOpen
 Stack.

 Copyright (C): Edouard TISSERANT and Francis DUPIN
 Modified by: Jaroslav Fojtik

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
 ** @file   emcy.c
 ** @author Luis Jimenez
 ** @date   Wed Sep 26 2007
 **
 ** @brief Definitions of the functions that manage EMCY (emergency) messages
 **
 **
 */

#include "data.h"
#include "emcy.h"
#include "sysdep.h"

#define Data data  /* temporary fix */

/*! This is called when Index 0x1003 is updated.
 **
 **
 ** @param d
 ** @param unsused_indextable
 ** @param unsused_bSubindex
 **
 ** @return
 **/
UNS32 OnNumberOfErrorsUpdate(const subindex * OD_entry, UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess) {
    UNS8 index;

    if (!writeAccess)
        return 0;
    // if 0, reset Pre-defined Error Field
    // else, don't change and give an abort message (eeror code: 0609 0030h)
    if (error_number == 0)
        for (index = 0; index < ObjDict_Data.error_history_size; ++index)
            *(error_first_element + index) = 0; /* clear all the fields in Pre-defined Error Field (1003h) */
    else
        ;  // abort message
    return 0;
}

/*! start the EMCY mangagement.
 **
 **
 ** @param d
 **/
void emergencyInit() {
    RegisterSetODentryCallBack(0x1003, 0x00, &OnNumberOfErrorsUpdate);

    error_number = 0;
}

/*!
 **
 **
 ** @param d
 **/
void emergencyStop() {

}

/*!
 **
 ** @param d
 ** @param cob_id
 **
 ** @return
 **/
UNS8 sendEMCY(UNS16 errCode, UNS8 errRegister, const void *Specific, UNS8 SpecificLength) {
    Message m;

    MSG_WAR(0x3051, "sendEMCY", 0);

    m.cob_id = error_cobid;
    m.rtr = NOT_A_REQUEST;
    m.Data[0] = errCode & 0xFF; /* LSB */
    m.Data[1] = (errCode >> 8) & 0xFF; /* MSB */
    m.Data[2] = errRegister;

    if (Specific == NULL) {
        m.Data[3] = 0; /* Manufacturer specific Error Field omitted */
        m.Data[4] = 0;
        m.Data[5] = 0;
        m.Data[6] = 0;
        m.Data[7] = 0;
        SpecificLength = 5;
    } else {
        if (SpecificLength > 5)
            SpecificLength = 5;
        memcpy(&m.Data[3], Specific, SpecificLength);
    }
    m.len = SpecificLength + 3;

    return canSend(&m);
}

/*! Sets a new error with code errCode. Also sets corresponding bits in Error register (1001h)
 **                                                                                                 
 **  
 ** @param d
 ** @param errCode Code of the error                                                                                        
 ** @param errRegister Bits of Error register (1001h) to be set.
 ** @return 1 if error, 0 if successful
 */
UNS8 EMCY_setError(UNS16 errCode, UNS8 errRegMask, UNS16 addInfo) {
    UNS8 index;
    UNS8 errRegister_tmp;

    for (index = 0; index < EMCY_MAX_ERRORS; ++index) {
        if (ObjDict_Data.error_data[index].errCode == errCode) { /* error already registered */
            if (ObjDict_Data.error_data[index].active) {
                MSG_WAR(0x3052, "EMCY message already sent", 0);
                return 0;
            } else
                ObjDict_Data.error_data[index].active = 1; /* set as active error */
            break;
        }
    }

    if (index == EMCY_MAX_ERRORS) /* if errCode not already registered */
        for (index = 0; index < EMCY_MAX_ERRORS; ++index)
            if (ObjDict_Data.error_data[index].active == 0)
                break; /* find first inactive error */

    if (index == EMCY_MAX_ERRORS) /* error_data full */
    {
        MSG_ERR(0x3053, "error_data full", 0);
        return 1;
    }

    ObjDict_Data.error_data[index].errCode = errCode;
    ObjDict_Data.error_data[index].errRegMask = errRegMask;
    ObjDict_Data.error_data[index].active = 1;

    /* set the new state in the error state machine */
    ObjDict_Data.error_state = Error_occurred;

    /* set Error Register (1001h) */
    for (index = 0, errRegister_tmp = 0; index < EMCY_MAX_ERRORS; ++index)
        if (ObjDict_Data.error_data[index].active == 1)
            errRegister_tmp |= ObjDict_Data.error_data[index].errRegMask;
	
    error_register = errRegister_tmp;

    /* set Pre-defined Error Field (1003h) */
    for (index = ObjDict_Data.error_history_size - 1; index > 0; --index)
        *(error_first_element + index) = *(error_first_element + index - 1);
    
    *(error_first_element) = errCode | ((UNS32) addInfo << 16);
    
    if (error_number < ObjDict_Data.error_history_size)
        ++(error_number);

    /* send EMCY message */
    if (ObjDict_Data.CurrentCommunicationState.csEmergency)
        return sendEMCY(errCode, error_register, NULL, 0);
    else
        return 1;
}

/*! Deletes error errCode. Also clears corresponding bits in Error register (1001h)
 **                                                                                                 
 **  
 ** @param d
 ** @param errCode Code of the error                                                                                        
 ** @param errRegister Bits of Error register (1001h) to be set.
 ** @return 1 if error, 0 if successful
 */
void EMCY_errorRecovered(UNS16 errCode) {
    UNS8 index;
    UNS8 errRegister_tmp;
    UNS8 anyActiveError = 0;

    for (index = 0; index < EMCY_MAX_ERRORS; ++index)
        if (ObjDict_Data.error_data[index].errCode == errCode)
            break; /* find the position of the error */

    if ((index != EMCY_MAX_ERRORS) && (ObjDict_Data.error_data[index].active == 1)) {
        ObjDict_Data.error_data[index].active = 0;

        /* set Error Register (1001h) and check error state machine */
        for (index = 0, errRegister_tmp = 0; index < EMCY_MAX_ERRORS; ++index)
            if (ObjDict_Data.error_data[index].active == 1) {
                anyActiveError = 1;
                errRegister_tmp |= ObjDict_Data.error_data[index].errRegMask;
            }
        if (anyActiveError == 0) {
            ObjDict_Data.error_state = Error_free;
            /* send a EMCY message with code "Error Reset or No Error" */
            if (ObjDict_Data.CurrentCommunicationState.csEmergency)
                sendEMCY(0x0000, 0x00, NULL, 0);
        }
        error_register = errRegister_tmp;
    } else
        MSG_WAR(0x3054, "recovered error was not active", 0);
}

/*! This function is responsible to process an EMCY canopen-message.
 **
 **
 ** @param d
 ** @param m The CAN-message which has to be analysed.
 **
 **/
void proceedEMCY(Message* m) {
//    UNS8 nodeID;
//    UNS16 errCode;
//    UNS8 errReg;

    MSG_WAR(0x3055, "EMCY received. Proceed. ", 0);

    /* Test if the size of the EMCY is ok */
    if (m->len != 8) {
        MSG_ERR(0x1056, "Error size EMCY. CobId  : ", m->cob_id);
        return;
    }

    /* post the received EMCY */
//    nodeID = m->cob_id & 0x7F;
//    errCode = m->Data[0] | ((UNS16) m->Data[1] << 8);
//    errReg = m->Data[2];
    // (*ObjDict_Data.post_emcy)(nodeID, errCode, errReg);
}

void _post_emcy(UNS8 nodeID, UNS16 errCode, UNS8 errReg) {
}
