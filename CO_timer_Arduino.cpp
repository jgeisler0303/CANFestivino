/*
 This file is part of CanFestival, a library implementing CanOpen Stack.

 Copyright (C): Edouard TISSERANT and Francis DUPIN
 AVR Port: Andreas GLAUSER and Peter CHRISTEN

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

// AVR implementation of the  CANopen timer driver, uses Timer 3 (16 bit)
// Includes for the Canfestival driver
#include "CO_timer.h"
#include "config.h"


// TODO: set number of timers via COInit
Timer<MAX_NB_TIMER> t;

TIMER_HANDLE SetAlarm(UNS8 id, timerCallback_t callback, TIMEVAL value, TIMEVAL period) {
    if(period) return t.every(period, callback, id);
    else return t.after(value, callback, id);
}

TIMER_HANDLE DelAlarm(TIMER_HANDLE handle) {
    t.stop(handle);
    return TIMER_NONE;
}

void TimeDispatch(void) {
    t.update();
}
