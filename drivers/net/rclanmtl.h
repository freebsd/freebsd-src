/*
** *************************************************************************
**
**
**     R C L A N M T L . H             $Revision: 6 $
**
**
**  RedCreek I2O LAN Message Transport Layer header file.
**
**  ---------------------------------------------------------------------
**  ---     Copyright (c) 1997-1999, RedCreek Communications Inc.     ---
**  ---                   All rights reserved.                        ---
**  ---------------------------------------------------------------------
**
**  File Description:
**
**  Header file for host I2O (Intelligent I/O) LAN message transport layer 
**  API and data types.
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
** *************************************************************************
*/

#ifndef RCLANMTL_H
#define RCLANMTL_H

/* Linux specific includes */
#include <asm/types.h>
#ifdef RC_LINUX_MODULE		/* linux modules need non-library version of string functions */
#include <linux/string.h>
#else
#include <string.h>
#endif
#include <linux/delay.h>	/* for udelay() */

#include <linux/netdevice.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/io.h>

/* Debug stuff. Define for debug output */
#undef RCDEBUG

#ifdef RCDEBUG
#define dprintk(args...) printk("rc: " args)
#else
#define dprintk(args...) { }
#endif

/* Typedefs */

 /* scalar data types */
typedef __u8 U8;
typedef __u16 U16;
typedef __u32 U32;
typedef __u8 *PU8;
typedef __u16 *PU16;
typedef __u32 *PU32;
typedef unsigned long BF;
typedef int RC_RETURN;

 /* 
    ** type PFNWAITCALLBACK
    **
    ** pointer to void function - type used for WaitCallback in some functions 
  */
typedef void (*PFNWAITCALLBACK) (void);	/* void argument avoids compiler complaint */

 /*
    ** type PFNTXCALLBACK 
    **
    ** Pointer to user's transmit callback function.  This user function is
    ** called from RCProcI2OMsgQ() when packet have been transmitted from buffers
    ** given in the RCI2OSendPacket() function.  BufferContext is a pointer to
    ** an array of 32 bit context values.  These are the values the user assigned
    ** and passed in the TCB to the RCI2OSendPacket() function.  PcktCount
    ** indicates the number of buffer context values in the BufferContext[] array.
    ** The User's TransmitCallbackFunction should recover (put back in free queue)
    ** the packet buffers associated with the buffer context values.
  */
typedef void (*PFNTXCALLBACK) (U32 Status,
			       U16 PcktCount,
			       PU32 BufferContext, struct net_device *);

 /* 
    ** type PFNRXCALLBACK 
    **
    ** Pointer to user's receive callback function.  This user function
    ** is called from RCProcI2OMsgQ() when packets have been received into
    ** previously posted packet buffers throught the RCPostRecvBuffers() function.
    ** The received callback function should process the Packet Descriptor Block
    ** pointed to by PacketDescBlock. See Packet Decription Block below.
  */
typedef void (*PFNRXCALLBACK) (U32 Status,
			       U8 PktCount,
			       U32 BucketsRemain,
			       PU32 PacketDescBlock, struct net_device *);

 /* 
    ** type PFNCALLBACK 
    **
    ** Pointer to user's generic callback function.  This user function
    ** can be passed to LANReset or LANShutdown and is called when the 
    ** the reset or shutdown is complete.
    ** Param1 and Param2 are invalid for LANReset and LANShutdown.
  */
typedef void (*PFNCALLBACK) (U32 Status,
			     U32 Param1, U32 Param2, struct net_device * dev);

