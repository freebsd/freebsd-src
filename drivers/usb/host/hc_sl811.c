/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*
 * SL811HS USB HCD for Linux Version 0.1 (10/28/2001) 0.3 (11/23/2003)
 *
 * requires (includes) hc_simple.[hc] simple generic HCD frontend
 *
 * COPYRIGHT(C) 2001 by CYPRESS SEMICONDUCTOR INC.
 *
 * ! This driver have end of live! Please use hcd/sl811.c instand !
 *
 * 05.06.2003 HNE
 * Support x86 architecture now.
 * Set "bus->bus_name" at init.
 * hc_reset,regTest: Don't load driver, if pattern-Test failed (better error returns)
 *
 * 06.06.2003 HNE
 * Moved regTest from hc_reset to hc_found_hci. So we check registers
 * only at start, and not at unload. Only all Register show, if full Debug.
 *
 * 22.09.2003 HNE
 * Do not write SL11H_INTSTATREG in loop, use delay instand.
 * If device disconnected, wait only for next insert interrupt (no Idle-Interrpts).
 *
 * 29.09.2003 HNE
 * Moving hc_sl811-arm.h and hc_sl811-x86.h to include/asm-.../hc_sl811-hw.h
 *
 * 03.10.2003 HNE
 * Low level only for port io into hardware-include.
 * GPRD as parameter (ARM only).

 * ToDo:
 * Separate IO-Base for second controller (see sl811.c)
 * Only as module tested! Compiled in Version not tested!
 *
 *-------------------------------------------------------------------------*
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *-------------------------------------------------------------------------*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/usb.h>

#define MODNAME "HC_SL811"

#undef HC_URB_TIMEOUT
#undef HC_SWITCH_INT
#undef HC_ENABLE_ISOC

// #define SL811_DEBUG
#define SL811_DEBUG_ERR
// #define SL811_DEBUG_IRQ
// #define SL811_DEBUG_VERBOSE

#ifdef SL811_DEBUG_ERR
#define DBGERR(fmt, args...) printk(fmt,## args)
#else
#define DBGERR(fmt, args...)
#endif

#ifdef SL811_DEBUG
#define DBG(fmt, args...) printk(fmt,## args)
#else
#define DBG(fmt, args...)
#endif

#ifdef SL811_DEBUG_FUNC
#define DBGFUNC(fmt, args...) printk(fmt,## args)
#else
#define DBGFUNC(fmt, args...)
#endif

#ifdef SL811_DEBUG_DATA
#define DBGDATAR(fmt, args...) printk(fmt,## args)
#define DBGDATAW(fmt, args...) printk(fmt,## args)
#else
#define DBGDATAR(fmt, args...)
#define DBGDATAW(fmt, args...)
#endif

#ifdef SL811_DEBUG_VERBOSE
#define DBGVERBOSE(fmt, args...) printk(fmt,## args)
#else
#define DBGVERBOSE(fmt, args...)
#endif

#define TRUE 1
#define FALSE 0

#define HC_SWITCH_INT
#include "hc_sl811.h"
#include "hc_simple.h"

static int urb_debug = 0;

#include "hc_simple.c"
#include "hc_sl811_rh.c"

/* Include hardware and board depens */
#include <asm/hc_sl811-hw.h>

/* forware declaration */
int SL11StartXaction (hci_t * hci, __u8 addr, __u8 epaddr, int pid, int len,
		      int toggle, int slow, int urb_state);

static int sofWaitCnt = 0;

MODULE_PARM (urb_debug, "i");
MODULE_PARM_DESC (urb_debug, "debug urb messages, default is 0 (no)");

static int hc_reset (hci_t * hci);

/***************************************************************************
 * Function Name : SL811Read
 *
 * Read a byte of data from the SL811H/SL11H
 *
 * Input:  hci = data structure for the host controller
 *         offset = address of SL811/SL11H register or memory
 *
 * Return: data
 **************************************************************************/

static __u8 inline SL811Read (hci_t *hci, __u8 offset)
{
	hcipriv_t *hp = &hci->hp;
#ifdef SL811_DEBUG_ERR
	if (!hp->hcport)
		DBGERR ("SL811Read: Error port not set!\n");
#endif
	sl811_write_index (hp, offset);
	return (sl811_read_data (hp));
}

/***************************************************************************
 * Function Name : SL811Write
 *
 * Write a byte of data to the SL811H/SL11H
 *
 * Input:  hci = data structure for the host controller
 *         offset = address of SL811/SL11H register or memory
 *         data  = the data going to write to SL811H
 *
 * Return: none
 **************************************************************************/

static void inline SL811Write (hci_t *hci, __u8 offset, __u8 data)
{
	hcipriv_t *hp = &hci->hp;
#ifdef SL811_DEBUG_ERR
	if (!hp->hcport)
		DBGERR ("SL811Write: Error port not set!\n");
#endif
	sl811_write_index_data (hp, offset, data);
}


/***************************************************************************
 * Function Name : SL811BufRead
 *
 * Read consecutive bytes of data from the SL811H/SL11H buffer
 *
 * Input:  hci = data structure for the host controller
 *         offset = SL811/SL11H register offset
 *         buf = the buffer where the data will store
 *         size = number of bytes to read
 *
 * Return: none
 **************************************************************************/

static void SL811BufRead (hci_t *hci, __u8 offset, __u8 *buf, __u8 size)
{
	hcipriv_t *hp = &hci->hp;
	if( size <= 0)
		return;
#ifdef SL811_DEBUG_ERR
	if (!hp->hcport)
		DBGERR ("SL811BufRead: Error port not set!\n");
#endif
	sl811_write_index (hp, offset);
	DBGDATAR ("SL811BufRead: io=%X offset = %02x, data = ", hp->hcport, offset);
	while (size--) {
		*buf++ = sl811_read_data(hp);
		DBGDATAR ("%02x ", *(buf-1));
	}
	DBGDATAR ("\n");
}

