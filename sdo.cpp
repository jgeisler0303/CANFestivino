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
 ** @file   sdo.c
 ** @author Edouard TISSERANT and Francis DUPIN
 ** @date   Tue Jun  5 09:32:32 2007
 **
 ** @brief
 **
 **
 */

/* #define DEBUG_WAR_CONSOLE_ON */
/* #define DEBUG_ERR_CONSOLE_ON */

#include <stdlib.h>

#include "applicfg.h"
#include "data.h"
#include "CO_timer.h"
#include "objacces.h"
#include "sysdep.h"

/* Uncomment if your compiler does not support inline functions */
#define NO_INLINE

#ifdef NO_INLINE
#define INLINE
#else
#define INLINE inline
#endif

/*Internals prototypes*/

UNS8 GetSDOClientFromNodeId(UNS8 nodeId);

/*!
 ** Called by writeNetworkDict
 **
 ** @param d
 ** @param nodeId
 ** @param index
 ** @param subIndex
 ** @param count
 ** @param dataType
 ** @param data
 ** @param Callback
 ** @param endianize
 **
 ** @return
 **/
INLINE UNS8 _writeNetworkDict(UNS8 nodeId, UNS16 index,
UNS8 subIndex, UNS32 count, UNS8 dataType, void *data, SDOCallback_t Callback, UNS8 endianize, UNS8 useBlockMode);

/*!
 ** Called by readNetworkDict
 **
 ** @param d
 ** @param nodeId
 ** @param index
 ** @param subIndex
 ** @param dataType
 ** @param Callback
 **
 ** @return
 **/
INLINE UNS8 _readNetworkDict(UNS8 nodeId, UNS16 index, UNS8 subIndex,
UNS8 dataType, SDOCallback_t Callback, UNS8 useBlockMode);

/***************************************************************************/
/* SDO (un)packing macros */

/** Returns the command specifier (cs, ccs, scs) from the first byte of the SDO
 */
#define getSDOcs(byte) (byte >> 5)

/** Returns the number of bytes without data from the first byte of the SDO. Coded in 2 bits
 */
#define getSDOn2(byte) ((byte >> 2) & 3)

/** Returns the number of bytes without data from the first byte of the SDO. Coded in 3 bits
 */
#define getSDOn3(byte) ((byte >> 1) & 7)

/** Returns the transfer type from the first byte of the SDO
 */
#define getSDOe(byte) ((byte >> 1) & 1)

/** Returns the size indicator from the first byte of the SDO
 */
#define getSDOs(byte) (byte & 1)

/** Returns the indicator of end transmission from the first byte of the SDO
 */
#define getSDOc(byte) (byte & 1)

/** Returns the toggle from the first byte of the SDO
 */
#define getSDOt(byte) ((byte >> 4) & 1)

/** Returns the index from the bytes 1 and 2 of the SDO
 */
#define getSDOindex(byte1, byte2) (((UNS16)byte2 << 8) | ((UNS16)byte1))

/** Returns the subIndex from the byte 3 of the SDO
 */
#define getSDOsubIndex(byte3) (byte3)

/** Returns the subcommand in SDO block transfer
 */
#define getSDOblockSC(byte) (byte & 3)

/*!
 **
 **
 ** @param d
 ** @param id
 **/
void SDOTimeoutAlarm(UNS8 id) {
    UNS8 nodeId;
    const subindex *si;
    UNS8 si_size;
    ODCallback_t *callbacks;
    /* Get the client->server cobid.*/
    if (!(si = ObjDict_scanIndexOD(0x1280 + ObjDict_Data.transfers[id].CliServNbr, &si_size, &callbacks))) {
        return;
    }
    nodeId = (UNS8) *((UNS8*) si[3].pObject);
    MSG_ERR(0x1A01, "SDO timeout. SDO response not received.", 0);
    MSG_WAR(0x2A02, "server node id : ", nodeId);
    MSG_WAR(0x2A02, "         index : ", ObjDict_Data.transfers[id].index);
    MSG_WAR(0x2A02, "      subIndex : ", ObjDict_Data.transfers[id].subIndex);
    /* Reset timer handler */
    ObjDict_Data.transfers[id].timer = TIMER_NONE;
    /*Set aborted state*/
    ObjDict_Data.transfers[id].state = SDO_ABORTED_INTERNAL;
    /* Sending a SDO abort */
    sendSDOabort(ObjDict_Data.transfers[id].whoami, ObjDict_Data.transfers[id].CliServNbr, ObjDict_Data.transfers[id].index, ObjDict_Data.transfers[id].subIndex, SDOABT_TIMED_OUT);
    ObjDict_Data.transfers[id].abortCode = SDOABT_TIMED_OUT;
    /* Call the user function to inform of the problem.*/
    if (ObjDict_Data.transfers[id].Callback)
        /*If ther is a callback, it is responsible to close SDO transfer (client)*/
        (*(ObjDict_Data.transfers[id].Callback))(nodeId);
    /*Reset the line if (whoami == SDO_SERVER) or the callback did not close the line.
     Otherwise this sdo transfer would never be closed. */
    if (ObjDict_Data.transfers[id].abortCode == SDOABT_TIMED_OUT)
        resetSDOline((UNS8) id);
}

#define StopSDO_TIMER(id) \
	MSG_WAR(0x3A05, "StopSDO_TIMER for line : ", line);\
	ObjDict_Data.transfers[id].timer = DelAlarm(ObjDict_Data.transfers[id].timer);

#define StartSDO_TIMER(id) \
	MSG_WAR(0x3A06, "StartSDO_TIMER for line : ", line);\
	ObjDict_Data.transfers[id].timer = SetAlarm(id,&SDOTimeoutAlarm,MS_TO_TIMEVAL(SDO_TIMEOUT_MS),0);

#define RestartSDO_TIMER(id) \
	MSG_WAR(0x3A07, "restartSDO_TIMER for line : ", line);\
if(ObjDict_Data.transfers[id].timer != TIMER_NONE) { StopSDO_TIMER(id) StartSDO_TIMER(id) }

/*!
 ** Reset all sdo buffers
 **
 ** @param d
 **/
void resetSDO() {
    UNS8 j;

    /* transfer structure initialization */
    for (j = 0; j < SDO_MAX_SIMULTANEOUS_TRANSFERS; j++)
        resetSDOline(j);
}

/*!
 **
 **
 ** @param d
 ** @param line
 **
 ** @return
 **/
UNS32 SDOlineToObjdict(UNS8 line) {
    UNS32 size;
    UNS32 errorCode;
    MSG_WAR(0x3A08, "Enter in SDOlineToObjdict ", line);
    /* if SDO initiated with e=0 and s=0 count is null, offset carry effective size*/
    if (ObjDict_Data.transfers[line].count == 0)
        ObjDict_Data.transfers[line].count = ObjDict_Data.transfers[line].offset;
    size = ObjDict_Data.transfers[line].count;

#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
    if (size > SDO_MAX_LENGTH_TRANSFER)
    {
        errorCode = setODentry(ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex,
                (void *) ObjDict_Data.transfers[line].dynamicData, &size, 1);
    }
    else
    {
        errorCode = setODentry(ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex,
                (void *) ObjDict_Data.transfers[line].data, &size, 1);
    }
#else //SDO_DYNAMIC_BUFFER_ALLOCATION
    errorCode =
            setODentry(ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex, (void * ) ObjDict_Data.transfers[line].data, &size, 1);
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

    if (errorCode != OD_SUCCESSFUL)
        return errorCode;
    MSG_WAR(0x3A08, "exit of SDOlineToObjdict ", line);
    return 0;

}

/*!
 **
 **
 ** @param d
 ** @param line
 **
 ** @return
 **/
