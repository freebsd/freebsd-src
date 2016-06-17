/*
** *************************************************************************
**
**
**     R C L A N M T L . C             $Revision: 6 $
**
**
**  RedCreek I2O LAN Message Transport Layer program module.
**
**  ---------------------------------------------------------------------
**  ---     Copyright (c) 1997-1999, RedCreek Communications Inc.     ---
**  ---                   All rights reserved.                        ---
**  ---------------------------------------------------------------------
**
**  File Description:
**
**  Host side I2O (Intelligent I/O) LAN message transport layer.
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.

**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.

**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** 1998-1999, LAN API was modified and enhanced by Alice Hennessy.
**
** Sometime in 1997, LAN API was written from scratch by Wendell Nichols.
** *************************************************************************
*/

#define DEBUG 1

#define RC_LINUX_MODULE
#include "rclanmtl.h"

 /* RedCreek LAN device Target ID */
#define RC_LAN_TARGET_ID  0x10
 /* RedCreek's OSM default LAN receive Initiator */
#define DEFAULT_RECV_INIT_CONTEXT  0xA17

/*
** I2O message structures
*/

#define    I2O_TID_SZ                                  12
#define    I2O_FUNCTION_SZ                             8

/* Transaction Reply Lists (TRL) Control Word structure */

#define    I2O_TRL_FLAGS_SINGLE_FIXED_LENGTH           0x00
#define    I2O_TRL_FLAGS_SINGLE_VARIABLE_LENGTH        0x40
#define    I2O_TRL_FLAGS_MULTIPLE_FIXED_LENGTH         0x80

/* LAN Class specific functions */

#define    I2O_LAN_PACKET_SEND                         0x3B
#define    I2O_LAN_SDU_SEND                            0x3D
#define    I2O_LAN_RECEIVE_POST                        0x3E
#define    I2O_LAN_RESET                               0x35
#define    I2O_LAN_SHUTDOWN                            0x37

/* Private Class specfic function */
#define    I2O_PRIVATE                                 0xFF

/*  I2O Executive Function Codes.  */

#define    I2O_EXEC_ADAPTER_ASSIGN                     0xB3
#define    I2O_EXEC_ADAPTER_READ                       0xB2
#define    I2O_EXEC_ADAPTER_RELEASE                    0xB5
#define    I2O_EXEC_BIOS_INFO_SET                      0xA5
#define    I2O_EXEC_BOOT_DEVICE_SET                    0xA7
#define    I2O_EXEC_CONFIG_VALIDATE                    0xBB
#define    I2O_EXEC_CONN_SETUP                         0xCA
#define    I2O_EXEC_DEVICE_ASSIGN                      0xB7
#define    I2O_EXEC_DEVICE_RELEASE                     0xB9
#define    I2O_EXEC_HRT_GET                            0xA8
#define    I2O_EXEC_IOP_CLEAR                          0xBE
#define    I2O_EXEC_IOP_CONNECT                        0xC9
#define    I2O_EXEC_IOP_RESET                          0xBD
#define    I2O_EXEC_LCT_NOTIFY                         0xA2
#define    I2O_EXEC_OUTBOUND_INIT                      0xA1
#define    I2O_EXEC_PATH_ENABLE                        0xD3
#define    I2O_EXEC_PATH_QUIESCE                       0xC5
#define    I2O_EXEC_PATH_RESET                         0xD7
#define    I2O_EXEC_STATIC_MF_CREATE                   0xDD
#define    I2O_EXEC_STATIC_MF_RELEASE                  0xDF
#define    I2O_EXEC_STATUS_GET                         0xA0
#define    I2O_EXEC_SW_DOWNLOAD                        0xA9
#define    I2O_EXEC_SW_UPLOAD                          0xAB
#define    I2O_EXEC_SW_REMOVE                          0xAD
#define    I2O_EXEC_SYS_ENABLE                         0xD1
#define    I2O_EXEC_SYS_MODIFY                         0xC1
#define    I2O_EXEC_SYS_QUIESCE                        0xC3
#define    I2O_EXEC_SYS_TAB_SET                        0xA3

 /* Init Outbound Q status */
#define    I2O_EXEC_OUTBOUND_INIT_IN_PROGRESS          0x01
#define    I2O_EXEC_OUTBOUND_INIT_REJECTED             0x02
#define    I2O_EXEC_OUTBOUND_INIT_FAILED               0x03
#define    I2O_EXEC_OUTBOUND_INIT_COMPLETE             0x04

#define    I2O_UTIL_NOP                                0x00

/* I2O Get Status State values */

#define    I2O_IOP_STATE_INITIALIZING                  0x01
#define    I2O_IOP_STATE_RESET                         0x02
#define    I2O_IOP_STATE_HOLD                          0x04
#define    I2O_IOP_STATE_READY                         0x05
#define    I2O_IOP_STATE_OPERATIONAL                   0x08
#define    I2O_IOP_STATE_FAILED                        0x10
#define    I2O_IOP_STATE_FAULTED                       0x11

/* Defines for Request Status Codes:  Table 3-1 Reply Status Codes.  */

#define    I2O_REPLY_STATUS_SUCCESS                    0x00
#define    I2O_REPLY_STATUS_ABORT_DIRTY                0x01
#define    I2O_REPLY_STATUS_ABORT_NO_DATA_TRANSFER     0x02
#define    I2O_REPLY_STATUS_ABORT_PARTIAL_TRANSFER     0x03
#define    I2O_REPLY_STATUS_ERROR_DIRTY                0x04
#define    I2O_REPLY_STATUS_ERROR_NO_DATA_TRANSFER     0x05
#define    I2O_REPLY_STATUS_ERROR_PARTIAL_TRANSFER     0x06
#define    I2O_REPLY_STATUS_PROCESS_ABORT_DIRTY        0x07
#define    I2O_REPLY_STATUS_PROCESS_ABORT_NO_DATA_TRANSFER   0x08
#define    I2O_REPLY_STATUS_PROCESS_ABORT_PARTIAL_TRANSFER   0x09
#define    I2O_REPLY_STATUS_TRANSACTION_ERROR          0x0A
#define    I2O_REPLY_STATUS_PROGRESS_REPORT            0x80

/* DetailedStatusCode defines for ALL messages: Table 3-2 Detailed Status Codes.*/

#define    I2O_DETAIL_STATUS_SUCCESS                        0x0000
#define    I2O_DETAIL_STATUS_BAD_KEY                        0x0001
#define    I2O_DETAIL_STATUS_CHAIN_BUFFER_TOO_LARGE         0x0002
#define    I2O_DETAIL_STATUS_DEVICE_BUSY                    0x0003
#define    I2O_DETAIL_STATUS_DEVICE_LOCKED                  0x0004
#define    I2O_DETAIL_STATUS_DEVICE_NOT_AVAILABLE           0x0005
#define    I2O_DETAIL_STATUS_DEVICE_RESET                   0x0006
#define    I2O_DETAIL_STATUS_INAPPROPRIATE_FUNCTION         0x0007
#define    I2O_DETAIL_STATUS_INSUFFICIENT_RESOURCE_HARD     0x0008
#define    I2O_DETAIL_STATUS_INSUFFICIENT_RESOURCE_SOFT     0x0009
#define    I2O_DETAIL_STATUS_INVALID_INITIATOR_ADDRESS      0x000A
#define    I2O_DETAIL_STATUS_INVALID_MESSAGE_FLAGS          0x000B
#define    I2O_DETAIL_STATUS_INVALID_OFFSET                 0x000C
#define    I2O_DETAIL_STATUS_INVALID_PARAMETER              0x000D
#define    I2O_DETAIL_STATUS_INVALID_REQUEST                0x000E
#define    I2O_DETAIL_STATUS_INVALID_TARGET_ADDRESS         0x000F
#define    I2O_DETAIL_STATUS_MESSAGE_TOO_LARGE              0x0010
#define    I2O_DETAIL_STATUS_MESSAGE_TOO_SMALL              0x0011
#define    I2O_DETAIL_STATUS_MISSING_PARAMETER              0x0012
#define    I2O_DETAIL_STATUS_NO_SUCH_PAGE                   0x0013
#define    I2O_DETAIL_STATUS_REPLY_BUFFER_FULL              0x0014
#define    I2O_DETAIL_STATUS_TCL_ERROR                      0x0015
#define    I2O_DETAIL_STATUS_TIMEOUT                        0x0016
#define    I2O_DETAIL_STATUS_UNKNOWN_ERROR                  0x0017
#define    I2O_DETAIL_STATUS_UNKNOWN_FUNCTION               0x0018
#define    I2O_DETAIL_STATUS_UNSUPPORTED_FUNCTION           0x0019
#define    I2O_DETAIL_STATUS_UNSUPPORTED_VERSION            0x001A

 /* I2O msg header defines for VersionOffset */