/***************************************************************************
 * Function Name : SL811BufWrite
 *
 * Write consecutive bytes of data to the SL811H/SL11H buffer
 *
 * Input:  hci = data structure for the host controller
 *         offset = SL811/SL11H register offset
 *         buf = the data buffer 
 *         size = number of bytes to write
 *
 * Return: none 
 **************************************************************************/

static void SL811BufWrite(hci_t *hci, __u8 offset, __u8 *buf, __u8 size)
{
	hcipriv_t *hp = &hci->hp;
	if(size<=0)
		return;
#ifdef SL811_DEBUG_ERR
	if (!hp->hcport)
		DBGERR ("SL811BufWrite: Error port not set!\n");
#endif
	sl811_write_index (hp, offset);
	DBGDATAW ("SL811BufWrite: io=%X offset = %02x, data = ", hp->hcport, offset);
	while (size--) {
		DBGDATAW ("%02x ", *buf);
		sl811_write_data (hp, *buf);
		buf++;
	}
	DBGDATAW ("\n");
}

/***************************************************************************
 * Function Name : regTest
 *
 * This routine test the Read/Write functionality of SL811HS registers
 *
 * 1) Store original register value into a buffer
 * 2) Write to registers with a RAMP pattern. (10, 11, 12, ..., 255)
 * 3) Read from register
 * 4) Compare the written value with the read value and make sure they are 
 *    equivalent
 * 5) Restore the original register value 
 *
 * Input:  hci = data structure for the host controller
 *   
 *
 * Return: TRUE = passed; FALSE = failed 
 **************************************************************************/
int regTest (hci_t * hci)
{
	int i, result = TRUE;
	__u8 buf[256], data;

	DBGFUNC ("Enter regTest\n");
	for (i = 0x10; i < 256; i++) {
		/* save the original buffer */
		buf[i] = SL811Read (hci, i);

		/* Write the new data to the buffer */
		SL811Write (hci, i, i);
	}

	/* compare the written data */
	for (i = 0x10; i < 256; i++) {
		data = SL811Read (hci, i);
		if (data != i) {
			DBGERR ("Pattern test failed!! value = 0x%x, s/b 0x%x\n",
				data, i);
			result = FALSE;

			/* If no Debug, show only first failed Address */
			if (!urb_debug)
			    break;
		}
	}

	/* restore the data */
	for (i = 0x10; i < 256; i++) {
		SL811Write (hci, i, buf[i]);
	}

	return (result);
}

#if 0 /* unused (hne) */
/***************************************************************************
 * Function Name : regShow
 *
 * Display all SL811HS register values
 *
 * Input:  hci = data structure for the host controller
 *
 * Return: none 
 **************************************************************************/
static void regShow (hci_t * hci)
{
	int i;
	for (i = 0; i < 256; i++) {
		printk ("offset %d: 0x%x\n", i, SL811Read (hci, i));
	}
}
#endif // if0

/************************************************************************
 * Function Name : USBReset
 *  
 * This function resets SL811HS controller and detects the speed of
 * the connecting device				  
 *
 * Input:  hci = data structure for the host controller
 *                
 * Return: 0 = no device attached; 1 = USB device attached
 *                
 ***********************************************************************/
/* [2.4.22] sl811_hc_reset */
static int USBReset (hci_t * hci)
{
	int status;
	hcipriv_t *hp = &hci->hp;

	DBGFUNC ("enter USBReset\n");

	SL811Write (hci, SL11H_CTLREG2, 0xae);

	// setup master and full speed

	SL811Write (hci, SL11H_CTLREG1, 0x08);	// reset USB
	mdelay (20);		// 20ms                             
	SL811Write (hci, SL11H_CTLREG1, 0);	// remove SE0        

	/* disable all interrupts (18.09.2003) */
	SL811Write (hci, SL11H_INTENBLREG, 0);

	/* 19.09.2003 [2.4.22] */
	mdelay(2);
	SL811Write (hci, SL11H_INTSTATREG, 0xff);	// clear all interrupt bits
	status = SL811Read (hci, SL11H_INTSTATREG);

	if (status & SL11H_INTMASK_USBRESET)	// Check if device is removed (0x40)
	{
		DBG ("USBReset: Device removed %03X\n", hci->hp.hcport);
		/* 19.09.2003 [2.4.22] only IRQ for insert...
		SL811Write (hci, SL11H_INTENBLREG,
			    SL11H_INTMASK_XFERDONE | SL11H_INTMASK_SOFINTR |
			    SL11H_INTMASK_INSRMV);
		*/
		SL811Write (hci, SL11H_INTENBLREG,
			    SL11H_INTMASK_INSRMV);
		hp->RHportStatus->portStatus &=
		    ~(PORT_CONNECT_STAT | PORT_ENABLE_STAT);

		return 0;
	}

	// Send SOF to address 0, endpoint 0.
	SL811Write (hci, SL11H_BUFLNTHREG_B, 0);	//zero lenth
	SL811Write (hci, SL11H_PIDEPREG_B, 0x50);	//send SOF to EP0       
	SL811Write (hci, SL11H_DEVADDRREG_B, 0x01);	//address0
	SL811Write (hci, SL11H_SOFLOWREG, 0xe0);

	if (!(status & SL11H_INTMASK_DSTATE)) {	/* 0x80 */
		/* slow speed device connect directly to root-hub */

		DBG ("USBReset: low speed Device attached %03X\n", hci->hp.hcport);
		SL811Write (hci, SL11H_CTLREG1, 0x8);
		mdelay (20);
		// SL811Write (hci, SL11H_SOFTMRREG, 0xee);	/* the same, but better reading (hne) */
		SL811Write (hci, SL11H_CTLREG2, 0xee);
		SL811Write (hci, SL11H_CTLREG1, 0x21);

		hp->RHportStatus->portStatus |=
		    (PORT_CONNECT_STAT | PORT_LOW_SPEED_DEV_ATTACH_STAT);

	} else {
		/* full speed device connect directly to root hub */

		DBG ("USBReset: full speed Device attached %03X \n", hci->hp.hcport);
		SL811Write (hci, SL11H_CTLREG1, 0x8);
		mdelay (20);
		// SL811Write (hci, SL11H_SOFTMRREG, 0xae);	/* the same, but better reading (hne) */
		SL811Write (hci, SL11H_CTLREG2, 0xae);
		SL811Write (hci, SL11H_CTLREG1, 0x01);

		hp->RHportStatus->portStatus |= (PORT_CONNECT_STAT);
		hp->RHportStatus->portStatus &= ~PORT_LOW_SPEED_DEV_ATTACH_STAT;

	}

	/* start the SOF or EOP */
	SL811Write (hci, SL11H_HOSTCTLREG_B, 0x01);

	/* clear all interrupt bits */
	/* 19.09.2003 [2.4.22] */
	mdelay(2);
	SL811Write (hci, SL11H_INTSTATREG, 0xff);

	/* enable all interrupts */
	SL811Write (hci, SL11H_INTENBLREG,
		    SL11H_INTMASK_XFERDONE | SL11H_INTMASK_SOFINTR |
		    SL11H_INTMASK_INSRMV);

	return 1;
}

