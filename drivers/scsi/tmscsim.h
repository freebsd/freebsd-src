/***********************************************************************
;*	File Name : TMSCSIM.H					       *
;*		    TEKRAM DC-390(T) PCI SCSI Bus Master Host Adapter  *
;*		    Device Driver				       *
;***********************************************************************/
/* $Id: tmscsim.h,v 2.15.2.3 2000/11/17 20:52:27 garloff Exp $ */

#ifndef _TMSCSIM_H
#define _TMSCSIM_H

#include <linux/types.h>
#include <linux/config.h>

#define SCSI_IRQ_NONE 255

#define MAX_ADAPTER_NUM 	4
#define MAX_SG_LIST_BUF 	16	/* Not used */
#define MAX_CMD_PER_LUN 	32
#define MAX_CMD_QUEUE		MAX_CMD_PER_LUN+MAX_CMD_PER_LUN/2+1	
#define MAX_SCSI_ID		8
#define MAX_SRB_CNT		MAX_CMD_QUEUE+1	/* Max number of started commands */

#define SEL_TIMEOUT		153	/* 250 ms selection timeout (@ 40 MHz) */

#define END_SCAN		2

typedef u8		UCHAR;	/*  8 bits */
typedef u16		USHORT;	/* 16 bits */
typedef u32		UINT;	/* 32 bits */
typedef unsigned long	ULONG;	/* 32/64 bits */

typedef UCHAR		*PUCHAR;
typedef USHORT		*PUSHORT;
typedef UINT		*PUINT;
typedef ULONG		*PULONG;
typedef Scsi_Host_Template	*PSHT;
typedef struct Scsi_Host	*PSH;
typedef Scsi_Device	*PSCSIDEV;
typedef Scsi_Cmnd	*PSCSICMD;
typedef void		*PVOID;
typedef struct scatterlist  *PSGL, SGL;


/*;-----------------------------------------------------------------------*/
typedef  struct  _SyncMsg
{
UCHAR		ExtendMsg;
UCHAR		ExtMsgLen;
UCHAR		SyncXferReq;
UCHAR		Period;
UCHAR		ReqOffset;
} SyncMsg;
/*;-----------------------------------------------------------------------*/
typedef  struct  _Capacity
{
ULONG		BlockCount;
ULONG		BlockLength;
} Capacity;
/*;-----------------------------------------------------------------------*/
typedef  struct  _SGentry
{
ULONG		SGXferDataPtr;
ULONG		SGXferDataLen;
} SGentry;

typedef  struct  _SGentry1
{
ULONG		SGXLen;
ULONG		SGXPtr;
} SGentry1, *PSGE;


/*
;-----------------------------------------------------------------------
; SCSI Request Block
;-----------------------------------------------------------------------
*/
struct	_SRB
{
//UCHAR		CmdBlock[12];

struct _SRB	*pNextSRB;
struct _DCB	*pSRBDCB;
PSCSICMD	pcmd;
PSGL		pSegmentList;

/* 0x10: */
SGL		Segmentx;	/* make a one entry of S/G list table */

/* 0x1c: */
ULONG		SGBusAddr;	/*;a segment starting address as seen by AM53C974A*/
ULONG		SGToBeXferLen;	/*; to be xfer length */
ULONG		TotalXferredLen;
ULONG		SavedTotXLen;
UINT		SRBState;

/* 0x30: */
UCHAR		SRBStatus;
UCHAR		SRBFlag;	/*; b0-AutoReqSense,b6-Read,b7-write */
				/*; b4-settimeout,b5-Residual valid */
UCHAR		AdaptStatus;
UCHAR		TargetStatus;

UCHAR		ScsiPhase;
UCHAR		TagNumber;
UCHAR		SGIndex;
UCHAR		SGcount;

/* 0x38: */
UCHAR		MsgCnt;
UCHAR		EndMessage;
UCHAR		RetryCnt;
UCHAR		SavedSGCount;			

ULONG		Saved_Ptr;

/* 0x40: */
UCHAR		MsgInBuf[6];
UCHAR		MsgOutBuf[6];

//UCHAR		IORBFlag;	/*;81h-Reset, 2-retry */
/* 0x4c: */
};