#define I2OMSGVER_1_5   0x0001
#define SGL_OFFSET_0    I2OMSGVER_1_5
#define SGL_OFFSET_4    (0x0040 | I2OMSGVER_1_5)
#define TRL_OFFSET_5    (0x0050 | I2OMSGVER_1_5)
#define TRL_OFFSET_6    (0x0060 | I2OMSGVER_1_5)

 /* I2O msg header defines for MsgFlags */
#define MSG_STATIC      0x0100
#define MSG_64BIT_CNTXT 0x0200
#define MSG_MULTI_TRANS 0x1000
#define MSG_FAIL        0x2000
#define MSG_LAST        0x4000
#define MSG_REPLY       0x8000

  /* normal LAN request message MsgFlags and VersionOffset (0x1041) */
#define LAN_MSG_REQST  (MSG_MULTI_TRANS | SGL_OFFSET_4)

 /* minimum size msg */
#define THREE_WORD_MSG_SIZE 0x00030000
#define FOUR_WORD_MSG_SIZE  0x00040000
#define FIVE_WORD_MSG_SIZE  0x00050000
#define SIX_WORD_MSG_SIZE   0x00060000
#define SEVEN_WORD_MSG_SIZE 0x00070000
#define EIGHT_WORD_MSG_SIZE 0x00080000
#define NINE_WORD_MSG_SIZE  0x00090000

/* Special TID Assignments */

#define I2O_IOP_TID   0
#define I2O_HOST_TID  0xB91

 /* RedCreek I2O private message codes */
#define RC_PRIVATE_GET_MAC_ADDR     0x0001/**/	/* OBSOLETE */
#define RC_PRIVATE_SET_MAC_ADDR     0x0002
#define RC_PRIVATE_GET_NIC_STATS    0x0003
#define RC_PRIVATE_GET_LINK_STATUS  0x0004
#define RC_PRIVATE_SET_LINK_SPEED   0x0005
#define RC_PRIVATE_SET_IP_AND_MASK  0x0006
/* #define RC_PRIVATE_GET_IP_AND_MASK  0x0007 *//* OBSOLETE */
#define RC_PRIVATE_GET_LINK_SPEED   0x0008
#define RC_PRIVATE_GET_FIRMWARE_REV 0x0009
/* #define RC_PRIVATE_GET_MAC_ADDR     0x000A */
#define RC_PRIVATE_GET_IP_AND_MASK  0x000B
#define RC_PRIVATE_DEBUG_MSG        0x000C
#define RC_PRIVATE_REPORT_DRIVER_CAPABILITY  0x000D
#define RC_PRIVATE_SET_PROMISCUOUS_MODE  0x000e
#define RC_PRIVATE_GET_PROMISCUOUS_MODE  0x000f
#define RC_PRIVATE_SET_BROADCAST_MODE    0x0010
#define RC_PRIVATE_GET_BROADCAST_MODE    0x0011

#define RC_PRIVATE_REBOOT           0x00FF

/* I2O message header */
typedef struct _I2O_MESSAGE_FRAME {
	U8 VersionOffset;
	U8 MsgFlags;
	U16 MessageSize;
	BF TargetAddress:I2O_TID_SZ;
	BF InitiatorAddress:I2O_TID_SZ;
	BF Function:I2O_FUNCTION_SZ;
	U32 InitiatorContext;
	/* SGL[] */
} I2O_MESSAGE_FRAME, *PI2O_MESSAGE_FRAME;

 /* assumed a 16K minus 256 byte space for outbound queue message frames */
#define MSG_FRAME_SIZE  512
#define NMBR_MSG_FRAMES 30

 /* 
    ** in reserved space right after PAB in host memory is area for returning
    ** values from card 
  */

/*
** typedef NICSTAT
**
** Data structure for NIC statistics retruned from PCI card.  Data copied from
** here to user allocated RCLINKSTATS (see rclanmtl.h) structure.
*/
typedef struct tag_NicStat {
	unsigned long TX_good;
	unsigned long TX_maxcol;
	unsigned long TX_latecol;
	unsigned long TX_urun;
	unsigned long TX_crs;	/* lost carrier sense */
	unsigned long TX_def;	/* transmit deferred */
	unsigned long TX_singlecol;	/* single collisions */
	unsigned long TX_multcol;
	unsigned long TX_totcol;
	unsigned long Rcv_good;
	unsigned long Rcv_CRCerr;
	unsigned long Rcv_alignerr;
	unsigned long Rcv_reserr;	/* rnr'd pkts */
	unsigned long Rcv_orun;
	unsigned long Rcv_cdt;
	unsigned long Rcv_runt;
	unsigned long dump_status;	/* last field directly from the chip */
} NICSTAT, *P_NICSTAT;

#define DUMP_DONE   0x0000A005	/* completed statistical dump */
#define DUMP_CLEAR  0x0000A007	/* completed stat dump and clear counters */

static volatile int msgFlag;

/* local function prototypes */
static void ProcessOutboundI2OMsg (PPAB pPab, U32 phyMsgAddr);
static int FillI2OMsgSGLFromTCB (PU32 pMsg, PRCTCB pXmitCntrlBlock);
static int GetI2OStatus (PPAB pPab);
static int SendI2OOutboundQInitMsg (PPAB pPab);
static int SendEnableSysMsg (PPAB pPab);