/*
**  Message Unit CSR definitions for RedCreek PCI45 board
*/
typedef struct tag_rcatu {
	volatile unsigned long APICRegSel;	/* APIC Register Select */
	volatile unsigned long reserved0;
	volatile unsigned long APICWinReg;	/* APIC Window Register */
	volatile unsigned long reserved1;
	volatile unsigned long InMsgReg0;	/* inbound message register 0 */
	volatile unsigned long InMsgReg1;	/* inbound message register 1 */
	volatile unsigned long OutMsgReg0;	/* outbound message register 0 */
	volatile unsigned long OutMsgReg1;	/* outbound message register 1 */
	volatile unsigned long InDoorReg;	/* inbound doorbell register */
	volatile unsigned long InIntStat;	/* inbound interrupt status register */
	volatile unsigned long InIntMask;	/* inbound interrupt mask register */
	volatile unsigned long OutDoorReg;	/* outbound doorbell register */
	volatile unsigned long OutIntStat;	/* outbound interrupt status register */
	volatile unsigned long OutIntMask;	/* outbound interrupt mask register */
	volatile unsigned long reserved2;
	volatile unsigned long reserved3;
	volatile unsigned long InQueue;	/* inbound queue port */
	volatile unsigned long OutQueue;	/* outbound queue port */
	volatile unsigned long reserved4;
	volatile unsigned long reserver5;
	/* RedCreek extension */
	volatile unsigned long EtherMacLow;
	volatile unsigned long EtherMacHi;
	volatile unsigned long IPaddr;
	volatile unsigned long IPmask;
} *PATU;

 /* 
    ** typedef PAB
    **
    ** PCI Adapter Block - holds instance specific information.
  */
typedef struct {
	PATU p_atu;		/* ptr to  ATU register block */
	PU8 pPci45LinBaseAddr;
	PU8 pLinOutMsgBlock;
	U32 outMsgBlockPhyAddr;
	PFNTXCALLBACK pTransCallbackFunc;
	PFNRXCALLBACK pRecvCallbackFunc;
	PFNCALLBACK pRebootCallbackFunc;
	PFNCALLBACK pCallbackFunc;
	U16 IOPState;
	U16 InboundMFrameSize;
} *PPAB;

/*
 * Driver Private Area, DPA.
 */
typedef struct {
	U8 id;			/* the AdapterID */

	/* These two field are basically for the RCioctl function.
	 * I could not determine if they could be avoided. (RAA)*/
	U32 pci_addr;		/* the pci address of the adapter */
	U32 pci_addr_len;

	struct timer_list timer;	/*  timer */
	struct net_device_stats stats;	/* the statistics structure */
	unsigned long numOutRcvBuffers;	/* number of outstanding receive buffers */
	unsigned char shutdown;
	unsigned char reboot;
	unsigned char nexus;
	PU8 msgbuf;		/* Pointer to Lan Api Private Area */
	PU8 PLanApiPA;		/* Pointer to Lan Api Private Area (aligned) */
	PPAB pPab;		/* Pointer to the PCI Adapter Block */
} *PDPA;

/* PCI/45 Configuration space values */
#define RC_PCI45_VENDOR_ID  0x4916
#define RC_PCI45_DEVICE_ID  0x1960

 /* RedCreek API function return values */
#define RC_RTN_NO_ERROR             0
#define RC_RTN_I2O_NOT_INIT         1
#define RC_RTN_FREE_Q_EMPTY         2
#define RC_RTN_TCB_ERROR            3
#define RC_RTN_TRANSACTION_ERROR    4
#define RC_RTN_ADAPTER_ALREADY_INIT 5
#define RC_RTN_MALLOC_ERROR         6
#define RC_RTN_ADPTR_NOT_REGISTERED 7
#define RC_RTN_MSG_REPLY_TIMEOUT    8
#define RC_RTN_NO_I2O_STATUS        9
#define RC_RTN_NO_FIRM_VER         10
#define RC_RTN_NO_LINK_SPEED       11

/* Driver capability flags */
#define WARM_REBOOT_CAPABLE      0x01

/*
** Status - Transmit and Receive callback status word 
**
** A 32 bit Status is returned to the TX and RX callback functions.  This value
** contains both the reply status and the detailed status as follows:
**
**  32    24     16            0
**  +------+------+------------+
**  | Reply|      |  Detailed  |
**  |Status|   0  |   Status   |
**  +------+------+------------+
**
** Reply Status and Detailed Status of zero indicates No Errors.
*/
 /* reply message status defines */
