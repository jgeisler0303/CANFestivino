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
 ** @file   sync.c
 ** @author Edouard TISSERANT and Francis DUPIN
 ** @date   Tue Jun  5 09:32:32 2007
 **
 ** @brief
 **
 **
 */

#include "data.h"
#include "sync.h"
#include "sysdep.h"

/* Prototypes for internals functions */
#ifdef INCLUDE_SYNC_FUNCTIONS
/*!                                                                                                
 **                                                                                                 
 **                                                                                                 
 ** @param d                                                                                        
 ** @param id                                                                                       
 **/
void SyncAlarm(CO_Data* d, UNS32 id);
UNS32 OnCOB_ID_SyncUpdate(CO_Data* d, const indextable * unsused_indextable,
        UNS8 unsused_bSubindex);

/*!                                                                                                
 **                                                                                                 
 **                                                                                                 
 ** @param d                                                                                        
 ** @param id                                                                                       
 **/
void SyncAlarm(CO_Data* d, UNS32 id)
{
    sendSYNC(d);
}

/*!                                                                                                
 ** This is called when Index 0x1005 is updated.                                                                                                
 **                                                                                                 
 ** @param d                                                                                        
 ** @param unsused_indextable                                                                       
 ** @param unsused_bSubindex                                                                        
 **                                                                                                 
 ** @return                                                                                         
 **/
UNS32 OnCOB_ID_SyncUpdate(const subindex * OD_entry, UNS16 bIndex, UNS8 bSubindex, UNS8 writeAccess) {
    startSYNC();
    return 0;
}

/*!                                                                                                
 **                                                                                                 
 **                                                                                                 
 ** @param d                                                                                        
 **/
void startSYNC()
{
    if(ObjDict_Data->syncTimer != TIMER_NONE) {
        stopSYNC();
    }

    RegisterSetODentryCallBack(ObjDict_Data, 0x1005, 0, &OnCOB_ID_SyncUpdate);
    RegisterSetODentryCallBack(ObjDict_Data, 0x1006, 0, &OnCOB_ID_SyncUpdate);

    if(COB_ID_Sync & 0x40000000ul && Sync_Cycle_Period)
    {
        ObjDict_Data->syncTimer = SetAlarm(
                0 /*No id needed*/,
                &SyncAlarm,
                MS_TO_TIMEVAL(*ObjDict_Data->Sync_Cycle_Period/1000),
                MS_TO_TIMEVAL(*ObjDict_Data->Sync_Cycle_Period/1000));
    }
}

/*!                                                                                                
 **                                                                                                 
 **                                                                                                 
 ** @param d                                                                                        
 **/
void stopSYNC()
{
    RegisterSetODentryCallBack(ObjDict_Data, 0x1005, 0, NULL);
    RegisterSetODentryCallBack(ObjDict_Data, 0x1006, 0, NULL);
    ObjDict_Data->syncTimer = DelAlarm(ObjDict_Data->syncTimer);
}

/*!                                                                                                
 **                                                                                                 
 **                                                                                                 
 ** @param d                                                                                        
 ** @param cob_id                                                                                   
 **                                                                                                 
 ** @return                                                                                         
 **/
UNS8 sendSYNCMessage(CO_Data* d)
{
    Message m;

    MSG_WAR(0x3001, "sendSYNC ", 0);

    m.cob_id = (UNS16)UNS16_LE(COB_ID_Sync);
    m.rtr = NOT_A_REQUEST;
    m.len = 0;

    return canSend(&m);
}

/*!                                                                                                
 **                                                                                                 
 **                                                                                                 
 ** @param d                                                                                        
 ** @param cob_id                                                                                   
 **                                                                                                 
 ** @return                                                                                         
 **/
UNS8 sendSYNC(CO_Data* d)
{
    UNS8 res;
    res = sendSYNCMessage(d);
    proceedSYNC(d);
    return res;
}

#else
void startSYNC(CO_Data* d) {
}
void stopSYNC(CO_Data* d) {
}
#endif

/*!                                                                                                
 **                                                                                                 
 **                                                                                                 
 ** @param d                                                                                        
 ** @param m                                                                                        
 **                                                                                                 
 ** @return                                                                                         
 **/
UNS8 proceedSYNC(CO_Data* d) {

    UNS8 res;

    MSG_WAR(0x3002, "SYNC received. Proceed. ", 0);

    (*d->post_sync)(d);

    /* only operational state allows PDO transmission */
    if (!d->CurrentCommunicationState.csPDO)
        return 0;

    res = _sendPDOevent(d, 1 /*isSyncEvent*/);

    /*Call user app callback*/
    (*d->post_TPDO)(d);

    return res;

}

void _post_sync(CO_Data* d) {
}
void _post_TPDO(CO_Data* d) {
}
