/*
File: include/asm-i386/sl811-hw.h

19.09.2003 hne@ist1.de
Use Kernel 2.4.20 and this source from 2.4.22
Splitt hardware depens into file sl811-x86.h and sl811-arm.h.
Functions are inline.

Handle multi instances. For two controller on one board.
"hc->data_io" not used for x86 arch.

22.09.2003 hne
Alternate IO-Base for second Controller (CF/USB1).

23.09.2003 hne
Move Hardware depend header sl811-x86.h into include/asm-i386/sl811-hw.h.

03.10.2003 hne
Low level only for port io into hardware-include.
*/

#ifndef __LINUX_SL811_HW_H
#define __LINUX_SL811_HW_H

#define MAX_CONTROLERS  2       /* Max number of SL811 controllers per module */

static int io[MAX_CONTROLERS] = { 0x220, 0x222 };	/* IO ports for controllers */
static int irq[MAX_CONTROLERS] = { 12, 12 };		/* Interrupt list */

MODULE_PARM(io, "1-" __MODULE_STRING(MAX_CONTROLERS) "i");
MODULE_PARM_DESC(io, "I/O base address(es), 0x220,0x222 default");
MODULE_PARM(irq, "1-" __MODULE_STRING(MAX_CONTROLERS) "i");
MODULE_PARM_DESC(irq, "IRQ number(s), 12,12 default");

#define OFFSET_DATA_REG		1	/* Offset from ADDR_IO to DATA_IO */
					/* Do not change this! */
#define SIZEOF_IO_REGION	2	/* Size for request/release region */

/*
 * Low level: Read from Data port [x86]
 */
static __u8 inline sl811_read_data (struct sl811_hc *hc)
{
	return ((__u8) inb (hc->addr_io+OFFSET_DATA_REG));
}

/*
 * Low level: Write to index register [x86]
 */
static void inline sl811_write_index (struct sl811_hc *hc, __u8 index)
{
	outb (index, hc->addr_io);
}

/*
 * Low level: Write to Data port [x86]
 */
static void inline sl811_write_data (struct sl811_hc *hc, __u8 data)
{
	outb (data, hc->addr_io+OFFSET_DATA_REG);
}

/*
 * Low level: Write to index register and data port [x86]
 */
static void inline sl811_write_index_data (struct sl811_hc *hc, __u8 index, __u8 data)
{
	outw (index|(((__u16)data) << 8), hc->addr_io);
}

/*
 * This	function is board specific.  It	sets up	the interrupt to
 * be an edge trigger and trigger on the rising	edge
 */
static void inline sl811_init_irq(void)
{
	/* nothing for x86 */
}


/*****************************************************************
 *
 * Function Name: release_regions [x86]
 *
 * This function is board specific. It release all io address
 * from memory (if can).
 *
 * Input: struct sl811_hc * *
 *
 * Return value  : 0 = OK
 *
 *****************************************************************/
static void inline sl811_release_regions(struct sl811_hc *hc)
{
	if (hc->addr_io)
		release_region(hc->addr_io, SIZEOF_IO_REGION);
	hc->addr_io = 0;

	/* data_io unused for x86 */
	/* ...
	if (hc->data_io)
		release_region(hc->data_io, SIZEOF_IO_REGION);
	hc->data_io = 0;
	... */
}

/*****************************************************************
 *
 * Function Name: request_regions [x86]
 *
 * This function is board specific. It request all io address and
 * maps into memory (if can).
 *
 * Input: struct sl811_hc *
 *
 * Return value  : 0 = OK
 *
 *****************************************************************/
static int inline sl811_request_regions (struct sl811_hc *hc, int addr_io, int data_io, const char *name)
{
	if (!request_region(addr_io, SIZEOF_IO_REGION, name)) {
		PDEBUG(3, "request address %d failed", addr_io);
		return -EBUSY;
	}
	hc->addr_io =	addr_io;

	/* data_io unused for x86 */
	/* hc->data_io = data_io; */

	return 0;
}

#endif // __LINUX_SL811_HW_H
