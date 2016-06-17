/*
  * mf.c
  * Copyright (C) 2001 Troy D. Armstrong  IBM Corporation
  *
  * This modules exists as an interface between a Linux secondary partition
  * running on an iSeries and the primary partition's Virtual Service
  * Processor (VSP) object.  The VSP has final authority over powering on/off
  * all partitions in the iSeries.  It also provides miscellaneous low-level
  * machine facility type operations.
  *
  * 
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  * 
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  * 
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
  */

#include <asm/iSeries/mf.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <asm/iSeries/HvLpConfig.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/nvram.h>
#include <asm/time.h>
#include <asm/iSeries/ItSpCommArea.h>
#include <asm/iSeries/mf_proc.h>
#include <asm/iSeries/iSeries_proc.h>
#include <asm/uaccess.h>
#include <linux/pci.h>

extern struct pci_dev * iSeries_vio_dev;

/*
 * This is the structure layout for the Machine Facilities LPAR event
 * flows.
 */
struct VspCmdData;
struct CeMsgData;
union SafeCast
{
	u64 ptrAsU64;
	void *ptr;
};


typedef void (*CeMsgCompleteHandler)( void *token, struct CeMsgData *vspCmdRsp );

struct CeMsgCompleteData
{
	CeMsgCompleteHandler xHdlr;
	void *xToken;
};

struct VspRspData
{
	struct semaphore *xSemaphore;
	struct VspCmdData *xResponse;
};

struct IoMFLpEvent
{
	struct HvLpEvent xHvLpEvent;

	u16 xSubtypeRc;
	u16 xRsvd1;
	u32 xRsvd2;

	union
	{

		struct AllocData
		{
			u16 xSize;
			u16 xType;
			u32 xCount;
			u16 xRsvd3;
			u8 xRsvd4;
			HvLpIndex xTargetLp;
		} xAllocData;

		struct CeMsgData
		{
			u8 xCEMsg[12];
			char xReserved[4];
			struct CeMsgCompleteData *xToken;
		} xCEMsgData;

		struct VspCmdData
		{
			union SafeCast xTokenUnion;
			u16 xCmd;
			HvLpIndex xLpIndex;
			u8 xRc;
			u32 xReserved1;

			union VspCmdSubData
			{
				struct
				{
					u64 xState;
				} xGetStateOut;

				struct
				{
					u64 xIplType;
				} xGetIplTypeOut, xFunction02SelectIplTypeIn;

				struct
				{
					u64 xIplMode;
				} xGetIplModeOut, xFunction02SelectIplModeIn;

				struct
				{
					u64 xPage[4];
				} xGetSrcHistoryIn;

				struct
				{
					u64 xFlag;
				} xGetAutoIplWhenPrimaryIplsOut,
					xSetAutoIplWhenPrimaryIplsIn,
					xWhiteButtonPowerOffIn,
					xFunction08FastPowerOffIn,
					xIsSpcnRackPowerIncompleteOut;

				struct
				{
					u64 xToken;
					u64 xAddressType;
					u64 xSide;
					u32 xTransferLength;
					u32 xOffset;
				} xSetKernelImageIn,
					xGetKernelImageIn,
					xSetKernelCmdLineIn,
					xGetKernelCmdLineIn;

				struct
				{
					u32 xTransferLength;
				} xGetKernelImageOut,xGetKernelCmdLineOut;


				u8 xReserved2[80];

			} xSubData;
		} xVspCmd;
	} xUnion;
};


/*
 * All outgoing event traffic is kept on a FIFO queue.  The first
 * pointer points to the one that is outstanding, and all new
 * requests get stuck on the end.  Also, we keep a certain number of
 * preallocated stack elements so that we can operate very early in
 * the boot up sequence (before kmalloc is ready).
 */
struct StackElement
{
	struct StackElement * next;
	struct IoMFLpEvent event;
	MFCompleteHandler hdlr;
	char dmaData[72];
	unsigned dmaDataLength;
	unsigned remoteAddress;
};
static spinlock_t spinlock;
static struct StackElement * head = NULL;
static struct StackElement * tail = NULL;
static struct StackElement * avail = NULL;
static struct StackElement prealloc[16];

/*
 * Put a stack element onto the available queue, so it can get reused.
 * Attention! You must have the spinlock before calling!
 */
void free( struct StackElement * element )
{
	if ( element != NULL )
	{
		element->next = avail;
		avail = element;
	}
}

/*
 * Enqueue the outbound event onto the stack.  If the queue was
 * empty to begin with, we must also issue it via the Hypervisor
 * interface.  There is a section of code below that will touch
 * the first stack pointer without the protection of the spinlock.
 * This is OK, because we know that nobody else will be modifying
 * the first pointer when we do this.
 */