#define    I2O_REPLY_STATUS_SUCCESS                    0x00
#define    I2O_REPLY_STATUS_ABORT_NO_DATA_TRANSFER     0x02
#define    I2O_REPLY_STATUS_TRANSACTION_ERROR          0x0A

/* DetailedStatusCode defines */
#define    I2O_LAN_DSC_SUCCESS                         0x0000
#define    I2O_LAN_DSC_DEVICE_FAILURE                  0x0001
#define    I2O_LAN_DSC_DESTINATION_NOT_FOUND           0x0002
#define    I2O_LAN_DSC_TRANSMIT_ERROR                  0x0003
#define    I2O_LAN_DSC_TRANSMIT_ABORTED                0x0004
#define    I2O_LAN_DSC_RECEIVE_ERROR                   0x0005
#define    I2O_LAN_DSC_RECEIVE_ABORTED                 0x0006
#define    I2O_LAN_DSC_DMA_ERROR                       0x0007
#define    I2O_LAN_DSC_BAD_PACKET_DETECTED             0x0008
#define    I2O_LAN_DSC_OUT_OF_MEMORY                   0x0009
#define    I2O_LAN_DSC_BUCKET_OVERRUN                  0x000A
#define    I2O_LAN_DSC_IOP_INTERNAL_ERROR              0x000B
#define    I2O_LAN_DSC_CANCELED                        0x000C
#define    I2O_LAN_DSC_INVALID_TRANSACTION_CONTEXT     0x000D
#define    I2O_LAN_DSC_DESTINATION_ADDRESS_DETECTED    0x000E
#define    I2O_LAN_DSC_DESTINATION_ADDRESS_OMITTED     0x000F
#define    I2O_LAN_DSC_PARTIAL_PACKET_RETURNED         0x0010

/*
** Packet Description Block   (Received packets)
**
** A pointer to this block structure is returned to the ReceiveCallback 
** function.  It contains the list of packet buffers which have either been
** filled with a packet or returned to host due to a LANReset function. 
** Currently there will only be one packet per receive bucket (buffer) posted. 
**
**   32   24               0     
**  +-----------------------+  -\
**  |   Buffer 1 Context    |    \
**  +-----------------------+     \
**  |      0xC0000000       |     / First Bucket Descriptor
**  +-----+-----------------+    /
**  |  0  | packet 1 length |   / 
**  +-----------------------+  -\
**  |   Buffer 2 Context    |    \
**  +-----------------------+     \
**  |      0xC0000000       |     / Second Bucket Descriptor
**  +-----+-----------------+    /
**  |  0  | packet 2 length |   / 
**  +-----+-----------------+  -
**  |         ...           |  ----- more bucket descriptors
**  +-----------------------+  -\
**  |   Buffer n Context    |    \
**  +-----------------------+     \
**  |      0xC0000000       |     / Last Bucket Descriptor
**  +-----+-----------------+    /
**  |  0  | packet n length |   / 
**  +-----+-----------------+  -
**
** Buffer Context values are those given to adapter in the TCB on calls to
** RCPostRecvBuffers().
**  
*/

