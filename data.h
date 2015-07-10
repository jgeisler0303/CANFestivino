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

#ifndef __data_h__
#define __data_h__

/* declaration of CO_Data type let us include all necessary headers
 struct struct_CO_Data can then be defined later
 */
typedef struct struct_CO_Data CO_Data;

#include "applicfg.h"
#include "states.h"
#include "def.h"
#include "CO_can.h"
#include "CO_timer.h"
#include "objdictdef.h"
#include "objacces.h"
#include "sdo.h"
#include "pdo.h"
#include "states.h"
#include "lifegrd.h"
#include "sync.h"
#include "nmtSlave.h"
#include "emcy.h"
#ifdef CO_ENABLE_LSS
#include "lss.h"
#endif

extern CO_Data ObjDict_Data;
extern UNS8 ObjDict_bDeviceNodeId;
extern const indextable ObjDict_objdict[];
extern s_PDO_status ObjDict_PDO_status[];
extern const UNS16 ObjDict_ObjdictSize;
extern const UNS8 ObjDict_iam_a_slave;
UNS32 ObjDict_valueRangeTest(UNS8 typeValue, void * value);
UNS8 ObjDict_DataSize(const subindex *s);

#define ConsumerHeartbeatCount ObjDict_highestSubIndex_obj1016
extern UNS8 ObjDict_highestSubIndex_obj1016;
#define ConsumerHeartbeatEntries ObjDict_obj1016
extern UNS32 ObjDict_obj1016[];
#define ProducerHeartBeatTime ObjDict_obj1017
extern UNS16 ObjDict_obj1017;
#define GuardTime ObjDict_obj100C
extern UNS16 ObjDict_obj100C;
#define LifeTimeFactor ObjDict_obj100D
extern UNS8 ObjDict_obj100D;
#define COB_ID_Sync ObjDict_obj1005
extern UNS16 ObjDict_obj100;
#define Sync_Cycle_Period ObjDict_obj1006
extern UNS32 ObjDict_obj1006;
// #define  Sync_window_length ObjDict_obj1007
// extern UNS32 ObjDict_obj1007;
const subindex * ObjDict_scanIndexOD(UNS16 wIndex, UNS8 *size, ODCallback_t **callbacks);
#define error_number ObjDict_highestSubIndex_obj1003
extern UNS8 ObjDict_highestSubIndex_obj1003;
#define error_first_element ObjDict_obj1003
extern UNS32 ObjDict_obj1003[];
#define error_register ObjDict_obj1001
extern UNS8 ObjDict_obj1001;
#define error_cobid ObjDict_obj1014
extern UNS16 ObjDict_obj1014;

/**
 * @ingroup od
 * @brief This structure contains all necessary informations to define a CANOpen node 
 */
struct struct_CO_Data {
    /* Object dictionary */
    // UNS8 *bDeviceNodeId;
    // const indextable *objdict;
    // s_PDO_status *PDO_status;
    UNS8 currentPDO;
    // TIMER_HANDLE *RxPDO_EventTimers;
    // void (*RxPDO_EventTimers_Handler)(UNS32);
    // const quick_index *firstIndex;
    // const quick_index *lastIndex;
    // const UNS16 *ObjdictSize;
    // const UNS8 *iam_a_slave;
    // valueRangeTest_t valueRangeTest;

    /* SDO */
    s_transfer transfers[SDO_MAX_SIMULTANEOUS_TRANSFERS];
    /* s_sdo_parameter *sdo_parameters; */

    /* State machine */
    e_nodeState nodeState;
    s_state_communication CurrentCommunicationState;
    // initialisation_t initialisation;
    // preOperational_t preOperational;
    // operational_t operational;
    // stopped_t stopped;
    // void (*NMT_Slave_Node_Reset_Callback)();
    // void (*NMT_Slave_Communications_Reset_Callback)();

    /* NMT-heartbeat */
    // UNS8 *ConsumerHeartbeatCount;
    // UNS32 *ConsumerHeartbeatEntries;
    // TIMER_HANDLE *ConsumerHeartBeatTimers;
    // UNS16 *ProducerHeartBeatTime;
    TIMER_HANDLE ProducerHeartBeatTimer;
    heartbeatError_t heartbeatError;
#if defined CO_ENABLE_CONSUMER_HEART_BEAT || defined CO_ENABLE_NODE_GUARD
    e_nodeState NMTable[NMT_MAX_NODE_ID];
#endif
    /* NMT-nodeguarding */
    TIMER_HANDLE GuardTimeTimer;
    // TIMER_HANDLE LifeTimeTimer;
    nodeguardError_t nodeguardError;
    // UNS16 *GuardTime;
    // UNS8 *LifeTimeFactor;
#ifdef CO_ENABLE_NODE_GUARD
    UNS8 nodeGuardStatus[NMT_MAX_NODE_ID];
#endif
    /* SYNC */
    TIMER_HANDLE syncTimer;
    // UNS16 *COB_ID_Sync;
    // UNS32 *Sync_Cycle_Period;
    /*UNS32 *Sync_window_length;;*/
    post_sync_t post_sync;
    post_TPDO_t post_TPDO;
    // post_SlaveBootup_t post_SlaveBootup;
    post_SlaveStateChange_t post_SlaveStateChange;