static int signalEvent( struct StackElement * newElement )
{
	int rc = 0;
	unsigned long flags;
	int go = 1;
	struct StackElement * element;
	HvLpEvent_Rc hvRc;

	/* enqueue the event */
	if ( newElement != NULL )
	{
		spin_lock_irqsave( &spinlock, flags );
		if ( head == NULL )
			head = newElement;
		else {
			go = 0;
			tail->next = newElement;
		}
		newElement->next = NULL;
		tail = newElement;
		spin_unlock_irqrestore( &spinlock, flags );
	}

	/* send the event */
	while ( go )
	{
		go = 0;

		/* any DMA data to send beforehand? */
		if ( head->dmaDataLength > 0 )
			HvCallEvent_dmaToSp( head->dmaData, head->remoteAddress, head->dmaDataLength, HvLpDma_Direction_LocalToRemote );

		hvRc = HvCallEvent_signalLpEvent(&head->event.xHvLpEvent);
		if ( hvRc != HvLpEvent_Rc_Good )
		{
			printk( KERN_ERR "mf.c: HvCallEvent_signalLpEvent() failed with %d\n", (int)hvRc );

			spin_lock_irqsave( &spinlock, flags );
			element = head;
			head = head->next;
			if ( head != NULL )
				go = 1;
			spin_unlock_irqrestore( &spinlock, flags );

			if ( element == newElement )
				rc = -EIO;
			else {
				if ( element->hdlr != NULL )
				{
					union SafeCast mySafeCast;
					mySafeCast.ptrAsU64 = element->event.xHvLpEvent.xCorrelationToken;
					(*element->hdlr)( mySafeCast.ptr, -EIO );
				}
			}

			spin_lock_irqsave( &spinlock, flags );
			free( element );
			spin_unlock_irqrestore( &spinlock, flags );
		}
	}

	return rc;
}

/*
 * Allocate a new StackElement structure, and initialize it.
 */
static struct StackElement * newStackElement( void )
{
	struct StackElement * newElement = NULL;
	HvLpIndex primaryLp = HvLpConfig_getPrimaryLpIndex();
	unsigned long flags;

	if ( newElement == NULL )
	{
		spin_lock_irqsave( &spinlock, flags );
		if ( avail != NULL )
		{
			newElement = avail;
			avail = avail->next;
		}
		spin_unlock_irqrestore( &spinlock, flags );
	}

	if ( newElement == NULL )
		newElement = kmalloc(sizeof(struct StackElement),GFP_ATOMIC);

	if ( newElement == NULL )
	{
		printk( KERN_ERR "mf.c: unable to kmalloc %ld bytes\n", sizeof(struct StackElement) );
		return NULL;
	}

	memset( newElement, 0, sizeof(struct StackElement) );
	newElement->event.xHvLpEvent.xFlags.xValid = 1;
	newElement->event.xHvLpEvent.xFlags.xAckType = HvLpEvent_AckType_ImmediateAck;
	newElement->event.xHvLpEvent.xFlags.xAckInd = HvLpEvent_AckInd_DoAck;
	newElement->event.xHvLpEvent.xFlags.xFunction = HvLpEvent_Function_Int;
	newElement->event.xHvLpEvent.xType = HvLpEvent_Type_MachineFac;
	newElement->event.xHvLpEvent.xSourceLp = HvLpConfig_getLpIndex();
	newElement->event.xHvLpEvent.xTargetLp = primaryLp;
	newElement->event.xHvLpEvent.xSizeMinus1 = sizeof(newElement->event)-1;
	newElement->event.xHvLpEvent.xRc = HvLpEvent_Rc_Good;
	newElement->event.xHvLpEvent.xSourceInstanceId = HvCallEvent_getSourceLpInstanceId(primaryLp,HvLpEvent_Type_MachineFac);
	newElement->event.xHvLpEvent.xTargetInstanceId = HvCallEvent_getTargetLpInstanceId(primaryLp,HvLpEvent_Type_MachineFac);

	return newElement;
}

static int signalVspInstruction( struct VspCmdData *vspCmd )
{
	struct StackElement * newElement = newStackElement();
	int rc = 0;
	struct VspRspData response;
	DECLARE_MUTEX_LOCKED(Semaphore);
	response.xSemaphore = &Semaphore;
	response.xResponse = vspCmd;

	if ( newElement == NULL )
		rc = -ENOMEM;
	else {
		newElement->event.xHvLpEvent.xSubtype = 6;
		newElement->event.xHvLpEvent.x.xSubtypeData = ('M'<<24)+('F'<<16)+('V'<<8)+('I'<<0);
		newElement->event.xUnion.xVspCmd.xTokenUnion.ptr = &response;
		newElement->event.xUnion.xVspCmd.xCmd = vspCmd->xCmd;
		newElement->event.xUnion.xVspCmd.xLpIndex = HvLpConfig_getLpIndex();
		newElement->event.xUnion.xVspCmd.xRc = 0xFF;
		newElement->event.xUnion.xVspCmd.xReserved1 = 0;
		memcpy(&(newElement->event.xUnion.xVspCmd.xSubData),&(vspCmd->xSubData), sizeof(vspCmd->xSubData));
		mb();

		rc = signalEvent(newElement);
	}

	if (rc == 0)
	{
		down(&Semaphore);
	}

	return rc;
}