UNS32 objdictToSDOline(UNS8 line) {
    UNS32 size = SDO_MAX_LENGTH_TRANSFER;
    UNS8 dataType;
    UNS32 errorCode;

    MSG_WAR(0x3A05, "objdict->line index : ", ObjDict_Data.transfers[line].index);
    MSG_WAR(0x3A06, "  subIndex : ", ObjDict_Data.transfers[line].subIndex);

#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
    /* Try to use the static buffer.                                            */
    errorCode = getODentry(ObjDict_Data.transfers[line].index,
            ObjDict_Data.transfers[line].subIndex,
            (void *)ObjDict_Data.transfers[line].data,
            &size, &dataType, 1);
    if (errorCode == SDOABT_OUT_OF_MEMORY) {
        /* The static buffer is too small, try again using a dynamic buffer.      *
         * 'size' now contains the real size of the requested object.             */
        if (size <= SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE) {
            ObjDict_Data.transfers[line].dynamicData = (UNS8 *) malloc(size * sizeof(UNS8));
            if (ObjDict_Data.transfers[line].dynamicData != NULL) {
                ObjDict_Data.transfers[line].dynamicDataSize = size;
                errorCode = getODentry(ObjDict_Data.transfers[line].index,
                        ObjDict_Data.transfers[line].subIndex,
                        (void *) ObjDict_Data.transfers[line].dynamicData,
                        &ObjDict_Data.transfers[line].dynamicDataSize,
                        &dataType,
                        1);
            }
        }
    }
#else //SDO_DYNAMIC_BUFFER_ALLOCATION
    errorCode =
            getODentry(ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex, (void * )ObjDict_Data.transfers[line].data, &size, &dataType, 1);
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

    if (errorCode != OD_SUCCESSFUL)
        return errorCode;

    ObjDict_Data.transfers[line].count = size;
    ObjDict_Data.transfers[line].offset = 0;

    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param line
 ** @param nbBytes
 ** @param data
 **
 ** @return
 **/
UNS8 lineToSDO(UNS8 line, UNS32 nbBytes, UNS8* data) {
    UNS8 i;
    UNS32 offset;

#ifndef SDO_DYNAMIC_BUFFER_ALLOCATION
    if ((ObjDict_Data.transfers[line].offset + nbBytes) > SDO_MAX_LENGTH_TRANSFER) {
        MSG_ERR(0x1A10, "SDO Size of data too large. Exceed SDO_MAX_LENGTH_TRANSFER", nbBytes);
        return 0xFF;
    }
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

    if ((ObjDict_Data.transfers[line].offset + nbBytes) > ObjDict_Data.transfers[line].count) {
        MSG_ERR(0x1A11, "SDO Size of data too large. Exceed count", nbBytes);
        return 0xFF;
    }
    offset = ObjDict_Data.transfers[line].offset;
#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
    if (ObjDict_Data.transfers[line].count <= SDO_MAX_LENGTH_TRANSFER)
    {
        for (i = 0; i < nbBytes; i++)
        * (data + i) = ObjDict_Data.transfers[line].data[offset + i];
    }
    else
    {
        if (ObjDict_Data.transfers[line].dynamicData == NULL)
        {
            MSG_ERR(0x1A11,"SDO's dynamic buffer not allocated. Line", line);
            return 0xFF;
        }
        for (i = 0; i < nbBytes; i++)
        * (data + i) = ObjDict_Data.transfers[line].dynamicData[offset + i];
    }
#else //SDO_DYNAMIC_BUFFER_ALLOCATION
    for (i = 0; i < nbBytes; i++)
        *(data + i) = ObjDict_Data.transfers[line].data[offset + i];
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION
    ObjDict_Data.transfers[line].offset = ObjDict_Data.transfers[line].offset + nbBytes;
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param line
 ** @param nbBytes
 ** @param data
 **
 ** @return
 **/
UNS8 SDOtoLine(UNS8 line, UNS32 nbBytes, UNS8* data) {
    UNS8 i;
    UNS32 offset;
#ifndef SDO_DYNAMIC_BUFFER_ALLOCATION
    if ((ObjDict_Data.transfers[line].offset + nbBytes) > SDO_MAX_LENGTH_TRANSFER) {
        MSG_ERR(0x1A15, "SDO Size of data too large. Exceed SDO_MAX_LENGTH_TRANSFER", nbBytes);
        return 0xFF;
    }
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

    offset = ObjDict_Data.transfers[line].offset;
#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
    {
        UNS8* lineData = ObjDict_Data.transfers[line].data;
        if ((ObjDict_Data.transfers[line].offset + nbBytes) > SDO_MAX_LENGTH_TRANSFER) {
            if (ObjDict_Data.transfers[line].dynamicData == NULL) {
                ObjDict_Data.transfers[line].dynamicData = (UNS8*) malloc(SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE);
                ObjDict_Data.transfers[line].dynamicDataSize = SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE;

                if (ObjDict_Data.transfers[line].dynamicData == NULL) {
                    MSG_ERR(0x1A15,"SDO allocating dynamic buffer failed, size", SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE);
                    return 0xFF;
                }
                //Copy present data
                memcpy(ObjDict_Data.transfers[line].dynamicData, ObjDict_Data.transfers[line].data, offset);
            }
            else if ((ObjDict_Data.transfers[line].offset + nbBytes) > ObjDict_Data.transfers[line].dynamicDataSize)
            {
                UNS8* newDynamicBuffer = (UNS8*) realloc(ObjDict_Data.transfers[line].dynamicData, ObjDict_Data.transfers[line].dynamicDataSize + SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE);
                if (newDynamicBuffer == NULL) {
                    MSG_ERR(0x1A15,"SDO reallocating dynamic buffer failed, size", ObjDict_Data.transfers[line].dynamicDataSize + SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE);
                    return 0xFF;
                }
                ObjDict_Data.transfers[line].dynamicData = newDynamicBuffer;
                ObjDict_Data.transfers[line].dynamicDataSize += SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE;
            }
            lineData = ObjDict_Data.transfers[line].dynamicData;
        }

        for (i = 0; i < nbBytes; i++)
        lineData[offset + i] = * (data + i);
    }
#else //SDO_DYNAMIC_BUFFER_ALLOCATION
    for (i = 0; i < nbBytes; i++)
        ObjDict_Data.transfers[line].data[offset + i] = *(data + i);
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

    ObjDict_Data.transfers[line].offset = ObjDict_Data.transfers[line].offset + nbBytes;
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param CliServNbr
 ** @param whoami
 ** @param index
 ** @param subIndex
 ** @param abortCode
 **
 ** @return
 **/
UNS8 failedSDO(UNS8 CliServNbr, UNS8 whoami, UNS16 index,
UNS8 subIndex, UNS32 abortCode) {
    UNS8 err;
    UNS8 line;
    err = getSDOlineOnUse(CliServNbr, whoami, &line);
    if (!err) /* If a line on use have been found.*/
        MSG_WAR(0x3A20, "FailedSDO : line found : ", line);
    if ((!err) && (whoami == SDO_SERVER)) {
        resetSDOline(line);
        MSG_WAR(0x3A21, "FailedSDO : line released : ", line);
    }
    if ((!err) && (whoami == SDO_CLIENT)) {
        StopSDO_TIMER(line);
        ObjDict_Data.transfers[line].state = SDO_ABORTED_INTERNAL;
        ObjDict_Data.transfers[line].abortCode = abortCode;
    }
    MSG_WAR(0x3A22, "Sending SDO abort ", 0);
    err = sendSDOabort(whoami, CliServNbr, index, subIndex, abortCode);
    if (err) {
        MSG_WAR(0x3A23, "Unable to send the SDO abort", 0);
        return 0xFF;
    }
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param line
 **/
void resetSDOline(UNS8 line) {
    UNS32 i;
    MSG_WAR(0x3A25, "reset SDO line nb : ", line);
    initSDOline(line, 0, 0, 0, SDO_RESET);
    for (i = 0; i < SDO_MAX_LENGTH_TRANSFER; i++)
        ObjDict_Data.transfers[line].data[i] = 0;
    ObjDict_Data.transfers[line].whoami = 0;
    ObjDict_Data.transfers[line].abortCode = 0;
}

/*!
 **
 **
 ** @param d
 ** @param line
 ** @param CliServNbr
 ** @param index
 ** @param subIndex
 ** @param state
 **
 ** @return
 **/
UNS8 initSDOline(UNS8 line, UNS8 CliServNbr, UNS16 index, UNS8 subIndex, UNS8 state) {
    MSG_WAR(0x3A25, "init SDO line nb : ", line);
    if (state == SDO_DOWNLOAD_IN_PROGRESS || state == SDO_UPLOAD_IN_PROGRESS || state == SDO_BLOCK_DOWNLOAD_IN_PROGRESS
            || state == SDO_BLOCK_UPLOAD_IN_PROGRESS) {
        StartSDO_TIMER(line)
    } else {
        StopSDO_TIMER(line)
    }
    ObjDict_Data.transfers[line].CliServNbr = CliServNbr;
    ObjDict_Data.transfers[line].index = index;
    ObjDict_Data.transfers[line].subIndex = subIndex;
    ObjDict_Data.transfers[line].state = state;
    ObjDict_Data.transfers[line].toggle = 0;
    ObjDict_Data.transfers[line].count = 0;
    ObjDict_Data.transfers[line].offset = 0;
    ObjDict_Data.transfers[line].peerCRCsupport = 0;
    ObjDict_Data.transfers[line].blksize = 0;
    ObjDict_Data.transfers[line].ackseq = 0;
    ObjDict_Data.transfers[line].objsize = 0;
    ObjDict_Data.transfers[line].lastblockoffset = 0;
    ObjDict_Data.transfers[line].seqno = 0;
    ObjDict_Data.transfers[line].endfield = 0;
    ObjDict_Data.transfers[line].rxstep = RXSTEP_INIT;
    ObjDict_Data.transfers[line].dataType = 0;
    ObjDict_Data.transfers[line].Callback = NULL;
#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
    free(ObjDict_Data.transfers[line].dynamicData);
    ObjDict_Data.transfers[line].dynamicData = 0;
    ObjDict_Data.transfers[line].dynamicDataSize = 0;
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param whoami
 ** @param line
 **
 ** @return
 **/
UNS8 getSDOfreeLine(UNS8 whoami, UNS8 *line) {

    UNS8 i;

    for (i = 0; i < SDO_MAX_SIMULTANEOUS_TRANSFERS; i++) {
        if (ObjDict_Data.transfers[i].state == SDO_RESET) {
            *line = i;
            ObjDict_Data.transfers[i].whoami = whoami;
            return 0;
        } /* end if */
    } /* end for */
    MSG_ERR(0x1A25, "Too many SDO in progress. Aborted.", i);
    return 0xFF;
}

/*!
 **
 **
 ** @param d
 ** @param CliServNbr
 ** @param whoami
 ** @param line
 **
 ** @return
 **/
UNS8 getSDOlineOnUse(UNS8 CliServNbr, UNS8 whoami, UNS8 *line) {

    UNS8 i;

    for (i = 0; i < SDO_MAX_SIMULTANEOUS_TRANSFERS; i++) {
        if ((ObjDict_Data.transfers[i].state != SDO_RESET) && (ObjDict_Data.transfers[i].state != SDO_ABORTED_INTERNAL)
                && (ObjDict_Data.transfers[i].CliServNbr == CliServNbr) && (ObjDict_Data.transfers[i].whoami == whoami)) {
            if (line)
                *line = i;
            return 0;
        }
    }
    return 0xFF;
}

/*!
 **
 **
 ** @param d
 ** @param CliServNbr
 ** @param whoami
 ** @param line
 **
 ** @return
 **/
UNS8 getSDOlineToClose(UNS8 CliServNbr, UNS8 whoami, UNS8 *line) {

    UNS8 i;

    for (i = 0; i < SDO_MAX_SIMULTANEOUS_TRANSFERS; i++) {
        if ((ObjDict_Data.transfers[i].state != SDO_RESET) && (ObjDict_Data.transfers[i].CliServNbr == CliServNbr)
                && (ObjDict_Data.transfers[i].whoami == whoami)) {
            if (line)
                *line = i;
            return 0;
        }
    }
    return 0xFF;
}

/*!
 **
 **
 ** @param d
 ** @param CliServNbr
 ** @param whoami
 **
 ** @return
 **/
UNS8 closeSDOtransfer(UNS8 nodeId, UNS8 whoami) {
    UNS8 err;
    UNS8 line;
    UNS8 CliNbr;
    /* First let's find the corresponding SDO client in our OD  */
    CliNbr = GetSDOClientFromNodeId(nodeId);
    if (CliNbr >= 0xFE)
        return SDO_ABORTED_INTERNAL;
    err = getSDOlineToClose(CliNbr, whoami, &line);
    if (err) {
        MSG_WAR(0x2A30, "No SDO communication to close", 0);
        return 0xFF;
    }
    resetSDOline(line);
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param line
 ** @param nbBytes
 **
 ** @return
 **/
UNS8 getSDOlineRestBytes(UNS8 line, UNS32 * nbBytes) {
    /* SDO initiated with e=0 and s=0 have count set to null */
    if (ObjDict_Data.transfers[line].count == 0)
        *nbBytes = 0;
    else
        *nbBytes = ObjDict_Data.transfers[line].count - ObjDict_Data.transfers[line].offset;
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param line
 ** @param nbBytes
 **
 ** @return
 **/
UNS8 setSDOlineRestBytes(UNS8 line, UNS32 nbBytes) {
#ifndef SDO_DYNAMIC_BUFFER_ALLOCATION
    if (nbBytes > SDO_MAX_LENGTH_TRANSFER) {
        MSG_ERR(0x1A35, "SDO Size of data too large. Exceed SDO_MAX_LENGTH_TRANSFER", nbBytes);
        return 0xFF;
    }
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

    ObjDict_Data.transfers[line].count = nbBytes;
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param whoami
 ** @param CliServNbr
 ** @param pData
 **
 ** @return
 **/
UNS8 sendSDO(UNS8 whoami, UNS8 CliServNbr, UNS8 *pData) {
    UNS8 i;
    Message m;
    const subindex *si;
    UNS8 si_size;
    ODCallback_t *callbacks;

    MSG_WAR(0x3A38, "sendSDO", 0);
    if (!((ObjDict_Data.nodeState == Operational) || (ObjDict_Data.nodeState == Pre_operational))) {
        MSG_WAR(0x2A39, "unable to send the SDO (not in op or pre-op mode", ObjDict_Data.nodeState);
        return 0xFF;
    }

    /*get the server->client cobid*/
    /* Get the client->server cobid.*/
    if (whoami == SDO_SERVER) {
        if (!(si = ObjDict_scanIndexOD(0x1200 + CliServNbr, &si_size, &callbacks))) {
            MSG_ERR(0x1A42, "SendSDO : SDO server not found", 0);
            return 0xFF;
        }
        m.cob_id = (UNS16) *((UNS32*) si[2].pObject);
        MSG_WAR(0x3A41, "I am server Tx cobId : ", m.cob_id);
    } else { /*case client*/
        /* Get the client->server cobid.*/
        if (!(si = ObjDict_scanIndexOD(0x1280 + CliServNbr, &si_size, &callbacks))) {
            MSG_ERR(0x1A42, "SendSDO : SDO client not found", 0);
            return 0xFF;
        }
        m.cob_id = (UNS16) *((UNS32*) si[1].pObject);
        MSG_WAR(0x3A41, "I am client Tx cobId : ", m.cob_id);
    }
    /* message copy for sending */
    m.rtr = NOT_A_REQUEST;
    /* the length of SDO must be 8 */
    m.len = 8;
    for (i = 0; i < 8; i++) {
        m.data[i] = pData[i];
    }
    return canSend(&m);
}

/*!
 **
 **
 ** @param d
 ** @param whoami
 ** @param index
 ** @param subIndex
 ** @param abortCode
 **
 ** @return
 **/
UNS8 sendSDOabort(UNS8 whoami, UNS8 CliServNbr, UNS16 index, UNS8 subIndex, UNS32 abortCode) {
    UNS8 data[8];
    UNS8 ret;

    MSG_WAR(0x2A50, "Sending SDO abort ", abortCode);
    data[0] = 0x80;
    /* Index */
    data[1] = index & 0xFF; /* LSB */
    data[2] = (index >> 8) & 0xFF; /* MSB */
    /* Subindex */
    data[3] = subIndex;
    /* Data */
    data[4] = (UNS8) (abortCode & 0xFF);
    data[5] = (UNS8) ((abortCode >> 8) & 0xFF);
    data[6] = (UNS8) ((abortCode >> 16) & 0xFF);
    data[7] = (UNS8) ((abortCode >> 24) & 0xFF);
    ret = sendSDO(whoami, CliServNbr, data);

    return ret;
}

/*!
 **
 **
 ** @param d
 ** @param m
 **
 ** @return
 **/
UNS8 proceedSDO(Message *m) {
    UNS8 err;
    UNS8 cs;
    UNS8 line;
    UNS32 nbBytes; /* received or to be transmited. */
//    UNS8 nodeId = 0; /* The node Id of the server if client otherwise unused */
    UNS8 CliServNbr;
    UNS8 whoami = SDO_UNKNOWN; /* SDO_SERVER or SDO_CLIENT.*/
    UNS32 errorCode; /* while reading or writing in the local object dictionary.*/
    UNS8 data[8]; /* data for SDO to transmit */
    UNS16 index;
    UNS8 subIndex;
//    UNS32 abortCode;
    UNS32 i;
    UNS8 j;
    UNS16 *pCobId = NULL;
    UNS8 SubCommand; /* Block transfer only */
    UNS8 SeqNo; /* Sequence number in block transfer */
    UNS8 AckSeq; /* Sequence number of last segment that was received successfully */
    UNS8 NbBytesNoData; /* Number of bytes that do not contain data in last segment of block transfer */
    const subindex *si;
    UNS8 si_size;
    ODCallback_t *callbacks;

    MSG_WAR(0x3A60, "proceedSDO ", 0);
    whoami = SDO_UNKNOWN;
    /* Looking for the cobId in the object dictionary. */
    /* Am-I a server ? */
    j = 0;
    while ((si = ObjDict_scanIndexOD(0x1200 + j, &si_size, &callbacks))) {
        if (si_size <= 1) {
            MSG_ERR(0x1A61, "Subindex 1  not found at index ", 0x1200 + j);
            return 0xFF;
        }
        /* Looking for the cobid received. */
        pCobId = (UNS16*) si[1].pObject;
        if (*pCobId == UNS16_LE(m->cob_id)) {
            whoami = SDO_SERVER;
            MSG_WAR(0x3A62, "proceedSDO. I am server. index : ", 0x1200 + j);
            /* Defining Server number = index minus 0x1200 where the cobid received is defined. */
            CliServNbr = j;
            break;
        }
        j++;
    } /* end while */
    if (whoami == SDO_UNKNOWN) {
        /* Am-I client ? */
        j = 0;
        while ((si = ObjDict_scanIndexOD(0x1280 + j, &si_size, &callbacks))) {
            if (si_size <= 3) {
                MSG_ERR(0x1A63, "Subindex 3  not found at index ", 0x1280 + j);
                return 0xFF;
            }
            /* Looking for the cobid received. */
            pCobId = (UNS16*) si[2].pObject;
            if (*pCobId == UNS16_LE(m->cob_id)) {
                whoami = SDO_CLIENT;
                MSG_WAR(0x3A64, "proceedSDO. I am client index : ", 0x1280 + j);
                /* Defining Client number = index minus 0x1280 where the cobid received is defined. */
                CliServNbr = j;
                /* Reading the server node ID, if client it is mandatory in the OD */
//                nodeId = *((UNS8*) si[3].pObject);
                break;
            }
            j++;
        } /* end while */
    }
    if (whoami == SDO_UNKNOWN) {
        return 0xFF;/* This SDO was not for us ! */
    }

    /* Test if the size of the SDO is ok */
    if ((*m).len != 8) {
        MSG_ERR(0x1A67, "Error size SDO", 0);
        failedSDO(CliServNbr, whoami, 0, 0, SDOABT_GENERAL_ERROR);
        return 0xFF;
    }

    if (whoami == SDO_CLIENT) {
        MSG_WAR(0x3A68, "I am CLIENT number ", CliServNbr);
    } else {
        MSG_WAR(0x3A69, "I am SERVER number ", CliServNbr);
    }

    /* Look for an SDO transfer already initiated. */
    err = getSDOlineOnUse(CliServNbr, whoami, &line);

    /* Let's find cs value, first it is set as "not valid" */
    cs = 0xFF;
    /* Special cases for block transfer : in frames with segment data cs is not spécified */
    if (!err) {
        if (((whoami == SDO_SERVER) && (ObjDict_Data.transfers[line].state == SDO_BLOCK_DOWNLOAD_IN_PROGRESS))
                || ((whoami == SDO_CLIENT) && (ObjDict_Data.transfers[line].state == SDO_BLOCK_UPLOAD_IN_PROGRESS))) {
            if (m->data[0] == 0x80) /* If first byte is 0x80 it is an abort frame (seqno = 0 not allowed) */
                cs = 4;
            else
                cs = 6;
        }
    }
    /* Other cases : cs is specified */
    if (cs == 0xFF)
        cs = getSDOcs(m->data[0]);

    /* Testing the command specifier */
    /* Allowed : cs = 0, 1, 2, 3, 4, 5, 6 */
    /* cs = other : Not allowed -> abort. */
    switch (cs) {

        case 0:
            /* I am SERVER */
            if (whoami == SDO_SERVER) {
                /* Receiving a download segment data : an SDO transfer should have been yet initiated. */
                if (!err)
                    err = ObjDict_Data.transfers[line].state != SDO_DOWNLOAD_IN_PROGRESS;
                if (err) {
                    MSG_ERR(0x1A70, "SDO error : Received download segment for unstarted trans. index 0x1200 + ", CliServNbr);
                    failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                /* Reset the wathdog */
                RestartSDO_TIMER(line)
                MSG_WAR(0x3A71, "Received SDO download segment defined at index 0x1200 + ", CliServNbr);
                index = ObjDict_Data.transfers[line].index;
                subIndex = ObjDict_Data.transfers[line].subIndex;
                /* Toggle test. */
                if (ObjDict_Data.transfers[line].toggle != getSDOt(m->data[0])) {
                    MSG_ERR(0x1A72, "SDO error : Toggle error : ", getSDOt(m->data[0]));
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_TOGGLE_NOT_ALTERNED);
                    return 0xFF;
                }
                /* Nb of data to be downloaded */
                nbBytes = 7 - getSDOn3(m->data[0]);
                /* Store the data in the transfer structure. */
                err = SDOtoLine(line, nbBytes, (*m).data + 1);
                if (err) {
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                    return 0xFF;
                }
                /* Sending the SDO response, CS = 1 */
                data[0] = (1 << 5) | (ObjDict_Data.transfers[line].toggle << 4);
                for (i = 1; i < 8; i++)
                    data[i] = 0;
                MSG_WAR(0x3A73, "SDO. Send response to download request defined at index 0x1200 + ", CliServNbr);
                sendSDO(whoami, CliServNbr, data);
                /* Inverting the toggle for the next segment. */
                ObjDict_Data.transfers[line].toggle = !ObjDict_Data.transfers[line].toggle & 1;
                /* If it was the last segment, */
                if (getSDOc(m->data[0])) {
                    /* Transfering line data to object dictionary. */
                    /* The code does not use the "d" of initiate frame. So it is safe if e=s=0 */
                    errorCode = SDOlineToObjdict(line);
                    if (errorCode) {
                        MSG_ERR(0x1A54, "SDO error : Unable to copy the data in the object dictionary", 0);
                        failedSDO(CliServNbr, whoami, index, subIndex, errorCode);
                        return 0xFF;
                    }
                    /* Release of the line */
                    resetSDOline(line);
                    MSG_WAR(0x3A74, "SDO. End of download defined at index 0x1200 + ", CliServNbr);
                }
            } /* end if SERVER */
#ifdef CO_ENABLE_SDO_CLIENT
            else { /* if CLIENT */
                /* I am CLIENT */
                /* It is a request for a previous upload segment. We should find a line opened for this.*/
                if (!err)
                err = ObjDict_Data.transfers[line].state != SDO_UPLOAD_IN_PROGRESS;
                if (err) {
                    MSG_ERR(0x1A75, "SDO error : Received segment response for unknown trans. from nodeId", nodeId);
                    failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                /* Reset the wathdog */
                RestartSDO_TIMER(line)
                index = ObjDict_Data.transfers[line].index;
                subIndex = ObjDict_Data.transfers[line].subIndex;
                /* test of the toggle; */
                if (ObjDict_Data.transfers[line].toggle != getSDOt(m->data[0])) {
                    MSG_ERR(0x1A76, "SDO error : Received segment response Toggle error. from nodeId", nodeId);
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_TOGGLE_NOT_ALTERNED);
                    return 0xFF;
                }
                /* nb of data to be uploaded */
                nbBytes = 7 - getSDOn3(m->data[0]);
                /* Storing the data in the line structure. */
                err = SDOtoLine(line, nbBytes, (*m).data + 1);
                if (err) {
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                    return 0xFF;
                }
                /* Inverting the toggle for the next segment. */
                ObjDict_Data.transfers[line].toggle = ! ObjDict_Data.transfers[line].toggle & 1;
                /* If it was the last segment,*/
                if ( getSDOc(m->data[0])) {
                    /* Put in state finished */
                    /* The code is safe for the case e=s=0 in initiate frame. */
                    StopSDO_TIMER(line)
                    ObjDict_Data.transfers[line].state = SDO_FINISHED;
                    if(ObjDict_Data.transfers[line].Callback) (*ObjDict_Data.transfers[line].Callback)(nodeId);

                    MSG_WAR(0x3A77, "SDO. End of upload from node : ", nodeId);
                }
                else { /* more segments to receive */
                    /* Sending the request for the next segment. */
                    data[0] = (3 << 5) | (ObjDict_Data.transfers[line].toggle << 4);
                    for (i = 1; i < 8; i++)
                    data[i] = 0;
                    sendSDO(whoami, CliServNbr, data);
                    MSG_WAR(0x3A78, "SDO send upload segment request to nodeId", nodeId);
                }
            } /* End if CLIENT */
#endif
            break;

        case 1:
            /* I am SERVER */
            /* Receive of an initiate download */
            if (whoami == SDO_SERVER) {
                index = getSDOindex(m->data[1], m->data[2]);
                subIndex = getSDOsubIndex(m->data[3]);
                MSG_WAR(0x3A79, "Received SDO Initiate Download (to store data) defined at index 0x1200 + ", CliServNbr);
                MSG_WAR(0x3A80, "Writing at index : ", index);
                MSG_WAR(0x3A80, "Writing at subIndex : ", subIndex);

                /* Search if a SDO transfer have been yet initiated */
                if (!err) {
                    MSG_ERR(0x1A81, "SDO error : Transmission yet started.", 0);
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                /* No line on use. Great ! */
                /* Try to open a new line. */
                err = getSDOfreeLine(whoami, &line);
                if (err) {
                    MSG_ERR(0x1A82, "SDO error : No line free, too many SDO in progress. Aborted.", 0);
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                initSDOline(line, CliServNbr, index, subIndex, SDO_DOWNLOAD_IN_PROGRESS);

                if (getSDOe(m->data[0])) { /* If SDO expedited */
                    /* nb of data to be downloaded */
                    nbBytes = 4 - getSDOn2(m->data[0]);
                    /* Storing the data in the line structure. */
                    ObjDict_Data.transfers[line].count = nbBytes;
                    err = SDOtoLine(line, nbBytes, (*m).data + 4);

                    if (err) {
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }

                    /* SDO expedited -> transfer finished. Data can be stored in the dictionary. */
                    /*The line will be reseted when it is downloading in the dictionary. */
                    MSG_WAR(0x3A83, "SDO Initiate Download is an expedited transfer. Finished. ", 0);
                    /* Transfering line data to object dictionary. */
                    errorCode = SDOlineToObjdict(line);
                    if (errorCode) {
                        MSG_ERR(0x1A84, "SDO error : Unable to copy the data in the object dictionary", 0);
                        failedSDO(CliServNbr, whoami, index, subIndex, errorCode);
                        return 0xFF;
                    }
                    /* Release of the line. */
                    resetSDOline(line);
                } else {/* So, if it is not an expedited transfer */
                    if (getSDOs(m->data[0])) {
                        nbBytes = (m->data[4]) + ((UNS32) (m->data[5]) << 8) + ((UNS32) (m->data[6]) << 16)
                                + ((UNS32) (m->data[7]) << 24);
                        err = setSDOlineRestBytes(line, nbBytes);
                        if (err) {
                            failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                            return 0xFF;
                        }
                    }
                }
                /*Sending a SDO, cs=3*/
                data[0] = 3 << 5;
                data[1] = index & 0xFF; /* LSB */
                data[2] = (index >> 8) & 0xFF; /* MSB */
                data[3] = subIndex;
                for (i = 4; i < 8; i++)
                    data[i] = 0;
                sendSDO(whoami, CliServNbr, data);
            } /* end if I am SERVER */
#ifdef CO_ENABLE_SDO_CLIENT
            else {
                /* I am CLIENT */
                /* It is a response for a previous download segment. We should find a line opened for this. */
                if (!err)
                err = ObjDict_Data.transfers[line].state != SDO_DOWNLOAD_IN_PROGRESS;
                if (err) {
                    MSG_ERR(0x1A85, "SDO error : Received segment response for unknown trans. from nodeId", nodeId);
                    failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                /* Reset the wathdog */
                RestartSDO_TIMER(line)
                index = ObjDict_Data.transfers[line].index;
                subIndex = ObjDict_Data.transfers[line].subIndex;
                /* test of the toggle; */
                if (ObjDict_Data.transfers[line].toggle != getSDOt(m->data[0])) {
                    MSG_ERR(0x1A86, "SDO error : Received segment response Toggle error. from nodeId", nodeId);
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_TOGGLE_NOT_ALTERNED);
                    return 0xFF;
                }

                /* End transmission or downloading next segment. We need to know if it will be the last one. */
                getSDOlineRestBytes(line, &nbBytes);
                if (nbBytes == 0) {
                    MSG_WAR(0x3A87, "SDO End download. segment response received. OK. from nodeId", nodeId);
                    StopSDO_TIMER(line)
                    ObjDict_Data.transfers[line].state = SDO_FINISHED;
                    if(ObjDict_Data.transfers[line].Callback) (*ObjDict_Data.transfers[line].Callback)(nodeId);
                    return 0x00;
                }
                /* At least one transfer to send.	*/
                if (nbBytes > 7) {
                    /* several segments to download.*/
                    /* code to send the next segment. (cs = 0; c = 0) */
                    ObjDict_Data.transfers[line].toggle = ! ObjDict_Data.transfers[line].toggle & 1;
                    data[0] = (ObjDict_Data.transfers[line].toggle << 4);
                    err = lineToSDO(line, 7, data + 1);
                    if (err) {
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                }
                else {
                    /* Last segment. */
                    /* code to send the last segment. (cs = 0; c = 1)*/
                    ObjDict_Data.transfers[line].toggle = ! ObjDict_Data.transfers[line].toggle & 1;
                    data[0] = (UNS8)((ObjDict_Data.transfers[line].toggle << 4) | ((7 - nbBytes) << 1) | 1);
                    err = lineToSDO(line, nbBytes, data + 1);
                    if (err) {
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                    for (i = nbBytes + 1; i < 8; i++)
                    data[i] = 0;
                }
                MSG_WAR(0x3A88, "SDO sending download segment to nodeId", nodeId);
                sendSDO(whoami, CliServNbr, data);
            } /* end if I am a CLIENT */
#endif
            break;

        case 2:
            /* I am SERVER */
            /* Receive of an initiate upload.*/
            if (whoami == SDO_SERVER) {
                index = getSDOindex(m->data[1], m->data[2]);
                subIndex = getSDOsubIndex(m->data[3]);
                MSG_WAR(0x3A89, "Received SDO Initiate upload (to send data) defined at index 0x1200 + ", CliServNbr);
                MSG_WAR(0x3A90, "Reading at index : ", index);
                MSG_WAR(0x3A91, "Reading at subIndex : ", subIndex);
                /* Search if a SDO transfer have been yet initiated*/
                if (!err) {
                    MSG_ERR(0x1A92, "SDO error : Transmission yet started at line : ", line);
                    MSG_WAR(0x3A93, "Server Nbr = ", CliServNbr);
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                /* No line on use. Great !*/
                /* Try to open a new line.*/
                err = getSDOfreeLine(whoami, &line);
                if (err) {
                    MSG_ERR(0x1A71, "SDO error : No line free, too many SDO in progress. Aborted.", 0);
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                initSDOline(line, CliServNbr, index, subIndex, SDO_UPLOAD_IN_PROGRESS);
                /* Transfer data from dictionary to the line structure. */
                errorCode = objdictToSDOline(line);

                if (errorCode) {
                    MSG_ERR(0x1A94, "SDO error : Unable to copy the data from object dictionary. Err code : ", errorCode);
                    failedSDO(CliServNbr, whoami, index, subIndex, errorCode);
                    return 0xFF;
                }
                /* Preparing the response.*/
                getSDOlineRestBytes(line, &nbBytes); /* Nb bytes to transfer ? */
                if (nbBytes > 4) {
                    /* normal transfer. (segmented). */
                    /* code to send the initiate upload response. (cs = 2) */
                    data[0] = (2 << 5) | 1;
                    data[1] = index & 0xFF; /* LSB */
                    data[2] = (index >> 8) & 0xFF; /* MSB */
                    data[3] = subIndex;
                    data[4] = (UNS8) nbBytes;
                    data[5] = (UNS8) (nbBytes >> 8);
                    data[6] = (UNS8) (nbBytes >> 16);
                    data[7] = (UNS8) (nbBytes >> 24);
                    MSG_WAR(0x3A95, "SDO. Sending normal upload initiate response defined at index 0x1200 + ", nodeId);
                    sendSDO(whoami, CliServNbr, data);
                } else {
                    /* Expedited upload. (cs = 2 ; e = 1) */
                    data[0] = (UNS8) ((2 << 5) | ((4 - nbBytes) << 2) | 3);
                    data[1] = index & 0xFF; /* LSB */
                    data[2] = (index >> 8) & 0xFF; /* MSB */
                    data[3] = subIndex;
                    err = lineToSDO(line, nbBytes, data + 4);
                    if (err) {
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                    for (i = 4 + nbBytes; i < 8; i++)
                        data[i] = 0;
                    MSG_WAR(0x3A96, "SDO. Sending expedited upload initiate response defined at index 0x1200 + ", CliServNbr);
                    sendSDO(whoami, CliServNbr, data);
                    /* Release the line.*/
                    resetSDOline(line);
                }
            } /* end if I am SERVER*/
#ifdef CO_ENABLE_SDO_CLIENT
            else {
                /* I am CLIENT */
                /* It is the response for the previous initiate upload request.*/
                /* We should find a line opened for this. */
                if (!err)
                err = ObjDict_Data.transfers[line].state != SDO_UPLOAD_IN_PROGRESS;
                if (err) {
                    MSG_ERR(0x1A97, "SDO error : Received response for unknown upload request from nodeId", nodeId);
                    failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                /* Reset the wathdog */
                RestartSDO_TIMER(line)
                index = ObjDict_Data.transfers[line].index;
                subIndex = ObjDict_Data.transfers[line].subIndex;

                if (getSDOe(m->data[0])) { /* If SDO expedited */
                    /* nb of data to be uploaded */
                    nbBytes = 4 - getSDOn2(m->data[0]);
                    /* Storing the data in the line structure. */
                    err = SDOtoLine(line, nbBytes, (*m).data + 4);
                    if (err) {
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                    /* SDO expedited -> transfer finished. data are available via  getReadResultNetworkDict(). */
                    MSG_WAR(0x3A98, "SDO expedited upload finished. Response received from node : ", nodeId);
                    StopSDO_TIMER(line)
                    ObjDict_Data.transfers[line].count = nbBytes;
                    ObjDict_Data.transfers[line].state = SDO_FINISHED;
                    if(ObjDict_Data.transfers[line].Callback) (*ObjDict_Data.transfers[line].Callback)(nodeId);
                    return 0;
                }
                else { /* So, if it is not an expedited transfer */
                    /* Storing the nb of data to receive. */
                    if (getSDOs(m->data[0])) {
                        nbBytes = m->data[4] + ((UNS32)(m->data[5])<<8) + ((UNS32)(m->data[6])<<16) + ((UNS32)(m->data[7])<<24);
                        err = setSDOlineRestBytes(line, nbBytes);
                        if (err) {
                            failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                            return 0xFF;
                        }
                    }
                    /* Requesting next segment. (cs = 3) */
                    data[0] = 3 << 5;
                    for (i = 1; i < 8; i++)
                    data[i] = 0;
                    MSG_WAR(0x3A99, "SDO. Sending upload segment request to node : ", nodeId);
                    sendSDO(whoami, CliServNbr, data);
                }
            } /* End if CLIENT */
#endif
            break;

        case 3:
            /* I am SERVER */
            if (whoami == SDO_SERVER) {
                /* Receiving a upload segment. */
                /* A SDO transfer should have been yet initiated. */
                if (!err)
                    err = ObjDict_Data.transfers[line].state != SDO_UPLOAD_IN_PROGRESS;
                if (err) {
                    MSG_ERR(0x1AA0, "SDO error : Received upload segment for unstarted trans. index 0x1200 + ", CliServNbr);
                    failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                /* Reset the wathdog */
                RestartSDO_TIMER(line)
                MSG_WAR(0x3AA1, "Received SDO upload segment defined at index 0x1200 + ", CliServNbr);
                index = ObjDict_Data.transfers[line].index;
                subIndex = ObjDict_Data.transfers[line].subIndex;
                /* Toggle test.*/
                if (ObjDict_Data.transfers[line].toggle != getSDOt(m->data[0])) {
                    MSG_ERR(0x1AA2, "SDO error : Toggle error : ", getSDOt(m->data[0]));
                    failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_TOGGLE_NOT_ALTERNED);
                    return 0xFF;
                }
                /* Uploading next segment. We need to know if it will be the last one. */
                getSDOlineRestBytes(line, &nbBytes);
                if (nbBytes > 7) {
                    /* The segment to transfer is not the last one.*/
                    /* code to send the next segment. (cs = 0; c = 0) */
                    data[0] = (ObjDict_Data.transfers[line].toggle << 4);
                    err = lineToSDO(line, 7, data + 1);
                    if (err) {
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                    /* Inverting the toggle for the next tranfert. */
                    ObjDict_Data.transfers[line].toggle = !ObjDict_Data.transfers[line].toggle & 1;
                    MSG_WAR(0x3AA3, "SDO. Sending upload segment defined at index 0x1200 + ", CliServNbr);
                    sendSDO(whoami, CliServNbr, data);
                } else {
                    /* Last segment. */
                    /* code to send the last segment. (cs = 0; c = 1) */
                    data[0] = (UNS8) ((ObjDict_Data.transfers[line].toggle << 4) | ((7 - nbBytes) << 1) | 1);
                    err = lineToSDO(line, nbBytes, data + 1);
                    if (err) {
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                    for (i = nbBytes + 1; i < 8; i++)
                        data[i] = 0;
                    MSG_WAR(0x3AA4, "SDO. Sending last upload segment defined at index 0x1200 + ", CliServNbr);
                    sendSDO(whoami, CliServNbr, data);
                    /* Release the line */
                    resetSDOline(line);
                }
            } /* end if SERVER*/
#ifdef CO_ENABLE_SDO_CLIENT
            else {
                /* I am CLIENT */
                /* It is the response for the previous initiate download request. */
                /* We should find a line opened for this. */
                if (!err)
                err = ObjDict_Data.transfers[line].state != SDO_DOWNLOAD_IN_PROGRESS;
                if (err) {
                    MSG_ERR(0x1AA5, "SDO error : Received response for unknown download request from nodeId", nodeId);
                    failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                /* Reset the watchdog */
                RestartSDO_TIMER(line)
                index = ObjDict_Data.transfers[line].index;
                subIndex = ObjDict_Data.transfers[line].subIndex;
                /* End transmission or requesting  next segment. */
                getSDOlineRestBytes(line, &nbBytes);
                if (nbBytes == 0) {
                    MSG_WAR(0x3AA6, "SDO End download expedited. Response received. from nodeId", nodeId);
                    StopSDO_TIMER(line)
                    ObjDict_Data.transfers[line].state = SDO_FINISHED;
                    if(ObjDict_Data.transfers[line].Callback) (*ObjDict_Data.transfers[line].Callback)(nodeId);
                    return 0x00;
                }
                if (nbBytes > 7) {
                    /* more than one request to send */
                    /* code to send the next segment. (cs = 0; c = 0)	*/
                    data[0] = (ObjDict_Data.transfers[line].toggle << 4);
                    err = lineToSDO(line, 7, data + 1);
                    if (err) {
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                }
                else {
                    /* Last segment.*/
                    /* code to send the last segment. (cs = 0; c = 1)	*/
                    data[0] = (UNS8)((ObjDict_Data.transfers[line].toggle << 4) | ((7 - nbBytes) << 1) | 1);
                    err = lineToSDO(line, nbBytes, data + 1);
                    if (err) {
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                    for (i = nbBytes + 1; i < 8; i++)
                    data[i] = 0;
                }
                MSG_WAR(0x3AA7, "SDO sending download segment to nodeId", nodeId);
                sendSDO(whoami, CliServNbr, data);

            } /* end if I am a CLIENT		*/
#endif
            break;

        case 4:
//            abortCode = (UNS32) m->data[4] | ((UNS32) m->data[5] << 8) | ((UNS32) m->data[6] << 16)
//                    | ((UNS32) m->data[7] << 24);
            /* Received SDO abort. */
            if (whoami == SDO_SERVER) {
                if (!err) {
                    resetSDOline(line);
                    MSG_WAR(0x3AA8, "SD0. Received SDO abort. Line released. Code : ", abortCode);
                } else
                    MSG_WAR(0x3AA9, "SD0. Received SDO abort. No line found. Code : ", abortCode);
                /* Tips : The end user has no way to know that the server node has received an abort SDO. */
                /* Its is ok, I think.*/
            }
#ifdef CO_ENABLE_SDO_CLIENT
            else { /* If I am CLIENT */
                if (!err) {
                    /* The line *must* be released by the core program. */
                    StopSDO_TIMER(line)
                    ObjDict_Data.transfers[line].state = SDO_ABORTED_RCV;
                    ObjDict_Data.transfers[line].abortCode = abortCode;
                    MSG_WAR(0x3AB0, "SD0. Received SDO abort. Line state ABORTED. Code : ", abortCode);
                    if(ObjDict_Data.transfers[line].Callback) (*ObjDict_Data.transfers[line].Callback)(nodeId);
                }
                else
                MSG_WAR(0x3AB1, "SD0. Received SDO abort. No line found. Code : ", abortCode);
            }
#endif
            break;
        case 5: /* Command specifier for data transmission - the client or server is the data producer */
            SubCommand = getSDOblockSC(m->data[0]);
            if (whoami == SDO_SERVER) { /* Server block upload */
                if (SubCommand == SDO_BCS_INITIATE_UPLOAD_REQUEST) {
                    index = getSDOindex(m->data[1], m->data[2]);
                    subIndex = getSDOsubIndex(m->data[3]);
                    MSG_WAR(0x3AB2, "Received SDO Initiate block upload defined at index 0x1200 + ", CliServNbr);
                    MSG_WAR(0x3AB3, "Reading at index : ", index);
                    MSG_WAR(0x3AB4, "Reading at subIndex : ", subIndex);
                    /* Search if a SDO transfer have been yet initiated */
                    if (!err) {
                        MSG_ERR(0x1A93, "SDO error : Transmission yet started at line : ", line);
                        MSG_WAR(0x3AB5, "Server Nbr = ", CliServNbr);
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_LOCAL_CTRL_ERROR);
                        return 0xFF;
                    }
                    /* No line on use. Great !*/
                    /* Try to open a new line.*/
                    err = getSDOfreeLine(whoami, &line);
                    if (err) {
                        MSG_ERR(0x1A73, "SDO error : No line free, too many SDO in progress. Aborted.", 0);
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_LOCAL_CTRL_ERROR);
                        return 0xFF;
                    }
                    initSDOline(line, CliServNbr, index, subIndex, SDO_BLOCK_UPLOAD_IN_PROGRESS);
                    ObjDict_Data.transfers[line].peerCRCsupport = ((m->data[0]) >> 2) & 1;
                    ObjDict_Data.transfers[line].blksize = m->data[4];
                    /* Transfer data from dictionary to the line structure. */
                    errorCode = objdictToSDOline(line);
                    if (errorCode) {
                        MSG_ERR(0x1A95, "SDO error : Unable to copy the data from object dictionary. Err code : ", errorCode);
                        failedSDO(CliServNbr, whoami, index, subIndex, errorCode);
                        return 0xFF;
                    }
                    /* Preparing the response.*/
                    getSDOlineRestBytes(line, &nbBytes); /* get Nb bytes to transfer */
                    ObjDict_Data.transfers[line].objsize = nbBytes;
                    data[0] = (6 << 5) | (1 << 1) | SDO_BSS_INITIATE_UPLOAD_RESPONSE;
                    data[1] = index & 0xFF; /* LSB */
                    data[2] = (index >> 8) & 0xFF; /* MSB */
                    data[3] = subIndex;
                    data[4] = (UNS8) nbBytes;
                    data[5] = (UNS8) (nbBytes >> 8);
                    data[6] = (UNS8) (nbBytes >> 16);
                    data[7] = (UNS8) (nbBytes >> 24);
                    MSG_WAR(0x3A9A, "SDO. Sending normal block upload initiate response defined at index 0x1200 + ", nodeId);
                    sendSDO(whoami, CliServNbr, data);
                } else if (SubCommand == SDO_BCS_END_UPLOAD_REQUEST) {
                    MSG_WAR(0x3AA2, "Received SDO block END upload request defined at index 0x1200 + ", CliServNbr);
                    /* A SDO transfer should have been yet initiated. */
                    if (!err)
                        err = ObjDict_Data.transfers[line].state != SDO_BLOCK_UPLOAD_IN_PROGRESS;
                    if (err) {
                        MSG_ERR(0x1AA1, "SDO error : Received block upload request for unstarted trans. index 0x1200 + ", CliServNbr);
                        failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                        return 0xFF;
                    }
                    /* Release the line */
                    resetSDOline(line);
                } else if ((SubCommand == SDO_BCS_UPLOAD_RESPONSE) || (SubCommand == SDO_BCS_START_UPLOAD)) {
                    /* A SDO transfer should have been yet initiated. */
                    if (!err)
                        err = ObjDict_Data.transfers[line].state != SDO_BLOCK_UPLOAD_IN_PROGRESS;
                    if (err) {
                        MSG_ERR(0x1AA1, "SDO error : Received block upload response for unstarted trans. index 0x1200 + ", CliServNbr);
                        failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                        return 0xFF;
                    }
                    /* Reset the wathdog */
                    RestartSDO_TIMER(line);
                    /* Uploading first or next block */
                    index = ObjDict_Data.transfers[line].index;
                    subIndex = ObjDict_Data.transfers[line].subIndex;
                    if (SubCommand == SDO_BCS_UPLOAD_RESPONSE) {
                        MSG_WAR(0x3AA2, "Received SDO block upload response defined at index 0x1200 + ", CliServNbr);
                        ObjDict_Data.transfers[line].blksize = m->data[2];
                        AckSeq = (m->data[1]) & 0x7f;
                        getSDOlineRestBytes(line, &nbBytes);
                        if ((nbBytes == 0) && (AckSeq == ObjDict_Data.transfers[line].seqno)) { /* Si tout est envoyé et confirmé reçu on envoi un block end upload response */
                            data[0] = (6 << 5) | ((ObjDict_Data.transfers[line].endfield) << 2) | SDO_BSS_END_UPLOAD_RESPONSE;
                            for (i = 1; i < 8; i++)
                                data[i] = 0;
                            MSG_WAR(0x3AA5, "SDO. Sending block END upload response defined at index 0x1200 + ", CliServNbr);
                            sendSDO(whoami, CliServNbr, data);
                            break;
                        } else
                            ObjDict_Data.transfers[line].offset = ObjDict_Data.transfers[line].lastblockoffset + 7 * AckSeq;
                        if (ObjDict_Data.transfers[line].offset > ObjDict_Data.transfers[line].count) { /* Bad AckSeq reveived (too high) */
                            MSG_ERR(0x1AA1, "SDO error : Received upload response with bad ackseq index 0x1200 + ", CliServNbr);
                            failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                            return 0xFF;
                        }
                    } else
                        MSG_WAR(0x3AA2, "Received SDO block START upload defined at index 0x1200 + ", CliServNbr);
                    ObjDict_Data.transfers[line].lastblockoffset = (UNS8) ObjDict_Data.transfers[line].offset;
                    for (SeqNo = 1; SeqNo <= ObjDict_Data.transfers[line].blksize; SeqNo++) {
                        ObjDict_Data.transfers[line].seqno = SeqNo;
                        getSDOlineRestBytes(line, &nbBytes);
                        if (nbBytes > 7) {
                            /* The segment to transfer is not the last one.*/
                            data[0] = SeqNo;
                            err = lineToSDO(line, 7, data + 1);
                            if (err) {
                                failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                                return 0xFF;
                            }
                            MSG_WAR(0x3AA5, "SDO. Sending upload segment defined at index 0x1200 + ", CliServNbr);
                            sendSDO(whoami, CliServNbr, data);
                        } else {
                            /* Last segment is in this block */
                            data[0] = 0x80 | SeqNo;
                            err = lineToSDO(line, nbBytes, data + 1);
                            if (err) {
                                failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                                return 0xFF;
                            }
                            for (i = nbBytes + 1; i < 8; i++)
                                data[i] = 0;
                            MSG_WAR(0x3AA5, "SDO. Sending last upload segment defined at index 0x1200 + ", CliServNbr);
                            sendSDO(whoami, CliServNbr, data);
                            ObjDict_Data.transfers[line].endfield = (UNS8) (7 - nbBytes);
                            break;
                        }
                    }
                }
            } /* end if SERVER */
#ifdef CO_ENABLE_SDO_CLIENT
            else { /* if CLIENT (block download) */
                if ((SubCommand == SDO_BSS_INITIATE_DOWNLOAD_RESPONSE) || (SubCommand == SDO_BSS_DOWNLOAD_RESPONSE)) {
                    /* We should find a line opened for this. */
                    if (!err)
                    err = ObjDict_Data.transfers[line].state != SDO_BLOCK_DOWNLOAD_IN_PROGRESS;
                    if (err) {
                        MSG_ERR(0x1AAA, "SDO error : Received response for unknown block download request from node id", nodeId);
                        failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                        return 0xFF;
                    }
                    /* Reset the watchdog */
                    RestartSDO_TIMER(line)
                    if (SubCommand == SDO_BSS_INITIATE_DOWNLOAD_RESPONSE) {
                        index = ObjDict_Data.transfers[line].index;
                        subIndex = ObjDict_Data.transfers[line].subIndex;
                        ObjDict_Data.transfers[line].peerCRCsupport = ((m->data[0])>>2) & 1;
                        ObjDict_Data.transfers[line].blksize = m->data[4];
                    }
                    else {
                        ObjDict_Data.transfers[line].blksize = m->data[2];
                        AckSeq = (m->data[1]) & 0x7f;
                        getSDOlineRestBytes(line, &nbBytes);
                        if((nbBytes == 0) && (AckSeq == ObjDict_Data.transfers[line].seqno)) { /* Si tout est envoyé et confirmé reçu on envoi un block end download request */
                            data[0] = (6 << 5) | ((ObjDict_Data.transfers[line].endfield) << 2) | SDO_BCS_END_DOWNLOAD_REQUEST;
                            for (i = 1; i < 8; i++)
                            data[i] = 0;
                            MSG_WAR(0x3AA5, "SDO. Sending block END download request defined at index 0x1200 + ", CliServNbr);
                            sendSDO(whoami, CliServNbr, data);
                            break;
                        }
                        else
                        ObjDict_Data.transfers[line].offset = ObjDict_Data.transfers[line].lastblockoffset + 7 * AckSeq;
                        if(ObjDict_Data.transfers[line].offset > ObjDict_Data.transfers[line].count) { /* Bad AckSeq reveived (too high) */
                            MSG_ERR(0x1AA1, "SDO error : Received upload segment with bad ackseq index 0x1200 + ",
                                    CliServNbr);
                            failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                            return 0xFF;
                        }
                    }
                    ObjDict_Data.transfers[line].lastblockoffset = (UNS8) ObjDict_Data.transfers[line].offset;
                    for(SeqNo = 1; SeqNo <= ObjDict_Data.transfers[line].blksize; SeqNo++) {
                        ObjDict_Data.transfers[line].seqno = SeqNo;
                        getSDOlineRestBytes(line, &nbBytes);
                        if (nbBytes > 7) {
                            /* The segment to transfer is not the last one.*/
                            data[0] = SeqNo;
                            err = lineToSDO(line, 7, data + 1);
                            if (err) {
                                failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                                return 0xFF;
                            }
                            MSG_WAR(0x3AAB, "SDO. Sending download segment to node id ", nodeId);
                            sendSDO(whoami, CliServNbr, data);
                        }
                        else {
                            /* Last segment is in this block */
                            data[0] = 0x80 | SeqNo;
                            err = lineToSDO(line, nbBytes, data + 1);
                            if (err) {
                                failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_GENERAL_ERROR);
                                return 0xFF;
                            }
                            for (i = nbBytes + 1; i < 8; i++)
                            data[i] = 0;
                            MSG_WAR(0x3AAB, "SDO. Sending last download segment to node id ", nodeId);
                            sendSDO(whoami, CliServNbr, data);
                            ObjDict_Data.transfers[line].endfield = (UNS8) (7 - nbBytes);
                            break;
                        }
                    }
                }
                else if (SubCommand == SDO_BSS_END_DOWNLOAD_RESPONSE) {
                    MSG_WAR(0x3AAC, "SDO End block download response from nodeId", nodeId);
                    StopSDO_TIMER(line)
                    ObjDict_Data.transfers[line].state = SDO_FINISHED;
                    if(ObjDict_Data.transfers[line].Callback) (*ObjDict_Data.transfers[line].Callback)(nodeId);
                    return 0x00;
                }
                else {
                    MSG_ERR(0x1AAB, "SDO error block download : Received wrong subcommand from nodeId", nodeId);
                    failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
            } /* end if CLIENT */
#endif
            break;
        case 6: /* Command specifier for data reception - the client or server is the data consumer */
            if (whoami == SDO_SERVER) { /* Server block download */
                if (err) {
                    /* Nothing already started */
                    SubCommand = (m->data[0]) & 1;
                    if (SubCommand != SDO_BCS_INITIATE_DOWNLOAD_REQUEST) {
                        MSG_ERR(0x1AAC, "SDO error block download : Received wrong subcommand from node id", nodeId);
                        failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                        return 0xFF;
                    }
                    index = getSDOindex(m->data[1], m->data[2]);
                    subIndex = getSDOsubIndex(m->data[3]);
                    MSG_WAR(0x3A9B, "Received SDO block download initiate defined at index 0x1200 + ", CliServNbr);
                    MSG_WAR(0x3A9B, "Writing at index : ", index);
                    MSG_WAR(0x3A9B, "Writing at subIndex : ", subIndex);
                    /* Try to open a new line. */
                    err = getSDOfreeLine(whoami, &line);
                    if (err) {
                        MSG_ERR(0x1A89, "SDO error : No line free, too many SDO in progress. Aborted.", 0);
                        failedSDO(CliServNbr, whoami, index, subIndex, SDOABT_LOCAL_CTRL_ERROR);
                        return 0xFF;
                    }
                    initSDOline(line, CliServNbr, index, subIndex, SDO_BLOCK_DOWNLOAD_IN_PROGRESS);
                    ObjDict_Data.transfers[line].rxstep = RXSTEP_STARTED;
                    ObjDict_Data.transfers[line].peerCRCsupport = ((m->data[0]) >> 2) & 1;
                    if ((m->data[0]) & 2) /* if data set size is indicated */
                        ObjDict_Data.transfers[line].objsize = (UNS32) m->data[4] + (UNS32) m->data[5] * 256
                                + (UNS32) m->data[6] * 256 * 256 + (UNS32) m->data[7] * 256 * 256 * 256;
                    data[0] = (5 << 5) | SDO_BSS_INITIATE_DOWNLOAD_RESPONSE;
                    data[1] = (UNS8) index; /* LSB */
                    data[2] = (UNS8) (index >> 8); /* MSB */
                    data[3] = subIndex;
                    data[4] = SDO_BLOCK_SIZE;
                    data[5] = data[6] = data[7] = 0;
                    MSG_WAR(0x3AAD, "SDO. Sending block download initiate response - index 0x1200 + ", CliServNbr);
                    sendSDO(whoami, CliServNbr, data);
                } else if (ObjDict_Data.transfers[line].rxstep == RXSTEP_STARTED) {
                    MSG_WAR(0x3A9B, "Received SDO block download data segment - index 0x1200 + ", CliServNbr);
                    RestartSDO_TIMER(line)
                    SeqNo = m->data[0] & 0x7F;
                    if (m->data[0] & 0x80) { /* Last segment ? */
                        if (SeqNo == (ObjDict_Data.transfers[line].seqno + 1)) {
                            ObjDict_Data.transfers[line].rxstep = RXSTEP_END;
                            ObjDict_Data.transfers[line].seqno = SeqNo;
                            /* Store the data temporary because we don't know yet how many bytes do not contain data */
                            memcpy(ObjDict_Data.transfers[line].tmpData, m->data, 8);
                        }
                        data[0] = (5 << 5) | SDO_BSS_DOWNLOAD_RESPONSE;
                        data[1] = ObjDict_Data.transfers[line].seqno;
                        data[2] = SDO_BLOCK_SIZE;
                        data[3] = data[4] = data[5] = data[6] = data[7] = 0;
                        MSG_WAR(0x3AAE, "SDO. Sending block download response - index 0x1200 + ", CliServNbr);
                        sendSDO(whoami, CliServNbr, data);
                        ObjDict_Data.transfers[line].seqno = 0;
                    } else {
                        if (SeqNo == (ObjDict_Data.transfers[line].seqno + 1)) {
                            ObjDict_Data.transfers[line].seqno = SeqNo;
                            /* Store the data in the transfer structure. */
                            err = SDOtoLine(line, 7, (*m).data + 1);
                            if (err) {
                                failedSDO(CliServNbr, whoami, ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex, SDOABT_GENERAL_ERROR);
                                return 0xFF;
                            }
                        }
                        if (SeqNo == SDO_BLOCK_SIZE) {
                            data[0] = (5 << 5) | SDO_BSS_DOWNLOAD_RESPONSE;
                            data[1] = ObjDict_Data.transfers[line].seqno;
                            data[2] = SDO_BLOCK_SIZE;
                            data[3] = data[4] = data[5] = data[6] = data[7] = 0;
                            MSG_WAR(0x3AAE, "SDO. Sending block download response - index 0x1200 + ", CliServNbr);
                            sendSDO(whoami, CliServNbr, data);
                            ObjDict_Data.transfers[line].seqno = 0;
                        }
                    }
                } else if (ObjDict_Data.transfers[line].rxstep == RXSTEP_END) { /* endphase */
                    MSG_WAR(0x3A9B, "Received SDO block download end request - index 0x1200 + ", CliServNbr);
                    /* here store remaining bytes in tmpData to line, check size and confirm or abort */
                    if ((m->data[0] & 1) != SDO_BCS_END_DOWNLOAD_REQUEST) {
                        MSG_ERR(0x1AAD, "SDO error block download : Received wrong subcommand - index 0x1200 + ", CliServNbr);
                        failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                        return 0xFF;
                    }
                    RestartSDO_TIMER(line)
                    NbBytesNoData = (m->data[0] >> 2) & 0x07;
                    /* Store the data in the transfer structure. */
                    err = SDOtoLine(line, 7 - NbBytesNoData, ObjDict_Data.transfers[line].tmpData + 1);
                    if (err) {
                        failedSDO(CliServNbr, whoami, ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                    if (ObjDict_Data.transfers[line].objsize) { /* If size was indicated in the initiate request */
                        if (ObjDict_Data.transfers[line].objsize != ObjDict_Data.transfers[line].offset) {
                            MSG_ERR(0x1AAE, "SDO error block download : sizes do not match - index 0x1200 + ", CliServNbr);
                            failedSDO(CliServNbr, whoami, ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex, SDOABT_LOCAL_CTRL_ERROR);
                            return 0xFF;
                        }
                    }
                    data[0] = (5 << 5) | SDO_BSS_END_DOWNLOAD_RESPONSE;
                    for (i = 1; i < 8; i++)
                        data[i] = 0;
                    MSG_WAR(0x3AAF, "SDO. Sending block download end response - index 0x1200 + ", CliServNbr);
                    sendSDO(whoami, CliServNbr, data);
                    /* Transfering line data to object dictionary. */
                    errorCode = SDOlineToObjdict(line);
                    if (errorCode) {
                        MSG_ERR(0x1AAF, "SDO error : Unable to copy the data in the object dictionary", 0);
                        failedSDO(CliServNbr, whoami, ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex, errorCode);
                        return 0xFF;
                    }
                    /* Release of the line */
                    resetSDOline(line);
                    MSG_WAR(0x3AAF, "SDO. End of block download defined at index 0x1200 + ", CliServNbr);
                }
            } /* end if SERVER */
#ifdef CO_ENABLE_SDO_CLIENT
            else { /* if CLIENT (block upload) */
                if (err) {
                    /* Nothing already started */
                    MSG_ERR(0x1AAD, "SDO error block upload : no transmission started", nodeId);
                    failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                    return 0xFF;
                }
                RestartSDO_TIMER(line)
                if (ObjDict_Data.transfers[line].rxstep == RXSTEP_INIT) {
                    if ((m->data[0] & 1) == SDO_BSS_INITIATE_UPLOAD_RESPONSE) {
                        MSG_WAR(0x3A9C, "Received SDO block upload response from node id ", nodeId);
                        ObjDict_Data.transfers[line].rxstep = RXSTEP_STARTED;
                        ObjDict_Data.transfers[line].peerCRCsupport = ((m->data[0])>>2) & 1;
                        if ((m->data[0]) & 2) /* if data set size is indicated */
                        ObjDict_Data.transfers[line].objsize = (UNS32)m->data[4] + (UNS32)m->data[5]*256 + (UNS32)m->data[6]*256*256 + (UNS32)m->data[7]*256*256*256;
                        data[0] = (5 << 5) | SDO_BCS_START_UPLOAD;
                        for (i = 1; i < 8; i++)
                        data[i] = 0;
                        MSG_WAR(0x3AB6, "SDO. Sending block upload start to node id ", nodeId);
                        sendSDO(whoami, CliServNbr, data);
                    }
                }
                else if (ObjDict_Data.transfers[line].rxstep == RXSTEP_STARTED) {
                    SeqNo = m->data[0] & 0x7F;
                    if (m->data[0] & 0x80) { /* Last segment ? */
                        if(SeqNo == (ObjDict_Data.transfers[line].seqno + 1)) {
                            ObjDict_Data.transfers[line].rxstep = RXSTEP_END;
                            ObjDict_Data.transfers[line].seqno = SeqNo;
                            /* Store the data temporary because we don't know yet how many bytes do not contain data */
                            memcpy(ObjDict_Data.transfers[line].tmpData, m->data, 8);
                        }
                        data[0] = (5 << 5) | SDO_BCS_UPLOAD_RESPONSE;
                        data[1] = ObjDict_Data.transfers[line].seqno;
                        data[2] = SDO_BLOCK_SIZE;
                        data[3] = data[4] = data[5] = data[6] = data[7] = 0;
                        MSG_WAR(0x3AB7, "SDO. Sending block upload response to node id ", nodeId);
                        sendSDO(whoami, CliServNbr, data);
                        ObjDict_Data.transfers[line].seqno = 0;
                    }
                    else {
                        if (SeqNo == (ObjDict_Data.transfers[line].seqno + 1)) {
                            ObjDict_Data.transfers[line].seqno = SeqNo;
                            /* Store the data in the transfer structure. */
                            err = SDOtoLine(line, 7, (*m).data + 1);
                            if (err) {
                                failedSDO(CliServNbr, whoami, ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex, SDOABT_GENERAL_ERROR);
                                return 0xFF;
                            }
                        }
                        if (SeqNo == SDO_BLOCK_SIZE) {
                            data[0] = (5 << 5) | SDO_BCS_UPLOAD_RESPONSE;
                            data[1] = ObjDict_Data.transfers[line].seqno;
                            data[2] = SDO_BLOCK_SIZE;
                            data[3] = data[4] = data[5] = data[6] = data[7] = 0;
                            MSG_WAR(0x3AAE, "SDO. Sending block upload response to node id ", nodeId);
                            sendSDO(whoami, CliServNbr, data);
                            ObjDict_Data.transfers[line].seqno = 0;
                        }
                    }
                }
                else if (ObjDict_Data.transfers[line].rxstep == RXSTEP_END) { /* endphase */
                    /* here store remaining bytes in tmpData to line, check size and confirm or abort */
                    if ((m->data[0] & 1) != SDO_BSS_END_UPLOAD_RESPONSE) {
                        MSG_ERR(0x1AAD, "SDO error block upload : Received wrong subcommand from node id ", nodeId);
                        failedSDO(CliServNbr, whoami, 0, 0, SDOABT_LOCAL_CTRL_ERROR);
                        return 0xFF;
                    }
                    NbBytesNoData = (m->data[0]>>2) & 0x07;
                    /* Store the data in the transfer structure. */
                    err = SDOtoLine(line, 7-NbBytesNoData, ObjDict_Data.transfers[line].tmpData + 1);
                    if (err) {
                        failedSDO(CliServNbr, whoami, ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex, SDOABT_GENERAL_ERROR);
                        return 0xFF;
                    }
                    if(ObjDict_Data.transfers[line].objsize) { /* If size was indicated in the initiate request */
                        if (ObjDict_Data.transfers[line].objsize != ObjDict_Data.transfers[line].offset) {
                            MSG_ERR(0x1AAE, "SDO error block download : sizes do not match - from node id ", nodeId);
                            failedSDO(CliServNbr, whoami, ObjDict_Data.transfers[line].index, ObjDict_Data.transfers[line].subIndex, SDOABT_LOCAL_CTRL_ERROR);
                            return 0xFF;
                        }
                    }
                    data[0] = (5 << 5) | SDO_BCS_END_UPLOAD_REQUEST;
                    for (i = 1; i < 8; i++)
                    data[i] = 0;
                    MSG_WAR(0x3AAF, "SDO. Sending block upload end request to node id ", nodeId);
                    sendSDO(whoami, CliServNbr, data);
                    MSG_WAR(0x3AAF, "SDO. End of block upload request", 0);
                    StopSDO_TIMER(line)
                    ObjDict_Data.transfers[line].state = SDO_FINISHED;
                    if(ObjDict_Data.transfers[line].Callback) (*ObjDict_Data.transfers[line].Callback)(nodeId);
                }
            } /* end if CLIENT */
#endif
            break;
        default:
            /* Error : Unknown cs */
            MSG_ERR(0x1AB2, "SDO. Received unknown command specifier : ", cs)
            ;
            return 0xFF;

    } /* End switch */
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param nodeId
 **
 ** @return
 ** 	0xFF : No SDO client available
 **     0xFE : Not found
 **     otherwise : SDO client number
 **/
UNS8 GetSDOClientFromNodeId(UNS8 nodeId) {
    UNS8 SDOfound = 0;
    UNS8 CliNbr;
    UNS8 nodeIdServer;

    const subindex *si;
    UNS8 si_size;
    ODCallback_t *callbacks;

    CliNbr = 0;
    while ((si = ObjDict_scanIndexOD(0x1280 + CliNbr, &si_size, &callbacks))) {
        if (si_size <= 3) {
            MSG_ERR(0x1AC8, "Subindex 3  not found at index ", 0x1280 + CliNbr);
            return 0xFF;
        }
        /* looking for the server nodeId */
        nodeIdServer = *((UNS8*) si[3].pObject);
        MSG_WAR(0x1AD2, "index : ", 0x1280 + CliNbr);
        MSG_WAR(0x1AD3, "nodeIdServer : ", nodeIdServer);

        if (nodeIdServer == nodeId) {
            SDOfound = 1;
            break;
        }
        CliNbr++;
    } /* end while */
    if (!SDOfound) {
        MSG_WAR(0x1AC9, "SDO No preset client found to communicate with node : ", nodeId);
        return 0xFE;
    }
    MSG_WAR(0x3AD0, "        SDO client defined at index  : ", 0x1280 + CliNbr);

    return CliNbr;
}

/*!
 **
 **
 ** @param d
 ** @param nodeId
 ** @param index
 ** @param subIndex
 ** @param count
 ** @param dataType
 ** @param data
 ** @param Callback
 ** @param endianize
 **
 ** @return
 **/
INLINE UNS8 _writeNetworkDict(UNS8 nodeId, UNS16 index,
UNS8 subIndex, UNS32 count, UNS8 dataType, void *data, SDOCallback_t Callback, UNS8 endianize, UNS8 useBlockMode) {
    UNS8 err;
    UNS8 line;
    UNS8 CliNbr;
    UNS32 j;
    UNS8 i;
    UNS8 buf[8];

    MSG_WAR(0x3AC0, "Send SDO to write in the dictionary of node : ", nodeId);
    MSG_WAR(0x3AC1, "                                   At index : ", index);
    MSG_WAR(0x3AC2, "                                   subIndex : ", subIndex);
    MSG_WAR(0x3AC3, "                                   nb bytes : ", count);

    /* First let's find the corresponding SDO client in our OD  */
    CliNbr = GetSDOClientFromNodeId(nodeId);
    if (CliNbr >= 0xFE)
        return CliNbr;
    /* Verify that there is no SDO communication yet. */
    err = getSDOlineOnUse(CliNbr, SDO_CLIENT, &line);
    if (!err) {
        MSG_ERR(0x1AC4, "SDO error : Communication yet established. with node : ", nodeId);
        return 0xFF;
    }
    /* Taking the line ... */
    err = getSDOfreeLine(SDO_CLIENT, &line);
    if (err) {
        MSG_ERR(0x1AC5, "SDO error : No line free, too many SDO in progress. Aborted for node : ", nodeId);
        return (0xFF);
    } else
        MSG_WAR(0x3AE1, "Transmission on line : ", line);
    if (useBlockMode) {
        initSDOline(line, CliNbr, index, subIndex, SDO_BLOCK_DOWNLOAD_IN_PROGRESS);
        ObjDict_Data.transfers[line].objsize = count;
    } else
        initSDOline(line, CliNbr, index, subIndex, SDO_DOWNLOAD_IN_PROGRESS);
    ObjDict_Data.transfers[line].count = count;
    ObjDict_Data.transfers[line].dataType = dataType;
#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
    {
        UNS8* lineData = ObjDict_Data.transfers[line].data;
        if (count > SDO_MAX_LENGTH_TRANSFER)
        {
            ObjDict_Data.transfers[line].dynamicData = (UNS8*) malloc(count);
            ObjDict_Data.transfers[line].dynamicDataSize = count;
            if (ObjDict_Data.transfers[line].dynamicData == NULL)
            {
                MSG_ERR(0x1AC9, "SDO. Error. Could not allocate enough bytes : ", count);
                return 0xFE;
            }
            lineData = ObjDict_Data.transfers[line].dynamicData;
        }
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

    /* Copy data to transfers structure. */
    for (j = 0; j < count; j++) {
#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
# ifdef CANOPEN_BIG_ENDIAN
        if (dataType == 0 && endianize)
        lineData[count - 1 - j] = ((char *)data)[j];
        else /* String of bytes. */
        lineData[j] = ((char *)data)[j];
#  else
        lineData[j] = ((char *)data)[j];
#  endif
    }
#else //SDO_DYNAMIC_BUFFER_ALLOCATION
# ifdef CANOPEN_BIG_ENDIAN
        if (dataType == 0 && endianize)
        ObjDict_Data.transfers[line].data[count - 1 - j] = ((char *)data)[j];
        else /* String of bytes. */
        ObjDict_Data.transfers[line].data[j] = ((char *)data)[j];
#  else
        ObjDict_Data.transfers[line].data[j] = ((char *) data)[j];
#  endif
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION
    }
    if (useBlockMode) {
        buf[0] = (6 << 5) | (1 << 1); /* CCS = 6 , CC = 0 , S = 1 , CS = 0 */
        for (i = 0; i < 4; i++)
            buf[i + 4] = (UNS8) ((count >> (i << 3))); /* i*8 */
    } else {
        /* Send the SDO to the server. Initiate download, cs=1. */
        if (count <= 4) { /* Expedited transfer */
            buf[0] = (UNS8) ((1 << 5) | ((4 - count) << 2) | 3);
            for (i = 4; i < 8; i++)
                buf[i] = ObjDict_Data.transfers[line].data[i - 4];
            ObjDict_Data.transfers[line].offset = count;
        } else { /** Normal transfer */
            buf[0] = (1 << 5) | 1;
            for (i = 0; i < 4; i++)
                buf[i + 4] = (UNS8) ((count >> (i << 3))); /* i*8 */
        }
    }
    buf[1] = index & 0xFF; /* LSB */
    buf[2] = (index >> 8) & 0xFF; /* MSB */
    buf[3] = subIndex;

    ObjDict_Data.transfers[line].Callback = Callback;

    err = sendSDO(SDO_CLIENT, CliNbr, buf);
    if (err) {
        MSG_ERR(0x1AD1, "SDO. Error while sending SDO to node : ", nodeId);
        /* release the line */
        resetSDOline(line);
        return 0xFF;
    }

    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param nodeId
 ** @param index
 ** @param subIndex
 ** @param count
 ** @param dataType
 ** @param data
 ** @param useBlockMode
 **
 ** @return
 **/
UNS8 writeNetworkDict(UNS8 nodeId, UNS16 index,
UNS8 subIndex, UNS32 count, UNS8 dataType, void *data, UNS8 useBlockMode) {
    return _writeNetworkDict(nodeId, index, subIndex, count, dataType, data, NULL, 1, useBlockMode);
}

/*!
 **
 **
 ** @param d
 ** @param nodeId
 ** @param index
 ** @param subIndex
 ** @param count
 ** @param dataType
 ** @param data
 ** @param Callback
 ** @param useBlockMode
 **
 ** @return
 **/
UNS8 writeNetworkDictCallBack(UNS8 nodeId, UNS16 index,
UNS8 subIndex, UNS32 count, UNS8 dataType, void *data, SDOCallback_t Callback, UNS8 useBlockMode) {
    return _writeNetworkDict(nodeId, index, subIndex, count, dataType, data, Callback, 1, useBlockMode);
}

UNS8 writeNetworkDictCallBackAI(UNS8 nodeId, UNS16 index,
UNS8 subIndex, UNS32 count, UNS8 dataType, void *data, SDOCallback_t Callback, UNS8 endianize, UNS8 useBlockMode) {
    UNS8 ret;
    UNS8 nodeIdServer;
    const subindex *si;
    UNS8 si_size;
    ODCallback_t *callbacks;
    UNS8 i = 0;

    ret = _writeNetworkDict(nodeId, index, subIndex, count, dataType, data, Callback, endianize, useBlockMode);
    if (ret == 0xFE) {
        while ((si = ObjDict_scanIndexOD(0x1280 + i, &si_size, &callbacks))) {
            if (si_size <= 3) {
                MSG_ERR(0x1AC8, "Subindex 3  not found at index ", 0x1280 + i);
                return 0xFF;
            }
            nodeIdServer = *(UNS8*) si[3].pObject;
            if (nodeIdServer == 0) {
                *(UNS16*) si[1].pObject = (UNS16) (0x600 + nodeId);
                *(UNS16*) si[2].pObject = (UNS16) (0x580 + nodeId);
                *(UNS8*) si[3].pObject = nodeId;
                return _writeNetworkDict(nodeId, index, subIndex, count, dataType, data, Callback, endianize, useBlockMode);
            }
            i++;
        }
        return 0xFF;
    } else if (ret == 0) {
        return 0;
    } else {
        return 0xFF;
    }
}

/*!
 **
 **
 ** @param d
 ** @param nodeId
 ** @param index
 ** @param subIndex
 ** @param dataType
 ** @param Callback
 ** @param useBlockMode
 **
 ** @return
 **/
INLINE UNS8 _readNetworkDict(UNS8 nodeId, UNS16 index, UNS8 subIndex, UNS8 dataType, SDOCallback_t Callback, UNS8 useBlockMode) {
    UNS8 err;
    UNS8 i;
    UNS8 CliNbr;
    UNS8 line;
    UNS8 data[8];

    MSG_WAR(0x3AD5, "Send SDO to read in the dictionary of node : ", nodeId);
    MSG_WAR(0x3AD6, "                                  At index : ", index);
    MSG_WAR(0x3AD7, "                                  subIndex : ", subIndex);

    /* First let's find the corresponding SDO client in our OD  */
    CliNbr = GetSDOClientFromNodeId(nodeId);
    if (CliNbr >= 0xFE)
        return CliNbr;

    /* Verify that there is no SDO communication yet. */
    err = getSDOlineOnUse(CliNbr, SDO_CLIENT, &line);
    if (!err) {
        MSG_ERR(0x1AD8, "SDO error : Communication yet established. with node : ", nodeId);
        return 0xFF;
    }
    /* Taking the line ... */
    err = getSDOfreeLine(SDO_CLIENT, &line);
    if (err) {
        MSG_ERR(0x1AD9, "SDO error : No line free, too many SDO in progress. Aborted for node : ", nodeId);
        return (0xFF);
    } else
        MSG_WAR(0x3AE0, "Transmission on line : ", line);

    if (useBlockMode) {
        initSDOline(line, CliNbr, index, subIndex, SDO_BLOCK_UPLOAD_IN_PROGRESS);
        /* Send the SDO to the server. Initiate block upload, cs=0. */
        ObjDict_Data.transfers[line].dataType = dataType;
        data[0] = (5 << 5) | SDO_BCS_INITIATE_UPLOAD_REQUEST;
        data[1] = index & 0xFF; /* LSB */
        data[2] = (index >> 8) & 0xFF; /* MSB */
        data[3] = subIndex;
        data[4] = SDO_BLOCK_SIZE;
        for (i = 5; i < 8; i++)
            data[i] = 0;
    } else {
        initSDOline(line, CliNbr, index, subIndex, SDO_UPLOAD_IN_PROGRESS);
        /* Send the SDO to the server. Initiate upload, cs=2. */
        ObjDict_Data.transfers[line].dataType = dataType;
        data[0] = (2 << 5);
        data[1] = index & 0xFF; /* LSB */
        data[2] = (index >> 8) & 0xFF; /* MSB */
        data[3] = subIndex;
        for (i = 4; i < 8; i++)
            data[i] = 0;
    }
    ObjDict_Data.transfers[line].Callback = Callback;
    err = sendSDO(SDO_CLIENT, CliNbr, data);
    if (err) {
        MSG_ERR(0x1AE5, "SDO. Error while sending SDO to node : ", nodeId);
        /* release the line */
        resetSDOline(line);
        return 0xFF;
    }
    return 0;
}

/*!
 **
 **
 ** @param d
 ** @param nodeId
 ** @param index
 ** @param subIndex
 ** @param dataType
 ** @param useBlockMode
 **
 ** @return
 **/
UNS8 readNetworkDict(UNS8 nodeId, UNS16 index, UNS8 subIndex, UNS8 dataType, UNS8 useBlockMode) {
    return _readNetworkDict(nodeId, index, subIndex, dataType, NULL, useBlockMode);
}

/*!
 **
 **
 ** @param d
 ** @param nodeId
 ** @param index
 ** @param subIndex
 ** @param dataType
 ** @param Callback
 ** @param useBlockMode
 **
 ** @return
 **/
UNS8 readNetworkDictCallback(UNS8 nodeId, UNS16 index, UNS8 subIndex, UNS8 dataType, SDOCallback_t Callback, UNS8 useBlockMode) {
    return _readNetworkDict(nodeId, index, subIndex, dataType, Callback, useBlockMode);
}

UNS8 readNetworkDictCallbackAI(UNS8 nodeId, UNS16 index, UNS8 subIndex, UNS8 dataType, SDOCallback_t Callback, UNS8 useBlockMode) {
    UNS8 ret;
    UNS8 nodeIdServer;
    UNS8 i;
    const subindex *si;
    UNS8 si_size;
    ODCallback_t *callbacks;

    ret = _readNetworkDict(nodeId, index, subIndex, dataType, Callback, useBlockMode);
    if (ret == 0xFE) {
        i = 0;
        while ((si = ObjDict_scanIndexOD(0x1280 + i, &si_size, &callbacks))) {
            if (si_size <= 3) {
                MSG_ERR(0x1AC8, "Subindex 3  not found at index ", 0x1280 + i);
                return 0xFF;
            }
            nodeIdServer = *(UNS8*) si[3].pObject;
            if (nodeIdServer == 0) {
                *(UNS16*) si[1].pObject = (UNS16) (0x600 + nodeId);
                *(UNS16*) si[2].pObject = (UNS16) (0x580 + nodeId);
                *(UNS8*) si[3].pObject = nodeId;
                return _readNetworkDict(nodeId, index, subIndex, dataType, Callback, useBlockMode);
            }
            i++;
        }
        return 0xFF;
    } else if (ret == 0) {
        return 0;
    } else {
        return 0xFF;
    }
}

/*!
 **
 **
 ** @param d
 ** @param nodeId
 ** @param data
 ** @param size : *size MUST contain the size of *data buffer before calling
 **                     The function set it to the actual number of written bytes
 ** @param abortCode
 **
 ** @return
 **    SDO_PROVIDED_BUFFER_TOO_SMALL if *data is not big enough
 **    or any transmission status value.
 **/
UNS8 getReadResultNetworkDict(UNS8 nodeId, void* data, UNS32 *size,
UNS32 * abortCode) {
    UNS32 i;
    UNS8 err;
    UNS8 CliNbr;
    UNS8 line;
    *abortCode = 0;

    /* First let's find the corresponding SDO client in our OD  */
    CliNbr = GetSDOClientFromNodeId(nodeId);
    if (CliNbr >= 0xFE) {
        *size = 0;
        return SDO_ABORTED_INTERNAL;
    }

    /* Looking for the line tranfert. */
    err = getSDOlineOnUse(CliNbr, SDO_CLIENT, &line);
    if (err) {
        MSG_ERR(0x1AF0, "SDO error : No line found for communication with node : ", nodeId);
        *size = 0;
        return SDO_ABORTED_INTERNAL;
    }

    /* If transfer not finished just return, but if aborted set abort code and size to 0 */
    if (ObjDict_Data.transfers[line].state != SDO_FINISHED) {
        if ((ObjDict_Data.transfers[line].state == SDO_ABORTED_RCV) || (ObjDict_Data.transfers[line].state == SDO_ABORTED_INTERNAL)) {
            *abortCode = ObjDict_Data.transfers[line].abortCode;
            *size = 0;
        }
        return ObjDict_Data.transfers[line].state;
    }

    /* if SDO initiated with e=0 and s=0 count is null, offset carry effective size*/
    if (ObjDict_Data.transfers[line].count == 0)
        ObjDict_Data.transfers[line].count = ObjDict_Data.transfers[line].offset;

    /* Check if the provided buffer is big enough */
    if (*size < ObjDict_Data.transfers[line].count) {
        *size = 0;
        return SDO_PROVIDED_BUFFER_TOO_SMALL;
    }

    /* Give back actual size */
    *size = ObjDict_Data.transfers[line].count;

    /* Copy payload to data pointer */
#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
    {
        UNS8 *lineData = ObjDict_Data.transfers[line].data;

        if (ObjDict_Data.transfers[line].dynamicData && ObjDict_Data.transfers[line].dynamicDataSize)
        {
            lineData = ObjDict_Data.transfers[line].dynamicData;
        }
        for ( i = 0; i < *size; i++) {
# ifdef CANOPEN_BIG_ENDIAN
            if (ObjDict_Data.transfers[line].dataType != visible_string)
            ( (char *) data)[*size - 1 - i] = lineData[i];
            else /* String of bytes. */
            ( (char *) data)[i] = lineData[i];
# else
            ( (char *) data)[i] = lineData[i];
# endif
        }
    }
#else //SDO_DYNAMIC_BUFFER_ALLOCATION
    for (i = 0; i < *size; i++) {
# ifdef CANOPEN_BIG_ENDIAN
        if (ObjDict_Data.transfers[line].dataType != visible_string)
        ( (char *) data)[*size - 1 - i] = ObjDict_Data.transfers[line].data[i];
        else /* String of bytes. */
        ( (char *) data)[i] = ObjDict_Data.transfers[line].data[i];
# else
        ((char *) data)[i] = ObjDict_Data.transfers[line].data[i];
# endif
    }
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION
    resetSDOline(line);
    return SDO_FINISHED;
}

/*!
 **
 **
 ** @param d
 ** @param nodeId
 ** @param abortCode
 **
 ** @return
 **/
UNS8 getWriteResultNetworkDict(UNS8 nodeId, UNS32 * abortCode) {
    UNS8 line = 0;
    UNS8 err;
    UNS8 CliNbr;
    *abortCode = 0;

    /* First let's find the corresponding SDO client in our OD  */
    CliNbr = GetSDOClientFromNodeId(nodeId);
    if (CliNbr >= 0xFE)
        return SDO_ABORTED_INTERNAL;

    /* Looking for the line tranfert. */
    err = getSDOlineOnUse(CliNbr, SDO_CLIENT, &line);
    if (err) {
        MSG_ERR(0x1AF1, "SDO error : No line found for communication with node : ", nodeId);
        return SDO_ABORTED_INTERNAL;
    }
    *abortCode = ObjDict_Data.transfers[line].abortCode;
    if (ObjDict_Data.transfers[line].state != SDO_FINISHED)
        return ObjDict_Data.transfers[line].state;
    resetSDOline(line);
    return SDO_FINISHED;
}
