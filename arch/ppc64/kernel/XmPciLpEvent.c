/*
   * File XmPciLpEvent.h created by Wayne Holm on Mon Jan 15 2001.
   *
   * This module handles PCI interrupt events sent by the iSeries Hypervisor.
*/


#include <linux/pci.h>
#include <linux/init.h>
#include <linux/threads.h>
#include <linux/smp.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/bootmem.h>
#include <linux/blk.h>
#include <linux/ide.h>
#include <linux/rtc.h>
#include <linux/time.h>

#include <asm/iSeries/HvTypes.h>
#include <asm/iSeries/HvLpEvent.h>
#include <asm/iSeries/HvCallPci.h>
#include <asm/iSeries/XmPciLpEvent.h>
#include <asm/ppcdebug.h>
#include <asm/time.h>
#include <asm/flight_recorder.h>

extern struct flightRecorder* PciFr;


long Pci_Interrupt_Count = 0;
long Pci_Event_Count     = 0;

enum XmPciLpEvent_Subtype {
	XmPciLpEvent_BusCreated	   = 0,		// PHB has been created
	XmPciLpEvent_BusError	   = 1,		// PHB has failed
	XmPciLpEvent_BusFailed	   = 2,		// Msg to Secondary, Primary failed bus
	XmPciLpEvent_NodeFailed	   = 4,		// Multi-adapter bridge has failed
	XmPciLpEvent_NodeRecovered = 5,		// Multi-adapter bridge has recovered
	XmPciLpEvent_BusRecovered  = 12,	// PHB has been recovered
	XmPciLpEvent_UnQuiesceBus  = 18,	// Secondary bus unqiescing
	XmPciLpEvent_BridgeError   = 21,	// Bridge Error
	XmPciLpEvent_SlotInterrupt = 22		// Slot interrupt
};

struct XmPciLpEvent_BusInterrupt {
	HvBusNumber		busNumber;
	HvSubBusNumber	subBusNumber;
};

struct XmPciLpEvent_NodeInterrupt {
	HvBusNumber		busNumber;
	HvSubBusNumber	subBusNumber;
	HvAgentId		deviceId;
};

struct XmPciLpEvent {
	struct HvLpEvent hvLpEvent;

	union {
		u64 alignData;			// Align on an 8-byte boundary

		struct {
			u32			fisr;
			HvBusNumber	busNumber;
			HvSubBusNumber	subBusNumber;
			HvAgentId		deviceId;
		} slotInterrupt;

		struct XmPciLpEvent_BusInterrupt busFailed;
		struct XmPciLpEvent_BusInterrupt busRecovered;
		struct XmPciLpEvent_BusInterrupt busCreated;

		struct XmPciLpEvent_NodeInterrupt nodeFailed;
		struct XmPciLpEvent_NodeInterrupt nodeRecovered;

	} eventData;

};

static void intReceived(struct XmPciLpEvent* eventParm, struct pt_regs* regsParm);
static void logXmEvent( char* ErrorText, int busNumber);

static void XmPciLpEvent_handler( struct HvLpEvent* eventParm, struct pt_regs* regsParm)
{
	//PPCDBG(PPCDBG_BUSWALK,"XmPciLpEvent_handler, type 0x%x\n",eventParm->xType );
	++Pci_Event_Count;

	if (eventParm && eventParm->xType == HvLpEvent_Type_PciIo) {
		switch( eventParm->xFlags.xFunction ) {
		case HvLpEvent_Function_Int:
			intReceived( (struct XmPciLpEvent*)eventParm, regsParm );
			break;
		case HvLpEvent_Function_Ack:
			printk(KERN_ERR "XmPciLpEvent.c: Unexpected ack received\n");
			break;
		default:
			printk(KERN_ERR "XmPciLpEvent.c: Unexpected event function %d\n",(int)eventParm->xFlags.xFunction);
			break;
		}
	}
	else if (eventParm) {
		printk(KERN_ERR "XmPciLpEvent.c: Unrecognized PCI event type 0x%x\n",(int)eventParm->xType);
	}
	else {
		printk(KERN_ERR "XmPciLpEvent.c: NULL event received\n");
	}
}