/*
 * Send a 12-byte CE message to the primary partition VSP object
 */
static int signalCEMsg( char * ceMsg, void * token )
{
	struct StackElement * newElement = newStackElement();
	int rc = 0;

	if ( newElement == NULL )
		rc = -ENOMEM;
	else {
		newElement->event.xHvLpEvent.xSubtype = 0;
		newElement->event.xHvLpEvent.x.xSubtypeData = ('M'<<24)+('F'<<16)+('C'<<8)+('E'<<0);
		memcpy( newElement->event.xUnion.xCEMsgData.xCEMsg, ceMsg, 12 );
		newElement->event.xUnion.xCEMsgData.xToken = token;
		rc = signalEvent(newElement);
	}

	return rc;
}

/*
 * Send a 12-byte CE message and DMA data to the primary partition VSP object
 */
static int dmaAndSignalCEMsg( char * ceMsg, void * token, void * dmaData, unsigned dmaDataLength, unsigned remoteAddress )
{
	struct StackElement * newElement = newStackElement();
	int rc = 0;

	if ( newElement == NULL )
		rc = -ENOMEM;
	else {
		newElement->event.xHvLpEvent.xSubtype = 0;
		newElement->event.xHvLpEvent.x.xSubtypeData = ('M'<<24)+('F'<<16)+('C'<<8)+('E'<<0);
		memcpy( newElement->event.xUnion.xCEMsgData.xCEMsg, ceMsg, 12 );
		newElement->event.xUnion.xCEMsgData.xToken = token;
		memcpy( newElement->dmaData, dmaData, dmaDataLength );
		newElement->dmaDataLength = dmaDataLength;
		newElement->remoteAddress = remoteAddress;
		rc = signalEvent(newElement);
	}

	return rc;
}

/*
 * Initiate a nice (hopefully) shutdown of Linux.  We simply are
 * going to try and send the init process a SIGINT signal.  If
 * this fails (why?), we'll simply force it off in a not-so-nice
 * manner.
 */
static int shutdown( void )
{
	extern int cad_pid; /* from kernel/sys.c */
	int rc = kill_proc(cad_pid,SIGINT,1);

	if ( rc )
	{
		printk( KERN_ALERT "mf.c: SIGINT to init failed (%d), hard shutdown commencing\n", rc );
		mf_powerOff();
	}
	else
		printk( KERN_INFO "mf.c: init has been successfully notified to proceed with shutdown\n" );

	return rc;
}

/*
 * The primary partition VSP object is sending us a new
 * event flow.  Handle it...
 */
static void intReceived( struct IoMFLpEvent * event )
{
	int freeIt = 0;
	struct StackElement * two = NULL;
	/* ack the interrupt */
	event->xHvLpEvent.xRc = HvLpEvent_Rc_Good;
	HvCallEvent_ackLpEvent( &event->xHvLpEvent );

    /* process interrupt */
	switch( event->xHvLpEvent.xSubtype )
	{
	case 0:	/* CE message */
		switch( event->xUnion.xCEMsgData.xCEMsg[3] )
		{
		case 0x5B:	/* power control notification */
			if ( (event->xUnion.xCEMsgData.xCEMsg[5]&0x20) != 0 )
			{
				printk( KERN_INFO "mf.c: Commencing partition shutdown\n" );
				if ( shutdown() == 0 )
					signalCEMsg( "\x00\x00\x00\xDB\x00\x00\x00\x00\x00\x00\x00\x00", NULL );
			}
			break;
		case 0xC0:	/* get time */
			{
				if ( (head != NULL) && ( head->event.xUnion.xCEMsgData.xCEMsg[3] == 0x40 ) )
				{
					freeIt = 1;
					if ( head->event.xUnion.xCEMsgData.xToken != 0 )
					{
						CeMsgCompleteHandler xHdlr = head->event.xUnion.xCEMsgData.xToken->xHdlr;
						void * token = head->event.xUnion.xCEMsgData.xToken->xToken;

						if (xHdlr != NULL)
							(*xHdlr)( token, &(event->xUnion.xCEMsgData) );
					}
				}
			}
			break;
		}

		/* remove from queue */
		if ( freeIt == 1 )
		{
			unsigned long flags;
			spin_lock_irqsave( &spinlock, flags );
			if ( head != NULL )
			{
				struct StackElement *oldHead = head;
				head = head->next;
				two = head;
				free( oldHead );
			}
			spin_unlock_irqrestore( &spinlock, flags );
		}

		/* send next waiting event */
		if ( two != NULL )
			signalEvent( NULL );
		break;
	case 1:	/* IT sys shutdown */
		printk( KERN_INFO "mf.c: Commencing system shutdown\n" );
		shutdown();
		break;
	}
}