/*
** Transaction Control Block (TCB) structure
**
** A structure like this is filled in by the user and passed by reference to 
** RCI2OSendPacket() and RCPostRecvBuffers() functions.  Minimum size is five
** 32-bit words for one buffer with one segment descriptor.  
** MAX_NMBR_POST_BUFFERS_PER_MSG defines the maximum single segment buffers
** that can be described in a given TCB.
**
**   32                    0
**  +-----------------------+
**  |   Buffer Count        |  Number of buffers in the TCB
**  +-----------------------+
**  |   Buffer 1 Context    |  first buffer reference
**  +-----------------------+
**  |   Buffer 1 Seg Count  |  number of segments in buffer
**  +-----------------------+
**  |   Buffer 1 Seg Desc 1 |  first segment descriptor (size, physical address)
**  +-----------------------+
**  |         ...           |  more segment descriptors (size, physical address)
**  +-----------------------+
**  |   Buffer 1 Seg Desc n |  last segment descriptor (size, physical address)
**  +-----------------------+
**  |   Buffer 2 Context    |  second buffer reference
**  +-----------------------+
**  |   Buffer 2 Seg Count  |  number of segments in buffer
**  +-----------------------+
**  |   Buffer 2 Seg Desc 1 |  segment descriptor (size, physical address)
**  +-----------------------+
**  |         ...           |  more segment descriptors (size, physical address)
**  +-----------------------+
**  |   Buffer 2 Seg Desc n |
**  +-----------------------+
**  |         ...           |  more buffer descriptor blocks ...
**  +-----------------------+
**  |   Buffer n Context    |
**  +-----------------------+
**  |   Buffer n Seg Count  |
**  +-----------------------+
**  |   Buffer n Seg Desc 1 |
**  +-----------------------+
**  |         ...           |
**  +-----------------------+
**  |   Buffer n Seg Desc n |
**  +-----------------------+
**
**
** A TCB for one contigous packet buffer would look like the following:
**
**   32                    0
**  +-----------------------+
**  |         1             |  one buffer in the TCB
**  +-----------------------+
**  |  <user's Context>     |  user's buffer reference
**  +-----------------------+
**  |         1             |  one segment buffer
**  +-----------------------+                            _
**  |    <buffer size>      |  size                       \ 
**  +-----------------------+                              \ segment descriptor
**  |  <physical address>   |  physical address of buffer  /
**  +-----------------------+                            _/
**
*/

 /* Buffer Segment Descriptor */
typedef struct {
	U32 size;
	U32 phyAddress;
} BSD, *PBSD;

typedef PU32 PRCTCB;
/*
** -------------------------------------------------------------------------
** Exported functions comprising the API to the LAN I2O message transport layer
** -------------------------------------------------------------------------
*/

 /*
    ** InitRCI2OMsgLayer()
    ** 
    ** Called once prior to using the I2O LAN message transport layer.  User 
    ** provides both the physical and virual address of a locked page buffer 
    ** that is used as a private buffer for the RedCreek I2O message
    ** transport layer.  This buffer must be a contigous memory block of a 
    ** minimum of 16K bytes and long word aligned.  The user also must provide
    ** the base address of the RedCreek PCI adapter assigned by BIOS or operating
    ** system.  
    **
    ** Inputs:  dev - the net_device struct for the device.
    **          TransmitCallbackFunction - address of user's TX callback function
    **          ReceiveCallbackFunction  - address of user's RX callback function
    **          RebootCallbackFunction  - address of user's reboot callback function
    **
  */
RC_RETURN RCInitI2OMsgLayer (struct net_device *dev,
			     PFNTXCALLBACK TransmitCallbackFunction,
			     PFNRXCALLBACK ReceiveCallbackFunction,
			     PFNCALLBACK RebootCallbackFunction);

 /*
    ** RCSetRavlinIPandMask()
    **
    ** Set the Ravlin 45/PCI cards IP address and network mask.
    **
    ** IP address and mask must be in network byte order.
    ** For example, IP address 1.2.3.4 and mask 255.255.255.0 would be
    ** 0x04030201 and 0x00FFFFFF on a little endian machine.
    **
  */
RC_RETURN RCSetRavlinIPandMask (struct net_device *dev, U32 ipAddr,
				U32 netMask);

/*
** =========================================================================
** RCGetRavlinIPandMask()
**
** get the IP address and MASK from the card
** 
** =========================================================================
*/
RC_RETURN
RCGetRavlinIPandMask (struct net_device *dev, PU32 pIpAddr, PU32 pNetMask,
		      PFNWAITCALLBACK WaitCallback);

 /* 
    ** RCProcI2OMsgQ()
    ** 
    ** Called from user's polling loop or Interrupt Service Routine for a PCI 
    ** interrupt from the RedCreek PCI adapter.  User responsible for determining
    ** and hooking the PCI interrupt. This function will call the registered
    ** callback functions, TransmitCallbackFunction or ReceiveCallbackFunction,
    ** if a TX or RX transaction has completed.
  */