typedef  struct  _SRB	 DC390_SRB, *PSRB;

/*
;-----------------------------------------------------------------------
; Device Control Block
;-----------------------------------------------------------------------
*/
struct	_DCB
{
struct _DCB	*pNextDCB;
struct _ACB	*pDCBACB;

/* Aborted Commands */
//PSCSICMD	AboIORBhead;
//PSCSICMD	AboIORBtail;
//ULONG		AboIORBcnt;

/* 0x08: */
/* Queued SRBs */
PSRB		pWaitingSRB;
PSRB		pWaitLast;
PSRB		pGoingSRB;
PSRB		pGoingLast;
PSRB		pActiveSRB;
UCHAR		WaitSRBCnt;	/* Not used */
UCHAR		GoingSRBCnt;

UCHAR		DevType;
UCHAR		MaxCommand;

/* 0x20: */
UINT		TagMask;

UCHAR		TargetID;	/*; SCSI Target ID  (SCSI Only) */
UCHAR		TargetLUN;	/*; SCSI Log.  Unit (SCSI Only) */
UCHAR		DevMode;
UCHAR		DCBFlag;

UCHAR		CtrlR1;
UCHAR		CtrlR3;
UCHAR		CtrlR4;
UCHAR		Inquiry7;

/* 0x2c: */
UCHAR		SyncMode;	/*; 0:async mode */
UCHAR		NegoPeriod;	/*;for nego. */
UCHAR		SyncPeriod;	/*;for reg. */
UCHAR		SyncOffset;	/*;for reg. and nego.(low nibble) */

/* 0x30:*/
//UCHAR		InqDataBuf[8];
//UCHAR		CapacityBuf[8];
///* 0x40: */
};

typedef  struct  _DCB	 DC390_DCB, *PDCB;
/*
;-----------------------------------------------------------------------
; Adapter Control Block
;-----------------------------------------------------------------------
*/
struct	_ACB
{
PSH		pScsiHost;
struct _ACB	*pNextACB;
USHORT		IOPortBase;
UCHAR		IRQLevel;
UCHAR		status;

UCHAR		SRBCount;
UCHAR		AdapterIndex;	/*; nth Adapter this driver */
UCHAR		DeviceCnt;
UCHAR		DCBCnt;

/* 0x10: */
UCHAR		TagMaxNum;
UCHAR		ACBFlag;
UCHAR		Gmode2;
UCHAR		scan_devices;

PDCB		pLinkDCB;
PDCB		pLastDCB;
PDCB		pDCBRunRobin;

PDCB		pActiveDCB;
PSRB		pFreeSRB;
PSRB		pTmpSRB;

/* 0x2c: */
ULONG		QueryCnt;
PSCSICMD	pQueryHead;
PSCSICMD	pQueryTail;

/* 0x38: */
UCHAR		msgin123[4];
UCHAR		DCBmap[MAX_SCSI_ID];
UCHAR		Connected;
UCHAR		pad;

/* 0x3c: */
#if defined(USE_SPINLOCKS) && USE_SPINLOCKS > 1 && (defined(CONFIG_SMP) || DEBUG_SPINLOCKS > 0)
spinlock_t	lock;
#endif
UCHAR		sel_timeout;
UCHAR		glitch_cfg;

UCHAR		MsgLen;
UCHAR		Ignore_IRQ;	/* Not used */

PDEVDECL1;			/* Pointer to PCI cfg. space */
/* 0x4c/0x48: */
ULONG		Cmds;
UINT		SelLost;
UINT		SelConn;
UINT		CmdInQ;
UINT		CmdOutOfSRB;
	
/* 0x60/0x5c: */
struct timer_list	Waiting_Timer;
/* 0x74/0x70: */
DC390_SRB	TmpSRB;
/* 0xd8/0xd4: */
DC390_SRB	SRB_array[MAX_SRB_CNT]; 	/* 50 SRBs */
/* 0xfb0/0xfac: */
};