/*
 * The primary partition VSP object is acknowledging the receipt
 * of a flow we sent to them.  If there are other flows queued
 * up, we must send another one now...
 */
static void ackReceived( struct IoMFLpEvent * event )
{
	unsigned long flags;
	struct StackElement * two = NULL;
	unsigned long freeIt = 0;

    /* handle current event */
	if ( head != NULL )
	{
		switch( event->xHvLpEvent.xSubtype )
		{
		case 0:     /* CE msg */
			if ( event->xUnion.xCEMsgData.xCEMsg[3] == 0x40 )
			{
				if ( event->xUnion.xCEMsgData.xCEMsg[2] != 0 )
				{
					freeIt = 1;
					if ( head->event.xUnion.xCEMsgData.xToken != 0 )
					{
						CeMsgCompleteHandler xHdlr = head->event.xUnion.xCEMsgData.xToken->xHdlr;
						void * token = head->event.xUnion.xCEMsgData.xToken->xToken;

						if (xHdlr != NULL)
							(*xHdlr)( token, &(event->xUnion.xCEMsgData) );
					}
				}
			} else {
				freeIt = 1;
			}
			break;
		case 4:	/* allocate */
		case 5:	/* deallocate */
			if ( head->hdlr != NULL )
			{
				union SafeCast mySafeCast;
				mySafeCast.ptrAsU64 = event->xHvLpEvent.xCorrelationToken;
				(*head->hdlr)( mySafeCast.ptr, event->xUnion.xAllocData.xCount );
			}
			freeIt = 1;
			break;
		case 6:
			{
				struct VspRspData *rsp = (struct VspRspData *)event->xUnion.xVspCmd.xTokenUnion.ptr;

				if (rsp != NULL)
				{
					if (rsp->xResponse != NULL)
						memcpy(rsp->xResponse, &(event->xUnion.xVspCmd), sizeof(event->xUnion.xVspCmd));
					if (rsp->xSemaphore != NULL)
						up(rsp->xSemaphore);
				} else {
					printk( KERN_ERR "mf.c: no rsp\n");
				}
				freeIt = 1;
			}
			break;
		}
	}
	else
		printk( KERN_ERR "mf.c: stack empty for receiving ack\n" );

    /* remove from queue */
	spin_lock_irqsave( &spinlock, flags );
	if (( head != NULL ) && ( freeIt == 1 ))
	{
		struct StackElement *oldHead = head;
		head = head->next;
		two = head;
		free( oldHead );
	} 
	spin_unlock_irqrestore( &spinlock, flags );

    /* send next waiting event */
	if ( two != NULL )
		signalEvent( NULL );
}

/*
 * This is the generic event handler we are registering with
 * the Hypervisor.  Ensure the flows are for us, and then
 * parse it enough to know if it is an interrupt or an
 * acknowledge.
 */
static void hvHandler( struct HvLpEvent * event, struct pt_regs * regs )
{
	if ( (event != NULL) && (event->xType == HvLpEvent_Type_MachineFac) )
	{
		switch( event->xFlags.xFunction )
		{
		case HvLpEvent_Function_Ack:
			ackReceived( (struct IoMFLpEvent *)event );
			break;
		case HvLpEvent_Function_Int:
			intReceived( (struct IoMFLpEvent *)event );
			break;
		default:
			printk( KERN_ERR "mf.c: non ack/int event received\n" );
			break;
		}
	}
	else
		printk( KERN_ERR "mf.c: alien event received\n" );
}

/*
 * Global kernel interface to allocate and seed events into the
 * Hypervisor.
 */
void mf_allocateLpEvents( HvLpIndex targetLp,
			  HvLpEvent_Type type,
			  unsigned size,
			  unsigned count,
			  MFCompleteHandler hdlr,
			  void * userToken )
{
	struct StackElement * newElement = newStackElement();
	int rc = 0;

	if ( newElement == NULL )
		rc = -ENOMEM;
	else {
		union SafeCast mine;
		mine.ptr = userToken;
		newElement->event.xHvLpEvent.xSubtype = 4;
		newElement->event.xHvLpEvent.xCorrelationToken = mine.ptrAsU64;
		newElement->event.xHvLpEvent.x.xSubtypeData = ('M'<<24)+('F'<<16)+('M'<<8)+('A'<<0);
		newElement->event.xUnion.xAllocData.xTargetLp = targetLp;
		newElement->event.xUnion.xAllocData.xType = type;
		newElement->event.xUnion.xAllocData.xSize = size;
		newElement->event.xUnion.xAllocData.xCount = count;
		newElement->hdlr = hdlr;
		rc = signalEvent(newElement);
	}

	if ( (rc != 0) && (hdlr != NULL) )
		(*hdlr)( userToken, rc );
}

/*
 * Global kernel interface to unseed and deallocate events already in
 * Hypervisor.
 */