/*-------------------------------------------------------------------------*/
/* tl functions */
static inline void hc_mark_last_trans (hci_t * hci)
{
	hcipriv_t *hp = &hci->hp;
	__u8 *ptd = hp->tl;

	dbg ("enter hc_mark_last_trans\n");
	if (ptd == NULL) {
		printk ("hc_mark_last_trans: ptd = null\n");
		return;
	}
	if (hp->xferPktLen > 0)
		*(ptd + hp->tl_last) |= (1 << 3);
}

static inline void hc_flush_data_cache (hci_t * hci, void *data, int len)
{
}

/************************************************************************
 * Function Name : hc_add_trans
 *  
 * This function sets up the SL811HS register and transmit the USB packets.
 * 
 * 1) Determine if enough time within the current frame to send the packet
 * 2) Load the data into the SL811HS register
 * 3) Set the appropriate command to the register and trigger the transmit
 *
 * Input:  hci = data structure for the host controller
 *         len = data length
 *         data = transmitting data
 *         toggle = USB toggle bit, either 0 or 1
 *         maxps = maximum packet size for this endpoint
 *         slow = speed of the device
 *         endpoint = endpoint number
 *         address = USB address of the device
 *         pid = packet ID
 *         format = 
 *         urb_state = the current stage of USB transaction
 *       
 * Return: 0 = no time left to schedule the transfer
 *         1 = success 
 *                
 ***********************************************************************/
static inline int hc_add_trans (hci_t * hci, int len, void *data, int toggle,
				int maxps, int slow, int endpoint, int address,
				int pid, int format, int urb_state)
{
	hcipriv_t *hp = &hci->hp;
	__u16 speed;
	int ii, jj, kk;

	DBGFUNC ("enter hc_add_trans: len=0x%x, toggle:0x%x, endpoing:0x%x,"
		 " addr:0x%x, pid:0x%x,format:0x%x\n", len, toggle, endpoint,
		 address, pid, format);

	if (len > maxps) {
		len = maxps;
	}

	speed = hp->RHportStatus->portStatus;
	if (speed & PORT_LOW_SPEED_DEV_ATTACH_STAT) {
//      ii = (8*7*8 + 6*3) * len + 800; 
		ii = 8 * 8 * len + 1024;
	} else {
		if (slow) {
//          ii = (8*7*8 + 6*3) * len + 800; 
			ii = 8 * 8 * len + 2048;
		} else
//          ii = (8*7 + 6*3)*len + 110;
			ii = 8 * len + 256;
	}

	ii += 2 * 10 * len;

	jj = SL811Read (hci, SL11H_SOFTMRREG);

	/* Read back SOF counter HIGH (bit0-bit5 only) 26.11.2002 (hne) */
	// kk = (jj & 0xFF) * 64 - ii;
	kk = (jj & (64-1)) * 64 - ii;

	if (kk < 0) {
		DBGVERBOSE
		    ("hc_add_trans: no bandwidth for schedule, ii = 0x%x,"
		     "jj = 0x%x, len =0x%x, active_trans = 0x%x\n", ii, jj, len,
		     hci->active_trans);
		return (-1);
	}

	if (pid != PID_IN) {
		/* Load data into hc */

		SL811BufWrite (hci, SL11H_DATA_START, (__u8 *) data, len);
	}

	/* transmit */

	SL11StartXaction (hci, (__u8) address, (__u8) endpoint, (__u8) pid, len,
			  toggle, slow, urb_state);

	return len;
}

/************************************************************************
 * Function Name : hc_parse_trans
 *  
 * This function checks the status of the transmitted or received packet
 * and copy the data from the SL811HS register into a buffer.
 *
 * 1) Check the status of the packet 
 * 2) If successful, and IN packet then copy the data from the SL811HS register
 *    into a buffer
 *
 * Input:  hci = data structure for the host controller
 *         actbytes = pointer to actual number of bytes
 *         data = data buffer
 *         cc = packet status
 *         length = the urb transmit length
 *         pid = packet ID
 *         urb_state = the current stage of USB transaction
 *       
 * Return: 0 
 ***********************************************************************/