void RCProcI2OMsgQ (struct net_device *dev);

 /*
    ** Disable and Enable I2O interrupts.  I2O interrupts are enabled at Init time
    ** but can be disabled and re-enabled through these two function calls.
    ** Packets will still be put into any posted received buffers and packets will
    ** be sent through RCI2OSendPacket() functions.  Disabling I2O interrupts
    ** will prevent hardware interrupt to host even though the outbound I2O msg
    ** queue is not emtpy.
  */
RC_RETURN RCEnableI2OInterrupts (struct net_device *dev);
RC_RETURN RCDisableI2OInterrupts (struct net_device *dev);

 /* 
    ** RCPostRecvBuffers()
    ** 
    ** Post user's page locked buffers for use by the PCI adapter to
    ** return ethernet packets received from the LAN.  Transaction Control Block,
    ** provided by user, contains buffer descriptor(s) which includes a buffer
    ** context number along with buffer size and physical address.  See TCB above.
    ** The buffer context and actual packet length are returned to the 
    ** ReceiveCallbackFunction when packets have been received.  Buffers posted
    ** to the RedCreek adapter are considered owned by the adapter until the
    ** context is return to user through the ReceiveCallbackFunction.
  */
RC_RETURN RCPostRecvBuffers (struct net_device *dev,
			     PRCTCB pTransactionCtrlBlock);
#define MAX_NMBR_POST_BUFFERS_PER_MSG 32

 /*
    ** RCI2OSendPacket()
    ** 
    ** Send user's ethernet packet from a locked page buffer.  
    ** Packet must have full MAC header, however without a CRC.  
    ** Initiator context is a user provided value that is returned 
    ** to the TransmitCallbackFunction when packet buffer is free.
    ** Transmit buffer are considered owned by the adapter until context's
    ** returned to user through the TransmitCallbackFunction.
  */
RC_RETURN RCI2OSendPacket (struct net_device *dev,
			   U32 context, PRCTCB pTransactionCtrlBlock);

 /* Ethernet Link Statistics structure */
typedef struct tag_RC_link_stats {
	U32 TX_good;		/* good transmit frames */
	U32 TX_maxcol;		/* frames not TX due to MAX collisions */
	U32 TX_latecol;		/* frames not TX due to late collisions */
	U32 TX_urun;		/* frames not TX due to DMA underrun */
	U32 TX_crs;		/* frames TX with lost carrier sense */
	U32 TX_def;		/* frames deferred due to activity on link */
	U32 TX_singlecol;	/* frames TX with one and only on collision */
	U32 TX_multcol;		/* frames TX with more than one collision */
	U32 TX_totcol;		/* total collisions detected during TX */
	U32 Rcv_good;		/* good frames received */
	U32 Rcv_CRCerr;		/* frames RX and discarded with CRC errors */
	U32 Rcv_alignerr;	/* frames RX with alignment and CRC errors */
	U32 Rcv_reserr;		/* good frames discarded due to no RX buffer */
	U32 Rcv_orun;		/* RX frames lost due to FIFO overrun */
	U32 Rcv_cdt;		/* RX frames with collision during RX */
	U32 Rcv_runt;		/* RX frames shorter than 64 bytes */
} RCLINKSTATS, *P_RCLINKSTATS;

 /*
    ** RCGetLinkStatistics()
    **
    ** Returns link statistics in user's structure at address StatsReturnAddr
    ** If given, not NULL, the function WaitCallback is called during the wait
    ** loop while waiting for the adapter to respond.
  */
RC_RETURN RCGetLinkStatistics (struct net_device *dev,
			       P_RCLINKSTATS StatsReturnAddr,
			       PFNWAITCALLBACK WaitCallback);

 /*
    ** RCGetLinkStatus()
    **
    ** Return link status, up or down, to user's location addressed by ReturnAddr.
    ** If given, not NULL, the function WaitCallback is called during the wait
    ** loop while waiting for the adapter to respond.
  */