void mf_deallocateLpEvents( HvLpIndex targetLp,
			    HvLpEvent_Type type,
			    unsigned count,
			    MFCompleteHandler hdlr,
			    void * userToken )
{
	struct StackElement * newElement = newStackElement();
	int rc = 0;

	if ( newElement == NULL )
		rc = -ENOMEM;
	else {
		union SafeCast mine;
		mine.ptr = userToken;
		newElement->event.xHvLpEvent.xSubtype = 5;
		newElement->event.xHvLpEvent.xCorrelationToken = mine.ptrAsU64;
		newElement->event.xHvLpEvent.x.xSubtypeData = ('M'<<24)+('F'<<16)+('M'<<8)+('D'<<0);
		newElement->event.xUnion.xAllocData.xTargetLp = targetLp;
		newElement->event.xUnion.xAllocData.xType = type;
		newElement->event.xUnion.xAllocData.xCount = count;
		newElement->hdlr = hdlr;
		rc = signalEvent(newElement);
	}

	if ( (rc != 0) && (hdlr != NULL) )
		(*hdlr)( userToken, rc );
}

/*
 * Global kernel interface to tell the VSP object in the primary
 * partition to power this partition off.
 */
void mf_powerOff( void )
{
	printk( KERN_INFO "mf.c: Down it goes...\n" );
	signalCEMsg( "\x00\x00\x00\x4D\x00\x00\x00\x00\x00\x00\x00\x00", NULL );
	for (;;);
}

/*
 * Global kernel interface to tell the VSP object in the primary
 * partition to reboot this partition.
 */
void mf_reboot( void )
{
	printk( KERN_INFO "mf.c: Preparing to bounce...\n" );
	signalCEMsg( "\x00\x00\x00\x4E\x00\x00\x00\x00\x00\x00\x00\x00", NULL );
	for (;;);
}

/*
 * Display a single word SRC onto the VSP control panel.
 */
void mf_displaySrc( u32 word )
{
	u8 ce[12];

	memcpy( ce, "\x00\x00\x00\x4A\x00\x00\x00\x01\x00\x00\x00\x00", 12 );
	ce[8] = word>>24;
	ce[9] = word>>16;
	ce[10] = word>>8;
	ce[11] = word;
	signalCEMsg( ce, NULL );
}

/*
 * Display a single word SRC of the form "PROGXXXX" on the VSP control panel.
 */
void mf_displayProgress( u16 value )
{
	u8 ce[12];
	u8 src[72];

	memcpy( ce, "\x00\x00\x04\x4A\x00\x00\x00\x48\x00\x00\x00\x00", 12 );
	memcpy( src,
		"\x01\x00\x00\x01"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"PROGxxxx"
		"                        ",
		72 );
	src[6] = value>>8;
	src[7] = value&255;
	src[44] = "0123456789ABCDEF"[(value>>12)&15];
	src[45] = "0123456789ABCDEF"[(value>>8)&15];
	src[46] = "0123456789ABCDEF"[(value>>4)&15];
	src[47] = "0123456789ABCDEF"[value&15];
	dmaAndSignalCEMsg( ce, NULL, src, sizeof(src), 9*64*1024 );
}

/*
 * Clear the VSP control panel.  Used to "erase" an SRC that was
 * previously displayed.
 */
void mf_clearSrc( void )
{
	signalCEMsg( "\x00\x00\x00\x4B\x00\x00\x00\x00\x00\x00\x00\x00", NULL );
}

/*
 * Initialization code here.
 */
void mf_init( void )
{
	int i;

    /* initialize */
	spin_lock_init( &spinlock );
	for ( i = 0; i < sizeof(prealloc)/sizeof(*prealloc); ++i )
		free( &prealloc[i] );
	HvLpEvent_registerHandler( HvLpEvent_Type_MachineFac, &hvHandler );

	/* virtual continue ack */
	signalCEMsg( "\x00\x00\x00\x57\x00\x00\x00\x00\x00\x00\x00\x00", NULL );

	/* initialization complete */
	printk( KERN_NOTICE "mf.c: iSeries Linux LPAR Machine Facilities initialized\n" );

	iSeries_proc_callback(&mf_proc_init);
}

void mf_setSide(char side)
{
	int rc = 0;
	u64 newSide = 0;
	struct VspCmdData myVspCmd;

	memset(&myVspCmd, 0, sizeof(myVspCmd));
	if (side == 'A')
		newSide = 0;
	else if (side == 'B')
		newSide = 1;
	else if (side == 'C')
		newSide = 2; 
	else
		newSide = 3;

	myVspCmd.xSubData.xFunction02SelectIplTypeIn.xIplType = newSide;
	myVspCmd.xCmd = 10;

	rc = signalVspInstruction(&myVspCmd);
}

