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

#ifndef __objdictdef_h__
#define __objdictdef_h__

/************************* CONSTANTES **********************************/
/** this are static defined datatypes taken fCODE the canopen standard. They
 *  are located at index 0x0001 to 0x001B. As described in the standard, they
 *  are in the object dictionary for definition purpose only. a device does not
 *  to support all of this datatypes.
 */
#define CANopen_TYPE_boolean         0x01
#define CANopen_TYPE_int8            0x02
#define CANopen_TYPE_int16           0x03
#define CANopen_TYPE_int32           0x04
#define CANopen_TYPE_uint8           0x05
#define CANopen_TYPE_uint16          0x06
#define CANopen_TYPE_uint32          0x07
#define CANopen_TYPE_real32          0x08
#define CANopen_TYPE_visible_string  0x09
#define CANopen_TYPE_octet_string    0x0A
#define CANopen_TYPE_unicode_string  0x0B
#define CANopen_TYPE_time_of_day     0x0C
#define CANopen_TYPE_time_difference 0x0D

#define CANopen_TYPE_domain          0x0F
#define CANopen_TYPE_int24           0x10
#define CANopen_TYPE_real64          0x11
#define CANopen_TYPE_int40           0x12
#define CANopen_TYPE_int48           0x13
#define CANopen_TYPE_int56           0x14
#define CANopen_TYPE_int64           0x15
#define CANopen_TYPE_uint24          0x16

#define CANopen_TYPE_uint40          0x18
#define CANopen_TYPE_uint48          0x19
#define CANopen_TYPE_uint56          0x1A
#define CANopen_TYPE_uint64          0x1B

#define CANopen_TYPE_pdo_communication_parameter 0x20
#define CANopen_TYPE_pdo_mapping                 0x21
#define CANopen_TYPE_sdo_parameter               0x22
#define CANopen_TYPE_identity                    0x23

/* CanFestival is using 0x24 to 0xFF to define some types containing a 
 value range (See how it works in objdict.c)
 */

/** Each entry of the object dictionary can be READONLY (RO), READ/WRITE (RW),
 *  WRITE-ONLY (WO)
 */
#define RW     0x00  
#define WO     0x01
#define RO     0x02

#define TO_BE_SAVE  0x04
#define DCF_TO_SEND 0x08

/************************ STRUCTURES ****************************/
/** This are some structs which are neccessary for creating the entries
 *  of the object dictionary.
 */
typedef struct td_subindex {
    UNS8 bAccessType :2;
    UNS8 bDataType :6; /* Defines of what datatype the entry is */
    //UNS8                   size;      /* The size (in Byte) of the variable */
    void* pObject; /* This is the pointer of the Variable */
}__attribute__ ((packed)) subindex;

/** Struct for creating entries in the communictaion profile
 */
typedef struct td_indextable {
    subindex* pSubindex; /* Pointer to the subindex */
    UNS8 bSubCount; /* the count of valid entries for this subindex
     * This count here defines how many memory has been
     * allocated. this memory does not have to be used.
     */
    //UNS16   index;
} indextable;

typedef struct s_quick_index {
    UNS16 SDO_SVR;
    UNS16 SDO_CLT;
    UNS16 PDO_RCV;
    UNS16 PDO_RCV_MAP;
    UNS16 PDO_TRS;
    UNS16 PDO_TRS_MAP;
} quick_index;

/*typedef struct struct_CO_Data CO_Data; */
typedef UNS32 (*ODCallback_t)(const subindex * OD_entry, UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess);

/************************** MACROS *********************************/

/* CANopen usefull helpers */
#define GET_NODE_ID(m)         (UNS16_LE(m.cob_id) & 0x7f)
#define GET_FUNCTION_CODE(m)   (UNS16_LE(m.cob_id) >> 7)

#endif /* __objdictdef_h__ */