/*
** =========================================================================
** RCInitI2OMsgLayer()
**
** Initialize the RedCreek I2O Module and adapter.
**
** Inputs:  dev - the devices net_device struct
**          TransmitCallbackFunction - address of transmit callback function
**          ReceiveCallbackFunction  - address of receive  callback function
**
** private message block is allocated by user.  It must be in locked pages.
** p_msgbuf and p_phymsgbuf point to the same location.  Must be contigous
** memory block of a minimum of 16K byte and long word aligned.
** =========================================================================
*/
RC_RETURN
RCInitI2OMsgLayer (struct net_device *dev,
		   PFNTXCALLBACK TransmitCallbackFunction,
		   PFNRXCALLBACK ReceiveCallbackFunction,
		   PFNCALLBACK RebootCallbackFunction)
{
	int result;
	PPAB pPab;
	PDPA pDpa = dev->priv;
	U32 pciBaseAddr = dev->base_addr;
	PU8 p_msgbuf = pDpa->PLanApiPA;
	PU8 p_phymsgbuf = (PU8) virt_to_bus ((void *) p_msgbuf);

	dprintk
	    ("InitI2O: Adapter:0x%x ATU:0x%x msgbuf:0x%x phymsgbuf:0x%x\n"
	     "TransmitCallbackFunction:0x%x  ReceiveCallbackFunction:0x%x\n",
	     pDpa->id, pciBaseAddr, (u32) p_msgbuf, (u32) p_phymsgbuf,
	     (u32) TransmitCallbackFunction, (u32) ReceiveCallbackFunction);

	/* Check if this interface already initialized - if so, shut it down */
	if (pDpa->pPab != NULL) {
		dprintk (KERN_WARNING
			"pDpa->pPab [%d] != NULL\n",
			pDpa->id);
/*          RCResetLANCard(pDpa->id, 0, (PU32)NULL, (PFNCALLBACK)NULL); */
		pDpa->pPab = NULL;
	}

	/* store adapter instance values in adapter block.
	 * Adapter block is at beginning of message buffer */

	pPab = kmalloc (sizeof (*pPab), GFP_KERNEL);
	if (!pPab) {
		dprintk (KERN_ERR 
				"RCInitI2OMsgLayer: Could not allocate memory for PAB struct!\n");
		result = RC_RTN_MALLOC_ERROR;
		goto err_out;
	}

	memset (pPab, 0, sizeof (*pPab));
	pDpa->pPab = pPab;
	pPab->p_atu = (PATU) pciBaseAddr;
	pPab->pPci45LinBaseAddr = (PU8) pciBaseAddr;

	/* Set outbound message frame addr */
	pPab->outMsgBlockPhyAddr = (U32) p_phymsgbuf;
	pPab->pLinOutMsgBlock = (PU8) p_msgbuf;

	/* store callback function addresses */
	pPab->pTransCallbackFunc = TransmitCallbackFunction;
	pPab->pRecvCallbackFunc = ReceiveCallbackFunction;
	pPab->pRebootCallbackFunc = RebootCallbackFunction;
	pPab->pCallbackFunc = (PFNCALLBACK) NULL;

	/*
	   ** Initialize I2O IOP
	 */
	result = GetI2OStatus (pPab);

	if (result != RC_RTN_NO_ERROR)
		goto err_out_dealloc;

	if (pPab->IOPState == I2O_IOP_STATE_OPERATIONAL) {
		dprintk (KERN_INFO "pPab->IOPState == op: resetting adapter\n");
		RCResetLANCard (dev, 0, (PU32) NULL, (PFNCALLBACK) NULL);
	}

	result = SendI2OOutboundQInitMsg (pPab);

	if (result != RC_RTN_NO_ERROR)
		goto err_out_dealloc;

	result = SendEnableSysMsg (pPab);

	if (result != RC_RTN_NO_ERROR)
		goto err_out_dealloc;

	return RC_RTN_NO_ERROR;

      err_out_dealloc:
	kfree (pPab);
      err_out:
	return result;
}

/*
** =========================================================================
** Disable and Enable I2O interrupts.  I2O interrupts are enabled at Init time
** but can be disabled and re-enabled through these two function calls.
** Packets will still be put into any posted received buffers and packets will
** be sent through RCI2OSendPacket() functions.  Disabling I2O interrupts
** will prevent hardware interrupt to host even though the outbound I2O msg
** queue is not emtpy.
** =========================================================================
*/
#define i960_OUT_POST_Q_INT_BIT        0x0008	/* bit set masks interrupts */

RC_RETURN
RCDisableI2OInterrupts (struct net_device * dev)
{
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	pPab->p_atu->OutIntMask |= i960_OUT_POST_Q_INT_BIT;

	return RC_RTN_NO_ERROR;
}

RC_RETURN
RCEnableI2OInterrupts (struct net_device * dev)
{
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	pPab->p_atu->OutIntMask &= ~i960_OUT_POST_Q_INT_BIT;

	return RC_RTN_NO_ERROR;

}