char mf_getSide(void)
{
	char returnValue = ' ';
	int rc = 0;
	struct VspCmdData myVspCmd;

	memset(&myVspCmd, 0, sizeof(myVspCmd));
	myVspCmd.xCmd = 2;
	myVspCmd.xSubData.xFunction02SelectIplTypeIn.xIplType = 0;
	mb();
	rc = signalVspInstruction(&myVspCmd);

	if (rc != 0)
	{
		return returnValue;
	} else {
		if (myVspCmd.xRc == 0)
		{
			if (myVspCmd.xSubData.xGetIplTypeOut.xIplType == 0)
				returnValue = 'A';
			else if (myVspCmd.xSubData.xGetIplTypeOut.xIplType == 1)
				returnValue = 'B';
			else if (myVspCmd.xSubData.xGetIplTypeOut.xIplType == 2)
				returnValue = 'C';
			else
				returnValue = 'D';
		}
	}

	return returnValue;
}

void mf_getSrcHistory(char *buffer, int size)
{
    /*    struct IplTypeReturnStuff returnStuff;
     struct StackElement * newElement = newStackElement();
     int rc = 0;
     char *pages[4];

     pages[0] = kmalloc(4096, GFP_ATOMIC);
     pages[1] = kmalloc(4096, GFP_ATOMIC);
     pages[2] = kmalloc(4096, GFP_ATOMIC);
     pages[3] = kmalloc(4096, GFP_ATOMIC);
     if (( newElement == NULL ) || (pages[0] == NULL) || (pages[1] == NULL) || (pages[2] == NULL) || (pages[3] == NULL))
     rc = -ENOMEM;
     else
     {
     returnStuff.xType = 0;
     returnStuff.xRc = 0;
     returnStuff.xDone = 0;
     newElement->event.xHvLpEvent.xSubtype = 6;
     newElement->event.xHvLpEvent.x.xSubtypeData = ('M'<<24)+('F'<<16)+('V'<<8)+('I'<<0);
     newElement->event.xUnion.xVspCmd.xEvent = &returnStuff;
     newElement->event.xUnion.xVspCmd.xCmd = 4;
     newElement->event.xUnion.xVspCmd.xLpIndex = HvLpConfig_getLpIndex();
     newElement->event.xUnion.xVspCmd.xRc = 0xFF;
     newElement->event.xUnion.xVspCmd.xReserved1 = 0;
     newElement->event.xUnion.xVspCmd.xSubData.xGetSrcHistoryIn.xPage[0] = (0x8000000000000000ULL | virt_to_absolute((unsigned long)pages[0]));
     newElement->event.xUnion.xVspCmd.xSubData.xGetSrcHistoryIn.xPage[1] = (0x8000000000000000ULL | virt_to_absolute((unsigned long)pages[1]));
     newElement->event.xUnion.xVspCmd.xSubData.xGetSrcHistoryIn.xPage[2] = (0x8000000000000000ULL | virt_to_absolute((unsigned long)pages[2]));
     newElement->event.xUnion.xVspCmd.xSubData.xGetSrcHistoryIn.xPage[3] = (0x8000000000000000ULL | virt_to_absolute((unsigned long)pages[3]));
     mb();
     rc = signalEvent(newElement);
     }

     if (rc != 0)
     {
     return;
     }
     else
     {
     while (returnStuff.xDone != 1)
     {
     udelay(10);
     }

     if (returnStuff.xRc == 0)
     {
     memcpy(buffer, pages[0], size);
     }
     }

     kfree(pages[0]);
     kfree(pages[1]);
     kfree(pages[2]);
     kfree(pages[3]);*/
}

void mf_setCmdLine(const char *cmdline, int size, u64 side)
{
	struct VspCmdData myVspCmd;
	int rc = 0;
	dma_addr_t dma_addr = 0;
	char *page = pci_alloc_consistent(iSeries_vio_dev, size, &dma_addr);

	if (page == NULL) {
		printk(KERN_ERR "mf.c: couldn't allocate memory to set command line\n");
		return;
	}

	copy_from_user(page, cmdline, size);

	memset(&myVspCmd, 0, sizeof(myVspCmd));
	myVspCmd.xCmd = 31;
	myVspCmd.xSubData.xSetKernelCmdLineIn.xToken = dma_addr;
	myVspCmd.xSubData.xSetKernelCmdLineIn.xAddressType = HvLpDma_AddressType_TceIndex;
	myVspCmd.xSubData.xSetKernelCmdLineIn.xSide = side;
	myVspCmd.xSubData.xSetKernelCmdLineIn.xTransferLength = size;
	mb();
	rc = signalVspInstruction(&myVspCmd);

	pci_free_consistent(iSeries_vio_dev, size, page, dma_addr);
}

