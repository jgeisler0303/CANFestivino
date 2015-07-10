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
#include "pdo.h"
#include "objacces.h"
#include "sysdep.h"

/*!
 ** @file   pdo.c
 ** @author Edouard TISSERANT and Francis DUPIN
 ** @date   Tue Jun  5 09:32:32 2007
 **
 ** @brief
 **
 **
 */

/*!
 **
 **
 ** @param d
 ** @param TPDO_com TPDO communication parameters OD entry
 ** @param TPDO_map TPDO mapping parameters OD entry
 **
 ** @return
 **/

UNS8 buildPDO(UNS8 numPdo, Message * pdo) {
    UNS8 size;
    ODCallback_t *Callback;

    const subindex *TPDO_com = ObjDict_scanIndexOD(0x1800 + numPdo, &size, &Callback);
    const subindex *TPDO_map = ObjDict_scanIndexOD(0x1A00 + numPdo, &size, &Callback);

    UNS8 prp_j = 0x00;
    UNS32 offset = 0x00000000;
    const UNS8 *pMappingCount = (UNS8 *) TPDO_map[0].pObject;

    pdo->cob_id = (UNS16) UNS16_LE(*(UNS16*)TPDO_com[1].pObject & 0x7FF);
    pdo->rtr = NOT_A_REQUEST;
    ObjDict_Data.currentPDO = numPdo;

    MSG_WAR(0x3009, "  PDO CobId is : ", *(UNS32 *) TPDO_com[1].pObject);
    MSG_WAR(0x300D, "  Number of objects mapped : ", *pMappingCount);

    do {
        UNS8 dataType; /* Unused */
        UNS8 tmp[] = { 0, 0, 0, 0, 0, 0, 0, 0 }; /* temporary space to hold bits */

        /* pointer fo the var which holds the mapping parameter of an mapping entry  */
        UNS32 *pMappingParameter = (UNS32 *) TPDO_map[prp_j + 1].pObject;
        UNS16 index = (UNS16) ((*pMappingParameter) >> 16);
        UNS32 Size = (UNS32) (*pMappingParameter & (UNS32) 0x000000FF); /* Size in bits */

        /* get variable only if Size != 0 and Size is lower than remaining bits in the PDO */
        if (Size && ((offset + Size) <= 64)) {
            UNS32 ByteSize = 1 + ((Size - 1) >> 3); /*1->8 => 1 ; 9->16 => 2, ... */
            UNS8 subIndex = (UNS8) (((*pMappingParameter) >> (UNS8) 8) & (UNS32) 0x000000FF);

            MSG_WAR(0x300F, "  got mapping parameter : ", *pMappingParameter);
            MSG_WAR(0x3050, "    at index : ", 0x1A00+numPdo);
            MSG_WAR(0x3051, "    sub-index : ", prp_j + 1);

            if (getODentry (index, subIndex, tmp, &ByteSize, &dataType, 0) != OD_SUCCESSFUL) {
                MSG_ERR(0x1013, " Couldn't find mapped variable at index-subindex-size : ", (UNS32) (*pMappingParameter));
                return 0xFF;
            }
            /* copy bit per bit in little endian */
            CopyBits((UNS8) Size, ((UNS8 *) tmp), 0, 0, (UNS8 *) &pdo->data[offset >> 3], (UNS8) (offset % 8), 0);

            offset += Size;
        }
        prp_j++;
    } while (prp_j < *pMappingCount);

    pdo->len = (UNS8) (1 + ((offset - 1) >> 3));

    MSG_WAR(0x3015, "  End scan mapped variable", 0);

    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param cobId
 **
 ** @return
 **/
UNS8 sendPDOrequest(UNS16 RPDOIndex) {
    UNS16 *pwCobId;
    const subindex *si;
    UNS8 size;
    ODCallback_t *callbacks;

    if (!ObjDict_Data.CurrentCommunicationState.csPDO) {
        return 0;
    }

    /* Sending the request only if the cobid have been found on the PDO
     receive */
    /* part dictionary */

    MSG_WAR(0x3930, "sendPDOrequest RPDO Index : ", RPDOIndex);

    si = ObjDict_scanIndexOD(RPDOIndex, &size, &callbacks);

    if (si) {
        /* get the CobId */
        pwCobId = (UNS16 *) si[1].pObject;

        MSG_WAR(0x3930, "sendPDOrequest cobId is : ", *pwCobId);
        {
            Message pdo;
            pdo.cob_id = UNS16_LE(*pwCobId);
            pdo.rtr = REQUEST;
            pdo.len = 0;
            return canSend(&pdo);
        }
    }
    MSG_ERR(0x1931, "sendPDOrequest : RPDO Index not found : ", RPDOIndex);
    return 0xFF;
}

/*!
 **
 **
 ** @param d
 ** @param m
 **
 ** @return
 **/
UNS8 proceedPDO(Message * m) {
    UNS8 numMap; /* Number of the mapped varable */
    UNS8 pMappingCount; /* count of mapped objects... */
    /* pointer to the var which is mapped to a pdo... */
    /*  void *     pMappedAppObject = NULL;   */
    /* pointer fo the var which holds the mapping parameter of an
     mapping entry */
    UNS32 pMappingParameter;
    UNS8 pTransmissionType; /* pointer to the transmission
     type */
    UNS16 pwCobId;
    UNS8 ode_size;
    UNS32 objDict;
    const subindex *param_si, *map_si;
    UNS8 si_size;
    ODCallback_t *callbacks;

    UNS8 data_offset = 0;

    MSG_WAR(0x3935, "proceedPDO, cobID : ", (UNS16_LE(m->cob_id) & 0x7ff));
    numMap = 0;

    if ((*m).rtr == NOT_A_REQUEST) {
        for (UNS8 numPdo = 0x00; 1; numPdo++) {
            if (!(param_si = ObjDict_scanIndexOD(0x1400 + numPdo, &si_size, &callbacks)))
                break;

            pwCobId = *(UNS16 *) param_si[1].pObject;
            if (pwCobId == UNS16_LE(m->cob_id)) {
                /* The cobId is recognized */
                MSG_WAR(0x3936, "cobId found at index ", RPDOIndex);
            } else {
                continue;
            }

            /* Get Mapped Objects Number */
            /* The cobId of the message received has been found in the
             dictionnary. */
            map_si = ObjDict_scanIndexOD(0x1600 + numPdo, &si_size, &callbacks);
            pMappingCount = *(UNS8 *) map_si[0].pObject;
            for (numMap = 0; numMap < pMappingCount; numMap++) {
                UNS8 tmp[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                UNS32 ByteSize;
                pMappingParameter = *(UNS32 *) map_si[numMap + 1].pObject;
                if (pMappingParameter == 0) {
                    MSG_ERR(0x1937, "Couldn't get mapping parameter : ", numMap + 1);
                    return 0xFF;
                }
                /* Get the addresse of the mapped variable. */
                /* detail of *pMappingParameter : */
                /* The 16 hight bits contains the index, the medium 8 bits
                 contains the subindex, */
                /* and the lower 8 bits contains the size of the mapped
                 variable. */

                ode_size = (UNS8) (pMappingParameter & (UNS32) 0x000000FF);

                /* set variable only if Size != 0 and
                 * Size is lower than remaining bits in the PDO */
                if (ode_size && ((data_offset + ode_size) <= (m->len << 3))) {
                    /* copy bit per bit in little endian */
                    CopyBits(ode_size, (UNS8 *) &m->data[data_offset >> 3], data_offset % 8, 0, ((UNS8 *) tmp), 0, 0);
                    /*1->8 => 1 ; 9->16 =>2, ... */
                    ByteSize = (UNS32) (1 + ((ode_size - 1) >> 3));

                    objDict =
                            setODentry((UNS16) (pMappingParameter >> 16), (UNS8) ((pMappingParameter >> 8) & 0xFF), tmp, &ByteSize, 0);

                    if (objDict != OD_SUCCESSFUL) {
                        MSG_ERR(0x1938, "error accessing to the mapped var : ", numMap + 1);
                        MSG_WAR(0x2939, "         Mapped at index : ", pMappingParameter >> 16);
                        MSG_WAR(0x2940, "                subindex : ", (pMappingParameter >> 8) & 0xFF);
                        return 0xFF;
                    }

                    MSG_WAR(0x3942, "Variable updated by PDO cobid : ", UNS16_LE(m->cob_id));
                    MSG_WAR(0x3943, "         Mapped at index : ", pMappingParameter >> 16);
                    MSG_WAR(0x3944, "                subindex : ", (pMappingParameter >> 8) & 0xFF);
                    data_offset += ode_size;
                }
            } /* end loop while on mapped variables */
//       if (ObjDict_Data.RxPDO_EventTimers) {
// 	  TIMEVAL EventTimerDuration = *(UNS16 *)ObjDict_objdict[offsetObjdict].pSubindex[5].pObject;
// 	  if(EventTimerDuration){
// 	      DelAlarm (ObjDict_Data.RxPDO_EventTimers[numPdo]);
// 	      ObjDict_Data.RxPDO_EventTimers[numPdo] = SetAlarm (numPdo, ObjDict_Data.RxPDO_EventTimers_Handler, MS_TO_TIMEVAL (EventTimerDuration), 0);
// 	  }
//       }
            return 0;
        } /* end while */
    } else if ((*m).rtr == REQUEST) {
        MSG_WAR(0x3946, "Receive a PDO request cobId : ", UNS16_LE(m->cob_id));
        for (UNS8 numPdo = 0x00; 1; numPdo++) {
            if (!(param_si = ObjDict_scanIndexOD(0x1400 + numPdo, &si_size, &callbacks)))
                break;

            /* study of all PDO stored in the objects dictionary */
            /* get CobId of the dictionary which match to the received PDO
             */
            pwCobId = *(UNS16 *) param_si[1].pObject;
            if (pwCobId != UNS16_LE(m->cob_id)) {
                continue;
            }

            pTransmissionType = *(UNS8 *) param_si[2].pObject;
            /* If PDO is to be sampled and send on RTR, do it */
            if ((pTransmissionType == TRANS_RTR)) {
            } else if (pTransmissionType == TRANS_RTR_SYNC) {
                /* RTR_SYNC means data prepared at SYNC, transmitted on RTR */
                if (ObjDict_PDO_status[numPdo].transmit_type_parameter & PDO_RTR_SYNC_READY) {
                    /*Data ready, just send */
                    canSend(&ObjDict_PDO_status[numPdo].last_message);
                    return 0;
                } else {
                    /* if SYNC did never occur, transmit current data */
                    /* DS301 do not tell what to do in such a case... */
                    MSG_ERR(0x1947, "Not ready RTR_SYNC TPDO send current data : ", UNS16_LE(m->cob_id));
                }
            } else if ((pTransmissionType == TRANS_EVENT_PROFILE) || (pTransmissionType == TRANS_EVENT_SPECIFIC)) {
                /* Zap all timers and inhibit flag */
                ObjDict_PDO_status[numPdo].event_timer = DelAlarm(ObjDict_PDO_status[numPdo].event_timer);
                ObjDict_PDO_status[numPdo].inhibit_timer = DelAlarm(ObjDict_PDO_status[numPdo].inhibit_timer);
                ObjDict_PDO_status[numPdo].transmit_type_parameter &= ~PDO_INHIBITED;
                /* Call  PDOEventTimerAlarm for this TPDO,
                 * this will trigger emission et reset timers */
                PDOEventTimerAlarm(numPdo);
                return 0;
            } else {
                /* The requested PDO is not to send on request. So, does
                 nothing. */
                MSG_WAR(0x2947, "PDO is not to send on request : ", UNS16_LE(m->cob_id));
                return 0xFF;
            }

            Message pdo;
            if (buildPDO(numPdo, &pdo)) {
                MSG_ERR(0x1948, " Couldn't build TPDO number : ", numPdo);
                return 0xFF;
            }
            canSend(&pdo);

            return 0;
        } /* end while */
    } /* end if Requete */

    return 0;
}

/*!
 **
 **
 ** @param NbBits
 ** @param SrcByteIndex
 ** @param SrcBitIndex
 ** @param SrcBigEndian
 ** @param DestByteIndex
 ** @param DestBitIndex
 ** @param DestBigEndian
 **/
void CopyBits(UNS8 NbBits, UNS8 * SrcByteIndex, UNS8 SrcBitIndex,
UNS8 SrcBigEndian, UNS8 * DestByteIndex, UNS8 DestBitIndex,
UNS8 DestBigEndian) {
    /* This loop copy as many bits that it can each time, crossing */
    /* successively bytes */
    // boundaries from LSB to MSB.
    while (NbBits > 0) {
        /* Bit missalignement between src and dest */
        INTEGER8 Vect = DestBitIndex - SrcBitIndex;

        /* We can now get src and align it to dest */
        UNS8 Aligned = Vect > 0 ? *SrcByteIndex << Vect : *SrcByteIndex >> -Vect;

        /* Compute the nb of bit we will be able to copy */
        UNS8 BoudaryLimit = (Vect > 0 ? 8 - DestBitIndex : 8 - SrcBitIndex);
        UNS8 BitsToCopy = BoudaryLimit > NbBits ? NbBits : BoudaryLimit;

        /* Create a mask that will serve in: */
        UNS8 Mask = ((0xff << (DestBitIndex + BitsToCopy)) | (0xff >> (8 - DestBitIndex)));

        /* - Filtering src */
        UNS8 Filtered = Aligned & ~Mask;

        /* - and erase bits where we write, preserve where we don't */
        *DestByteIndex &= Mask;

        /* Then write. */
        *DestByteIndex |= Filtered;

        /*Compute next time cursors for src */
        if ((SrcBitIndex += BitsToCopy) > 7) /* cross boundary ? */
        {
            SrcBitIndex = 0; /* First bit */
            SrcByteIndex += (SrcBigEndian ? -1 : 1); /* Next byte */
        }

        /*Compute next time cursors for dest */
        if ((DestBitIndex += BitsToCopy) > 7) {
            DestBitIndex = 0; /* First bit */
            DestByteIndex += (DestBigEndian ? -1 : 1); /* Next byte */
        }

        /*And decrement counter. */
        NbBits -= BitsToCopy;
    }

}

static void sendPdo(UNS32 pdoNum, Message * pdo) {
    /*store_as_last_message */
    ObjDict_PDO_status[pdoNum].last_message = *pdo;
    MSG_WAR(0x396D, "sendPDO cobId :", UNS16_LE(pdo->cob_id));
    MSG_WAR(0x396E, "     Nb octets  : ", pdo->len);

    canSend(pdo);
}

/*!
 **
 **
 ** @param d
 **
 ** @return
 **/

UNS8 sendPDOevent() {
    /* Calls _sendPDOevent specifying it is not a sync event */
    return _sendPDOevent(0);
}

UNS8 sendOnePDOevent(UNS8 pdoNum) {
    const subindex *param_si;
    UNS8 si_size;
    ODCallback_t *callbacks;
    Message pdo;

    if (!ObjDict_Data.CurrentCommunicationState.csPDO || (ObjDict_PDO_status[pdoNum].transmit_type_parameter & PDO_INHIBITED)) {
        return 0;
    }

    param_si = ObjDict_scanIndexOD(0x1800 + pdoNum, &si_size, &callbacks);

    MSG_WAR(0x3968, "  PDO is on EVENT. Trans type : ", *((UNS8 *) param_si[2].pObject));

    memset(&pdo, 0, sizeof(pdo));
    if (buildPDO(pdoNum, &pdo)) {
        MSG_ERR(0x3907, " Couldn't build TPDO number : ", pdoNum);
        return 0;
    }

    /*Compare new and old PDO */
    if (!ObjDict_PDO_status[pdoNum].event_trigger) {
        return 0;
    } else {
        TIMEVAL EventTimerDuration;
        TIMEVAL InhibitTimerDuration;

        MSG_WAR(0x306A, "Changes TPDO number : ", pdoNum);
        /* Changes detected -> transmit message */
        EventTimerDuration = *(UNS16 *) param_si[5].pObject;
        InhibitTimerDuration = *(UNS16 *) param_si[3].pObject;

        UNS8 pTransmissionType = *((UNS8 *) param_si[2].pObject);

        if ((pTransmissionType == TRANS_EVENT_PROFILE) || (pTransmissionType == TRANS_EVENT_SPECIFIC)) {
            /* Start both event_timer and inhibit_timer */
            if (EventTimerDuration) {
                DelAlarm(ObjDict_PDO_status[pdoNum].event_timer);
                ObjDict_PDO_status[pdoNum].event_timer =
                        SetAlarm(pdoNum, &PDOEventTimerAlarm, MS_TO_TIMEVAL(EventTimerDuration), 0);
            }

            if (InhibitTimerDuration) {
                DelAlarm(ObjDict_PDO_status[pdoNum].inhibit_timer);
                ObjDict_PDO_status[pdoNum].inhibit_timer =
                        SetAlarm(pdoNum, &PDOInhibitTimerAlarm, MS_TO_TIMEVAL(InhibitTimerDuration / 10), 0);
                /* and inhibit TPDO */
                ObjDict_PDO_status[pdoNum].transmit_type_parameter |=
                PDO_INHIBITED;
            }
        }
        ObjDict_PDO_status[pdoNum].event_trigger = 0;
        sendPdo(pdoNum, &pdo);
    }
    return 1;
}

void PDOEventTimerAlarm(UNS8 pdoNum) {
    /* This is needed to avoid deletion of re-attribuated timer */
    ObjDict_PDO_status[pdoNum].event_timer = TIMER_NONE;
    /* force emission of PDO by artificially changing last emitted */
    ObjDict_PDO_status[pdoNum].event_trigger = 1;
    sendOnePDOevent((UNS8) pdoNum);
}

void PDOInhibitTimerAlarm(UNS8 pdoNum) {
    /* This is needed to avoid deletion of re-attribuated timer */
    ObjDict_PDO_status[pdoNum].inhibit_timer = TIMER_NONE;
    /* Remove inhibit flag */
    ObjDict_PDO_status[pdoNum].transmit_type_parameter &= ~PDO_INHIBITED;
    sendOnePDOevent((UNS8) pdoNum);
}

// void _RxPDO_EventTimers_Handler(UNS32 pdoNum) {}

/*!
 **
 **
 ** @param d
 ** @param isSyncEvent
 **
 ** @return
 **/

UNS8 _sendPDOevent(UNS8 isSyncEvent) {
    ; /* number of the actual processed pdo-nr. */
    UNS8 pTransmissionType;
    const subindex *param_si;
    UNS8 si_size;
    ODCallback_t *callbacks;

    if (!ObjDict_Data.CurrentCommunicationState.csPDO) {
        return 0;
    }

    /* study all PDO stored in the objects dictionary */
    Message pdo;/* = Message_Initializer;*/
    for (UNS8 pdoNum = 0x00; 1; pdoNum++) {
        if (!(param_si = ObjDict_scanIndexOD(0x1800 + pdoNum, &si_size, &callbacks)))
            break;

        if (*(UNS16 *) param_si[1].pObject & 0x8000) {
            MSG_WAR(0x3960, "Not a valid PDO ", 0x1800 + pdoNum);
            continue;
            /*Go next TPDO */
        }
        /* get the PDO transmission type */
        pTransmissionType = *(UNS8 *) param_si[2].pObject;
        MSG_WAR(0x3962, "Reading PDO at index : ", 0x1800 + pdoNum);

        /* check if transmission type is SYNCRONOUS */
        /* message transmited every n SYNC with n=TransmissionType */
        if (isSyncEvent && (pTransmissionType >= TRANS_SYNC_MIN) && (pTransmissionType <= TRANS_SYNC_MAX)
                && (++ObjDict_PDO_status[pdoNum].transmit_type_parameter == pTransmissionType)) {
            /*Reset count of SYNC */
            ObjDict_PDO_status[pdoNum].transmit_type_parameter = 0;
            MSG_WAR(0x3964, "  PDO is on SYNCHRO. Trans type : ", pTransmissionType);

            memset(&pdo, 0, sizeof(pdo));
            if (buildPDO(pdoNum, &pdo)) {
                MSG_ERR(0x1906, " Couldn't build TPDO number : ", pdoNum);
                continue;
            }
            sendPdo(pdoNum, &pdo);
            /* If transmission RTR, with data sampled on SYNC */
        } else if (isSyncEvent && (pTransmissionType == TRANS_RTR_SYNC)) {
            if (buildPDO(pdoNum, &ObjDict_PDO_status[pdoNum].last_message)) {
                MSG_ERR(0x1966, " Couldn't build TPDO number : ", pdoNum);
                ObjDict_PDO_status[pdoNum].transmit_type_parameter &= ~PDO_RTR_SYNC_READY;
            } else {
                ObjDict_PDO_status[pdoNum].transmit_type_parameter |=
                PDO_RTR_SYNC_READY;
            }
            continue;
            /* If transmission on Event and not inhibited, check for changes */
        } else if ((isSyncEvent && (pTransmissionType == TRANS_SYNC_ACYCLIC))
                || (!isSyncEvent
                        && (pTransmissionType == TRANS_EVENT_PROFILE || pTransmissionType == TRANS_EVENT_SPECIFIC)
                        && !(ObjDict_PDO_status[pdoNum].transmit_type_parameter & PDO_INHIBITED))) {
            sendOnePDOevent(pdoNum);
        } else {
            MSG_WAR(0x306C, "  PDO is not on EVENT or synchro or not at this SYNC. Trans type : ", *pTransmissionType);
        }
    } /* end for */
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param OD_entry
 ** @param bSubindex
 ** @return always 0
 **/

UNS32 TPDO_Communication_Parameter_Callback(const subindex * OD_entry,
UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess) {
    /* If PDO are actives */
    if (!writeAccess)
        return 0;

    if (ObjDict_Data.CurrentCommunicationState.csPDO)
        switch (bSubindex) {
            case 2: /* Changed transmition type */
            case 3: /* Changed inhibit time */
            case 5: /* Changed event time */
            {
                UNS8 numPdo = (UNS8) (bIndex - 0x1800); /* number of the actual processed pdo-nr. */

                /* Zap all timers and inhibit flag */
                ObjDict_PDO_status[numPdo].event_timer = DelAlarm(ObjDict_PDO_status[numPdo].event_timer);
                ObjDict_PDO_status[numPdo].inhibit_timer = DelAlarm(ObjDict_PDO_status[numPdo].inhibit_timer);
                ObjDict_PDO_status[numPdo].transmit_type_parameter = 0;
                /* Call  PDOEventTimerAlarm for this TPDO, this will trigger emission et reset timers */
                PDOEventTimerAlarm(numPdo);
                return 0;
            }

            default: /* other subindex are ignored */
                break;
        }
    return 0;
}

void PDOInit() {
    /* For each TPDO mapping parameters */
    UNS16 pdoIndex = 0x1800; /* OD index of TDPO */
    UNS8 size;
    ODCallback_t *CallbackList;

    while (ObjDict_scanIndexOD(pdoIndex, &size, &CallbackList)) {
        if (CallbackList) {
            /*Assign callbacks to corresponding subindex */
            /* Transmission type */
            CallbackList[2] = &TPDO_Communication_Parameter_Callback;
            /* Inhibit time */
            CallbackList[3] = &TPDO_Communication_Parameter_Callback;
            /* Event timer */
            CallbackList[5] = &TPDO_Communication_Parameter_Callback;
        }
        pdoIndex++;
    }

    /* Trigger a non-sync event */
    _sendPDOevent(0);
}

void PDOStop() {
    /* For each TPDO mapping parameters */
    UNS8 pdoNum = 0x00; /* number of the actual processed pdo-nr. */
    UNS16 pdoIndex = 0x1800; /* OD index of TDPO */
    UNS8 size;
    ODCallback_t *CallbackList;

    // TODO: introduce number_of_txpds const
    while (ObjDict_scanIndexOD(pdoIndex, &size, &CallbackList)) {
        /* Delete TPDO timers */
        ObjDict_PDO_status[pdoNum].event_timer = DelAlarm(ObjDict_PDO_status[pdoNum].event_timer);
        ObjDict_PDO_status[pdoNum].inhibit_timer = DelAlarm(ObjDict_PDO_status[pdoNum].inhibit_timer);
        /* Reset transmit type parameter */
        ObjDict_PDO_status[pdoNum].transmit_type_parameter = 0;
        ObjDict_PDO_status[pdoNum].last_message.cob_id = 0;
        pdoNum++;
        pdoIndex++;
    }
}