RC_RETURN RCGetLinkStatus (struct net_device *dev,
			   PU32 pReturnStatus, PFNWAITCALLBACK WaitCallback);

 /* Link Status defines - value returned in pReturnStatus */
#define RC_LAN_LINK_STATUS_DOWN     0
#define RC_LAN_LINK_STATUS_UP       1

 /*
    ** RCGetMAC()
    **
    ** Get the current MAC address assigned to user.  RedCreek Ravlin 45/PCI 
    ** has two MAC addresses.  One which is private to the PCI Card, and 
    ** another MAC which is given to the user as its link layer MAC address. The
    ** adapter runs in promiscous mode because of the dual address requirement.
    ** The MAC address is returned to the unsigned char array pointer to by mac.
  */
RC_RETURN RCGetMAC (struct net_device *dev, PFNWAITCALLBACK WaitCallback);

 /*
    ** RCSetMAC()
    **
    ** Set a new user port MAC address.  This address will be returned on
    ** subsequent RCGetMAC() calls.
  */
RC_RETURN RCSetMAC (struct net_device *dev, PU8 mac);

 /*
    ** RCSetLinkSpeed()
    **
    ** set adapter's link speed based on given input code.
  */
RC_RETURN RCSetLinkSpeed (struct net_device *dev, U16 LinkSpeedCode);
 /* Set link speed codes */
#define LNK_SPD_AUTO_NEG_NWAY   0
#define LNK_SPD_100MB_FULL      1
#define LNK_SPD_100MB_HALF      2
#define LNK_SPD_10MB_FULL       3
#define LNK_SPD_10MB_HALF       4

 /*
    ** RCGetLinkSpeed()
    **
    ** Return link speed code.
  */
 /* Return link speed codes */
#define LNK_SPD_UNKNOWN         0
#define LNK_SPD_100MB_FULL      1
#define LNK_SPD_100MB_HALF      2
#define LNK_SPD_10MB_FULL       3
#define LNK_SPD_10MB_HALF       4

RC_RETURN
RCGetLinkSpeed (struct net_device *dev, PU32 pLinkSpeedCode,
		PFNWAITCALLBACK WaitCallback);
/*
** =========================================================================
** RCSetPromiscuousMode(struct net_device *dev, U16 Mode)
**
** Defined values for Mode:
**  0 - turn off promiscuous mode
**  1 - turn on  promiscuous mode
**
** =========================================================================
*/
#define PROMISCUOUS_MODE_OFF 0
#define PROMISCUOUS_MODE_ON  1
RC_RETURN RCSetPromiscuousMode (struct net_device *dev, U16 Mode);
/*
** =========================================================================
** RCGetPromiscuousMode(struct net_device *dev, PU32 pMode, PFNWAITCALLBACK WaitCallback)
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
RCGetPromiscuousMode (struct net_device *dev, PU32 pMode,
		      PFNWAITCALLBACK WaitCallback);

/*
** =========================================================================
** RCSetBroadcastMode(struct net_device *dev, U16 Mode)
**
** Defined values for Mode:
**  0 - turn off promiscuous mode
**  1 - turn on  promiscuous mode
**
** =========================================================================
*/
#define BROADCAST_MODE_OFF 0
#define BROADCAST_MODE_ON  1
RC_RETURN RCSetBroadcastMode (struct net_device *dev, U16 Mode);
/*
** =========================================================================
** RCGetBroadcastMode(struct net_device *dev, PU32 pMode, PFNWAITCALLBACK WaitCallback)
**
** get broadcast mode setting
**
** Possible return values placed in pMode:
**  0 = broadcast mode not set
**  1 = broadcast mode is set
**
** =========================================================================
*/
RC_RETURN
RCGetBroadcastMode (struct net_device *dev, PU32 pMode,
		    PFNWAITCALLBACK WaitCallback);
