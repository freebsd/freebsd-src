/*
File: include/asm-arm/hc_sl811-hw.h
*/

/* The base_addr, data_reg_addr, and irq number are board specific.
 * The current values are design to run on the Accelent SA1110 IDP
 * NOTE: values need to modify for different development boards
 */

#define SIZEOF_IO_REGION	256	/* Size for request/release region */
static int base_addr	 = 0xd3800000;
static int data_reg_addr = 0xd3810000;
static int irq		 = 34;		/* Also change SL811_SET_GPDR! */
static int gprd 	 = 13;		/* also change irq  !!! */

MODULE_PARM (base_addr, "i");
MODULE_PARM_DESC (base_addr, "sl811 base address 0xd3800000");
MODULE_PARM (data_reg_addr, "i");
MODULE_PARM_DESC (data_reg_addr, "sl811 data register address 0xd3810000");
MODULE_PARM (irq, "i");
MODULE_PARM_DESC (irq, "IRQ 34 (default)");
MODULE_PARM(gprd,"i");
MODULE_PARM_DESC(gprd,"sl811 GPRD port 13(default)");

/*
 * Low level: Read from Data port [arm]
 */
static __u8 inline sl811_read_data (hcipriv_t *hp)
{
	__u8 data;
	data = readb (hp->hcport2);
	rmb ();
	return (data);
}

/*
 * Low level: Write to index register [arm]
 */
static void inline sl811_write_index (hcipriv_t *hp, __u8 index)
{
	writeb (offset, hp->hcport);
	wmb ();
}

/*
 * Low level: Write to Data port [arm]
 */
static void inline sl811_write_data (hcipriv_t *hp, __u8 data)
{
	writeb (data, hp->hcport2);
}

/*
 * Low level: Write to index register and data port [arm]
 */
static void inline sl811_write_index_data (hcipriv_t *hp, __u8 index, __u8 data)
{
	writeb (offset, hp->hcport);
	writeb (data, hp->hcport2);
	wmb ();
}

/*****************************************************************
 *
 * Function Name: init_irq [arm]
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
	GPDR &= ~(1 << gprd);
	set_GPIO_IRQ_edge (1 << gprd, GPIO_RISING_EDGE);
}

/*****************************************************************
 *
 * Function Name: release_regions [arm]
 *
 * This function is board specific.  It frees all io address.
 *
 * Input: hcipriv_t *
 *
 * Return value  : none
 *
 *****************************************************************/
static void inline release_regions (hcipriv_t *hp)
{
	if (hp->hcport > 0) {
		release_region (hp->hcport, SIZEOF_IO_REGION);
		hp->hcport = 0;
	}

	if (hp->hcport2 > 0) {
		release_region (hp->hcport2, SIZEOF_IO_REGION);
		hp->hcport2 = 0;
	}
}

/*****************************************************************
 *
 * Function Name: request_regions [arm]
 *
 * This function is board specific. It request all io address and
 * maps into memory (if can).
 *
 * Input: hcipriv_t *
 *
 * Return value  : 0 = OK
 *                
 *****************************************************************/
static int inline request_regions (hcipriv_t *hp, int addr1, int addr2)
{
	if (!request_region (addr1, SIZEOF_IO_REGION, "SL811 USB HOST")) {
		DBGERR ("request address %d failed", addr1);
		return -EBUSY;
	}
	hp->hcport = addr1;
	if (!hp->hcport) {
		DBGERR ("Error mapping SL811 Memory 0x%x", addr1);
	}

	if (!request_region (addr2, SIZEOF_IO_REGION, "SL811 USB HOST")) {
		DBGERR ("request address %d failed", addr2);
		return -EBUSY;
	}
	hp->hcport2 = addr2;
	if (!hp->hcport2) {
		DBGERR ("Error mapping SL811 Memory 0x%x", addr2);
	}

	return 0;
}