int mf_getCmdLine(char *cmdline, int *size, u64 side)
{
	struct VspCmdData myVspCmd;
	int rc = 0;
	int len = *size;
	dma_addr_t dma_addr = pci_map_single(iSeries_vio_dev, cmdline, *size, PCI_DMA_FROMDEVICE);

	memset(cmdline, 0, *size);
	memset(&myVspCmd, 0, sizeof(myVspCmd));
	myVspCmd.xCmd = 33;
	myVspCmd.xSubData.xGetKernelCmdLineIn.xToken = dma_addr;
	myVspCmd.xSubData.xGetKernelCmdLineIn.xAddressType = HvLpDma_AddressType_TceIndex;
	myVspCmd.xSubData.xGetKernelCmdLineIn.xSide = side;
	myVspCmd.xSubData.xGetKernelCmdLineIn.xTransferLength = *size;
	mb();
	rc = signalVspInstruction(&myVspCmd);

	if ( ! rc ) {

		if (myVspCmd.xRc == 0)
		{
			len = myVspCmd.xSubData.xGetKernelCmdLineOut.xTransferLength;
		}
		/* else
			{
			memcpy(cmdline, "Bad cmdline", 11);
			}
		*/
	}

	pci_unmap_single(iSeries_vio_dev, dma_addr, *size, PCI_DMA_FROMDEVICE);

	return len;
}


int mf_setVmlinuxChunk(const char *buffer, int size, int offset, u64 side)
{
	struct VspCmdData myVspCmd;
	int rc = 0;

	dma_addr_t dma_addr = 0;

	char *page = pci_alloc_consistent(iSeries_vio_dev, size, &dma_addr);

	if (page == NULL) {
		printk(KERN_ERR "mf.c: couldn't allocate memory to set vmlinux chunk\n");
		return -ENOMEM;
	}

	copy_from_user(page, buffer, size);
	memset(&myVspCmd, 0, sizeof(myVspCmd));

	myVspCmd.xCmd = 30;
	myVspCmd.xSubData.xGetKernelImageIn.xToken = dma_addr;
	myVspCmd.xSubData.xGetKernelImageIn.xAddressType = HvLpDma_AddressType_TceIndex;
	myVspCmd.xSubData.xGetKernelImageIn.xSide = side;
	myVspCmd.xSubData.xGetKernelImageIn.xOffset = offset;
	myVspCmd.xSubData.xGetKernelImageIn.xTransferLength = size;
	mb();
	rc = signalVspInstruction(&myVspCmd);

	if (rc == 0)
	{
		if (myVspCmd.xRc == 0)
		{
			rc = 0;
		} else {
			rc = -ENOMEM;
		}
	}

	pci_free_consistent(iSeries_vio_dev, size, page, dma_addr);

	return rc;
}

int mf_getVmlinuxChunk(char *buffer, int *size, int offset, u64 side)
{
	struct VspCmdData myVspCmd;
	int rc = 0;
	int len = *size;

	dma_addr_t dma_addr = pci_map_single(iSeries_vio_dev, buffer, *size, PCI_DMA_FROMDEVICE);

	memset(buffer, 0, len);

	memset(&myVspCmd, 0, sizeof(myVspCmd));
	myVspCmd.xCmd = 32;
	myVspCmd.xSubData.xGetKernelImageIn.xToken = dma_addr;
	myVspCmd.xSubData.xGetKernelImageIn.xAddressType = HvLpDma_AddressType_TceIndex;
	myVspCmd.xSubData.xGetKernelImageIn.xSide = side;
	myVspCmd.xSubData.xGetKernelImageIn.xOffset = offset;
	myVspCmd.xSubData.xGetKernelImageIn.xTransferLength = len;
	mb();
	rc = signalVspInstruction(&myVspCmd);

	if (rc == 0)
	{
		if (myVspCmd.xRc == 0)
		{
			*size = myVspCmd.xSubData.xGetKernelImageOut.xTransferLength;
		} else {
			rc = -ENOMEM;
		}
	}

	pci_unmap_single(iSeries_vio_dev, dma_addr, len, PCI_DMA_FROMDEVICE);

	return rc;
}

int mf_setRtcTime(unsigned long time)
{
	struct rtc_time tm;

	to_tm(time, &tm);

	return mf_setRtc( &tm );
}

struct RtcTimeData
{
	struct semaphore *xSemaphore;
	struct CeMsgData xCeMsg;
	int xRc;
};

void getRtcTimeComplete(void * token, struct CeMsgData *ceMsg)
{
	struct RtcTimeData *rtc = (struct RtcTimeData *)token;

	memcpy(&(rtc->xCeMsg), ceMsg, sizeof(rtc->xCeMsg));

	rtc->xRc = 0;
	up(rtc->xSemaphore);
}

static unsigned long lastsec = 1;

