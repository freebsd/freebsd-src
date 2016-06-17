/*
 *	Defines for comx-hw-slicecom.c - MUNICH32X specific
 *
 *	Author:        Bartok Istvan <bartoki@itc.hu>
 *	Last modified: Tue Jan 11 14:27:36 CET 2000
 *
 *	:set tabstop=6
 */

#define TXBUFFER_SIZE	1536			/* Max mennyit tud a kartya hardver atvenni				*/
#define RXBUFFER_SIZE	(TXBUFFER_SIZE+4)	/* For Rx reasons it must be a multiple of 4, and =>4 (page 265)	*/
							/* +4 .. see page 265, bit FE							*/
							/* TOD: a MODE1-be nem is ezt teszem, hanem a TXBUFFER-t, lehet hogy nem is kell? */

//#define PCI_VENDOR_ID_SIEMENS			0x110a
#define PCI_DEVICE_ID_SIEMENS_MUNICH32X	0x2101

/*
 *	PCI config space registers (page 120)
 */

#define MUNICH_PCI_PCIRES	0x4c		/* 0xe0000 resets	the chip	*/


/*
 *	MUNICH slave register offsets relative to base_address[0] (PCI BAR1) (page 181):
 *	offsets are in bytes, registers are u32's, so we need a >>2 for indexing
 *	the int[] by byte offsets. Use it like:
 *
 *	bar1[ STAT ] = ~0L;  or
 *	x = bar1[ STAT ];
 */

#define CONF	(0x00 >> 2)
#define CMD		(0x04 >> 2)
#define STAT	(0x08 >> 2)
#define STACK	(0x08 >> 2)
#define IMASK	(0x0c >> 2)
#define PIQBA	(0x14 >> 2)
#define PIQL	(0x18 >> 2)
#define MODE1	(0x20 >> 2)
#define MODE2	(0x24 >> 2)
#define CCBA	(0x28 >> 2)
#define TXPOLL	(0x2c >> 2)
#define TIQBA	(0x30 >> 2)
#define TIQL	(0x34 >> 2)
#define RIQBA	(0x38 >> 2)
#define RIQL	(0x3c >> 2)
#define LCONF	(0x40 >> 2)		/* LBI Configuration Register		*/
#define LCCBA	(0x44 >> 2)		/* LBI Configuration Control Block	*/	/* DE: lehet hogy nem is kell? */
#define LTIQBA	(0x50 >> 2)		/* DE: lehet hogy nem is kell? page 210: LBI DMA Controller intq - nem hasznalunk DMA-t.. */
#define LTIQL	(0x54 >> 2)		/* DE: lehet hogy nem is kell? */
#define LRIQBA	(0x58 >> 2)		/* DE: lehet hogy nem is kell? */
#define LRIQL	(0x5c >> 2)		/* DE: lehet hogy nem is kell? */
#define LREG0	(0x60 >> 2)		/* LBI Indirect External Configuration register 0	*/
#define LREG1	(0x64 >> 2)
#define LREG2	(0x68 >> 2)
#define LREG3	(0x6c >> 2)
#define LREG4	(0x70 >> 2)
#define LREG5	(0x74 >> 2)
#define LREG6	(0x78 >> 2)		/* LBI Indirect External Configuration register 6		*/
#define LSTAT	(0x7c >> 2)		/* LBI Status Register							*/
#define GPDIR	(0x80 >> 2)		/* General Purpose Bus DIRection - 0..input, 1..output	*/
#define GPDATA	(0x84 >> 2)		/* General Purpose Bus DATA						*/


/*
 *	MUNICH commands: (they go into register CMD)
 */

#define CMD_ARPCM	0x01			/* Action Request Serial PCM Core	*/
#define CMD_ARLBI	0x02			/* Action Request LBI			*/


/*
 *	MUNICH event bits in the STAT, STACK, IMASK registers (page 188,189)
 */

#define STAT_PTI	(1 << 15)
#define STAT_PRI	(1 << 14)
#define STAT_LTI	(1 << 13)
#define STAT_LRI	(1 << 12)
#define STAT_IOMI	(1 << 11)
#define STAT_SSCI	(1 << 10)
#define STAT_LBII	(1 << 9)
#define STAT_MBI	(1 << 8)

#define STAT_TI	(1 << 6)
#define STAT_TSPA	(1 << 5)
#define STAT_RSPA	(1 << 4)
#define STAT_LBIF	(1 << 3)
#define STAT_LBIA	(1 << 2)
#define STAT_PCMF	(1 << 1)
#define STAT_PCMA	(1) 

/*
 *	We do not handle these (and do not touch their STAT bits) in the interrupt loop
 */

#define STAT_NOT_HANDLED_BY_INTERRUPT	(STAT_PCMF | STAT_PCMA)