static inline int hc_parse_trans (hci_t * hci, int *actbytes, __u8 * data,
				  int *cc, int *toggle, int length, int pid,
				  int urb_state)
{
	__u8 addr;
	__u8 len;
	__u8 pkt_stat;

	DBGFUNC ("enter hc_parse_trans\n");

	/* get packet status; convert ack rcvd to ack-not-rcvd */

	*cc = pkt_stat \
	    = SL811Read (hci, SL11H_PKTSTATREG);

	if (pkt_stat &
	    (SL11H_STATMASK_ERROR | SL11H_STATMASK_TMOUT | SL11H_STATMASK_OVF |
	     SL11H_STATMASK_NAK | SL11H_STATMASK_STALL)) {
		if (*cc & SL11H_STATMASK_OVF)
			DBGERR ("parse trans: error recv ack, cc = 0x%x/0x%x, TX_BASE_Len = "
				"0x%x, TX_count=0x%x\n", pkt_stat,
				SL811Read (hci, SL11H_PKTSTATREG),
				SL811Read (hci, SL11H_BUFLNTHREG),
				SL811Read (hci, SL11H_XFERCNTREG));
		else 
			DBGVERBOSE ("parse trans: error recv ack, cc = 0x%x/0x%x\n",
				pkt_stat, SL811Read (hci, SL11H_PKTSTATREG));
	} else {
		DBGVERBOSE ("parse trans: recv ack, cc=0x%x, len=0x%x, pid=0x%x, urb=%d\n",
			    pkt_stat, length, pid, urb_state);

		/* Successful data */
		if ((pid == PID_IN) && (urb_state != US_CTRL_SETUP)) {

			/* Find the base address */
			addr = SL811Read (hci, SL11H_BUFADDRREG);

			/* Find the Transmit Length */
			len = SL811Read (hci, SL11H_BUFLNTHREG);

			/* The actual data length = xmit length reg - xfer count reg */
			*actbytes = len - SL811Read (hci, SL11H_XFERCNTREG);

			if ((data != NULL) && (*actbytes > 0)) {
				SL811BufRead (hci, addr, data, *actbytes);

			} else if ((data == NULL) && (*actbytes <= 0)) {
				DBGERR ("hc_parse_trans: data = NULL or actbyte = 0x%x\n",
					*actbytes);
				return 0;
			}
		} else if (pid == PID_OUT) {
			*actbytes = length;
		} else {
			// printk ("ERR:parse_trans, pid != IN or OUT, pid = 0x%x\n", pid);
		}
		*toggle = !*toggle;
	}

	return 0;
}

/************************************************************************
 * Function Name : hc_start_int
 *  
 * This function enables SL811HS interrupts
 *
 * Input:  hci = data structure for the host controller
 *       
 * Return: none 
 ***********************************************************************/
static void hc_start_int (hci_t * hci)
{
#ifdef HC_SWITCH_INT
	int mask =
	    SL11H_INTMASK_XFERDONE | SL11H_INTMASK_SOFINTR |
	    SL11H_INTMASK_INSRMV | SL11H_INTMASK_USBRESET;
	SL811Write (hci, IntEna, mask);
#endif
}

/************************************************************************
 * Function Name : hc_stop_int
 *  
 * This function disables SL811HS interrupts
 *
 * Input:  hci = data structure for the host controller
 *       
 * Return: none 
 ***********************************************************************/
static void hc_stop_int (hci_t * hci)
{
#ifdef HC_SWITCH_INT
	SL811Write (hci, SL11H_INTSTATREG, 0xff);
//  SL811Write(hci, SL11H_INTENBLREG, SL11H_INTMASK_INSRMV);

#endif
}

/************************************************************************
 * Function Name : handleInsRmvIntr
 *  
 * This function handles the insertion or removal of device on  SL811HS. 
 * It resets the controller and updates the port status
 *
 * Input:  hci = data structure for the host controller
 *       
 * Return: none 
 ***********************************************************************/
void handleInsRmvIntr (hci_t * hci)
{
	hcipriv_t *hp = &hci->hp;

	USBReset (hci);

	/* Changes in connection status */

	hp->RHportStatus->portChange |= PORT_CONNECT_CHANGE;

	/* Port Enable or Disable */

	if (hp->RHportStatus->portStatus & PORT_CONNECT_STAT) {
		/* device is connected to the port:
		 *    1) Enable port 
		 *    2) Resume ?? 
		 */
//               hp->RHportStatus->portChange |= PORT_ENABLE_CHANGE;

		/* Over Current is not supported by the SL811 HW ?? */

		/* How about the Port Power ?? */

	} else {
		/* Device has disconnect:
		 *    1) Disable port
		 */

		hp->RHportStatus->portStatus &= ~(PORT_ENABLE_STAT);
		hp->RHportStatus->portChange |= PORT_ENABLE_CHANGE;

	}
}

/*****************************************************************
 *
 * Function Name: SL11StartXaction
 *  
 * This functions load the registers with appropriate value and 
 * transmit the packet.				  
 *
 * Input:  hci = data structure for the host controller
 *         addr = USB address of the device
 *         epaddr = endpoint number
 *         pid = packet ID
 *         len = data length
 *         toggle = USB toggle bit, either 0 or 1
 *         slow = speed of the device
 *         urb_state = the current stage of USB transaction
 *
 * Return: 0 = error; 1 = successful
 *                
 *****************************************************************/