int mf_getRtcTime(unsigned long *time)
{
/*    unsigned long usec, tsec; */
	
	u32 dataWord1 = *((u32 *)(&xSpCommArea.xBcdTimeAtIplStart));
	u32 dataWord2 = *(((u32 *)&(xSpCommArea.xBcdTimeAtIplStart)) + 1);
	int year = 1970;
	int year1 = ( dataWord1 >> 24 ) & 0x000000FF;
	int year2 = ( dataWord1 >> 16 ) & 0x000000FF;
	int sec = ( dataWord1 >> 8 ) & 0x000000FF;
	int min = dataWord1 & 0x000000FF;
	int hour = ( dataWord2 >> 24 ) & 0x000000FF;
	int day = ( dataWord2 >> 8 ) & 0x000000FF;
	int mon = dataWord2 & 0x000000FF;

	BCD_TO_BIN(sec);
	BCD_TO_BIN(min);
	BCD_TO_BIN(hour);
	BCD_TO_BIN(day);
	BCD_TO_BIN(mon);
	BCD_TO_BIN(year1);
	BCD_TO_BIN(year2);
	year = year1 * 100 + year2;

	*time = mktime(year, mon, day, hour, min, sec);

	*time += ( jiffies / HZ );
    
	/* Now THIS is a nasty hack!
	 * It ensures that the first two calls to mf_getRtcTime get different
	 * answers.  That way the loop in init_time (time.c) will not think
	 * the clock is stuck.
	 */
	if ( lastsec ) {
		*time -= lastsec;
		--lastsec;
	}
    
	return 0;

}

int mf_getRtc( struct rtc_time * tm )
{

	struct CeMsgCompleteData ceComplete;
	struct RtcTimeData rtcData;
	int rc = 0;
	DECLARE_MUTEX_LOCKED(Semaphore);

	memset(&ceComplete, 0, sizeof(ceComplete));
	memset(&rtcData, 0, sizeof(rtcData));

	rtcData.xSemaphore = &Semaphore;

	ceComplete.xHdlr = &getRtcTimeComplete;
	ceComplete.xToken = (void *)&rtcData;

	rc = signalCEMsg( "\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00", &ceComplete );

	if ( rc == 0 )
	{
		down(&Semaphore);

		if ( rtcData.xRc == 0)
		{
			if ( ( rtcData.xCeMsg.xCEMsg[2] == 0xa9 ) ||
			     ( rtcData.xCeMsg.xCEMsg[2] == 0xaf ) ) {
				/* TOD clock is not set */
				tm->tm_sec = 1;
				tm->tm_min = 1;
				tm->tm_hour = 1;
				tm->tm_mday = 10;
				tm->tm_mon = 8;
				tm->tm_year = 71;
				mf_setRtc( tm );
			}
			{
				u32 dataWord1 = *((u32 *)(rtcData.xCeMsg.xCEMsg+4));
				u32 dataWord2 = *((u32 *)(rtcData.xCeMsg.xCEMsg+8));
				u8 year = (dataWord1 >> 16 ) & 0x000000FF;
				u8 sec = ( dataWord1 >> 8 ) & 0x000000FF;
				u8 min = dataWord1 & 0x000000FF;
				u8 hour = ( dataWord2 >> 24 ) & 0x000000FF;
				u8 day = ( dataWord2 >> 8 ) & 0x000000FF;
				u8 mon = dataWord2 & 0x000000FF;

				BCD_TO_BIN(sec);
				BCD_TO_BIN(min);
				BCD_TO_BIN(hour);
				BCD_TO_BIN(day);
				BCD_TO_BIN(mon);
				BCD_TO_BIN(year);

				if ( year <= 69 )
					year += 100;
	    
				tm->tm_sec = sec;
				tm->tm_min = min;
				tm->tm_hour = hour;
				tm->tm_mday = day;
				tm->tm_mon = mon;
				tm->tm_year = year;
			}
		} else {
			rc = rtcData.xRc;
			tm->tm_sec = 0;
			tm->tm_min = 0;
			tm->tm_hour = 0;
			tm->tm_mday = 15;
			tm->tm_mon = 5;
			tm->tm_year = 52;

		}
		tm->tm_wday = 0;
		tm->tm_yday = 0;
		tm->tm_isdst = 0;

	}

	return rc;

}

int mf_setRtc(struct rtc_time * tm)
{
	char ceTime[12] = "\x00\x00\x00\x41\x00\x00\x00\x00\x00\x00\x00\x00";
	int rc = 0;
	u8 day, mon, hour, min, sec, y1, y2;
	unsigned year;
    
	year = 1900 + tm->tm_year;
	y1 = year / 100;
	y2 = year % 100;
    
	sec = tm->tm_sec;
	min = tm->tm_min;
	hour = tm->tm_hour;
	day = tm->tm_mday;
	mon = tm->tm_mon + 1;
	    
	BIN_TO_BCD(sec);
	BIN_TO_BCD(min);
	BIN_TO_BCD(hour);
	BIN_TO_BCD(mon);
	BIN_TO_BCD(day);
	BIN_TO_BCD(y1);
	BIN_TO_BCD(y2);

	ceTime[4] = y1;
	ceTime[5] = y2;
	ceTime[6] = sec;
	ceTime[7] = min;
	ceTime[8] = hour;
	ceTime[10] = day;
	ceTime[11] = mon;
   
	rc = signalCEMsg( ceTime, NULL );

	return rc;
}



