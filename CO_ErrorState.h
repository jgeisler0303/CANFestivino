#ifndef ERROR_STATE_H
#define ERROR_STATE_H

#include <avr/io.h>

const uint8_t CANopenErrReg_Generic= _BV(0);
const uint8_t CANopenErrReg_Current= _BV(1);
const uint8_t CANopenErrReg_Voltage= _BV(2);
const uint8_t CANopenErrReg_Temperature= _BV(3);
const uint8_t CANopenErrReg_Communication= _BV(4);
const uint8_t CANopenErrReg_DevProfile= _BV(5);
const uint8_t CANopenErrReg_Manufacturer= _BV(7);

enum tx_error_state_enum {
  tx_no_error= 0,
  tx_warning,
  tx_all_busy,
  tx_passive,
  tx_error,
  tx_bus_off
};

enum rx_error_state_enum {
  rx_no_error= 0,
  rx_warning,
  rx_passive,
  rx_overflow
};

void setTxErrorState(tx_error_state_enum e);
void resetTxErrorState();
void setRxErrorState(rx_error_state_enum e);
void resetRxErrorState();
bool isRxNoError();
bool isTxNoError();
void updateTxErrorState();
void updateRxErrorState();
bool nextRedBlinkState();
bool nextGreenBlinkState();
void flashRed();
void flashGreen();
bool greePatternStarted();

#endif // ERROR_STATE_H
