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
 ** @file   objacces.c
 ** @author Edouard TISSERANT and Francis DUPIN
 ** @date   Tue Jun  5 08:55:23 2007
 **
 ** @brief
 **
 **
 */

#include "data.h"

// TODO: add eeprom persistance

UNS8 objectSize(const subindex *s) {
    switch (s->bDataType) {
        case CANopen_TYPE_boolean:
        case CANopen_TYPE_int8:
        case CANopen_TYPE_uint8:
        case CANopen_TYPE_domain:
            return 1;

        case CANopen_TYPE_int16:
        case CANopen_TYPE_uint16:
            return 2;

        case CANopen_TYPE_int32:
        case CANopen_TYPE_uint32:
        case CANopen_TYPE_real32:
            return 4;

        case CANopen_TYPE_int24:
        case CANopen_TYPE_uint24:
            return 3;

        case CANopen_TYPE_real64:
        case CANopen_TYPE_int64:
        case CANopen_TYPE_uint64:
            return 8;

        case CANopen_TYPE_int40:
        case CANopen_TYPE_uint40:
            return 5;

        case CANopen_TYPE_int48:
        case CANopen_TYPE_uint48:
            return 6;

        case CANopen_TYPE_int56:
        case CANopen_TYPE_uint56:
            return 7;

        case CANopen_TYPE_visible_string:
            return 10; // TODO: figure aut way to return true size
        case CANopen_TYPE_octet_string:
            return 10; // TODO: figure aut way to return true size
        case CANopen_TYPE_unicode_string:
            return 10; // TODO: figure aut way to return true size
    }

    return ObjDict_DataSize(s);
}

//We need the function implementation for linking
//Only a placeholder with a define isnt enough!
UNS8 accessDictionaryError(UNS16 index, UNS8 subIndex,
UNS32 sizeDataDict, UNS32 sizeDataGiven, UNS32 code) {
#ifdef DEBUG_WAR_CONSOLE_ON
    MSG_WAR(0x2B09, "Dictionary index : ", index);
    MSG_WAR(0X2B10, "           subindex : ", subIndex);
    switch (code) {
        case OD_NO_SUCH_OBJECT:
            MSG_WAR(0x2B11, "Index not found ", index)
            ;
            break;
        case OD_NO_SUCH_SUBINDEX:
            MSG_WAR(0x2B12, "SubIndex not found ", subIndex)
            ;
            break;
        case OD_WRITE_NOT_ALLOWED:
            MSG_WAR(0x2B13, "Write not allowed, data is read only ", index)
            ;
            break;
        case OD_LENGTH_DATA_INVALID:
            MSG_WAR(0x2B14, "Conflict size data. Should be (bytes)  : ", sizeDataDict)
            ;
            MSG_WAR(0x2B15, "But you have given the size  : ", sizeDataGiven)
            ;
            break;
        case OD_NOT_MAPPABLE:
            MSG_WAR(0x2B16, "Not mappable data in a PDO at index    : ", index)
            ;
            break;
        case OD_VALUE_TOO_LOW:
            MSG_WAR(0x2B17, "Value range error : value too low. SDOabort : ", code)
            ;
            break;
        case OD_VALUE_TOO_HIGH:
            MSG_WAR(0x2B18, "Value range error : value too high. SDOabort : ", code)
            ;
            break;
        default:
            MSG_WAR(0x2B20, "Unknown error code : ", code)
            ;
    }
#endif

    return 0;
}

UNS32 getODentry(UNS16 wIndex, UNS8 bSubindex, void * pDestData, UNS32 * pExpectedSize, UNS8 * pDataType, UNS8 checkAccess) {
    /* DO NOT USE MSG_ERR because the macro may send a PDO -> infinite loop if it fails. */
    UNS8 size;
    UNS32 errorCode;
    UNS32 szData;
    const subindex *si;
    ODCallback_t *Callback;

    si = ObjDict_scanIndexOD(wIndex, &size, &Callback);

    if (!si)
        return OD_NO_SUCH_OBJECT;
    if (size <= bSubindex) {
        /* Subindex not found */
        accessDictionaryError(wIndex, bSubindex, 0, 0, OD_NO_SUCH_SUBINDEX);
        return OD_NO_SUCH_SUBINDEX;
    }

    if (checkAccess && (si[bSubindex].bAccessType & WO)) {
        MSG_WAR(0x2B30, "Access Type : ", si[bSubindex].bAccessType);
        accessDictionaryError(wIndex, bSubindex, 0, 0, OD_READ_NOT_ALLOWED);
        return OD_READ_NOT_ALLOWED;
    }

    if (pDestData == 0) {
        return SDOABT_GENERAL_ERROR;
    }

    if (objectSize(&si[bSubindex]) > *pExpectedSize) {
        /* Requested variable is too large to fit into a transfer line, inform    *
         * the caller about the real size of the requested variable.              */
        *pExpectedSize = objectSize(&si[bSubindex]);
        return SDOABT_OUT_OF_MEMORY;
    }

    if (wIndex >= 0x2000 && wIndex < 0x6000 && Callback && Callback[bSubindex]) {
        errorCode = (Callback[bSubindex])(si, wIndex, bSubindex, 0);
        if (errorCode != OD_SUCCESSFUL)
            return errorCode;
    }

    *pDataType = si[bSubindex].bDataType;
    szData = objectSize(&si[bSubindex]);

    if (*pDataType != CANopen_TYPE_visible_string) {
        memcpy(pDestData, si[bSubindex].pObject, szData);
        *pExpectedSize = szData;
    } else {
        /* TODO : CONFORM TO DS-301 :
         *  - stop using NULL terminated strings
         *  - store string size in td_subindex
         * */
        /* Copy null terminated string to user, and return discovered size */
        UNS8 *ptr = (UNS8*) si[bSubindex].pObject;
        UNS8 *ptr_start = ptr;
        /* *pExpectedSize IS < szData . if null, use szData */
        UNS8 *ptr_end = ptr + (*pExpectedSize ? *pExpectedSize : szData);
        UNS8 *ptr_dest = (UNS8*) pDestData;
        while (*ptr && ptr < ptr_end) {
            *(ptr_dest++) = *(ptr++);
        }

        *pExpectedSize = (UNS32) (ptr - ptr_start);
        /* terminate string if not maximum length */
        if (*pExpectedSize < szData)
            *(ptr) = 0;
    }

    return OD_SUCCESSFUL;
}