/*
 *	MUNICH MODE1/MODE2 slave register fields (page 193,196)
 *	these are not all masks, MODE1_XX_YY are my magic values!
 */

#define MODE1_PCM_E1	(1 << 31)		/* E1, 2.048 Mbit/sec		*/
#define MODE1_TBS_4	(1 << 24)		/* TBS = 4 .. no Tx bit shift	*/
#define MODE1_RBS_4	(1 << 18)		/* RBS = 4 .. no Rx bit shift	*/
#define MODE1_REN		(1 << 15)		/* Rx Enable			*/
#define MODE1_MFL_MY	TXBUFFER_SIZE	/* Maximum Frame Length		*/
#define MODE1_MAGIC	(MODE1_PCM_E1 | MODE1_TBS_4 | MODE1_RBS_4 | MODE1_REN | MODE1_MFL_MY)

#define MODE2_HPOLL	(1 << 8)		/* Hold Poll			*/
#define MODE2_SPOLL	(1 << 7)		/* Slow Poll			*/
#define MODE2_TSF		(1)			/* real magic - discovered by probing :)	*/
// #define MODE2_MAGIC	(MODE2_TSF)
#define MODE2_MAGIC	(MODE2_SPOLL | MODE2_TSF)


/*
 *	LCONF bits (page 205)
 *	these are not all masks, LCONF_XX_YY are my magic values!
 */

#define LCONF_IPA			(1 << 31)	/* Interrupt Pass. Use 1 for FALC54							*/
#define LCONF_DCA			(1 << 30)	/* Disregard the int's for Channel A - DMSM does not try to handle them	*/
#define LCONF_DCB			(1 << 29)	/* Disregard the int's for Channel B						*/
#define LCONF_EBCRES		(1 << 22)	/* Reset LBI External Bus Controller, 0..reset, 1..normal operation	*/
#define LCONF_LBIRES		(1 << 21)	/* Reset LBI DMSM, 0..reset, 1..normal operation				*/
#define LCONF_BTYP_16DEMUX	(1 << 7)	/* 16-bit demultiplexed bus	*/
#define LCONF_ABM			(1 << 4)	/* Arbitration Master		*/

/* writing LCONF_MAGIC1 followed by a LCONF_MAGIC2 into LCONF resets the EBC and DMSM: */

#define LCONF_MAGIC1		(LCONF_BTYP_16DEMUX | LCONF_ABM | LCONF_IPA | LCONF_DCA | LCONF_DCB)
#define LCONF_MAGIC2		(LCONF_MAGIC1 | LCONF_EBCRES | LCONF_LBIRES)


/*
 *	LREGx magic values if a FALC54 is on the LBI (page 217)
 */

#define LREG0_MAGIC	0x00000264
#define LREG1_MAGIC	0x6e6a6b66
#define LREG2_MAGIC	0x00000264
#define LREG3_MAGIC	0x6e686966
#define LREG4_MAGIC	0x00000000
#define LREG5_MAGIC	( (7<<27) | (3<<24) | (1<<21) | (7<<3) | (2<<9) )


/*
 *	PCM Action Specification fields (munich_ccb_t.action_spec)
 */

#define CCB_ACTIONSPEC_IN			(1 << 15)	/* init				*/
#define CCB_ACTIONSPEC_ICO			(1 << 14)	/* init only this channel	*/
#define CCB_ACTIONSPEC_RES			(1 << 6)	/* reset all channels		*/
#define CCB_ACTIONSPEC_LOC			(1 << 5)
#define CCB_ACTIONSPEC_LOOP			(1 << 4)
#define CCB_ACTIONSPEC_LOOPI			(1 << 3)
#define CCB_ACTIONSPEC_IA			(1 << 2)


/*
 *	Interrupt Information bits in the TIQ, RIQ
 */

#define PCM_INT_HI	(1 << 12)
#define PCM_INT_FI	(1 << 11)
#define PCM_INT_IFC	(1 << 10)
#define PCM_INT_SF	(1 << 9)
#define PCM_INT_ERR	(1 << 8)
#define PCM_INT_FO	(1 << 7)
#define PCM_INT_FE2	(1 << 6)

#define PCM_INT_CHANNEL( info )	(info & 0x1F)


/*
 *	Rx status info in the rx_desc_t.status
 */

#define RX_STATUS_SF	(1 << 6)
#define RX_STATUS_LOSS	(1 << 5)
#define RX_STATUS_CRCO	(1 << 4)
#define RX_STATUS_NOB	(1 << 3)
#define RX_STATUS_LFD	(1 << 2)
#define RX_STATUS_RA	(1 << 1)
#define RX_STATUS_ROF	1 