typedef  struct  _ACB	 DC390_ACB, *PACB;

/*;-----------------------------------------------------------------------*/


#define BIT31	0x80000000
#define BIT30	0x40000000
#define BIT29	0x20000000
#define BIT28	0x10000000
#define BIT27	0x08000000
#define BIT26	0x04000000
#define BIT25	0x02000000
#define BIT24	0x01000000
#define BIT23	0x00800000
#define BIT22	0x00400000
#define BIT21	0x00200000
#define BIT20	0x00100000
#define BIT19	0x00080000
#define BIT18	0x00040000
#define BIT17	0x00020000
#define BIT16	0x00010000
#define BIT15	0x00008000
#define BIT14	0x00004000
#define BIT13	0x00002000
#define BIT12	0x00001000
#define BIT11	0x00000800
#define BIT10	0x00000400
#define BIT9	0x00000200
#define BIT8	0x00000100
#define BIT7	0x00000080
#define BIT6	0x00000040
#define BIT5	0x00000020
#define BIT4	0x00000010
#define BIT3	0x00000008
#define BIT2	0x00000004
#define BIT1	0x00000002
#define BIT0	0x00000001

/*;---UnitCtrlFlag */
#define UNIT_ALLOCATED	BIT0
#define UNIT_INFO_CHANGED BIT1
#define FORMATING_MEDIA BIT2
#define UNIT_RETRY	BIT3

/*;---UnitFlags */
#define DASD_SUPPORT	BIT0
#define SCSI_SUPPORT	BIT1
#define ASPI_SUPPORT	BIT2

/*;----SRBState machine definition */
#define SRB_FREE	0
#define SRB_WAIT	BIT0
#define SRB_READY	BIT1
#define SRB_MSGOUT	BIT2	/*;arbitration+msg_out 1st byte*/
#define SRB_MSGIN	BIT3
#define SRB_MSGIN_MULTI BIT4
#define SRB_COMMAND	BIT5
#define SRB_START_	BIT6	/*;arbitration+msg_out+command_out*/
#define SRB_DISCONNECT	BIT7
#define SRB_DATA_XFER	BIT8
#define SRB_XFERPAD	BIT9
#define SRB_STATUS	BIT10
#define SRB_COMPLETED	BIT11
#define SRB_ABORT_SENT	BIT12
#define DO_SYNC_NEGO	BIT13
#define SRB_UNEXPECT_RESEL BIT14

/*;---SRBstatus */
#define SRB_OK		BIT0
#define ABORTION	BIT1
#define OVER_RUN	BIT2
#define UNDER_RUN	BIT3
#define PARITY_ERROR	BIT4
#define SRB_ERROR	BIT5

/*;---ACBFlag */
#define RESET_DEV	BIT0
#define RESET_DETECT	BIT1
#define RESET_DONE	BIT2

/*;---DCBFlag */
#define ABORT_DEV_	BIT0

/*;---SRBFlag */
#define DATAOUT 	BIT7
#define DATAIN		BIT6
#define RESIDUAL_VALID	BIT5
#define ENABLE_TIMER	BIT4
#define RESET_DEV0	BIT2
#define ABORT_DEV	BIT1
#define AUTO_REQSENSE	BIT0

/*;---Adapter status */
#define H_STATUS_GOOD	 0
#define H_SEL_TIMEOUT	 0x11
#define H_OVER_UNDER_RUN 0x12
#define H_UNEXP_BUS_FREE 0x13
#define H_TARGET_PHASE_F 0x14
#define H_INVALID_CCB_OP 0x16
#define H_LINK_CCB_BAD	 0x17
#define H_BAD_TARGET_DIR 0x18
#define H_DUPLICATE_CCB  0x19
#define H_BAD_CCB_OR_SG  0x1A
#define H_ABORT 	 0x0FF

