#include "CO_can.h"
#include "CO_ErrorState.h"
#include "applicfg.h"
#include "states.h"

// TODO: add rx and tx queue to can driver and make interrupt driven
// TODO: make CS selectable via template
MCP_CAN CAN(7); // TODO add CS to CO_Init call!

UNS8 canSend(Message *m) {
    if (CAN.sendMsgBuf((uint32_t)m->cob_id, 0, m->rtr, m->len, m->data) == CAN_ALL_TX_BUSY) {
        if (CAN.checkTxError())
            setTxErrorState (tx_error);
        else
            setTxErrorState (tx_all_busy);
    } else {
        if (isTxNoError())
            flashRed();
    }
    return 0;
}



void initCAN() {
    // TODO: make baud rate variable
  while(CAN.begin(CAN_500KBPS)!=0) delay(1000);

  CAN.init_Mask(0,0, 0x007E); // allow two consecutive ids
  CAN.init_Filt(0,0, getNodeId()); // RxPDO and SDO
  CAN.init_Filt(1,0, getNodeId()); // same

  CAN.init_Mask(1,0, 0x07FF);
  CAN.init_Filt(2,0, 0x0000); // NMT
  CAN.init_Filt(3,0, 0x0080); // sync
  CAN.init_Filt(4,0, 0x0100); // time stamp
  CAN.init_Filt(5,0, 0x0000); // dummy
}
