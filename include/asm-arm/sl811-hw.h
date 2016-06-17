/*
File: include/asm-arm/sl811-hw.h

19.09.2003 hne@ist1.de
Use Kernel 2.4.20 and this source from 2.4.22
Splitt hardware depens into file sl811-x86.h and sl811-arm.h.
Functions as inline.

23.09.2003 hne
Move Hardware depend header sl811-arm.h into include/asm-arm/sl811-hw.h.
GPRD as parameter.

24.09.2003 hne
Use Offset from ADDR to DATA instand of direct io.

03.10.2003 hne
Low level only for port io into hardware-include.
*/

#ifndef __LINUX_SL811_HW_H
#define __LINUX_SL811_HW_H

#define MAX_CONTROLERS		1	/* Max number of sl811 controllers */
					/* Always 1 for this architecture! */

#define SIZEOF_IO_REGION	1	/* Size for request/release region */

#define OFFSET_DATA_REG data_off	/* Offset from ADDR_IO to DATA_IO (future) */
					/* Can change by arg */

static int io = 0xf100000e;	/* Base addr_io */
static int data_off = 1;	/* Offset from addr_io to addr_io */
static int irq = 44;		/* also change gprd !!! */
static int gprd = 23;		/* also change irq  !!! */

MODULE_PARM(io,"i");
MODULE_PARM_DESC(io,"sl811 address io port 0xf100000e");
MODULE_PARM(data_off,"i");
MODULE_PARM_DESC(data_off,"sl811 data io port offset from address port (default 1)");
MODULE_PARM(irq,"i");
MODULE_PARM_DESC(irq,"sl811 irq 44(default)");
MODULE_PARM(gprd,"i");
MODULE_PARM_DESC(gprd,"sl811 GPRD port 23(default)");

/*
 * Low level: Read from Data port [arm]
 */
static __u8 inline sl811_read_data (struct sl811_hc *hc)
{
	__u8 data;
	data = readb(hc->data_io);
	rmb();
	return data;
}

/*
 * Low level: Write to index register [arm]
 */
static void inline sl811_write_index (struct sl811_hc *hc, __u8 index)
{
	writeb(index, hc->addr_io);
	wmb();
}

/*
 * Low level: Write to Data port [arm]
 */
static void inline sl811_write_data (struct sl811_hc *hc, __u8 data)
{
	writeb(data, hc->data_io);
	wmb();
}

/*
 * Low level: Write to index register and data port [arm]
 */
static void inline sl811_write_index_data (struct sl811_hc *hc, __u8 index, __u8 data)
{
	writeb(index, hc->addr_io);
	writeb(data, hc->data_io);
	wmb();
}


/*
 * This	function is board specific.  It	sets up	the interrupt to
 * be an edge trigger and trigger on the rising	edge
 */
static void inline sl811_init_irq(void)
{
	GPDR &=	~(1<<gprd);
	set_GPIO_IRQ_edge(1<<gprd, GPIO_RISING_EDGE);
}

/*****************************************************************
 *
 * Function Name: release_regions [arm]
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

	if (hc->data_io)
		release_region(hc->data_io, SIZEOF_IO_REGION);
	hc->data_io = 0;
}

/*****************************************************************
 *
 * Function Name: request_regions [arm]
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

	if (!request_region(data_io, SIZEOF_IO_REGION, MODNAME)) {
		PDEBUG(3, "request address %d failed", data_io);
		/* release_region(hc->addr_io, SIZEOF_IO_REGION); */
		return -EBUSY;
	}
	hc->data_io =	data_io;

	return 0;
}

#endif // __LINUX_SL811_HW_H
