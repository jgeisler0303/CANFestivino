#include "CO_ErrorState.h"
#include "BlinkPattern.h"
#include "emcy.h"

BlinkPattern RedBlink= BlinkPattern();
BlinkPattern GreenBlink= BlinkPattern();

static tx_error_state_enum tx_error_state= tx_no_error;
static tx_error_state_enum tx_last_error= tx_no_error;
static rx_error_state_enum rx_error_state= rx_no_error;
static rx_error_state_enum rx_last_error= rx_no_error;

void setTxErrorState(tx_error_state_enum e) {
  if(e>tx_error_state) {
    tx_error_state= e;
    switch(e) {
      case tx_warning:
        RedBlink.setPattern(BlinkPattern::Blink2);
        break;
      case tx_all_busy:
        RedBlink.setPattern(BlinkPattern::Blink31);
        break;
      case tx_passive:
        RedBlink.setPattern(BlinkPattern::Blink4);
        break;
      case tx_error:
        RedBlink.setPattern(BlinkPattern::Blink62);
        break;
      case tx_bus_off:
        RedBlink.setPattern(BlinkPattern::Blink8);
        break;
    }
  }
}

void resetTxErrorState() {
  tx_error_state= tx_no_error;
  RedBlink.setPattern(BlinkPattern::OFF);
}

void setRxErrorState(rx_error_state_enum e) {
  if(e>rx_error_state) {
    rx_error_state= e;
    switch(e) {
      case rx_warning:
        GreenBlink.setPattern(BlinkPattern::Blink2);
        break;
      case rx_passive:
        GreenBlink.setPattern(BlinkPattern::Blink4);
        break;
      case rx_overflow:
        GreenBlink.setPattern(BlinkPattern::Blink14);
        break;
    }
  }
}

void resetRxErrorState() {
  rx_error_state= rx_no_error;
  GreenBlink.setPattern(BlinkPattern::OFF);
}

bool isRxNoError() {
    return rx_error_state == rx_no_error;
}

bool isTxNoError() {
    return tx_error_state == tx_no_error;
}

const uint16_t CANopenMonErr_TxWarn= 0x8100;
const uint16_t CANopenMonErr_TxAllBusy= 0x8101;
const uint16_t CANopenMonErr_TxPassive= 0x8102;
const uint16_t CANopenMonErr_TxErr= 0x8103;
const uint16_t CANopenMonErr_TxBusOff= 0x8104;

const uint16_t CANopenMonErr_RxWarn= 0x8200;
const uint16_t CANopenMonErr_RxPassive= 0x8201;
const uint16_t CANopenMonErr_RxOverFlow= 0x8202;

void updateTxErrorState() {
    if (tx_last_error != tx_error_state) {
        if (tx_error_state == tx_no_error) {
            EMCY_errorRecovered(CANopenMonErr_TxBusOff);
            EMCY_errorRecovered(CANopenMonErr_TxErr);
            EMCY_errorRecovered(CANopenMonErr_TxPassive);
            EMCY_errorRecovered(CANopenMonErr_TxAllBusy);
            EMCY_errorRecovered(CANopenMonErr_TxWarn);
        } else {
            switch (tx_error_state) {
                case tx_warning:
                    EMCY_setError(CANopenMonErr_TxWarn, CANopenErrReg_Communication, 0);
                    break;
                case tx_all_busy: // TODO: will probably not get sent !!
                    EMCY_setError(CANopenMonErr_TxAllBusy, CANopenErrReg_Communication, 0);
                    break;
                case tx_passive:
                    EMCY_setError(CANopenMonErr_TxPassive, CANopenErrReg_Communication, 0);
                    break;
                case tx_error: // TODO: will probably not get sent !!
                    EMCY_setError(CANopenMonErr_TxErr, CANopenErrReg_Communication, 0);
                    break;
                case tx_bus_off: // TODO: will probably not get sent !!
                    EMCY_setError(CANopenMonErr_TxBusOff, CANopenErrReg_Communication, 0);
                    break;
            }
            switch (tx_error_state) {
                case tx_bus_off:
                    EMCY_errorRecovered(CANopenMonErr_TxErr);
                case tx_error:
                    EMCY_errorRecovered(CANopenMonErr_TxPassive);
                case tx_passive:
                    EMCY_errorRecovered(CANopenMonErr_TxAllBusy);
                case tx_all_busy:
                    EMCY_errorRecovered(CANopenMonErr_TxWarn);
            }
        }
        tx_last_error = tx_error_state;
    }
}

void updateRxErrorState() {
    if (rx_last_error != rx_error_state) {
        if (rx_error_state == rx_no_error) {
            EMCY_errorRecovered(CANopenMonErr_RxOverFlow);
            EMCY_errorRecovered(CANopenMonErr_RxPassive);
            EMCY_errorRecovered(CANopenMonErr_RxWarn);
        } else {
            switch (rx_error_state) {
                case rx_warning:
                    EMCY_setError(CANopenMonErr_RxWarn, CANopenErrReg_Communication, 0);
                    break;
                case rx_passive:
                    EMCY_setError(CANopenMonErr_RxPassive, CANopenErrReg_Communication, 0);
                    break;
                case rx_overflow:
                    EMCY_setError(CANopenMonErr_RxOverFlow, CANopenErrReg_Communication, 0);
                    break;
            }
            switch (rx_error_state) {
                case rx_overflow:
                    EMCY_errorRecovered(CANopenMonErr_RxPassive);
                case rx_passive:
                    EMCY_errorRecovered(CANopenMonErr_RxWarn);
            }
        }
        rx_last_error = rx_error_state;
    }
}

bool nextRedBlinkState() {
    return RedBlink.nextState();
}

bool nextGreenBlinkState() {
    return GreenBlink.nextState();
}

void flashRed() {
    RedBlink.setPattern(BlinkPattern::Flash1);
}

void flashGreen() {
    GreenBlink.setPattern(BlinkPattern::Flash1);
}

bool greePatternStarted() {
    return GreenBlink.patternStarted();
}