UNS32 setODentry(UNS16 wIndex, UNS8 bSubindex, void * pSourceData, UNS32 * pExpectedSize, UNS8 checkAccess) {
    UNS32 szData;
    UNS8 dataType;
    UNS32 errorCode;
    UNS8 size;
    const subindex *si;
    ODCallback_t *Callback;

    si = ObjDict_scanIndexOD(wIndex, &size, &Callback);
    if (!si)
        return OD_NO_SUCH_OBJECT;

    if (size <= bSubindex) {
        /* Subindex not found */
        accessDictionaryError(wIndex, bSubindex, 0, *pExpectedSize, OD_NO_SUCH_SUBINDEX);
        return OD_NO_SUCH_SUBINDEX;
    }
    if (checkAccess && (si[bSubindex].bAccessType == RO)) {
        MSG_WAR(0x2B25, "Access Type : ", si[bSubindex].bAccessType);
        accessDictionaryError(wIndex, bSubindex, 0, *pExpectedSize, OD_WRITE_NOT_ALLOWED);
        return OD_WRITE_NOT_ALLOWED;
    }

    dataType = si[bSubindex].bDataType;
    szData = objectSize(&si[bSubindex]);

    if (*pExpectedSize == 0 || *pExpectedSize == szData
            || /* allow to store a shorter string than entry size */(dataType == CANopen_TYPE_visible_string
                    && *pExpectedSize < szData)) {
        errorCode = ObjDict_valueRangeTest(dataType, pSourceData);
        if (errorCode) {
            accessDictionaryError(wIndex, bSubindex, szData, *pExpectedSize, errorCode);
            return errorCode;
        }
        memcpy(si[bSubindex].pObject, pSourceData, *pExpectedSize);
        /* TODO : CONFORM TO DS-301 : 
         *  - stop using NULL terminated strings
         *  - store string size in td_subindex 
         * */
        /* terminate visible_string with '\0' */
        if (dataType == CANopen_TYPE_visible_string && *pExpectedSize < szData)
            ((UNS8*) si[bSubindex].pObject)[*pExpectedSize] = 0;

        *pExpectedSize = szData;

        // TODO: distinguishbetween read and write
        /* Callbacks */
        if (Callback && Callback[bSubindex]) {
            errorCode = (Callback[bSubindex])(si, wIndex, bSubindex, 1);
            if (errorCode != OD_SUCCESSFUL) {
                return errorCode;
            }
        }

        /* TODO : Store dans NVRAM */
//       if (ptrTable->pSubindex[bSubindex].bAccessType & TO_BE_SAVE){
//         (*d->storeODSubIndex)(wIndex, bSubindex);
//       }
        return OD_SUCCESSFUL;
    } else {
        *pExpectedSize = szData;
        accessDictionaryError(wIndex, bSubindex, szData, *pExpectedSize, OD_LENGTH_DATA_INVALID);
        return OD_LENGTH_DATA_INVALID;
    }
}

UNS32 RegisterSetODentryCallBack(UNS16 wIndex, UNS8 bSubindex, ODCallback_t Callback) {
    UNS8 size;
    ODCallback_t *CallbackList;
    const subindex *si;

    si = ObjDict_scanIndexOD(wIndex, &size, &CallbackList);
    if (si && CallbackList && bSubindex < size) {
        CallbackList[bSubindex] = Callback;
        return OD_SUCCESSFUL;
    } else
        return OD_NO_SUCH_OBJECT;
}