/*
** =========================================================================
** RCI2OSendPacket()
** =========================================================================
*/
RC_RETURN
RCI2OSendPacket (struct net_device * dev, U32 InitiatorContext,
		 PRCTCB pTransCtrlBlock)
{
	U32 msgOffset;
	PU32 pMsg;
	int size;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	dprintk ("RCI2OSendPacket()...\n");

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	/* get Inbound free Q entry - reading from In Q gets free Q entry */
	/* offset to Msg Frame in PCI msg block */

	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		dprintk ("RCI2OSendPacket(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	size = FillI2OMsgSGLFromTCB (pMsg + 4, pTransCtrlBlock);

	if (size == -1) {	/* error processing TCB - send NOP msg */
		dprintk ("RCI2OSendPacket(): Error Rrocess TCB!\n");
		pMsg[0] = THREE_WORD_MSG_SIZE | SGL_OFFSET_0;
		pMsg[1] =
		    I2O_UTIL_NOP << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
		return RC_RTN_TCB_ERROR;
	} else {		/* send over msg header */

		pMsg[0] = (size + 4) << 16 | LAN_MSG_REQST;	/* send over message size and flags */
		pMsg[1] =
		    I2O_LAN_PACKET_SEND << 24 | I2O_HOST_TID << 12 |
		    RC_LAN_TARGET_ID;
		pMsg[2] = InitiatorContext;
		pMsg[3] = 0;	/* batch reply */
		/* post to Inbound Post Q */
		pPab->p_atu->InQueue = msgOffset;
		return RC_RTN_NO_ERROR;
	}
}

/*
** =========================================================================
** RCI2OPostRecvBuffer()
**
** inputs:  pBufrCntrlBlock - pointer to buffer control block
**
** returns TRUE if successful in sending message, else FALSE.
** =========================================================================
*/
RC_RETURN
RCPostRecvBuffers (struct net_device * dev, PRCTCB pTransCtrlBlock)
{
	U32 msgOffset;
	PU32 pMsg;
	int size;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	dprintk ("RCPostRecvBuffers()...\n");

	/* search for DeviceHandle */

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	/* get Inbound free Q entry - reading from In Q gets free Q entry */
	/* offset to Msg Frame in PCI msg block */
	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		dprintk ("RCPostRecvBuffers(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}
	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	size = FillI2OMsgSGLFromTCB (pMsg + 4, pTransCtrlBlock);

	if (size == -1) {	/* error prcessing TCB - send 3 DWORD private msg == NOP */
		dprintk
		    ("RCPostRecvBuffers(): Error Processing TCB! size = %d\n",
		     size);
		pMsg[0] = THREE_WORD_MSG_SIZE | SGL_OFFSET_0;
		pMsg[1] =
		    I2O_UTIL_NOP << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
		/* post to Post Q */
		pPab->p_atu->InQueue = msgOffset;
		return RC_RTN_TCB_ERROR;
	} else {		/* send over size msg header */

		pMsg[0] = (size + 4) << 16 | LAN_MSG_REQST;	/* send over message size and flags */
		pMsg[1] =
		    I2O_LAN_RECEIVE_POST << 24 | I2O_HOST_TID << 12 |
		    RC_LAN_TARGET_ID;
		pMsg[2] = DEFAULT_RECV_INIT_CONTEXT;
		pMsg[3] = *(PU32) pTransCtrlBlock;	/* number of packet buffers */
		/* post to Post Q */
		pPab->p_atu->InQueue = msgOffset;
		return RC_RTN_NO_ERROR;
	}
}

/*
** =========================================================================
** RCProcI2OMsgQ()
**
** Process I2O outbound message queue until empty.
** =========================================================================
*/
void
RCProcI2OMsgQ (struct net_device *dev)
{
	U32 phyAddrMsg;
	PU8 p8Msg;
	PU32 p32;
	U16 count;
	PPAB pPab = ((PDPA) dev->priv)->pPab;
	unsigned char debug_msg[20];

	if (pPab == NULL)
		return;

	phyAddrMsg = pPab->p_atu->OutQueue;

	while (phyAddrMsg != 0xFFFFFFFF) {
		p8Msg =
		    pPab->pLinOutMsgBlock + (phyAddrMsg -
					     pPab->outMsgBlockPhyAddr);
		p32 = (PU32) p8Msg;

		dprintk ("msg: 0x%x  0x%x \n", p8Msg[7], p32[5]);

		/* Send Packet Reply Msg */
		if (I2O_LAN_PACKET_SEND == p8Msg[7]) {	/* function code byte */
			count = *(PU16) (p8Msg + 2);
			count -= p8Msg[0] >> 4;
			/* status, count, context[], adapter */
			(*pPab->pTransCallbackFunc) (p8Msg[19], count, p32 + 5,
						     dev);
		} else if (I2O_LAN_RECEIVE_POST == p8Msg[7]) {	/* Receive Packet Reply Msg */
			dprintk
			    ("I2O_RECV_REPLY pPab:0x%x p8Msg:0x%x p32:0x%x\n",
			     (u32) pPab, (u32) p8Msg, (u32) p32);
			dprintk ("msg: 0x%x:0x%x:0x%x:0x%x\n",
				 p32[0], p32[1], p32[2], p32[3]);
			dprintk ("     0x%x:0x%x:0x%x:0x%x\n",
				 p32[4], p32[5], p32[6], p32[7]);
			dprintk ("     0x%x:0X%x:0x%x:0x%x\n",
				 p32[8], p32[9], p32[10], p32[11]);
			/*  status, count, buckets remaining, packetParmBlock, adapter */
			(*pPab->pRecvCallbackFunc) (p8Msg[19], p8Msg[12],
						    p32[5], p32 + 6, dev);
		} else if (I2O_LAN_RESET == p8Msg[7]
			   || I2O_LAN_SHUTDOWN == p8Msg[7])
			if (pPab->pCallbackFunc)
				(*pPab->pCallbackFunc) (p8Msg[19], 0, 0, dev);
			else
				pPab->pCallbackFunc = (PFNCALLBACK) 1;
		else if (I2O_PRIVATE == p8Msg[7]) {
			dprintk ("i2o private 0x%x, 0x%x \n", p8Msg[7], p32[5]);
			switch (p32[5]) {
			case RC_PRIVATE_DEBUG_MSG:
				msgFlag = 1;
				dprintk ("Received I2O_PRIVATE msg\n");
				debug_msg[15] = (p32[6] & 0xff000000) >> 24;
				debug_msg[14] = (p32[6] & 0x00ff0000) >> 16;
				debug_msg[13] = (p32[6] & 0x0000ff00) >> 8;
				debug_msg[12] = (p32[6] & 0x000000ff);

				debug_msg[11] = (p32[7] & 0xff000000) >> 24;
				debug_msg[10] = (p32[7] & 0x00ff0000) >> 16;
				debug_msg[9] = (p32[7] & 0x0000ff00) >> 8;
				debug_msg[8] = (p32[7] & 0x000000ff);

				debug_msg[7] = (p32[8] & 0xff000000) >> 24;
				debug_msg[6] = (p32[8] & 0x00ff0000) >> 16;
				debug_msg[5] = (p32[8] & 0x0000ff00) >> 8;
				debug_msg[4] = (p32[8] & 0x000000ff);

				debug_msg[3] = (p32[9] & 0xff000000) >> 24;
				debug_msg[2] = (p32[9] & 0x00ff0000) >> 16;
				debug_msg[1] = (p32[9] & 0x0000ff00) >> 8;
				debug_msg[0] = (p32[9] & 0x000000ff);

				debug_msg[16] = '\0';
				dprintk ("%s", debug_msg);
				break;
			case RC_PRIVATE_REBOOT:
				dprintk ("Adapter reboot initiated...\n");
				if (pPab->pRebootCallbackFunc)
					(*pPab->pRebootCallbackFunc) (0, 0, 0,
								      dev);
				break;
			default:
				dprintk (KERN_WARNING
					"Unknown private I2O msg received: 0x%x\n",
					p32[5]);
				break;
			}
		}

		/* 
		   ** Process other Msg's
		 */
		else
			ProcessOutboundI2OMsg (pPab, phyAddrMsg);

		/* return MFA to outbound free Q */
		pPab->p_atu->OutQueue = phyAddrMsg;

		/* any more msgs? */
		phyAddrMsg = pPab->p_atu->OutQueue;
	}
}

/*
** =========================================================================
**  Returns LAN interface statistical counters to space provided by caller at
**  StatsReturnAddr.  Returns 0 if success, else RC_RETURN code.
**  This function will call the WaitCallback function provided by
**  user while waiting for card to respond.
** =========================================================================
*/
RC_RETURN
RCGetLinkStatistics (struct net_device *dev,
		     P_RCLINKSTATS StatsReturnAddr,
		     PFNWAITCALLBACK WaitCallback)
{
	U32 msgOffset;
	volatile PU32 pMsg;
	volatile PU32 p32, pReturnAddr;
	P_NICSTAT pStats;
	int i;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

/*dprintk("Get82558Stats() StatsReturnAddr:0x%x\n", StatsReturnAddr); */

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		dprintk ("Get8255XStats(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

/*dprintk("Get82558Stats - pMsg = 0x%x, InQ msgOffset = 0x%x\n", pMsg, msgOffset);*/
/*dprintk("Get82558Stats - pMsg = 0x%08X, InQ msgOffset = 0x%08X\n", pMsg, msgOffset);*/

	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = DEFAULT_RECV_INIT_CONTEXT;
	pMsg[3] = 0x112;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_GET_NIC_STATS;
	pMsg[5] = pPab->outMsgBlockPhyAddr;

//	p32 = (PU32) pPab->outMsgBlockPhyAddr;
	p32 = (PU32)pPab->pLinOutMsgBlock;
	pStats = (P_NICSTAT) pPab->pLinOutMsgBlock;
	pStats->dump_status = 0xFFFFFFFF;

	/* post to Inbound Post Q */
	pPab->p_atu->InQueue = msgOffset;

	i = 0;
	while (pStats->dump_status == 0xFFFFFFFF) {
		if (i++ > 0xff) {
			dprintk ("Timeout waiting for NIC statistics\n");
			return RC_RTN_MSG_REPLY_TIMEOUT;
		}
		udelay(50);
	}
	pReturnAddr = (PU32) StatsReturnAddr;

	/* copy Nic stats to user's structure */
	for (i = 0; i < (int) sizeof (RCLINKSTATS) / 4; i++)
		pReturnAddr[i] = p32[i];

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** Get82558LinkStatus()
** =========================================================================
*/
RC_RETURN
RCGetLinkStatus (struct net_device * dev, PU32 ReturnAddr,
		 PFNWAITCALLBACK WaitCallback)
{
	int i;
	U32 msgOffset;
	volatile PU32 pMsg;
	volatile PU32 p32;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	dprintk ("Get82558LinkStatus() ReturnPhysAddr:0x%x\n",
		 (u32) ReturnAddr);

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		dprintk ("Get82558LinkStatus(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);
/*dprintk("Get82558LinkStatus - pMsg = 0x%x, InQ msgOffset = 0x%x\n", pMsg, msgOffset);*/
/*dprintk("Get82558LinkStatus - pMsg = 0x%08X, InQ msgOffset = 0x%08X\n", pMsg, msgOffset);*/

	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = DEFAULT_RECV_INIT_CONTEXT;
	pMsg[3] = 0x112;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_GET_LINK_STATUS;
	pMsg[5] = pPab->outMsgBlockPhyAddr;

	p32 = (PU32) pPab->pLinOutMsgBlock;
	*p32 = 0xFFFFFFFF;

	/* post to Inbound Post Q */
	pPab->p_atu->InQueue = msgOffset;

	i = 0;
	while (*p32 == 0xFFFFFFFF) {
		if (i++ > 0xff) {
			dprintk ("Timeout waiting for link status\n");
			return RC_RTN_MSG_REPLY_TIMEOUT;
		}
		udelay(50);
	}

	*ReturnAddr = *p32;	/* 1 = up 0 = down */

	return RC_RTN_NO_ERROR;

}

/*
** =========================================================================
** RCGetMAC()
**
** get the MAC address the adapter is listening for in non-promiscous mode.
** MAC address is in media format.
** =========================================================================
*/
RC_RETURN
RCGetMAC (struct net_device * dev, PFNWAITCALLBACK WaitCallback)
{
	U32 off, i;
	PU8 mac = dev->dev_addr;
	PU32 p;
	U32 temp[2];
	PPAB pPab = ((PDPA) dev->priv)->pPab;
	PATU p_atu;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	p_atu = pPab->p_atu;

	p_atu->EtherMacLow = 0;	/* first zero return data */
	p_atu->EtherMacHi = 0;

	off = p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	p = (PU32) (pPab->pPci45LinBaseAddr + off);

	dprintk ("RCGetMAC: p_atu 0x%08x, off 0x%08x, p 0x%08x\n",
		 (uint) p_atu, (uint) off, (uint) p);
	/* setup private message */
	p[0] = FIVE_WORD_MSG_SIZE | SGL_OFFSET_0;
	p[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	p[2] = 0;		/* initiator context */
	p[3] = 0x218;		/* transaction context */
	p[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_GET_MAC_ADDR;

	p_atu->InQueue = off;	/* send it to the I2O device */
	dprintk ("RCGetMAC: p_atu 0x%08x, off 0x%08x, p 0x%08x\n",
		 (uint) p_atu, (uint) off, (uint) p);

	/* wait for the rcpci45 board to update the info */
	i = 0;
	while (0 == p_atu->EtherMacLow) {
		if (i++ > 0xff) {
			dprintk ("rc_getmac: Timeout\n");
			return RC_RTN_MSG_REPLY_TIMEOUT;
		}	
		udelay(50);
	}

	/* read the mac address  */
	temp[0] = p_atu->EtherMacLow;
	temp[1] = p_atu->EtherMacHi;
	memcpy ((char *) mac, (char *) temp, 6);

	dprintk ("rc_getmac: 0x%x\n", (u32) mac);

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCSetMAC()
**
** set MAC address the adapter is listening for in non-promiscous mode.
** MAC address is in media format.
** =========================================================================
*/
RC_RETURN
RCSetMAC (struct net_device * dev, PU8 mac)
{
	U32 off;
	PU32 pMsg;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	off = pPab->p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	pMsg = (PU32) (pPab->pPci45LinBaseAddr + off);

	/* setup private message */
	pMsg[0] = SEVEN_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_SET_MAC_ADDR;
	pMsg[5] = *(unsigned *) mac;	/* first four bytes */
	pMsg[6] = *(unsigned *) (mac + 4);	/* last two bytes */

	pPab->p_atu->InQueue = off;	/* send it to the I2O device */

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCSetLinkSpeed()
**
** set ethernet link speed. 
** input: speedControl - determines action to take as follows
**          0 = reset and auto-negotiate (NWay)
**          1 = Full Duplex 100BaseT
**          2 = Half duplex 100BaseT
**          3 = Full Duplex  10BaseT
**          4 = Half duplex  10BaseT
**          all other values are ignore (do nothing)
** =========================================================================
*/
RC_RETURN
RCSetLinkSpeed (struct net_device * dev, U16 LinkSpeedCode)
{
	U32 off;
	PU32 pMsg;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	off = pPab->p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	pMsg = (PU32) (pPab->pPci45LinBaseAddr + off);

	/* setup private message */
	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_SET_LINK_SPEED;
	pMsg[5] = LinkSpeedCode;	/* link speed code */

	pPab->p_atu->InQueue = off;	/* send it to the I2O device */

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCSetPromiscuousMode()
**
** Defined values for Mode:
**  0 - turn off promiscuous mode
**  1 - turn on  promiscuous mode
**
** =========================================================================
*/
RC_RETURN
RCSetPromiscuousMode (struct net_device * dev, U16 Mode)
{
	U32 off;
	PU32 pMsg;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	off = pPab->p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	pMsg = (PU32) (pPab->pPci45LinBaseAddr + off);

	/* setup private message */
	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_SET_PROMISCUOUS_MODE;
	pMsg[5] = Mode;		/* promiscuous mode setting */

	pPab->p_atu->InQueue = off;	/* send it to the device */

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCGetPromiscuousMode()
**
** get promiscuous mode setting
**
** Possible return values placed in pMode:
**  0 = promisuous mode not set
**  1 = promisuous mode is set
**
** =========================================================================
*/
RC_RETURN
RCGetPromiscuousMode (struct net_device * dev, PU32 pMode,
		      PFNWAITCALLBACK WaitCallback)
{
	PU32 pMsg;
	volatile PU32 p32;
	U32 msgOffset, i;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		dprintk (KERN_WARNING
			"RCGetLinkSpeed(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	/* virtual pointer to return buffer - clear first two dwords */
	p32 = (volatile PU32) pPab->pLinOutMsgBlock;
	p32[0] = 0xff;

	/* setup private message */
	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_GET_PROMISCUOUS_MODE;
	/* phys address to return status - area right after PAB */
	pMsg[5] = pPab->outMsgBlockPhyAddr;

	/* post to Inbound Post Q */

	pPab->p_atu->InQueue = msgOffset;

	i = 0;

	/* wait for response */
	while (p32[0] == 0xff) {
		if (i++ > 0xff) {
			dprintk ("Timeout waiting for promiscuous mode\n");
			return RC_RTN_NO_LINK_SPEED;
		}
		udelay(50);
	}

	/* get mode */
	*pMode = (U8) ((volatile PU8) p32)[0] & 0x0f;

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCSetBroadcastMode()
**
** Defined values for Mode:
**  0 - turn off promiscuous mode
**  1 - turn on  promiscuous mode
**
** =========================================================================
*/
RC_RETURN
RCSetBroadcastMode (struct net_device * dev, U16 Mode)
{
	U32 off;
	PU32 pMsg;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	off = pPab->p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	pMsg = (PU32) (pPab->pPci45LinBaseAddr + off);

	/* setup private message */
	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_SET_BROADCAST_MODE;
	pMsg[5] = Mode;		/* promiscuous mode setting */

	pPab->p_atu->InQueue = off;	/* send it to the device */

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCGetBroadcastMode()
**
** get promiscuous mode setting
**
** Possible return values placed in pMode:
**  0 = promisuous mode not set
**  1 = promisuous mode is set
**
** =========================================================================
*/
RC_RETURN
RCGetBroadcastMode (struct net_device * dev, PU32 pMode,
		    PFNWAITCALLBACK WaitCallback)
{
	U32 msgOffset;
	PU32 pMsg;
	volatile PU32 p32;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		dprintk (KERN_WARNING
			"RCGetLinkSpeed(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	/* virtual pointer to return buffer - clear first two dwords */
	p32 = (volatile PU32) pPab->pLinOutMsgBlock;
	p32[0] = 0xff;

	/* setup private message */
	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_GET_BROADCAST_MODE;
	/* phys address to return status - area right after PAB */
	pMsg[5] = pPab->outMsgBlockPhyAddr;

	/* post to Inbound Post Q */

	pPab->p_atu->InQueue = msgOffset;

	/* wait for response */
	if (p32[0] == 0xff) {
		dprintk (KERN_WARNING
			"Timeout waiting for promiscuous mode\n");
		return RC_RTN_NO_LINK_SPEED;
	}

	/* get mode */
	*pMode = (U8) ((volatile PU8) p32)[0] & 0x0f;

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCGetLinkSpeed()
**
** get ethernet link speed. 
**
** 0 = Unknown
** 1 = Full Duplex 100BaseT
** 2 = Half duplex 100BaseT
** 3 = Full Duplex  10BaseT
** 4 = Half duplex  10BaseT
**
** =========================================================================
*/
RC_RETURN
RCGetLinkSpeed (struct net_device * dev, PU32 pLinkSpeedCode,
		PFNWAITCALLBACK WaitCallback)
{
	U32 msgOffset, i;
	PU32 pMsg;
	volatile PU32 p32;
	U8 IOPLinkSpeed;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		dprintk (KERN_WARNING
			"RCGetLinkSpeed(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	/* virtual pointer to return buffer - clear first two dwords */
	p32 = (volatile PU32) pPab->pLinOutMsgBlock;
	p32[0] = 0xff;

	/* setup private message */
	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_GET_LINK_SPEED;
	/* phys address to return status - area right after PAB */
	pMsg[5] = pPab->outMsgBlockPhyAddr;

	/* post to Inbound Post Q */

	pPab->p_atu->InQueue = msgOffset;

	/* wait for response */
	i = 0;
	while (p32[0] == 0xff) {
		if (i++ > 0xff) {
			dprintk ("Timeout waiting for link speed\n");
			return RC_RTN_NO_LINK_SPEED;
		}
		udelay(50);
	}
	/* get Link speed */
	IOPLinkSpeed = (U8) ((volatile PU8) p32)[0] & 0x0f;
	*pLinkSpeedCode = IOPLinkSpeed;

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCReportDriverCapability(struct net_device *dev, U32 capability)
**
** Currently defined bits:
** WARM_REBOOT_CAPABLE   0x01
**
** =========================================================================
*/
RC_RETURN
RCReportDriverCapability (struct net_device * dev, U32 capability)
{
	U32 off;
	PU32 pMsg;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	off = pPab->p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	pMsg = (PU32) (pPab->pPci45LinBaseAddr + off);

	/* setup private message */
	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] =
	    RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_REPORT_DRIVER_CAPABILITY;
	pMsg[5] = capability;

	pPab->p_atu->InQueue = off;	/* send it to the I2O device */

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCGetFirmwareVer()
**
** Return firmware version in the form "SoftwareVersion : Bt BootVersion"
**
** =========================================================================
*/
RC_RETURN
RCGetFirmwareVer (struct net_device * dev, PU8 pFirmString,
		  PFNWAITCALLBACK WaitCallback)
{
	U32 msgOffset, i;
	PU32 pMsg;
	volatile PU32 p32;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	msgOffset = pPab->p_atu->InQueue;
	if (msgOffset == 0xFFFFFFFF) {
		dprintk ("RCGetFirmwareVer(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	/* virtual pointer to return buffer - clear first two dwords */
	p32 = (volatile PU32) pPab->pLinOutMsgBlock;
	p32[0] = 0xff;

	/* setup private message */
	pMsg[0] = SIX_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_GET_FIRMWARE_REV;
	/* phys address to return status - area right after PAB */
	pMsg[5] = pPab->outMsgBlockPhyAddr;

	/* post to Inbound Post Q */

	pPab->p_atu->InQueue = msgOffset;

	/* wait for response */
	i = 0;
	while (p32[0] == 0xff) {
		if (i++ > 0xff) {
			dprintk ("Timeout waiting for link speed\n");
			return RC_RTN_NO_FIRM_VER;
		}
		udelay(50);
	}
	strcpy (pFirmString, (PU8) p32);
	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCResetLANCard()
**
** ResourceFlags indicates whether to return buffer resource explicitly
** to host or keep and reuse.
** CallbackFunction (if not NULL) is the function to be called when 
** reset is complete.
** If CallbackFunction is NULL, ReturnAddr will have a 1 placed in it when
** reset is done (if not NULL).
**
** =========================================================================
*/
RC_RETURN
RCResetLANCard (struct net_device * dev, U16 ResourceFlags, PU32 ReturnAddr,
		PFNCALLBACK CallbackFunction)
{
	unsigned long off;
	PU32 pMsg;
	PPAB pPab = ((PDPA) dev->priv)->pPab;
	long timeout = 0;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	off = pPab->p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	pPab->pCallbackFunc = CallbackFunction;

	pMsg = (PU32) (pPab->pPci45LinBaseAddr + off);

	/* setup message */
	pMsg[0] = FOUR_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_LAN_RESET << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = DEFAULT_RECV_INIT_CONTEXT;
	pMsg[3] = ResourceFlags << 16;	/* resource flags */

	pPab->p_atu->InQueue = off;	/* send it to the I2O device */

	if (CallbackFunction == (PFNCALLBACK) NULL) {
		/* call RCProcI2OMsgQ() until something in pPab->pCallbackFunc
		   or until timer goes off */
		while (pPab->pCallbackFunc == (PFNCALLBACK) NULL) {
			RCProcI2OMsgQ (dev);
			mdelay (1);
			timeout++;
			if (timeout > 200) {
				break;
			}
		}
		if (ReturnAddr != (PU32) NULL)
			*ReturnAddr = (U32) pPab->pCallbackFunc;
	}

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCResetIOP()
**
** Send StatusGet Msg, wait for results return directly to buffer.
**
** =========================================================================
*/
RC_RETURN
RCResetIOP (struct net_device * dev)
{
	U32 msgOffset, i;
	PU32 pMsg;
	PPAB pPab = ((PDPA) dev->priv)->pPab;
	volatile PU32 p32;

	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	pMsg[0] = NINE_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_EXEC_IOP_RESET << 24 | I2O_HOST_TID << 12 | I2O_IOP_TID;
	pMsg[2] = 0;		/* universal context */
	pMsg[3] = 0;		/* universal context */
	pMsg[4] = 0;		/* universal context */
	pMsg[5] = 0;		/* universal context */
	/* phys address to return status - area right after PAB */
	pMsg[6] = pPab->outMsgBlockPhyAddr;
	pMsg[7] = 0;
	pMsg[8] = 1;		/*  return 1 byte */

	/* virtual pointer to return buffer - clear first two dwords */
	p32 = (volatile PU32) pPab->pLinOutMsgBlock;
	p32[0] = 0;
	p32[1] = 0;

	/* post to Inbound Post Q */

	pPab->p_atu->InQueue = msgOffset;

	/* wait for response */
	i = 0;
	while (!p32[0] && !p32[1]) {
		if (i++ > 0xff) {
			dprintk ("RCResetIOP timeout\n");
			return RC_RTN_MSG_REPLY_TIMEOUT;
		}
		udelay(100);
	}
	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCShutdownLANCard()
**
** ResourceFlags indicates whether to return buffer resource explicitly
** to host or keep and reuse.
** CallbackFunction (if not NULL) is the function to be called when 
** shutdown is complete.
** If CallbackFunction is NULL, ReturnAddr will have a 1 placed in it when
** shutdown is done (if not NULL).
**
** =========================================================================
*/
RC_RETURN
RCShutdownLANCard (struct net_device * dev, U16 ResourceFlags,
		   PU32 ReturnAddr, PFNCALLBACK CallbackFunction)
{
	volatile PU32 pMsg;
	U32 off;
	PPAB pPab = ((PDPA) dev->priv)->pPab;
	long timeout = 0;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	off = pPab->p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	pPab->pCallbackFunc = CallbackFunction;

	pMsg = (PU32) (pPab->pPci45LinBaseAddr + off);

	/* setup message */
	pMsg[0] = FOUR_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] =
	    I2O_LAN_SHUTDOWN << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = DEFAULT_RECV_INIT_CONTEXT;
	pMsg[3] = ResourceFlags << 16;	/* resource flags */

	pPab->p_atu->InQueue = off;	/* send it to the I2O device */

	if (CallbackFunction == (PFNCALLBACK) NULL) {
		/* call RCProcI2OMsgQ() until something in pPab->pCallbackFunc
		   or until timer goes off */
		while (pPab->pCallbackFunc == (PFNCALLBACK) NULL) {
			RCProcI2OMsgQ (dev);
			mdelay (1);
			timeout++;
			if (timeout > 200) {
				dprintk (KERN_WARNING
					"RCShutdownLANCard(): timeout\n");
				break;
			}
		}
		if (ReturnAddr != (PU32) NULL)
			*ReturnAddr = (U32) pPab->pCallbackFunc;
	}
	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** RCSetRavlinIPandMask()
**
** Set the Ravlin 45/PCI cards IP address and network mask.
**
** IP address and mask must be in network byte order.
** For example, IP address 1.2.3.4 and mask 255.255.255.0 would be
** 0x04030201 and 0x00FFFFFF on a little endian machine.
**
** =========================================================================
*/
RC_RETURN
RCSetRavlinIPandMask (struct net_device * dev, U32 ipAddr, U32 netMask)
{
	volatile PU32 pMsg;
	U32 off;
	PPAB pPab = ((PDPA) dev->priv)->pPab;

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	off = pPab->p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	pMsg = (PU32) (pPab->pPci45LinBaseAddr + off);

	/* setup private message */
	pMsg[0] = SEVEN_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x219;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_SET_IP_AND_MASK;
	pMsg[5] = ipAddr;
	pMsg[6] = netMask;

	pPab->p_atu->InQueue = off;	/* send it to the I2O device */
	return RC_RTN_NO_ERROR;

}

/*
** =========================================================================
** RCGetRavlinIPandMask()
**
** get the IP address and MASK from the card
** 
** =========================================================================
*/
RC_RETURN
RCGetRavlinIPandMask (struct net_device * dev, PU32 pIpAddr, PU32 pNetMask,
		      PFNWAITCALLBACK WaitCallback)
{
	U32 off, i;
	PU32 pMsg, p32;
	PPAB pPab = ((PDPA) dev->priv)->pPab;
	PATU p_atu;

	dprintk
	    ("RCGetRavlinIPandMask: pIpAddr is 0x%x, *IpAddr is 0x%x\n",
	     (u32) pIpAddr, *pIpAddr);

	if (pPab == NULL)
		return RC_RTN_ADPTR_NOT_REGISTERED;

	p_atu = pPab->p_atu;
	off = p_atu->InQueue;	/* get addresss of message */

	if (0xFFFFFFFF == off)
		return RC_RTN_FREE_Q_EMPTY;

	p32 = (volatile PU32) pPab->pLinOutMsgBlock;
	*p32 = 0xFFFFFFFF;

	pMsg = (PU32) (pPab->pPci45LinBaseAddr + off);

	dprintk
	    ("RCGetRavlinIPandMask: p_atu 0x%x, off 0x%x, p32 0x%x\n",
	     (u32) p_atu, off, (u32) p32);
	/* setup private message */
	pMsg[0] = FIVE_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_PRIVATE << 24 | I2O_HOST_TID << 12 | RC_LAN_TARGET_ID;
	pMsg[2] = 0;		/* initiator context */
	pMsg[3] = 0x218;	/* transaction context */
	pMsg[4] = RC_PCI45_VENDOR_ID << 16 | RC_PRIVATE_GET_IP_AND_MASK;
	pMsg[5] = pPab->outMsgBlockPhyAddr;

	p_atu->InQueue = off;	/* send it to the I2O device */
	dprintk
	    ("RCGetRavlinIPandMask: p_atu 0x%x, off 0x%x, p32 0x%x\n",
	     (u32) p_atu, off, (u32) p32);

	/* wait for the rcpci45 board to update the info */
	i = 0;
	while (0xffffffff == *p32) {
		if (i++ > 0xff) {
			dprintk ("RCGetRavlinIPandMask: Timeout\n");
			return RC_RTN_MSG_REPLY_TIMEOUT;
		}
		udelay(50);
	}

	dprintk
	    ("RCGetRavlinIPandMask: after time out\np32[0] (IpAddr) 0x%x, p32[1] (IPmask) 0x%x\n",
	     p32[0], p32[1]);

	/* send IP and mask to user's space  */
	*pIpAddr = p32[0];
	*pNetMask = p32[1];

	dprintk
	    ("RCGetRavlinIPandMask: pIpAddr is 0x%x, *IpAddr is 0x%x\n",
	     (u32) pIpAddr, *pIpAddr);

	return RC_RTN_NO_ERROR;
}

/* 
** /////////////////////////////////////////////////////////////////////////
** /////////////////////////////////////////////////////////////////////////
**
**                        local functions
**
** /////////////////////////////////////////////////////////////////////////
** /////////////////////////////////////////////////////////////////////////
*/

/*
** =========================================================================
** SendI2OOutboundQInitMsg()
**
** =========================================================================
*/
static int
SendI2OOutboundQInitMsg (PPAB pPab)
{
	U32 msgOffset, phyOutQFrames, i;
	volatile PU32 pMsg;
	volatile PU32 p32;

	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		dprintk ("SendI2OOutboundQInitMsg(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	dprintk
	    ("SendI2OOutboundQInitMsg - pMsg = 0x%x, InQ msgOffset = 0x%x\n",
	     (u32) pMsg, msgOffset);

	pMsg[0] = EIGHT_WORD_MSG_SIZE | TRL_OFFSET_6;
	pMsg[1] =
	    I2O_EXEC_OUTBOUND_INIT << 24 | I2O_HOST_TID << 12 | I2O_IOP_TID;
	pMsg[2] = DEFAULT_RECV_INIT_CONTEXT;
	pMsg[3] = 0x106;	/* transaction context */
	pMsg[4] = 4096;		/* Host page frame size */
	pMsg[5] = MSG_FRAME_SIZE << 16 | 0x80;	/* outbound msg frame size and Initcode */
	pMsg[6] = 0xD0000004;	/* simple sgl element LE, EOB */
	/* phys address to return status - area right after PAB */
	pMsg[7] = pPab->outMsgBlockPhyAddr;

	/* virtual pointer to return buffer - clear first two dwords */
	p32 = (PU32) pPab->pLinOutMsgBlock;
	p32[0] = 0;

	/* post to Inbound Post Q */
	pPab->p_atu->InQueue = msgOffset;

	/* wait for response */
	i = 0;
	while (!p32[0]) {
		if (i++ > 0xff) {
			printk("rc: InitOutQ timeout\n");
			return RC_RTN_NO_I2O_STATUS;
		}
		udelay(50);
	}
	if (p32[0] != I2O_EXEC_OUTBOUND_INIT_COMPLETE) {
		printk("rc: exec outbound init failed (%x)\n",
				p32[0]);
		return RC_RTN_NO_I2O_STATUS;
	}
	/* load PCI outbound free Q with MF physical addresses */
	phyOutQFrames = pPab->outMsgBlockPhyAddr;

	for (i = 0; i < NMBR_MSG_FRAMES; i++) {
		pPab->p_atu->OutQueue = phyOutQFrames;
		phyOutQFrames += MSG_FRAME_SIZE;
	}
	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** GetI2OStatus()
**
** Send StatusGet Msg, wait for results return directly to buffer.
**
** =========================================================================
*/
static int
GetI2OStatus (PPAB pPab)
{
	U32 msgOffset, i;
	PU32 pMsg;
	volatile PU32 p32;

	msgOffset = pPab->p_atu->InQueue;
	dprintk ("GetI2OStatus: msg offset = 0x%x\n", msgOffset);
	if (msgOffset == 0xFFFFFFFF) {
		dprintk ("GetI2OStatus(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	pMsg[0] = NINE_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_EXEC_STATUS_GET << 24 | I2O_HOST_TID << 12 | I2O_IOP_TID;
	pMsg[2] = 0;		/* universal context */
	pMsg[3] = 0;		/* universal context */
	pMsg[4] = 0;		/* universal context */
	pMsg[5] = 0;		/* universal context */
	/* phys address to return status - area right after PAB */
	pMsg[6] = pPab->outMsgBlockPhyAddr;
	pMsg[7] = 0;
	pMsg[8] = 88;		/*  return 88 bytes */

	/* virtual pointer to return buffer - clear first two dwords */
	p32 = (volatile PU32) pPab->pLinOutMsgBlock;
	p32[0] = 0;
	p32[1] = 0;

	dprintk
	    ("GetI2OStatus - pMsg:0x%x, msgOffset:0x%x, [1]:0x%x, [6]:0x%x\n",
	     (u32) pMsg, msgOffset, pMsg[1], pMsg[6]);

	/* post to Inbound Post Q */
	pPab->p_atu->InQueue = msgOffset;

	dprintk ("Return status to p32 = 0x%x\n", (u32) p32);

	/* wait for response */
	i = 0;
	while (!p32[0] || !p32[1]) {
		if (i++ > 0xff) {
			dprintk ("Timeout waiting for status from IOP\n");
			return RC_RTN_NO_I2O_STATUS;
		}
		udelay(50);
	}

	dprintk ("0x%x:0x%x:0x%x:0x%x\n", p32[0], p32[1],
		 p32[2], p32[3]);
	dprintk ("0x%x:0x%x:0x%x:0x%x\n", p32[4], p32[5],
		 p32[6], p32[7]);
	dprintk ("0x%x:0x%x:0x%x:0x%x\n", p32[8], p32[9],
		 p32[10], p32[11]);
	/* get IOP state */
	pPab->IOPState = ((volatile PU8) p32)[10];
	pPab->InboundMFrameSize = ((volatile PU16) p32)[6];

	dprintk ("IOP state 0x%x InFrameSize = 0x%x\n",
		 pPab->IOPState, pPab->InboundMFrameSize);
	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** SendEnableSysMsg()
**
**
** =========================================================================
*/
static int
SendEnableSysMsg (PPAB pPab)
{
	U32 msgOffset;
	volatile PU32 pMsg;

	msgOffset = pPab->p_atu->InQueue;

	if (msgOffset == 0xFFFFFFFF) {
		dprintk ("SendEnableSysMsg(): Inbound Free Q empty!\n");
		return RC_RTN_FREE_Q_EMPTY;
	}

	/* calc virtual address of msg - virtual already mapped to physical */
	pMsg = (PU32) (pPab->pPci45LinBaseAddr + msgOffset);

	dprintk
	    ("SendEnableSysMsg - pMsg = 0x%x, InQ msgOffset = 0x%x\n",
	     (u32) pMsg, msgOffset);

	pMsg[0] = FOUR_WORD_MSG_SIZE | SGL_OFFSET_0;
	pMsg[1] = I2O_EXEC_SYS_ENABLE << 24 | I2O_HOST_TID << 12 | I2O_IOP_TID;
	pMsg[2] = DEFAULT_RECV_INIT_CONTEXT;
	pMsg[3] = 0x110;	/* transaction context */
	pMsg[4] = 0x50657465;	/*  RedCreek Private */

	/* post to Inbound Post Q */
	pPab->p_atu->InQueue = msgOffset;

	return RC_RTN_NO_ERROR;
}

/*
** =========================================================================
** FillI2OMsgFromTCB()
**
** inputs   pMsgU32 - virtual pointer (mapped to physical) of message frame
**          pXmitCntrlBlock - pointer to caller buffer control block.
**
** fills in LAN SGL after Transaction Control Word or Bucket Count.
** =========================================================================
*/
static int
FillI2OMsgSGLFromTCB (PU32 pMsgFrame, PRCTCB pTransCtrlBlock)
{
	unsigned int nmbrBuffers, nmbrSeg, nmbrDwords, context, flags;
	PU32 pTCB, pMsg;

	/* SGL element flags */
#define EOB        0x40000000
#define LE         0x80000000
#define SIMPLE_SGL 0x10000000
#define BC_PRESENT 0x01000000

	pTCB = (PU32) pTransCtrlBlock;
	pMsg = pMsgFrame;
	nmbrDwords = 0;

	dprintk ("FillI2OMsgSGLFromTCBX\n");
	dprintk ("TCB  0x%x:0x%x:0x%x:0x%x:0x%x\n",
		 pTCB[0], pTCB[1], pTCB[2], pTCB[3], pTCB[4]);
	dprintk ("pTCB 0x%x, pMsg 0x%x\n", (u32) pTCB, (u32) pMsg);

	nmbrBuffers = *pTCB++;

	if (!nmbrBuffers) {
		return -1;
	}

	do {
		context = *pTCB++;	/* buffer tag (context) */
		nmbrSeg = *pTCB++;	/* number of segments */

		if (!nmbrSeg) {
			return -1;
		}

		flags = SIMPLE_SGL | BC_PRESENT;

		if (1 == nmbrSeg) {
			flags |= EOB;

			if (1 == nmbrBuffers)
				flags |= LE;
		}

		/* 1st SGL buffer element has context */
		pMsg[0] = pTCB[0] | flags;	/* send over count (segment size) */
		pMsg[1] = context;
		pMsg[2] = pTCB[1];	/* send buffer segment physical address */
		nmbrDwords += 3;
		pMsg += 3;
		pTCB += 2;

		if (--nmbrSeg) {
			do {
				flags = SIMPLE_SGL;

				if (1 == nmbrSeg) {
					flags |= EOB;

					if (1 == nmbrBuffers)
						flags |= LE;
				}

				pMsg[0] = pTCB[0] | flags;	/* send over count */
				pMsg[1] = pTCB[1];	/* send buffer segment physical address */
				nmbrDwords += 2;
				pTCB += 2;
				pMsg += 2;

			} while (--nmbrSeg);
		}

	} while (--nmbrBuffers);

	return nmbrDwords;
}

/*
** =========================================================================
** ProcessOutboundI2OMsg()
**
** process I2O reply message
** * change to msg structure *
** =========================================================================
*/
static void
ProcessOutboundI2OMsg (PPAB pPab, U32 phyAddrMsg)
{
	PU8 p8Msg;
	PU32 p32;
/*      U16 count; */

	p8Msg = pPab->pLinOutMsgBlock + (phyAddrMsg - pPab->outMsgBlockPhyAddr);
	p32 = (PU32) p8Msg;

	dprintk
	    ("VXD: ProcessOutboundI2OMsg - pPab 0x%x, phyAdr 0x%x, linAdr 0x%x\n",
	     (u32) pPab, phyAddrMsg, (u32) p8Msg);
	dprintk ("msg :0x%x:0x%x:0x%x:0x%x\n", p32[0], p32[1],
		 p32[2], p32[3]);
	dprintk ("msg :0x%x:0x%x:0x%x:0x%x\n", p32[4], p32[5],
		 p32[6], p32[7]);

	if (p32[4] >> 24 != I2O_REPLY_STATUS_SUCCESS) {
		dprintk ("Message reply status not success\n");
		return;
	}

	switch (p8Msg[7]) {	/* function code byte */
	case I2O_EXEC_SYS_TAB_SET:
		msgFlag = 1;
		dprintk ("Received I2O_EXEC_SYS_TAB_SET reply\n");
		break;

	case I2O_EXEC_HRT_GET:
		msgFlag = 1;
		dprintk ("Received I2O_EXEC_HRT_GET reply\n");
		break;

	case I2O_EXEC_LCT_NOTIFY:
		msgFlag = 1;
		dprintk ("Received I2O_EXEC_LCT_NOTIFY reply\n");
		break;

	case I2O_EXEC_SYS_ENABLE:
		msgFlag = 1;
		dprintk ("Received I2O_EXEC_SYS_ENABLE reply\n");
		break;

	default:
		dprintk ("Received UNKNOWN reply\n");
		break;
	}
}