/*; SCSI Status byte codes*/ 
/* The values defined in include/scsi/scsi.h, to be shifted << 1 */

#define SCSI_STAT_UNEXP_BUS_F	0xFD	/*;  Unexpect Bus Free */
#define SCSI_STAT_BUS_RST_DETECT 0xFE	/*;  Scsi Bus Reset detected */
#define SCSI_STAT_SEL_TIMEOUT	0xFF	/*;  Selection Time out */

/* cmd->result */
#define RES_TARGET		0x000000FF	/* Target State */
#define RES_TARGET_LNX		STATUS_MASK	/* Only official ... */
#define RES_ENDMSG		0x0000FF00	/* End Message */
#define RES_DID			0x00FF0000	/* DID_ codes */
#define RES_DRV			0xFF000000	/* DRIVER_ codes */

#define MK_RES(drv,did,msg,tgt) ((int)(drv)<<24 | (int)(did)<<16 | (int)(msg)<<8 | (int)(tgt))
#define MK_RES_LNX(drv,did,msg,tgt) ((int)(drv)<<24 | (int)(did)<<16 | (int)(msg)<<8 | (int)(tgt)<<1)

#define SET_RES_TARGET(who,tgt) { who &= ~RES_TARGET; who |= (int)(tgt); }
#define SET_RES_TARGET_LNX(who,tgt) { who &= ~RES_TARGET_LNX; who |= (int)(tgt) << 1; }
#define SET_RES_MSG(who,msg) { who &= ~RES_ENDMSG; who |= (int)(msg) << 8; }
#define SET_RES_DID(who,did) { who &= ~RES_DID; who |= (int)(did) << 16; }
#define SET_RES_DRV(who,drv) { who &= ~RES_DRV; who |= (int)(drv) << 24; }

/*;---Sync_Mode */
#define SYNC_DISABLE	0
#define SYNC_ENABLE	BIT0
#define SYNC_NEGO_DONE	BIT1
#define WIDE_ENABLE	BIT2	/* Not used ;-) */
#define WIDE_NEGO_DONE	BIT3	/* Not used ;-) */
#define EN_TAG_QUEUEING	BIT4
#define EN_ATN_STOP	BIT5

#define SYNC_NEGO_OFFSET 15

/*;---SCSI bus phase*/
#define SCSI_DATA_OUT	0
#define SCSI_DATA_IN	1
#define SCSI_COMMAND	2
#define SCSI_STATUS_	3
#define SCSI_NOP0	4
#define SCSI_NOP1	5
#define SCSI_MSG_OUT	6
#define SCSI_MSG_IN	7

/*;----SCSI MSG BYTE*/ /* see scsi/scsi.h */ /* One is missing ! */
#define ABORT_TAG	0x0d

/*
**  Inquiry Data format
*/

typedef struct	_SCSIInqData { /* INQUIRY */

	UCHAR	 DevType;		/* Periph Qualifier & Periph Dev Type*/
	UCHAR	 RMB_TypeMod;		/* rem media bit & Dev Type Modifier */
	UCHAR	 Vers;			/* ISO, ECMA, & ANSI versions	     */
	UCHAR	 RDF;			/* AEN, TRMIOP, & response data format*/
	UCHAR	 AddLen;		/* length of additional data	     */
	UCHAR	 Res1;			/* reserved			     */
	UCHAR	 Res2;			/* reserved			     */
	UCHAR	 Flags; 		/* RelADr,Wbus32,Wbus16,Sync,etc.    */
	UCHAR	 VendorID[8];		/* Vendor Identification	     */
	UCHAR	 ProductID[16]; 	/* Product Identification	     */
	UCHAR	 ProductRev[4]; 	/* Product Revision		     */


} SCSI_INQDATA, *PSCSI_INQDATA;