static void intReceived(struct XmPciLpEvent* eventParm, struct pt_regs* regsParm)
{
	int irq;

	++Pci_Interrupt_Count;
	//PPCDBG(PPCDBG_BUSWALK,"PCI: XmPciLpEvent.c: intReceived\n");

	switch (eventParm->hvLpEvent.xSubtype) {
	case XmPciLpEvent_SlotInterrupt:
		irq = eventParm->hvLpEvent.xCorrelationToken;
		/* Dispatch the interrupt handlers for this irq */
		ppc_irq_dispatch_handler(regsParm, irq);
		HvCallPci_eoi(eventParm->eventData.slotInterrupt.busNumber,
			      eventParm->eventData.slotInterrupt.subBusNumber,
			      eventParm->eventData.slotInterrupt.deviceId);
		break;
		/* Ignore error recovery events for now */
	case XmPciLpEvent_BusCreated:
	        logXmEvent("System bus created.",eventParm->eventData.busCreated.busNumber);
		break;
	case XmPciLpEvent_BusError:
	case XmPciLpEvent_BusFailed:
		logXmEvent("System bus failed.", eventParm->eventData.busFailed.busNumber);
		break;
	case XmPciLpEvent_BusRecovered:
	case XmPciLpEvent_UnQuiesceBus:
		logXmEvent("System bus recovered.",eventParm->eventData.busRecovered.busNumber);
		break;
	case XmPciLpEvent_NodeFailed:
	case XmPciLpEvent_BridgeError:
	        logXmEvent("Multi-adapter bridge failed.",eventParm->eventData.nodeFailed.busNumber);
	        break;
	case XmPciLpEvent_NodeRecovered:
		logXmEvent("Multi-adapter bridge recovered",eventParm->eventData.nodeRecovered.busNumber);
		break;
	default:
	        logXmEvent("Unrecognized event subtype.",eventParm->hvLpEvent.xSubtype);
		break;
	};
}


/* This should be called sometime prior to buswalk (init_IRQ would be good) */
int XmPciLpEvent_init()
{
	int xRc;
	PPCDBG(PPCDBG_BUSWALK,"XmPciLpEvent_init, Register Event type 0x%04X\n",HvLpEvent_Type_PciIo);

	xRc = HvLpEvent_registerHandler(HvLpEvent_Type_PciIo, &XmPciLpEvent_handler);
	if (xRc == 0) {
		xRc = HvLpEvent_openPath(HvLpEvent_Type_PciIo, 0);
		if (xRc != 0) {
			printk(KERN_ERR "XmPciLpEvent.c: open event path failed with rc 0x%x\n", xRc);
		}
	}
	else {
		printk(KERN_ERR "XmPciLpEvent.c: register handler failed with rc 0x%x\n", xRc);
    	}
    return xRc;
}
/***********************************************************************/
/* printk                                                              */
/*   XmPciLpEvent: System bus failed, bus 0x4A. Time:128-16.10.44      */
/* pcifr                                                               */
/*  0045. XmPciLpEvent: System bus failed, bus 0x4A. Time:128-16.10.44 */
/***********************************************************************/
static void logXmEvent( char* ErrorText, int busNumber) {
	struct  timeval  TimeClock;
	struct  rtc_time CurTime;

	do_gettimeofday(&TimeClock);
	to_tm(TimeClock.tv_sec, &CurTime);
	char    EventLog[128];

	sprintf(EventLog,"XmPciLpEvent: %s, Bus:0x%03X.  Time:%02d.%02d.%02d",
		ErrorText,busNumber,
		CurTime.tm_hour,CurTime.tm_min,CurTime.tm_sec);

	fr_Log_Entry(PciFr,EventLog);
	printk(KERN_INFO "%s\n",EventLog);
}