/*
** =========================================================================
** RCReportDriverCapability(struct net_device *dev, U32 capability)
**
** Currently defined bits:
** WARM_REBOOT_CAPABLE   0x01
**
** =========================================================================
*/
RC_RETURN RCReportDriverCapability (struct net_device *dev, U32 capability);

/*
** RCGetFirmwareVer()
**
** Return firmware version in the form "SoftwareVersion : Bt BootVersion"
**
** WARNING: user's space pointed to by pFirmString should be at least 60 bytes.
*/
RC_RETURN
RCGetFirmwareVer (struct net_device *dev, PU8 pFirmString,
		  PFNWAITCALLBACK WaitCallback);

/*
** ----------------------------------------------
** LAN adapter Reset and Shutdown functions
** ----------------------------------------------
*/
 /* resource flag bit assignments for RCResetLANCard() & RCShutdownLANCard() */
#define RC_RESOURCE_RETURN_POSTED_RX_BUCKETS  0x0001
#define RC_RESOURCE_RETURN_PEND_TX_BUFFERS    0x0002

 /*
    ** RCResetLANCard()
    **
    ** Reset LAN card operation.  Causes a software reset of the ethernet
    ** controller and restarts the command and receive units. Depending on 
    ** the ResourceFlags given, the buffers are either returned to the
    ** host with reply status of I2O_REPLY_STATUS_ABORT_NO_DATA_TRANSFER and
    ** detailed status of I2O_LAN_DSC_CANCELED (new receive buffers must be
    ** posted after issuing this) OR the buffers are kept and reused by
    ** the ethernet controller. If CallbackFunction is not NULL, the function
    ** will be called when the reset is complete.  If the CallbackFunction is
    ** NULL,a 1 will be put into the ReturnAddr after waiting for the reset 
    ** to complete (please disable I2O interrupts during this method).
    ** Any outstanding transmit or receive buffers that are complete will be
    ** returned via the normal reply messages before the requested resource
    ** buffers are returned.
    ** A call to RCPostRecvBuffers() is needed to return the ethernet to full
    ** operation if the receive buffers were returned during LANReset.
    ** Note: The IOP status is not affected by a LAN reset.
  */
RC_RETURN RCResetLANCard (struct net_device *dev, U16 ResourceFlags,
			  PU32 ReturnAddr, PFNCALLBACK CallbackFunction);

 /*
    ** RCShutdownLANCard()
    **
    ** Shutdown LAN card operation and put into an idle (suspended) state.
    ** The LAN card is restarted with RCResetLANCard() function.
    ** Depending on the ResourceFlags given, the buffers are either returned 
    ** to the host with reply status of I2O_REPLY_STATUS_ABORT_NO_DATA_TRANSFER 
    ** and detailed status of I2O_LAN_DSC_CANCELED (new receive buffers must be
    ** posted after issuing this) OR the buffers are kept and reused by
    ** the ethernet controller. If CallbackFunction is not NULL, the function
    ** will be called when the reset is complete.  If the CallbackFunction is
    ** NULL,a 1 will be put into the ReturnAddr after waiting for the reset 
    ** to complete (please disable I2O interrupts during this method).
    ** Any outstanding transmit or receive buffers that are complete will be
    ** returned via the normal reply messages before the requested resource
    ** buffers are returned.
    ** Note: The IOP status is not affected by a LAN shutdown.
  */
RC_RETURN
RCShutdownLANCard (struct net_device *dev, U16 ResourceFlags, PU32 ReturnAddr,
		   PFNCALLBACK CallbackFunction);

 /*
    ** RCResetIOP();
    **     Initializes IOPState to I2O_IOP_STATE_RESET.
    **     Stops access to outbound message Q.
    **     Discards any outstanding transmit or posted receive buffers.
    **     Clears outbound message Q. 
  */
RC_RETURN RCResetIOP (struct net_device *dev);

#endif				/* RCLANMTL_H */