    /* General */
    UNS8 toggle;
    // CAN_PORT canHandle;	
    // scanIndexOD_t scanIndexOD;
    // storeODSubIndex_t storeODSubIndex; 

    /* DCF concise */
// 	const indextable* dcf_odentry;
// 	UNS8* dcf_cursor;
// 	UNS32 dcf_entries_count;
// 	UNS8 dcf_status;
// 	UNS32 dcf_size;
// 	UNS8* dcf_data;
    /* EMCY */
    e_errorState error_state;UNS8 error_history_size;
// 	UNS8* error_number;
// 	UNS32* error_first_element;
// 	UNS8* error_register;
// 	UNS16* error_cobid;
    s_errors error_data[EMCY_MAX_ERRORS];
// 	post_emcy_t post_emcy;

#ifdef CO_ENABLE_LSS
    /* LSS */
    lss_transfer_t lss_transfer;
    lss_StoreConfiguration_t lss_StoreConfiguration;
#endif	
};

#define NMTable_Initializer Unknown_state,
#if defined CO_ENABLE_CONSUMER_HEART_BEAT || defined CO_ENABLE_NODE_GUARD
#define NMTable_Array_Initializer {REPEAT_NMT_MAX_NODE_ID_TIMES(NMTable_Initializer)}, /* is  well initialized at "Unknown_state". Is it ok ? (FD)*/
#else
#define NMTable_Array_Initializer
#endif

#define nodeGuardStatus_Initializer 0x00,
#ifdef CO_ENABLE_NODE_GUARD
#define nodeGuardStatus_Array_Initializer {REPEAT_NMT_MAX_NODE_ID_TIMES(nodeGuardStatus_Initializer)},
#else
#define nodeGuardStatus_Array_Initializer 
#endif

#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
#define s_transfer_Initializer {\
		0,          /* CliServNbr */\
		0,          /* wohami */\
		SDO_RESET,  /* state */\
		0,          /* toggle */\
		0,          /* abortCode */\
		0,          /* index */\
		0,          /* subIndex */\
		0,          /* count */\
		0,          /* offset */\
		{0},        /* data (static use, so that all the table is initialize at 0)*/\
    NULL,       /* dynamicData */ \
    0,          /* dynamicDataSize */ \
		0,          /* peerCRCsupport */\
		0,          /* blksize */\
		0,          /* ackseq */\
		0,          /* objsize */\
		0,          /* lastblockoffset */\
		0,          /* seqno */\
		0,          /* endfield */\
		RXSTEP_INIT,/* rxstep */\
		{0},        /* tmpData */\
		0,          /* dataType */\
		-1,         /* timer */\
		NULL        /* Callback */\
	  },
#else
#define s_transfer_Initializer {\
		0,          /* nodeId */\
		0,          /* wohami */\
		SDO_RESET,  /* state */\
		0,          /* toggle */\
		0,          /* abortCode */\
		0,          /* index */\
		0,          /* subIndex */\
		0,          /* count */\
		0,          /* offset */\
		{0},        /* data (static use, so that all the table is initialize at 0)*/\
		0,          /* peerCRCsupport */\
		0,          /* blksize */\
		0,          /* ackseq */\
		0,          /* objsize */\
		0,          /* lastblockoffset */\
		0,          /* seqno */\
		0,          /* endfield */\
		RXSTEP_INIT,/* rxstep */\
		{0},        /* tmpData */\
		0,          /*  */\
		-1,         /*  */\
		NULL        /*  */\
	  },
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

#define ERROR_DATA_INITIALIZER \
	{\
	0, /* errCode */\
	0, /* errRegMask */\
	0 /* active */\
	},

#ifdef CO_ENABLE_LSS

#ifdef CO_ENABLE_LSS_FS	
#define lss_fs_Initializer \
		,0,						/* IDNumber */\
  		128, 					/* BitChecked */\
  		0,						/* LSSSub */\
  		0, 						/* LSSNext */\
  		0, 						/* LSSPos */\
  		LSS_FS_RESET,			/* FastScan_SM */\
  		-1,						/* timerFS */\
  		{{0,0,0,0},{0,0,0,0}}   /* lss_fs_transfer */
#else
#define lss_fs_Initializer
#endif		

#define lss_Initializer {\
		LSS_RESET,  			/* state */\
		0,						/* command */\
		LSS_WAITING_MODE, 		/* mode */\
		0,						/* dat1 */\
		0,						/* dat2 */\
		0,          			/* NodeID */\
		0,          			/* addr_sel_match */\
		0,          			/* addr_ident_match */\
		"none",         		/* BaudRate */\
		0,          			/* SwitchDelay */\
		SDELAY_OFF,   			/* SwitchDelayState */\
		NULL,					/* canHandle_t */\
		-1,						/* TimerMSG */\
		-1,          			/* TimerSDELAY */\
		NULL,        			/* Callback */\
		0						/* LSSanswer */\
		lss_fs_Initializer		/*FastScan service initialization */\
	  },\
	  NULL 	/* _lss_StoreConfiguration*/