int SL11StartXaction (hci_t * hci, __u8 addr, __u8 epaddr, int pid, int len,
		      int toggle, int slow, int urb_state)
{

	hcipriv_t *hp = &hci->hp;
	__u8 cmd = 0;
	__u8 setup_data[4];
	__u16 speed;

	speed = hp->RHportStatus->portStatus;
	if (!(speed & PORT_LOW_SPEED_DEV_ATTACH_STAT) && slow) {
		cmd |= SL11H_HCTLMASK_PREAMBLE;
	}
	switch (pid) {
	case PID_SETUP:
		// cmd &= SL11H_HCTLMASK_PREAMBLE;	/* 26.11.2002 (hne) */
		cmd |= SL11H_HCTLMASK_ARM
		     | SL11H_HCTLMASK_ENBLEP
		     | SL11H_HCTLMASK_WRITE;
		DBGVERBOSE ("SL811 Xaction: SETUP cmd=%02X\n", cmd);
		break;

	case PID_OUT:
		// cmd &= (SL11H_HCTLMASK_SEQ | SL11H_HCTLMASK_PREAMBLE); /* (hne) 26.11.2002 */
		cmd |= SL11H_HCTLMASK_ARM
		     | SL11H_HCTLMASK_ENBLEP
		     | SL11H_HCTLMASK_WRITE;
		if (toggle) {
			cmd |= SL11H_HCTLMASK_SEQ;
		}
		DBGVERBOSE ("SL811 Xaction: OUT cmd=%02X\n", cmd);
		break;

	case PID_IN:
		// cmd &= (SL11H_HCTLMASK_SEQ | SL11H_HCTLMASK_PREAMBLE);	/* (hne) 26.11.2002 */
		cmd |= SL11H_HCTLMASK_ARM
		     | SL11H_HCTLMASK_ENBLEP;
		DBGVERBOSE ("SL811 Xaction: IN cmd=%02x\n", cmd);
		break;

	default:
		DBGERR ("ERR: SL11StartXaction: unknow pid = 0x%x\n", pid);
		return 0;
	}
	setup_data[0] = SL11H_DATA_START;			/* 01:SL11H_BUFADDRREG */
	setup_data[1] = len;					/* 02:SL11H_BUFLNTHREG */
	setup_data[2] = (((pid & 0x0F) << 4) | (epaddr & 0xF));	/* 03:SL11H_PIDEPREG */
	setup_data[3] = addr & 0x7F;				/* 04:SL11H_DEVADDRREG */

	SL811BufWrite (hci, SL11H_BUFADDRREG, (__u8 *) & setup_data[0], 4);

	// SL811Write (hci, SL11H_PIDEPREG, cmd);			/* 03: grrr (hne) */
	SL811Write (hci, SL11H_HOSTCTLREG, cmd);		/* 00: 26.11.2002 (hne) */

#if 0
	/* The SL811 has a hardware flaw when hub devices sends out
	 * SE0 between packets. It has been found in a TI chipset and
	 * cypress hub chipset. It causes the SL811 to hang
	 * The workaround is to re-issue the preample again.
	 */

	if ((cmd & SL11H_HCTLMASK_PREAMBLE)) {
		SL811Write (hci, SL11H_PIDEPREG_B, 0xc0);
		SL811Write (hci, SL11H_HOSTCTLREG_B, 0x1);	// send the premable
	}
#endif
	return 1;
}

/*****************************************************************
 *
 * Function Name: hc_interrupt
 *
 * Interrupt service routine. 
 *
 * 1) determine the causes of interrupt
 * 2) clears all interrupts
 * 3) calls appropriate function to service the interrupt
 *
 * Input:  irq = interrupt line associated with the controller 
 *         hci = data structure for the host controller
 *         r = holds the snapshot of the processor's context before 
 *             the processor entered interrupt code. (not used here) 
 *
 * Return value  : None.
 *                
 *****************************************************************/
