/*
File: include/asm-i386/hc_sl811-hw.h

18.11.2002 hne@ist1.de
Use Kernel 2.4.19 and Prepatch 2.4.20
Splitt hardware depenc into file hc_sl811-x86.c and hc_sl811-arm.c.

20.11.2002 HNE
READ/WRITE_(INDEX_)DATA using for fast hardware access.

02.09.2003 HNE
IO Region size only 2 (old 16)

18.09.2003 HNE
Handle multi instances. For two controller on one board.
Portcheck in low level source (DEBUG) only.

03.10.2003 HNE
Low level only for port io into hardware-include.

*/

#ifdef MODULE

#define MAX_CONTROLERS	2	/* Max number of SL811 controllers per module */
static int io_base	= 0x220;
static int irq	= 12;

MODULE_PARM(io_base,"i");
MODULE_PARM_DESC(io_base,"sl811 base address 0x220");
MODULE_PARM(irq,"i");
MODULE_PARM_DESC(irq,"IRQ 12 (default)");

#endif // MODULE

/* Define general IO Macros for our platform (hne) */
#define SIZEOF_IO_REGION	2	/* Size for request/release region */

// #define sl811_write_index(hp,i) outb ((i), hp->hcport)
// #define sl811_write_data(hp,d) outb ((d), hp->hcport+1)
// #define sl811_write_index_data(hp,i,d) outw ((i)|(((__u16)(d)) << 8), hp->hcport)
// #define sl811_read_data(hp) ((__u8) inb (hp->hcport+1))

/*
 * Low level: Read from Data port [x86]
 */
static __u8 inline sl811_read_data (hcipriv_t *hp)
{
	return ((__u8) inb (hp->hcport+1));
}

/*
 * Low level: Write to index register [x86]
 */
static void inline sl811_write_index (hcipriv_t *hp, __u8 index)
{
	outb (index, hp->hcport);
}

/*
 * Low level: Write to Data port [x86]
 */
static void inline sl811_write_data (hcipriv_t *hp, __u8 data)
{
	outb (data, hp->hcport+1);
}

/*
 * Low level: Write to index register and data port [x86]
 */
static void inline sl811_write_index_data (hcipriv_t *hp, __u8 index, __u8 data)
{
	outw (index|(((__u16)data) << 8), hp->hcport);
}

/*****************************************************************
 *
 * Function Name: init_irq [x86]
 *
 * This function is board specific.  It sets up the interrupt to
 * be an edge trigger and trigger on the rising edge
 *
 * Input: none
 *
 * Return value  : none
 *
 *****************************************************************/
static void inline init_irq (void)
{
	/* nothing */
}

/*****************************************************************
 *
 * Function Name: release_regions [x86]
 *
 * This function is board specific.  It frees all io address.
 *
 * Input: hcipriv_t *
 *
 * Return value  : none
 *
 *****************************************************************/
static void inline sl811_release_regions (hcipriv_t *hp)
{
	DBGFUNC ("Enter release_regions\n");
	if (hp->hcport > 0) {
		release_region (hp->hcport, SIZEOF_IO_REGION);
		hp->hcport = 0;
	}

	/* hcport2 unused for x86 */
}

/*****************************************************************
 *
 * Function Name: request_regions [x86]
 *
 * This function is board specific. It request all io address and
 * maps into memory (if can).
 *
 * Input: hcipriv_t *
 *
 * Return value  : 0 = OK
 *
 *****************************************************************/
static int inline sl811_request_regions (hcipriv_t *hp, int base1, int base2)
{
	DBGFUNC ("Enter request_regions\n");
	if (!request_region (base1, SIZEOF_IO_REGION, "SL811")) {
		DBGERR ("request address 0x%X %d failed\n", base1, SIZEOF_IO_REGION);
		return -EBUSY;
	}
	hp->hcport = base1;

	/* hcport2 unused for x86 */
	return 0;
}

