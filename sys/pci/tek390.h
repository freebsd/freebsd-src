/***********************************************************************
;*	File Name : TEK390.H					       *
;*		    TEKRAM DC-390 PCI SCSI Bus Master Host Adapter     *
;*		    Device Driver				       *
;***********************************************************************/

#ifndef TEK390_H
#define TEK390_H

typedef unsigned char	UCHAR;
typedef unsigned short	USHORT;
typedef unsigned long	ULONG;
typedef unsigned int	UINT;

typedef UCHAR		*PUCHAR;
typedef USHORT		*PUSHORT;
typedef ULONG		*PULONG;
typedef struct scsi_link    *PSCLINK, SCSILINK;
typedef struct scsi_xfer    *PSCSICMD, SCSICMD;
typedef void		*PVOID;


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
ULONG		SGXLen;
ULONG		SGXPtr;
} SGentry, *PSEG;

typedef  struct  _SGentry1
{
ULONG		SGXPtr1;
ULONG		SGXLen1;
} SGentry1, *PSEG1;


#define MAX_ADAPTER_NUM 	4
#define MAX_SCSI_ID		8
#define MAX_SG_ENTRY		33
#define MAX_DEVICES		10
#define MAX_CMD_QUEUE		20
#define MAX_CMD_PER_LUN 	6
#define MAX_SRB_CNT		MAX_CMD_PER_LUN*4
#define PAGELEN 		4096

/*
;-----------------------------------------------------------------------
; SCSI Request Block
;-----------------------------------------------------------------------
*/
struct	_SRB
{
UCHAR		CmdBlock[12];

struct _SRB	*pNextSRB;
struct _DCB	*pSRBDCB;
PSCSICMD	pcmd;
PSEG		pSegmentList;

ULONG		PhysSRB;
ULONG		TotalXferredLen;
ULONG		SGPhysAddr;	/*;a segment starting address */
ULONG		SGToBeXferLen;	/*; to be xfer length */
ULONG		Segment0[2];
ULONG		Segment1[2];

SGentry 	SGsegment[MAX_SG_ENTRY];
SGentry 	Segmentx;	/* make a one entry of S/G list table */

PUCHAR		pMsgPtr;
USHORT		SRBState;
USHORT		Revxx2; 	/* ??? */

UCHAR		MsgInBuf[6];
UCHAR		MsgOutBuf[6];

UCHAR		AdaptStatus;
UCHAR		TargetStatus;
UCHAR		MsgCnt;
UCHAR		EndMessage;
UCHAR		TagNumber;
UCHAR		SGcount;
UCHAR		SGIndex;
UCHAR		IORBFlag;	/*;81h-Reset, 2-retry */

UCHAR		SRBStatus;
UCHAR		RetryCnt;
UCHAR		SRBFlag;	/*; b0-AutoReqSense,b6-Read,b7-write */
				/*; b4-settimeout,b5-Residual valid */
UCHAR		ScsiCmdLen;
UCHAR		ScsiPhase;
UCHAR		Reserved3[3];	/*;for dword alignment */
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

PSRB		pWaitingSRB;
PSRB		pWaitLast;
PSRB		pGoingSRB;
PSRB		pGoingLast;
PSRB		pActiveSRB;
USHORT		GoingSRBCnt;
USHORT		WaitSRBCnt;	/* ??? */

ULONG		TagMask;

USHORT		MaxCommand;
USHORT		AdaptIndex;	/*; UnitInfo struc start */
USHORT		UnitIndex;	/*; nth Unit on this card */
UCHAR		UnitSCSIID;	/*; SCSI Target ID  (SCSI Only) */
UCHAR		UnitSCSILUN;	/*; SCSI Log.  Unit (SCSI Only) */

UCHAR		IdentifyMsg;
UCHAR		CtrlR1;
UCHAR		CtrlR3;
UCHAR		CtrlR4;

UCHAR		InqDataBuf[8];
UCHAR		CapacityBuf[8];
UCHAR		DevMode;
UCHAR		AdpMode;
UCHAR		SyncMode;	/*; 0:async mode */
UCHAR		NegoPeriod;	/*;for nego. */
UCHAR		SyncPeriod;	/*;for reg. */
UCHAR		SyncOffset;	/*;for reg. and nego.(low nibble) */
UCHAR		UnitCtrlFlag;
UCHAR		DCBFlag;
UCHAR		DevType;
UCHAR		Reserved2[3];	/*;for dword alignment */
};