static void hc_interrupt (int irq, void *__hci, struct pt_regs *r)
{
	__u8 ii;
	hci_t *hci = __hci;
	int isExcessNak = 0;
	int urb_state = 0;
	// __u8 tmpIrq = 0;
	int irq_loop = 16;	/* total irq handled at one hardware irq */

#ifdef SL811_DEBUG_IRQ
	hcipriv_t *hp = &hci->hp;
	unsigned char sta1, sta2;

	outb (SL11H_INTSTATREG, 0x220);	// Interrupt-Status register, controller1
	sta1 = (__u8) inb (0x220+1);

	outb (SL11H_INTSTATREG, 0x222);	// Interrupt-Status register, controller2
	sta2 = (__u8) inb (0x222+1);
#endif

    do {
	/* Get value from interrupt status register */

	ii = SL811Read (hci, SL11H_INTSTATREG);

	/* All interrupts handled? (hne) */
	if ( !(ii & (SL11H_INTMASK_INSRMV /* | SL11H_INTMASK_USBRESET */ |
		     SL11H_INTMASK_XFERDONE | SL11H_INTMASK_SOFINTR)) )
	{
#ifdef SL811_DEBUG_IRQ
	// printk ("%Xh IRQ ista=%02X not me\n", hp->hcport, ii);

	    if ( sta1 != 0x80 && sta1 != 0x90 &&
		 sta2 != 0x80 && sta2 != 0x90 )
		printk ("%Xh IRQ sta=%02X,%02X\n", hp->hcport, sta1, sta2);
#endif
		return;
	}

	/* Interrupt will be handle now (18.09.2003 2.4.22) */
	SL811Write (hci, SL11H_INTSTATREG, 0xff);

	/* SOF-outputs are to slow. No debug any SOF */
	if ( !(ii & SL11H_INTMASK_SOFINTR) )
	    DBGVERBOSE ("SL811 ISR: %s%s%s%s io=%03X\n",
		(ii & SL11H_INTMASK_XFERDONE) ?	" DONE": "",
		(ii & SL11H_INTMASK_SOFINTR) ?	" SOFINTR": "",
		(ii & SL11H_INTMASK_INSRMV) ? 	" INSRMV": "",
		(ii & SL11H_INTMASK_USBRESET) ? " USBRESET": "",
		hci->hp.hcport );

	// if (ii & (SL11H_INTMASK_INSRMV | SL11H_INTMASK_USBRESET)) {
	// Found in 2.5.75 (19.09.2003)
	// "SL11H_INTMASK_USBRESET" is always on, if no device connected!
	if (ii & SL11H_INTMASK_INSRMV) {
		/* Device insertion or removal detected for the USB port */
		/* Disable all interrupts */
		SL811Write (hci, SL11H_INTENBLREG, 0);
		/* No SOF, Full speed */
		SL811Write (hci, SL11H_CTLREG1, 0);

		mdelay (100);	// wait for device stable 
		handleInsRmvIntr (hci);
		return;
	}

	/* Clear all interrupts */
	// SL811Write (hci, SL11H_INTSTATREG, 0xff);

	if (ii & SL11H_INTMASK_XFERDONE) {

		/* USB Done interrupt occurred */
		// DBGVERBOSE ("xsta=%02X\n", SL811Read (hci, SL11H_PKTSTATREG));

		urb_state = sh_done_list (hci, &isExcessNak);
#ifdef WARNING
		if (hci->td_array->len > 0)
			printk ("WARNING: IRQ, td_array->len = 0x%x, s/b:0\n",
				hci->td_array->len);
#endif
		if (hci->td_array->len == 0 && !isExcessNak
		    && !(ii & SL11H_INTMASK_SOFINTR) && (urb_state == 0)) {
			if (urb_state == 0) {
				/* All urb_state has not been finished yet! 
				 * continue with the current urb transaction 
				 */

				if (hci->last_packet_nak == 0) {
					if (!usb_pipecontrol
					    (hci->td_array->td[0].urb->pipe))
						sh_add_packet (hci, hci->td_array-> td[0].urb);
				}
			} else {
				/* The last transaction has completed:
				 * schedule the next transaction 
				 */

				sh_schedule_trans (hci, 0);
			}
		}
		/* +++ (hne)
		SL811Write (hci, SL11H_INTSTATREG, 0xff);
		--- */
		return;
	}

	if (ii & SL11H_INTMASK_SOFINTR) {
		hci->frame_number = (hci->frame_number + 1) % 2048;
		if (hci->td_array->len == 0)
			sh_schedule_trans (hci, 1);
		else {
			if (sofWaitCnt++ > 100) {
				/* The last transaction has not completed.
				 * Need to retire the current td, and let
				 * it transmit again later on.
				 * (THIS NEEDS TO BE WORK ON MORE, IT SHOULD NEVER 
				 *  GET TO THIS POINT)
				 */

				DBGERR ("SOF interrupt: td_array->len = 0x%x, s/b:0 io=%03X\n",
					hci->td_array->len,
					hci->hp.hcport	);
				urb_print (hci->td_array->td[hci->td_array->len - 1].urb,
					   "INTERRUPT", 0);
				/* FIXME: Here sh_done_list was call with urb->dev=NULL 21.11.2002 (hne) */
				sh_done_list (hci, &isExcessNak);
				/* +++ (hne)
				SL811Write (hci, SL11H_INTSTATREG, 0xff);
				--- */
				hci->td_array->len = 0;
				sofWaitCnt = 0;
			}
		}

#if 0 /* grrr! This READ clears my XFERDONE interrupt! Its better handle this in a loop. (hne) */
		tmpIrq = SL811Read (hci, SL11H_INTSTATREG) & SL811Read (hci, SL11H_INTENBLREG);
		if (tmpIrq) {
			DBG ("IRQ occurred while service SOF: irq = 0x%x\n",
			     tmpIrq);

			/* If we receive a DONE IRQ after schedule, need to 
			 * handle DONE IRQ again 
			 */

			if (tmpIrq & SL11H_INTMASK_XFERDONE) {
				DBGERR ("XFERDONE occurred while service SOF: irq = 0x%x\n",
					tmpIrq);
				urb_state = sh_done_list (hci, &isExcessNak);
			}
			SL811Write (hci, SL11H_INTSTATREG, 0xff);
		}
#endif
	} else {
		DBG ("SL811 ISR: unknown, int=0x%x io=%03X\n", ii, hci->hp.hcport);
		return;
	}

	/* +++ (hne)
	SL811Write (hci, SL11H_INTSTATREG, 0xff);
	--- */

	/* loop, if any interrupts can read (hne) */
    } while (--irq_loop);
    
	return;
}

/*****************************************************************
 *
 * Function Name: hc_reset
 *
 * This function does register test and resets the SL811HS 
 * controller.
 *
 * Input:  hci = data structure for the host controller
 *
 * Return value  : 0
 *                
 *****************************************************************/
static int hc_reset (hci_t * hci)
{
	int attachFlag = 0;

	DBGFUNC ("Enter hc_reset\n");
	attachFlag = USBReset (hci);
	if (attachFlag) {
		setPortChange (hci, PORT_CONNECT_CHANGE);
	}
	return (0);
}

/*****************************************************************
 *
 * Function Name: hc_alloc_trans_buffer
 *
 * This function allocates all transfer buffer  
 *
 * Input:  hci = data structure for the host controller
 *
 * Return value  : 0
 *                
 *****************************************************************/
static int hc_alloc_trans_buffer (hci_t * hci)
{
	hcipriv_t *hp = &hci->hp;
	int maxlen;

	hp->itl0_len = 0;
	hp->itl1_len = 0;
	hp->atl_len = 0;

	hp->itl_buffer_len = 1024;
	hp->atl_buffer_len = 4096 - 2 * hp->itl_buffer_len;	/* 2048 */

	maxlen = (hp->itl_buffer_len > hp->atl_buffer_len) ? hp->itl_buffer_len : hp->atl_buffer_len;

	hp->tl = kmalloc (maxlen, GFP_KERNEL);

	if (!hp->tl)
		return -ENOMEM;

	memset (hp->tl, 0, maxlen);
	return 0;
}

/*****************************************************************
 *
 * Function Name: getPortStatusAndChange
 *
 * This function gets the ports status from SL811 and format it 
 * to a USB request format
 *
 * Input:  hci = data structure for the host controller
 *
 * Return value  : port status and change
 *                
 *****************************************************************/
static __u32 getPortStatusAndChange (hci_t * hci)
{
	hcipriv_t *hp = &hci->hp;
	__u32 portstatus;

	DBGFUNC ("enter getPorStatusAndChange\n");

	portstatus = hp->RHportStatus->portChange << 16 | hp->RHportStatus->portStatus;

	return (portstatus);
}

/*****************************************************************
 *
 * Function Name: setPortChange
 *
 * This function set the bit position of portChange.
 *
 * Input:  hci = data structure for the host controller
 *         bitPos = the bit position
 *
 * Return value  : none 
 *                
 *****************************************************************/