/*  Inquiry byte 0 masks */


#define SCSI_DEVTYPE	    0x1F      /* Peripheral Device Type 	    */
#define SCSI_PERIPHQUAL     0xE0      /* Peripheral Qualifier		    */
#define TYPE_NODEV	    SCSI_DEVTYPE    /* Unknown or no device type    */


/*  Inquiry byte 1 mask */

#define SCSI_REMOVABLE_MEDIA  0x80    /* Removable Media bit (1=removable)  */


/*  Peripheral Device Type definitions */
/*  see include/scsi/scsi.h for the rest */

#ifndef TYPE_PRINTER
# define TYPE_PRINTER		 0x02	   /* Printer device		   */
#endif
#ifndef TYPE_COMM
# define TYPE_COMM		 0x09	   /* Communications device	   */
#endif

/*
** Inquiry flag definitions (Inq data byte 7)
*/

#define SCSI_INQ_RELADR       0x80    /* device supports relative addressing*/
#define SCSI_INQ_WBUS32       0x40    /* device supports 32 bit data xfers  */
#define SCSI_INQ_WBUS16       0x20    /* device supports 16 bit data xfers  */
#define SCSI_INQ_SYNC	      0x10    /* device supports synchronous xfer   */
#define SCSI_INQ_LINKED       0x08    /* device supports linked commands    */
#define SCSI_INQ_CMDQUEUE     0x02    /* device supports command queueing   */
#define SCSI_INQ_SFTRE	      0x01    /* device supports soft resets */


/*
;==========================================================
; EEPROM byte offset
;==========================================================
*/
typedef  struct  _EEprom
{
UCHAR	EE_MODE1;
UCHAR	EE_SPEED;
UCHAR	xx1;
UCHAR	xx2;
} EEprom, *PEEprom;

#define REAL_EE_ADAPT_SCSI_ID 64
#define REAL_EE_MODE2	65
#define REAL_EE_DELAY	66
#define REAL_EE_TAG_CMD_NUM	67

#define EE_ADAPT_SCSI_ID 32
#define EE_MODE2	33
#define EE_DELAY	34
#define EE_TAG_CMD_NUM	35

#define EE_LEN		40

/*; EE_MODE1 bits definition*/
#define PARITY_CHK_	BIT0
#define SYNC_NEGO_	BIT1
#define EN_DISCONNECT_	BIT2
#define SEND_START_	BIT3
#define TAG_QUEUEING_	BIT4

/*; EE_MODE2 bits definition*/
#define MORE2_DRV	BIT0
#define GREATER_1G	BIT1
#define RST_SCSI_BUS	BIT2
#define ACTIVE_NEGATION BIT3
#define NO_SEEK 	BIT4
#define LUN_CHECK	BIT5

#define ENABLE_CE	1
#define DISABLE_CE	0
#define EEPROM_READ	0x80

/*
;==========================================================
;	AMD 53C974 Registers bit Definition
;==========================================================
*/
/*
;====================
; SCSI Register
;====================
*/

/*; Command Reg.(+0CH) (rw) */
#define DMA_COMMAND		BIT7
#define NOP_CMD 		0
#define CLEAR_FIFO_CMD		1
#define RST_DEVICE_CMD		2
#define RST_SCSI_BUS_CMD	3

#define INFO_XFER_CMD		0x10
#define INITIATOR_CMD_CMPLTE	0x11
#define MSG_ACCEPTED_CMD	0x12
#define XFER_PAD_BYTE		0x18
#define SET_ATN_CMD		0x1A
#define RESET_ATN_CMD		0x1B