#else
#define lss_Initializer
#endif

/* A macro to initialize the data in client app.*/
/* CO_Data structure */
#define CANOPEN_NODE_DATA_INITIALIZER(NODE_PREFIX) {\
	/* Object dictionary*/\
	/*& NODE_PREFIX ## _bDeviceNodeId,      bDeviceNodeId */\
	/*NODE_PREFIX ## _objdict,              objdict  */\
	/* NODE_PREFIX ## _PDO_status,          PDO_status */\
	0,                                   /* currentPDO */ \
	/* NULL,                             RxPDO_EventTimers */\
	/* _RxPDO_EventTimers_Handler,          RxPDO_EventTimers_Handler */\
	/* & NODE_PREFIX ## _firstIndex,        firstIndex */\
	/* & NODE_PREFIX ## _lastIndex,         lastIndex */\
	/* & NODE_PREFIX ## _ObjdictSize,       ObjdictSize */\
	/* & NODE_PREFIX ## _iam_a_slave,       iam_a_slave */\
	/* NODE_PREFIX ## _valueRangeTest,      valueRangeTest */\
	\
	/* SDO, structure s_transfer */\
	{\
          REPEAT_SDO_MAX_SIMULTANEOUS_TRANSFERS_TIMES(s_transfer_Initializer)\
	},\
	\
	/* State machine*/\
	Unknown_state,      /* nodeState */\
	/* structure s_state_communication */\
	{\
		0,          /* csBoot_Up */\
		0,          /* csSDO */\
		0,          /* csEmergency */\
		0,          /* csSYNC */\
		0,          /* csHeartbeat */\
		0,           /* csPDO */\
		0           /* csLSS */\
	},\
	/* _initialisation,     initialisation */\
	/* _preOperational,     preOperational */\
	/* _operational,        operational */\
	/* _stopped,            stopped */\
	/* NULL,                NMT node reset callback */\
	/* NULL,                NMT communications reset callback */\
	\
	/* NMT-heartbeat */\
	/* & NODE_PREFIX ## _highestSubIndex_obj1016, ConsumerHeartbeatCount */\
	/* NODE_PREFIX ## _obj1016,                   ConsumerHeartbeatEntries */\
	/* NODE_PREFIX ## _heartBeatTimers,           ConsumerHeartBeatTimers  */\
	/* & NODE_PREFIX ## _obj1017,                 ProducerHeartBeatTime */\
	TIMER_NONE,                                /* ProducerHeartBeatTimer */\
	_heartbeatError,           /* heartbeatError */\
	\
	NMTable_Array_Initializer  \
	\
	/* NMT-nodeguarding */\
	TIMER_NONE,                              /*   GuardTimeTimer */\
	/* TIMER_NONE,                                LifeTimeTimer */\
	_nodeguardError,          /*  nodeguardError */\
	/* & NODE_PREFIX ## _obj100C,                 GuardTime */\
	/* & NODE_PREFIX ## _obj100D,                 LifeTimeFactor */\
	nodeGuardStatus_Array_Initializer\
	\
	/* SYNC */\
	TIMER_NONE,                                /* syncTimer */\
	/* & NODE_PREFIX ## _obj1005,                 COB_ID_Sync */\
	/* & NODE_PREFIX ## _obj1006,                 Sync_Cycle_Period */\
	/*& NODE_PREFIX ## _obj1007, */            /* Sync_window_length */\
	_post_sync,                 /* post_sync */\
	_post_TPDO,                 /* post_TPDO */\
	/* _post_SlaveBootup,			post_SlaveBootup */\
  _post_SlaveStateChange,			/* post_SlaveStateChange */\
	\
	/* General */\
	0,                                         /* toggle */\
	/* NULL,                   canSend */\
	/* NODE_PREFIX ## _scanIndexOD,                scanIndexOD */\
	/* _storeODSubIndex,                storeODSubIndex */\
    /* DCF concise */\
	/*NULL,       dcf_odentry*/\
	/*NULL,		dcf_cursor*/\
	/*1,		dcf_entries_count*/\
	/*0,		 dcf_status*/\
	/*0,       dcf_size */\
	 /*NULL,   dcf_data */\
	\
	/* EMCY */\
	Error_free,                      /* error_state */\
	sizeof(NODE_PREFIX ## _obj1003) / sizeof(NODE_PREFIX ## _obj1003[0]),      /* error_history_size */\
	/* & NODE_PREFIX ## _highestSubIndex_obj1003,    error_number */\
	/* & NODE_PREFIX ## _obj1003[0],    error_first_element */\
	/* & NODE_PREFIX ## _obj1001,       error_register */\
	/* & NODE_PREFIX ## _obj1014,       error_cobid */\
	/* error_data: structure s_errors */\
	{\
	REPEAT_EMCY_MAX_ERRORS_TIMES(ERROR_DATA_INITIALIZER)\
	},\
	/* _post_emcy,              post_emcy */\
	/* LSS */\
	lss_Initializer\
}

// #ifdef __cplusplus
// };
// #endif

#endif /* __data_h__ */