static void setPortChange (hci_t * hci, __u16 bitPos)
{
	hcipriv_t *hp = &hci->hp;

	switch (bitPos) {
	case PORT_CONNECT_STAT:
		hp->RHportStatus->portChange |= bitPos;
		break;

	case PORT_ENABLE_STAT:
		hp->RHportStatus->portChange |= bitPos;
		break;

	case PORT_RESET_STAT:
		hp->RHportStatus->portChange |= bitPos;
		break;

	case PORT_POWER_STAT:
		hp->RHportStatus->portChange |= bitPos;
		break;

	case PORT_SUSPEND_STAT:
		hp->RHportStatus->portChange |= bitPos;
		break;

	case PORT_OVER_CURRENT_STAT:
		hp->RHportStatus->portChange |= bitPos;
		break;
	}
}

/*****************************************************************
 *
 * Function Name: clrPortChange
 *
 * This function clear the bit position of portChange.
 *
 * Input:  hci = data structure for the host controller
 *         bitPos = the bit position
 *
 * Return value  : none 
 *                
 *****************************************************************/
static void clrPortChange (hci_t * hci, __u16 bitPos)
{
	hcipriv_t *hp = &hci->hp;
	switch (bitPos) {
	case PORT_CONNECT_CHANGE:
		hp->RHportStatus->portChange &= ~bitPos;
		break;

	case PORT_ENABLE_CHANGE:
		hp->RHportStatus->portChange &= ~bitPos;
		break;

	case PORT_RESET_CHANGE:
		hp->RHportStatus->portChange &= ~bitPos;
		break;

	case PORT_SUSPEND_CHANGE:
		hp->RHportStatus->portChange &= ~bitPos;
		break;

	case PORT_OVER_CURRENT_CHANGE:
		hp->RHportStatus->portChange &= ~bitPos;
		break;
	}
}

/*****************************************************************
 *
 * Function Name: clrPortStatus
 *
 * This function clear the bit position of portStatus.
 *
 * Input:  hci = data structure for the host controller
 *         bitPos = the bit position
 *
 * Return value  : none 
 *                
 *****************************************************************/
static void clrPortStatus (hci_t * hci, __u16 bitPos)
{
	hcipriv_t *hp = &hci->hp;
	switch (bitPos) {
	case PORT_ENABLE_STAT:
		hp->RHportStatus->portStatus &= ~bitPos;
		break;

	case PORT_RESET_STAT:
		hp->RHportStatus->portStatus &= ~bitPos;
		break;

	case PORT_POWER_STAT:
		hp->RHportStatus->portStatus &= ~bitPos;
		break;

	case PORT_SUSPEND_STAT:
		hp->RHportStatus->portStatus &= ~bitPos;
		break;
	}
}

/*****************************************************************
 *
 * Function Name: setPortStatus
 *
 * This function set the bit position of portStatus.
 *
 * Input:  hci = data structure for the host controller
 *         bitPos = the bit position
 *
 * Return value  : none 
 *                
 *****************************************************************/
static void setPortStatus (hci_t * hci, __u16 bitPos)
{
	hcipriv_t *hp = &hci->hp;
	switch (bitPos) {
	case PORT_ENABLE_STAT:
		hp->RHportStatus->portStatus |= bitPos;
		break;

	case PORT_RESET_STAT:
		hp->RHportStatus->portStatus |= bitPos;
		break;

	case PORT_POWER_STAT:
		hp->RHportStatus->portStatus |= bitPos;
		break;

	case PORT_SUSPEND_STAT:
		hp->RHportStatus->portStatus |= bitPos;
		break;
	}
}

/*****************************************************************
 *
 * Function Name: hc_start
 *
 * This function starts the root hub functionality. 
 *
 * Input:  hci = data structure for the host controller
 *
 * Return value  : 0 
 *                
 *****************************************************************/
static int hc_start (hci_t * hci)
{
	DBGFUNC ("Enter hc_start\n");

	rh_connect_rh (hci);

	return 0;
}

/*****************************************************************
 *
 * Function Name: hc_alloc_hci
 *
 * This function allocates all data structure and store in the 
 * private data structure. 
 *
 * Input:  hci = data structure for the host controller
 *
 * Return value  : 0 
 *                
 *****************************************************************/
static hci_t *__devinit hc_alloc_hci (void)
{
	hci_t *hci;
	hcipriv_t *hp;
	portstat_t *ps;
	struct usb_bus *bus;

	DBGFUNC ("Enter hc_alloc_hci\n");
	hci = (hci_t *) kmalloc (sizeof (hci_t), GFP_KERNEL);
	if (!hci)
		return NULL;

	memset (hci, 0, sizeof (hci_t));

	hp = &hci->hp;

	hp->irq = -1;
	hp->hcport = -1;

	/* setup root hub port status */

	ps = (portstat_t *) kmalloc (sizeof (portstat_t), GFP_KERNEL);

	if (!ps)
		return NULL;
	ps->portStatus = PORT_STAT_DEFAULT;
	ps->portChange = PORT_CHANGE_DEFAULT;
	hp->RHportStatus = ps;

	hci->nakCnt = 0;
	hci->last_packet_nak = 0;

	hci->a_td_array.len = 0;
	hci->i_td_array[0].len = 0;
	hci->i_td_array[1].len = 0;
	hci->td_array = &hci->a_td_array;
	hci->active_urbs = 0;
	hci->active_trans = 0;
	INIT_LIST_HEAD (&hci->hci_hcd_list);
	list_add (&hci->hci_hcd_list, &hci_hcd_list);
	init_waitqueue_head (&hci->waitq);

	INIT_LIST_HEAD (&hci->ctrl_list);
	INIT_LIST_HEAD (&hci->bulk_list);
	INIT_LIST_HEAD (&hci->iso_list);
	INIT_LIST_HEAD (&hci->intr_list);
	INIT_LIST_HEAD (&hci->del_list);

	bus = usb_alloc_bus (&hci_device_operations);
	if (!bus) {
		kfree (hci);
		return NULL;
	}

	hci->bus = bus;
	bus->bus_name = "sl811";
	bus->hcpriv = (void *) hci;

	return hci;
}