#define SEL_WO_ATN		0x41	/* currently not used */
#define SEL_W_ATN		0x42
#define SEL_W_ATN_STOP		0x43
#define SEL_W_ATN3		0x46
#define EN_SEL_RESEL		0x44
#define DIS_SEL_RESEL		0x45	/* currently not used */
#define RESEL			0x40	/* " */
#define RESEL_ATN3		0x47	/* " */

#define DATA_XFER_CMD		INFO_XFER_CMD


/*; SCSI Status Reg.(+10H) (r) */
#define INTERRUPT		BIT7
#define ILLEGAL_OP_ERR		BIT6
#define PARITY_ERR		BIT5
#define COUNT_2_ZERO		BIT4
#define GROUP_CODE_VALID	BIT3
#define SCSI_PHASE_MASK 	(BIT2+BIT1+BIT0) 
/* BIT2: MSG phase; BIT1: C/D physe; BIT0: I/O phase */

/*; Interrupt Status Reg.(+14H) (r) */
#define SCSI_RESET		BIT7
#define INVALID_CMD		BIT6
#define DISCONNECTED		BIT5
#define SERVICE_REQUEST 	BIT4
#define SUCCESSFUL_OP		BIT3
#define RESELECTED		BIT2
#define SEL_ATTENTION		BIT1
#define SELECTED		BIT0

/*; Internal State Reg.(+18H) (r) */
#define SYNC_OFFSET_FLAG	BIT3
#define INTRN_STATE_MASK	(BIT2+BIT1+BIT0)
/* 0x04: Sel. successful (w/o stop), 0x01: Sel. successful (w/ stop) */

/*; Clock Factor Reg.(+24H) (w) */
#define CLK_FREQ_40MHZ		0
#define CLK_FREQ_35MHZ		(BIT2+BIT1+BIT0)
#define CLK_FREQ_30MHZ		(BIT2+BIT1)
#define CLK_FREQ_25MHZ		(BIT2+BIT0)
#define CLK_FREQ_20MHZ		BIT2
#define CLK_FREQ_15MHZ		(BIT1+BIT0)
#define CLK_FREQ_10MHZ		BIT1

/*; Control Reg. 1(+20H) (rw) */
#define EXTENDED_TIMING 	BIT7
#define DIS_INT_ON_SCSI_RST	BIT6
#define PARITY_ERR_REPO 	BIT4
#define SCSI_ID_ON_BUS		(BIT2+BIT1+BIT0) /* host adapter ID */

/*; Control Reg. 2(+2CH) (rw) */
#define EN_FEATURE		BIT6
#define EN_SCSI2_CMD		BIT3

/*; Control Reg. 3(+30H) (rw) */
#define ID_MSG_CHECK		BIT7
#define EN_QTAG_MSG		BIT6
#define EN_GRP2_CMD		BIT5
#define FAST_SCSI		BIT4	/* ;10MB/SEC */
#define FAST_CLK		BIT3	/* ;25 - 40 MHZ */

/*; Control Reg. 4(+34H) (rw) */
#define EATER_12NS		0
#define EATER_25NS		BIT7
#define EATER_35NS		BIT6
#define EATER_0NS		(BIT7+BIT6)
#define REDUCED_POWER		BIT5
#define CTRL4_RESERVED		BIT4	/* must be 1 acc. to AM53C974.c */
#define NEGATE_REQACKDATA	BIT2
#define NEGATE_REQACK		BIT3

#define GLITCH_TO_NS(x) (((~x>>6 & 2) >> 1) | ((x>>6 & 1) << 1 ^ (x>>6 & 2)))
#define NS_TO_GLITCH(y) (((~y<<7) | ~((y<<6) ^ ((y<<5 & 1<<6) | ~0x40))) & 0xc0)