typedef  struct  _DCB	 DC390_DCB, *PDCB;
/*
;-----------------------------------------------------------------------
; Adapter Control Block
;-----------------------------------------------------------------------
*/
struct	_ACB
{
ULONG		PhysACB;
struct _ACB	*pNextACB;
USHORT		IOPortBase;
USHORT		Revxx1; 	/* ??? */

PDCB		pLinkDCB;
PDCB		pDCBRunRobin;
PDCB		pActiveDCB;
PDCB		pDCB_free;
PSRB		pFreeSRB;
PSRB		pTmpSRB;
USHORT		SRBCount;
USHORT		AdapterIndex;	/*; nth Adapter this driver */
USHORT		max_id;
USHORT		max_lun;
SCSILINK	ScsiLink;

UCHAR		msgin123[4];
UCHAR		status;
UCHAR		AdaptSCSIID;	/*; Adapter SCSI Target ID */
UCHAR		AdaptSCSILUN;	/*; Adapter SCSI LUN */
UCHAR		DeviceCnt;
UCHAR		IRQLevel;
UCHAR		TagMaxNum;
UCHAR		ACBFlag;
UCHAR		Gmode2;
UCHAR		LUNchk;
UCHAR		scan_devices;
UCHAR		HostID_Bit;
UCHAR		Reserved1[1];	/*;for dword alignment */
UCHAR		DCBmap[MAX_SCSI_ID];
DC390_DCB	DCB_array[MAX_DEVICES]; 	/* +74h,  Len=3E8 */
DC390_SRB	SRB_array[MAX_SRB_CNT]; 	/* +45Ch, Len=	*/
DC390_SRB	TmpSRB;
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

/*;---ACBFlag */
#define RESET_DEV	BIT0
#define RESET_DETECT	BIT1
#define RESET_DONE	BIT2

/*;---DCBFlag */
#define ABORT_DEV_	BIT0

/*;---SRBstatus */
#define SRB_OK		BIT0
#define ABORTION	BIT1
#define OVER_RUN	BIT2
#define UNDER_RUN	BIT3
#define PARITY_ERROR	BIT4
#define SRB_ERROR	BIT5

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
#define SCSI_STAT_GOOD		0x0	/*;  Good status */
#define SCSI_STAT_CHECKCOND	0x02	/*;  SCSI Check Condition */
#define SCSI_STAT_CONDMET	0x04	/*;  Condition Met */
#define SCSI_STAT_BUSY		0x08	/*;  Target busy status */
#define SCSI_STAT_INTER 	0x10	/*;  Intermediate status */
#define SCSI_STAT_INTERCONDMET	0x14	/*;  Intermediate condition met */
#define SCSI_STAT_RESCONFLICT	0x18	/*;  Reservation conflict */
#define SCSI_STAT_CMDTERM	0x22	/*;  Command Terminated */
#define SCSI_STAT_QUEUEFULL	0x28	/*;  Queue Full */

#define SCSI_STAT_UNEXP_BUS_F	0xFD	/*;  Unexpect Bus Free */
#define SCSI_STAT_BUS_RST_DETECT 0xFE	/*;  Scsi Bus Reset detected */
#define SCSI_STAT_SEL_TIMEOUT	0xFF	/*;  Selection Time out */

/*;---Sync_Mode */
#define SYNC_DISABLE	0
#define SYNC_ENABLE	BIT0
#define SYNC_NEGO_DONE	BIT1
#define WIDE_ENABLE	BIT2
#define WIDE_NEGO_DONE	BIT3
#define EN_TAG_QUEUING	BIT4
#define EN_ATN_STOP	BIT5

#define SYNC_NEGO_OFFSET 15

/*;---SCSI bus phase*/
#define SCSI_DATA_OUT_	0
#define SCSI_DATA_IN_	1
#define SCSI_COMMAND	2
#define SCSI_STATUS_	3
#define SCSI_NOP0	4
#define SCSI_NOP1	5
#define SCSI_MSG_OUT	6
#define SCSI_MSG_IN	7

/*;----SCSI MSG BYTE*/
#define MSG_COMPLETE		0x00
#define MSG_EXTENDED		0x01
#define MSG_SAVE_PTR		0x02
#define MSG_RESTORE_PTR 	0x03
#define MSG_DISCONNECT		0x04
#define MSG_INITIATOR_ERROR	0x05
#define MSG_ABORT		0x06
#define MSG_REJECT_		0x07
#define MSG_NOP 		0x08
#define MSG_PARITY_ERROR	0x09
#define MSG_LINK_CMD_COMPL	0x0A
#define MSG_LINK_CMD_COMPL_FLG	0x0B
#define MSG_BUS_RESET		0x0C
#define MSG_ABORT_TAG		0x0D
#define MSG_SIMPLE_QTAG 	0x20
#define MSG_HEAD_QTAG		0x21
#define MSG_ORDER_QTAG		0x22
#define MSG_IDENTIFY		0x80
#define MSG_HOST_ID		0x0C0

/*;----SCSI STATUS BYTE*/
#define STATUS_GOOD		0x00
#define CHECK_CONDITION_	0x02
#define STATUS_BUSY		0x08
#define STATUS_INTERMEDIATE	0x10
#define RESERVE_CONFLICT	0x18

/* cmd->result */
#define STATUS_MASK_		0xFF
#define MSG_MASK		0xFF00
#define RETURN_MASK		0xFF0000

/*
**  Inquiry Data format
*/

typedef struct	_SCSIInqData { /* INQ */

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


/*  Inquiry byte 1 mask */

#define SCSI_REMOVABLE_MEDIA  0x80    /* Removable Media bit (1=removable)  */


/*  Peripheral Device Type definitions */

#define SCSI_DASD		 0x00	   /* Direct-access Device	   */
#define SCSI_SEQACESS		 0x01	   /* Sequential-access device	   */
#define SCSI_PRINTER		 0x02	   /* Printer device		   */
#define SCSI_PROCESSOR		 0x03	   /* Processor device		   */
#define SCSI_WRITEONCE		 0x04	   /* Write-once device 	   */
#define SCSI_CDROM		 0x05	   /* CD-ROM device		   */
#define SCSI_SCANNER		 0x06	   /* Scanner device		   */
#define SCSI_OPTICAL		 0x07	   /* Optical memory device	   */
#define SCSI_MEDCHGR		 0x08	   /* Medium changer device	   */
#define SCSI_COMM		 0x09	   /* Communications device	   */
#define SCSI_NODEV		 0x1F	   /* Unknown or no device type    */

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

#define EE_ADAPT_SCSI_ID 64
#define EE_MODE2	65
#define EE_DELAY	66
#define EE_TAG_CMD_NUM	67

/*; EE_MODE1 bits definition*/
#define PARITY_CHK_	BIT0
#define SYNC_NEGO_	BIT1
#define EN_DISCONNECT_	BIT2
#define SEND_START_	BIT3
#define TAG_QUEUING_	BIT4

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

/*; Command Reg.(+0CH) */
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
#define SELECT_W_ATN		0x42
#define SEL_W_ATN_STOP		0x43
#define EN_SEL_RESEL		0x44
#define SEL_W_ATN2		0x46
#define DATA_XFER_CMD		INFO_XFER_CMD


/*; SCSI Status Reg.(+10H) */
#define INTERRUPT		BIT7
#define ILLEGAL_OP_ERR		BIT6
#define PARITY_ERR		BIT5
#define COUNT_2_ZERO		BIT4
#define GROUP_CODE_VALID	BIT3
#define SCSI_PHASE_MASK 	(BIT2+BIT1+BIT0)

/*; Interrupt Status Reg.(+14H) */
#define SCSI_RESET_		BIT7
#define INVALID_CMD		BIT6
#define DISCONNECTED		BIT5
#define SERVICE_REQUEST 	BIT4
#define SUCCESSFUL_OP		BIT3
#define RESELECTED		BIT2
#define SEL_ATTENTION		BIT1
#define SELECTED		BIT0

/*; Internal State Reg.(+18H) */
#define SYNC_OFFSET_FLAG	BIT3
#define INTRN_STATE_MASK	(BIT2+BIT1+BIT0)

/*; Clock Factor Reg.(+24H) */
#define CLK_FREQ_40MHZ		0
#define CLK_FREQ_35MHZ		(BIT2+BIT1+BIT0)
#define CLK_FREQ_30MHZ		(BIT2+BIT1)
#define CLK_FREQ_25MHZ		(BIT2+BIT0)
#define CLK_FREQ_20MHZ		BIT2
#define CLK_FREQ_15MHZ		(BIT1+BIT0)
#define CLK_FREQ_10MHZ		BIT1

/*; Control Reg. 1(+20H) */
#define EXTENDED_TIMING 	BIT7
#define DIS_INT_ON_SCSI_RST	BIT6
#define PARITY_ERR_REPO 	BIT4
#define SCSI_ID_ON_BUS		(BIT2+BIT1+BIT0)

/*; Control Reg. 2(+2CH) */
#define EN_FEATURE		BIT6
#define EN_SCSI2_CMD		BIT3

/*; Control Reg. 3(+30H) */
#define ID_MSG_CHECK		BIT7
#define EN_QTAG_MSG		BIT6
#define EN_GRP2_CMD		BIT5
#define FAST_SCSI		BIT4	/* ;10MB/SEC */
#define FAST_CLK		BIT3	/* ;25 - 40 MHZ */

/*; Control Reg. 4(+34H) */
#define EATER_12NS		0
#define EATER_25NS		BIT7
#define EATER_35NS		BIT6
#define EATER_0NS		(BIT7+BIT6)
#define NEGATE_REQACKDATA	BIT2
#define NEGATE_REQACK		BIT3
/*
;====================
; DMA Register
;====================
*/
/*; DMA Command Reg.(+40H) */
#define READ_DIRECTION		BIT7
#define WRITE_DIRECTION 	0
#define EN_DMA_INT		BIT6
#define MAP_TO_MDL		BIT5
#define DMA_DIAGNOSTIC		BIT4
#define DMA_IDLE_CMD		0
#define DMA_BLAST_CMD		BIT0
#define DMA_ABORT_CMD		BIT1
#define DMA_START_CMD		(BIT1+BIT0)

/*; DMA Status Reg.(+54H) */
#define PCI_MS_ABORT		BIT6
#define BLAST_COMPLETE		BIT5
#define SCSI_INTERRUPT		BIT4
#define DMA_XFER_DONE		BIT3
#define DMA_XFER_ABORT		BIT2
#define DMA_XFER_ERROR		BIT1
#define POWER_DOWN		BIT0

/*
; DMA SCSI Bus and Ctrl.(+70H)
;EN_INT_ON_PCI_ABORT
*/

/*
;==========================================================
; SCSI Chip register address offset
;==========================================================
*/
#define CtcReg_Low	0x00
#define CtcReg_Mid	0x04
#define ScsiFifo	0x08
#define ScsiCmd 	0x0C
#define Scsi_Status	0x10
#define INT_Status	0x14
#define Sync_Period	0x18
#define Sync_Offset	0x1C
#define CtrlReg1	0x20
#define Clk_Factor	0x24
#define CtrlReg2	0x2C
#define CtrlReg3	0x30
#define CtrlReg4	0x34
#define CtcReg_High	0x38
#define DMA_Cmd 	0x40
#define DMA_XferCnt	0x44
#define DMA_XferAddr	0x48
#define DMA_Wk_ByteCntr 0x4C
#define DMA_Wk_AddrCntr 0x50
#define DMA_Status	0x54
#define DMA_MDL_Addr	0x58
#define DMA_Wk_MDL_Cntr 0x5C
#define DMA_ScsiBusCtrl 0x70

#define StcReg_Low	CtcReg_Low
#define StcReg_Mid	CtcReg_Mid
#define Scsi_Dest_ID	Scsi_Status
#define Scsi_TimeOut	INT_Status
#define Intern_State	Sync_Period
#define Current_Fifo	Sync_Offset
#define StcReg_High	CtcReg_High

#define am_target	Scsi_Status
#define am_timeout	INT_Status
#define am_seq_step	Sync_Period
#define am_fifo_count	Sync_Offset


#define DC390_read8(address)			       \
	inb(DC390_ioport + (address)))

#define DC390_read16(address)			       \
	inw(DC390_ioport + (address)))

#define DC390_read32(address)			       \
	inl(DC390_ioport + (address)))

#define DC390_write8(address,value)		       \
	outb((value), DC390_ioport + (address)))

#define DC390_write16(address,value)		       \
	outw((value), DC390_ioport + (address)))

#define DC390_write32(address,value)		       \
	outl((value), DC390_ioport + (address)))


/* Configuration method #1 */
#define PCI_CFG1_ADDRESS_REG		0xcf8
#define PCI_CFG1_DATA_REG		0xcfc
#define PCI_CFG1_ENABLE 		0x80000000
#define PCI_CFG1_TUPPLE(bus, device, function, register)		\
	(PCI_CFG1_ENABLE | (((bus) << 16) & 0xff0000) | 		\
	(((device) << 11) & 0xf800) | (((function) << 8) & 0x700)|	\
	(((register) << 2) & 0xfc))

/* Configuration method #2 */
#define PCI_CFG2_ENABLE_REG		0xcf8
#define PCI_CFG2_FORWARD_REG		0xcfa
#define PCI_CFG2_ENABLE 		0x0f0
#define PCI_CFG2_TUPPLE(function)					\
	(PCI_CFG2_ENABLE | (((function) << 1) & 0xe))


#endif /* TEK390_H */