/*****************************************************************
 *
 * Function Name: hc_release_hci
 *
 * This function De-allocate all resources  
 *
 * Input:  hci = data structure for the host controller
 *
 * Return value  : 0 
 *                
 *****************************************************************/
static void hc_release_hci (hci_t * hci)
{
	hcipriv_t *hp = &hci->hp;

	DBGFUNC ("Enter hc_release_hci\n");

	/* disconnect all devices */
	if (hci->bus->root_hub)
		usb_disconnect (&hci->bus->root_hub);

	hc_reset (hci);

	if (hp->hcport) {
		// Disable all Interrupts
		SL811Write (hci, SL11H_INTENBLREG, 0x00);

		// Remove all Interrups
		mdelay(2);
		SL811Write (hci, SL11H_INTSTATREG, 0xff);
	}

	if (hp->tl)
		kfree (hp->tl);

	sl811_release_regions(hp);

	if (hp->irq >= 0) {
		free_irq (hp->irq, hci);
		hp->irq = -1;
	}

	usb_deregister_bus (hci->bus);
	usb_free_bus (hci->bus);

	list_del (&hci->hci_hcd_list);
	INIT_LIST_HEAD (&hci->hci_hcd_list);

	kfree (hci);
}

/*****************************************************************
 *
 * Function Name: hc_found_hci
 *
 * This function request IO memory regions, request IRQ, and
 * allocate all other resources. 
 *
 * Input: addr = first IO address
 *        addr2 = second IO address
 *        irq = interrupt number 
 *
 * Return: 0 = success or error condition 
 *                
 *****************************************************************/
static int __devinit hc_found_hci (int irq, int iobase1, int iobase2)
{
	hci_t *hci;
	hcipriv_t *hp;

	DBGFUNC ("Enter hc_found_hci\n");
	hci = hc_alloc_hci ();
	if (!hci) {
		return -ENOMEM;
	}

	init_irq ();
	hp = &hci->hp;

	if (sl811_request_regions (hp, iobase1, iobase2)) {
		hc_release_hci (hci);
		return -EBUSY;
	}

	if (!regTest (hci)) {
	    DBGERR (KERN_ERR "regTest: Controller fault!\n");
		hc_release_hci (hci);
	    return -ENXIO;	/* No such device or address */
	}

	if (hc_alloc_trans_buffer (hci)) {
		hc_release_hci (hci);
		return -ENOMEM;
	}

	usb_register_bus (hci->bus);

	if (request_irq (irq, hc_interrupt, SA_SHIRQ, MODNAME, hci) != 0) {
		DBGERR ("request interrupt %d failed", irq);
		hc_release_hci (hci);
		return -EBUSY;
	}
	hp->irq = irq;

	printk (KERN_INFO __FILE__ ": USB SL811 at %x,%x, IRQ %d\n",
		hp->hcport, hp->hcport2, irq);

	#ifdef SL811_DEBUG_VERBOSE
	{
	    __u8 u = SL811Read (hci, SL11H_HWREVREG);
	    
	    DBGVERBOSE ("SL811 HW: %02Xh ", u);
	    switch (u & 0xF0) {
	    case 0x00: DBGVERBOSE ("SL11H\n");		break;
	    case 0x10: DBGVERBOSE ("SL811HS rev1.2\n");	break;
	    case 0x20: DBGVERBOSE ("SL811HS rev1.5\n");	break;
	    default:   DBGVERBOSE ("unknown!\n");
	    }
	}
	#endif // SL811_DEBUG_VERBOSE

	if (hc_reset (hci)) {
		hc_release_hci (hci);
		return -EBUSY;
	}

	if (hc_start (hci) < 0) {
		DBGERR ("can't start usb-%x", hp->hcport);
		hc_release_hci (hci);
		return -EBUSY;
	}

	return 0;
}

/*****************************************************************
 *
 * Function Name: hci_hcd_init
 *
 * This is an init function, and it is the first function being called
 *
 * Input: none 
 *
 * Return: 0 = success or error condition 
 *                
 *****************************************************************/
static int __init hci_hcd_init (void)
{
	int ret;
#ifndef __arm__
	int io_offset = 0;
#endif // !__arm__

	DBGFUNC ("Enter hci_hcd_init\n");
	DBGVERBOSE ("SL811 VERBOSE enabled\n");

#ifdef __arm__
	ret = hc_found_hci (irq, base_addr, data_reg_addr);
#else // __arm__

	// registering "another instance"
	for (io_offset = 0; io_offset < MAX_CONTROLERS * 2; io_offset += 2) {

		ret = hc_found_hci (irq, io_base + io_offset, 0);
		if (ret)
			return (ret);

	} /* endfor */
#endif // __arm__

	return ret;
}

/*****************************************************************
 *
 * Function Name: hci_hcd_cleanup
 *
 * This is a cleanup function, and it is called when module is 
 * unloaded. 
 *
 * Input: none 
 *
 * Return: none 
 *                
 *****************************************************************/
static void __exit hci_hcd_cleanup (void)
{
	struct list_head *hci_l;
	hci_t *hci;

	DBGFUNC ("Enter hci_hcd_cleanup\n");
	for (hci_l = hci_hcd_list.next; hci_l != &hci_hcd_list;) {
		hci = list_entry (hci_l, hci_t, hci_hcd_list);
		hci_l = hci_l->next;
		hc_release_hci (hci);
	}
}

module_init (hci_hcd_init);
module_exit (hci_hcd_cleanup);

MODULE_AUTHOR ("Pei Liu <pbl@cypress.com>, Henry Nestler <hne@ist1.de>");
MODULE_DESCRIPTION ("USB SL811HS Host Controller Driver");