/*
;====================
; DMA Register
;====================
*/
/*; DMA Command Reg.(+40H) (rw) */
#define READ_DIRECTION		BIT7
#define WRITE_DIRECTION 	0
#define EN_DMA_INT		BIT6
#define EN_PAGE_INT		BIT5	/* page transfer interrupt enable */
#define MAP_TO_MDL		BIT4
#define DIAGNOSTIC		BIT2
#define DMA_IDLE_CMD		0
#define DMA_BLAST_CMD		BIT0
#define DMA_ABORT_CMD		BIT1
#define DMA_START_CMD		(BIT1+BIT0)

/*; DMA Status Reg.(+54H) (r) */
#define PCI_MS_ABORT		BIT6
#define BLAST_COMPLETE		BIT5
#define SCSI_INTERRUPT		BIT4
#define DMA_XFER_DONE		BIT3
#define DMA_XFER_ABORT		BIT2
#define DMA_XFER_ERROR		BIT1
#define POWER_DOWN		BIT0

/*; DMA SCSI Bus and Ctrl.(+70H) */
#define EN_INT_ON_PCI_ABORT	BIT25
#define WRT_ERASE_DMA_STAT	BIT24
#define PW_DOWN_CTRL		BIT21
#define SCSI_BUSY		BIT20
#define SCLK			BIT19
#define SCAM			BIT18
#define SCSI_LINES		0x0003ffff

/*
;==========================================================
; SCSI Chip register address offset
;==========================================================
;Registers are rw unless declared otherwise 
*/
#define CtcReg_Low	0x00	/* r	curr. transfer count */
#define CtcReg_Mid	0x04	/* r */
#define CtcReg_High	0x38	/* r */
#define ScsiFifo	0x08
#define ScsiCmd 	0x0C
#define Scsi_Status	0x10	/* r */
#define INT_Status	0x14	/* r */
#define Sync_Period	0x18	/* w */
#define Sync_Offset	0x1C	/* w */
#define Clk_Factor	0x24	/* w */
#define CtrlReg1	0x20	
#define CtrlReg2	0x2C
#define CtrlReg3	0x30
#define CtrlReg4	0x34
#define DMA_Cmd 	0x40
#define DMA_XferCnt	0x44	/* rw	starting transfer count (32 bit) */
#define DMA_XferAddr	0x48	/* rw	starting physical address (32 bit) */
#define DMA_Wk_ByteCntr 0x4C	/* r	working byte counter */
#define DMA_Wk_AddrCntr 0x50	/* r	working address counter */
#define DMA_Status	0x54	/* r */
#define DMA_MDL_Addr	0x58	/* rw	starting MDL address */
#define DMA_Wk_MDL_Cntr 0x5C	/* r	working MDL counter */
#define DMA_ScsiBusCtrl 0x70	/* rw	SCSI Bus, PCI/DMA Ctrl */

#define StcReg_Low	CtcReg_Low	/* w	start transfer count */
#define StcReg_Mid	CtcReg_Mid	/* w */
#define StcReg_High	CtcReg_High	/* w */
#define Scsi_Dest_ID	Scsi_Status	/* w */
#define Scsi_TimeOut	INT_Status	/* w */
#define Intern_State	Sync_Period	/* r */
#define Current_Fifo	Sync_Offset	/* r	Curr. FIFO / int. state */


#define DC390_read8(address)			\
	(inb (pACB->IOPortBase + (address)))

#define DC390_read8_(address, base)		\
	(inb ((USHORT)(base) + (address)))

#define DC390_read16(address)			\
	(inw (pACB->IOPortBase + (address)))

#define DC390_read32(address)			\
	(inl (pACB->IOPortBase + (address)))

#define DC390_write8(address,value)		\
	outb ((value), pACB->IOPortBase + (address))

#define DC390_write8_(address,value,base)	\
	outb ((value), (USHORT)(base) + (address))

#define DC390_write16(address,value)		\
	outw ((value), pACB->IOPortBase + (address))

#define DC390_write32(address,value)		\
	outl ((value), pACB->IOPortBase + (address))


#endif /* _TMSCSIM_H */
