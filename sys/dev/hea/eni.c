/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * Efficient ENI adapter support
 * -----------------------------
 *
 * Module supports PCI interface to ENI adapter
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/eventhandler.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_var.h>

#ifndef	lint
__RCSID("@(#) $FreeBSD$");
#endif

/*
 * Typedef local functions
 */
static const char	*eni_pci_probe __P((pcici_t, pcidi_t));
static void	eni_pci_attach __P((pcici_t, int));
static int 	eni_get_ack __P((Eni_unit *));
static int	eni_get_sebyte __P((Eni_unit *));
static void	eni_read_seeprom __P((Eni_unit *));
#if BSD < 199506
static int	eni_pci_shutdown __P((struct kern_devconf *, int));
#else
static void	eni_pci_shutdown __P((void *, int));
#endif
static void	eni_pci_reset __P((Eni_unit *));

/*
 * Used by kernel to return number of claimed devices
 */
static u_long eni_nunits;

static struct pci_device eni_pci_device = {
	ENI_DEV_NAME,
	eni_pci_probe,
	eni_pci_attach,
	&eni_nunits,
#if BSD < 199506
	eni_pci_shutdown
#else
	NULL
#endif
};

COMPAT_PCI_DRIVER (eni_pci, eni_pci_device);

/*
 * Called by kernel with PCI device_id which was read from the PCI
 * register set. If the identified vendor is Efficient, see if we
 * recognize the particular device. If so, return an identifying string,
 * if not, return null.
 *
 * Arguments:
 *	config_id	PCI config token
 *	device_id	contents of PCI device ID register
 *
 * Returns:
 *	string		Identifying string if we will handle this device
 *	NULL		unrecognized vendor/device
 *
 */
static const char *
eni_pci_probe ( pcici_t config_id, pcidi_t device_id )
{

	if ( (device_id & 0xFFFF) == EFF_VENDOR_ID ) {
		switch ( (device_id >> 16) ) {
			case EFF_DEV_ID:
				return ( "Efficient ENI ATM Adapter" );
/* NOTREACHED */
				break;
		}
	}

	return ( NULL );
}

/*
 * The ENI-155p adapter uses an ATMEL AT24C01 serial EEPROM to store
 * configuration information. The SEEPROM is accessed via two wires,
 * CLOCK and DATA, which are accessible via the PCI configuration
 * registers. The following macros manipulate the lines to access the
 * SEEPROM. See http://www.atmel.com/atmel/products/prod162.htm for
 * a description of the AT24C01 part. Value to be read/written is
 * part of the per unit structure.
 */
/*
 * Write bits to SEEPROM
 */
#define	WRITE_SEEPROM()	(						\
    {									\
	(void) pci_conf_write ( eup->eu_pcitag, SEEPROM,		\
		eup->eu_sevar );					\
	DELAY(SEPROM_DELAY);						\
    }									\
)
/*
 * Stobe first the DATA, then the CLK lines high
 */
#define	STROBE_HIGH()	(						\
    {									\
	eup->eu_sevar |= SEPROM_DATA; WRITE_SEEPROM();			\
	eup->eu_sevar |= SEPROM_CLK;  WRITE_SEEPROM();			\
    }									\
)
/*
 * Strobe first the CLK, then the DATA lines high
 */
#define	INV_STROBE_HIGH()	(					\
    {									\
	eup->eu_sevar |= SEPROM_CLK;  WRITE_SEEPROM();			\
	eup->eu_sevar |= SEPROM_DATA; WRITE_SEEPROM();			\
    }									\
)
/*
 * Strobe first the CLK, then the DATA lines low - companion to
 * STROBE_HIGH()
 */
#define	STROBE_LOW()	(						\
    {									\
	eup->eu_sevar &= ~SEPROM_CLK;  WRITE_SEEPROM();			\
	eup->eu_sevar &= ~SEPROM_DATA; WRITE_SEEPROM();			\
    }									\
)
/*
 * Strobe first the DATA, then the CLK lines low - companion to
 * INV_STROBE_HIGH()
 */
#define	INV_STROBE_LOW()	(					\
    {									\
	eup->eu_sevar &= ~SEPROM_DATA; WRITE_SEEPROM();			\
	eup->eu_sevar &= ~SEPROM_CLK;  WRITE_SEEPROM();			\
    }									\
)
/*
 * Strobe the CLK line high, then low
 */
#define	STROBE_CLK()	(						\
    {									\
	eup->eu_sevar |= SEPROM_CLK;   WRITE_SEEPROM();			\
	eup->eu_sevar &= ~SEPROM_CLK;  WRITE_SEEPROM();			\
    }									\
)

/*
 * Look for a positive ACK from the SEEPROM. Cycle begins by asserting
 * the DATA line, then the CLK line. The DATA line is then read to
 * retrieve the ACK status, and then the cycle is finished by deasserting
 * the CLK line, and asserting the DATA line.
 *
 * Arguments:
 *	eup		pointer to per unit structure
 *
 * Returns:
 *	0/1		value of ACK
 *
 */
static int
eni_get_ack ( eup )
	Eni_unit	*eup;
{
	int		ack;

	STROBE_HIGH();
	/*
	 * Read DATA line from SEPROM
	 */
	eup->eu_sevar = pci_conf_read ( eup->eu_pcitag, SEEPROM );
	DELAY ( SEPROM_DELAY );
	ack = eup->eu_sevar & SEPROM_DATA;

	eup->eu_sevar &= ~SEPROM_CLK;
	WRITE_SEEPROM ();
	eup->eu_sevar |= SEPROM_DATA;
	WRITE_SEEPROM ();

	return ( ack );
}

/*
 * Read a byte from the SEEPROM. Data is read as 8 bits. There are two types
 * of read operations. The first is a single byte read, the second is
 * multiple sequential bytes read. Both cycles begin with a 'START' operation,
 * followed by a memory address word. Following the memory address, the
 * SEEPROM will send a data byte, followed by an ACK. If the host responds
 * with a 'STOP' operation, then a single byte cycle is performed. If the
 * host responds with an 'ACK', then the memory address is incremented, and
 * the next sequential memory byte is serialized.
 *
 * Arguments:
 *	eup		pointer to per unit structure
 *
 * Returns:
 *	val		value of byte read from SEEPROM
 *
 */
static int
eni_get_sebyte( eup )
	Eni_unit	*eup;
{
	int	i;
	int	data;
	int	rval;

	/* Initial value */
	rval = 0;
	/* Read 8 bits */
	for ( i = 0; i < 8; i++ ) {
		/* Shift bits to left so the next bit goes to position 0 */
		rval <<= 1;
		/* Indicate we're ready to read bit */
		STROBE_HIGH();
		/*
		 * Read DATA line from SEPROM
		 */
		data = pci_conf_read ( eup->eu_pcitag, SEEPROM );
		DELAY ( SEPROM_DELAY );
		/* (Possibly) mask bit into accumulating value */
		if ( data & SEPROM_DATA )
			rval |= 1;		/* If DATA bit '1' */
		/* Indicate we're done reading this bit */
		STROBE_LOW();
	}

	/* Return acquired byte */
	return ( rval );
}

/*
 * The AT24C01 is a 1024 bit part organized as 128 words by 8 bits.
 * We will read the entire contents into the per unit structure. Later,
 * we'll retrieve the MAC address and serial number from the data read.
 *
 * Arguments:
 *	eup		pointer to per unit structure
 *
 * Returns:
 *	none
 *
 */
static void
eni_read_seeprom ( eup )
	Eni_unit	*eup;
{
	int	addr;
	int	i, j;

	/*
	 * Set initial state
	 */
	eup->eu_sevar = SEPROM_DATA | SEPROM_CLK;
	WRITE_SEEPROM ();

	/* Loop for all bytes */
	for ( i = 0; i < SEPROM_SIZE ; i++ ) {
		/* Send START operation */
		STROBE_HIGH();
		INV_STROBE_LOW();

		/*
		 * Send address. Addresses are sent as 7 bits plus
		 * last bit high.
		 */
		addr = ((i) << 1) + 1;
		/*
		 * Start with high order bit first working toward low
		 * order bit.
		 */
		for ( j = 7; j >= 0; j-- ) {
			/* Set current bit value */
			eup->eu_sevar = ( addr >> j ) & 1 ?
			    eup->eu_sevar | SEPROM_DATA :
				eup->eu_sevar & ~SEPROM_DATA;
			WRITE_SEEPROM ();
			/* Indicate we've sent it */
			STROBE_CLK();
		}
		/*
		 * We expect a zero ACK after sending the address
		 */
		if ( !eni_get_ack ( eup ) ) {
			/* Address okay - read data byte */
			eup->eu_seeprom[i] = eni_get_sebyte ( eup );
			/* Grab but ignore the ACK op */
			(void) eni_get_ack ( eup );
		} else {
			/* Address ACK was bad - can't retrieve data byte */
			eup->eu_seeprom[i] = 0xff;
		}
	}

	return;
}

/*
 * The kernel has found a device which we are willing to support.
 * We are now being called to do any necessary work to make the
 * device initially usable. In our case, this means allocating
 * structure memory, configuring registers, mapping device
 * memory, setting pointers, registering with the core services,
 * and doing the initial PDU processing configuration.
 *
 * Arguments:
 *	config_id		PCI device token
 *	unit			instance of the unit
 *
 * Returns:
 *	none		
 *
 */
static void
eni_pci_attach ( pcici_t config_id, int unit )
{
	vm_offset_t	va;
	vm_offset_t	pa;
	Eni_unit	*eup;
	long		val;

	/*
	 * Just checking...
	 */
	if ( unit >= ENI_MAX_UNITS ) {
		log ( LOG_ERR, "%s%d: too many devices\n",
			ENI_DEV_NAME, unit );
		return;
	}

	/*
	 * Make sure this isn't a duplicate unit
	 */
	if ( eni_units[unit] != NULL )
		return;

	/*
	 * Allocate a new unit structure
	 */
	eup = (Eni_unit *) atm_dev_alloc ( sizeof(Eni_unit), sizeof(int), 0 );
	if ( eup == NULL )
		return;

	/*
	 * Start initializing it
	 */
	eup->eu_unit = unit;
	eup->eu_mtu = ENI_IFF_MTU;
	eup->eu_pcitag = config_id;
	eup->eu_ioctl = eni_atm_ioctl;
	eup->eu_instvcc = eni_instvcc;
	eup->eu_openvcc = eni_openvcc;
	eup->eu_closevcc = eni_closevcc;
	eup->eu_output = eni_output;
	eup->eu_vcc_pool = &eni_vcc_pool;
	eup->eu_nif_pool = &eni_nif_pool;

 	/*
	 * Enable Memory Mapping / Bus Mastering 
	 */
	val = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	val |= (PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, val);

	/*
	 * Map in adapter RAM
	 */
	val = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	if ((val & PCIM_CMD_MEMEN) == 0) {
		log(LOG_ERR, "%s%d: memory mapping not enabled\n", 
			ENI_DEV_NAME, unit);
		goto failed;
	}
	if ( ( pci_map_mem ( config_id, PCI_MAP_REG_START, &va, &pa ) ) == 0 )
	{
		log(LOG_ERR, "%s%d: unable to map memory\n", 
			ENI_DEV_NAME, unit);
		goto failed;
	}
	/*
	 * Map okay - retain address assigned
	 */
	eup->eu_base = (Eni_mem)va;
	eup->eu_ram = (Eni_mem)(eup->eu_base + RAM_OFFSET);

	/*
	 * Map memory structures into adapter space
	 */
	eup->eu_suni = (Eni_mem)(eup->eu_base + SUNI_OFFSET);
	eup->eu_midway = (Eni_mem)(eup->eu_base + MIDWAY_OFFSET);
	eup->eu_vcitbl = (VCI_Table *)(eup->eu_base + VCITBL_OFFSET);
	eup->eu_rxdma = (Eni_mem)(eup->eu_base + RXQUEUE_OFFSET);
	eup->eu_txdma = (Eni_mem)(eup->eu_base + TXQUEUE_OFFSET);
	eup->eu_svclist = (Eni_mem)(eup->eu_base + SVCLIST_OFFSET);
	eup->eu_servread = 0;

	/*
	 * Reset the midway chip
	 */
	eup->eu_midway[MIDWAY_ID] = MIDWAY_RESET;

	/*
	 * Size and test adapter memory. Initialize our adapter memory
	 * allocater.
	 */
	if ( eni_init_memory ( eup ) < 0 ) {
		/*
		 * Adapter memory test failed. Clean up and
		 * return.
		 */
		log(LOG_ERR, "%s%d: memory test failed\n", 
			ENI_DEV_NAME, unit);
		goto failed;
	}

	/*
	 * Read the contents of the SEEPROM
	 */
	eni_read_seeprom ( eup );
	/*
	 * Copy MAC address to PIF and config structures
	 */
	KM_COPY ( (caddr_t)&eup->eu_seeprom[SEPROM_MAC_OFF],
	    (caddr_t)&eup->eu_pif.pif_macaddr, sizeof(struct mac_addr) );
	eup->eu_config.ac_macaddr = eup->eu_pif.pif_macaddr;

	/*
	 * Copy serial number into config space
	 */
	eup->eu_config.ac_serial =
		ntohl(*(u_long *)&eup->eu_seeprom[SEPROM_SN_OFF]);

	/*
	 * Convert Endianess on DMA
	 */
	val = pci_conf_read ( config_id, PCI_CONTROL_REG );
	val |= ENDIAN_SWAP_DMA;
	pci_conf_write ( config_id, PCI_CONTROL_REG, val );

	/*
	 * Map interrupt in
	 */
	if ( !pci_map_int ( config_id, eni_intr, (void *)eup, &net_imask ) )
	{
		log(LOG_ERR, "%s%d: unable to map interrupt\n", 
			ENI_DEV_NAME, unit);
		goto failed;
	}

	/*
	 * Setup some of the adapter configuration
	 */
	/*
	 * Get MIDWAY ID
	 */
	val = eup->eu_midway[MIDWAY_ID];
	eup->eu_config.ac_vendor = VENDOR_ENI;
	eup->eu_config.ac_vendapi = VENDAPI_ENI_1;
	eup->eu_config.ac_device = DEV_ENI_155P;
	eup->eu_config.ac_media = val & MEDIA_MASK ? MEDIA_UTP155 : MEDIA_OC3C;
	eup->eu_pif.pif_pcr = ATM_PCR_OC3C;
	eup->eu_config.ac_bustype = BUS_PCI;
	eup->eu_config.ac_busslot = config_id->bus << 8 | config_id->slot;

	/*
	 * Make a hw version number from the ID register values.
	 * Format: {Midway ID}.{Mother board ID}.{Daughter board ID}
	 */
	snprintf ( eup->eu_config.ac_hard_vers,
	    sizeof ( eup->eu_config.ac_hard_vers ),
		"%ld/%ld/%ld",
		(val >> ID_SHIFT) & ID_MASK,
		(val >> MID_SHIFT) & MID_MASK,
		(val >> DID_SHIFT) & DID_MASK );

	/*
	 * There is no software version number
	 */
	eup->eu_config.ac_firm_vers[0] = '\0';

	/*
	 * Save device ram info for user-level programs
	 * NOTE: This really points to start of EEPROM
	 * and includes all the device registers in the
	 * lower 2 Megabytes.
	 */
	eup->eu_config.ac_ram = (long)eup->eu_base;
	eup->eu_config.ac_ramsize = eup->eu_ramsize + ENI_REG_SIZE;

	/*
	 * Setup max VPI/VCI values
	 */
	eup->eu_pif.pif_maxvpi = ENI_MAX_VPI;
	eup->eu_pif.pif_maxvci = ENI_MAX_VCI;

	/*
	 * Register this interface with ATM core services
	 */
	if ( atm_physif_register
		( (Cmn_unit *)eup, ENI_DEV_NAME, eni_services ) != 0 )
	{
		/*
		 * Registration failed - back everything out
		 */
		log(LOG_ERR, "%s%d: atm_physif_register failed\n", 
			ENI_DEV_NAME, unit);
		goto failed;
	}

	eni_units[unit] = eup;

#if BSD >= 199506
	/*
	 * Add hook to out shutdown function
	 */
	EVENTHANDLER_REGISTER(shutdown_post_sync, eni_pci_shutdown, eup,
			      SHUTDOWN_PRI_DEFAULT);
	
#endif

	/*
	 * Initialize driver processing
	 */
	if ( eni_init ( eup ) ) {
		log(LOG_ERR, "%s%d: adapter init failed\n", 
			ENI_DEV_NAME, unit);
		goto failed;
	}

	return;

failed:
	/*
	 * Attach failed - clean up
	 */
	eni_pci_reset(eup);
	(void) pci_unmap_int(config_id);
	atm_dev_free(eup);
	return;
}

/*
 * Device reset routine
 *
 * Arguments:
 *	eup			pointer to per unit structure
 *
 * Returns:
 *	none
 *
 */
static void
eni_pci_reset ( eup )
	Eni_unit *eup;
{

	/*
	 * We should really close down any open VCI's and
	 * release all memory (TX and RX) buffers. For now,
	 * we assume we're shutting the card down for good.
	 */

	if (eup->eu_midway) {
		/*
		 * Issue RESET command to Midway chip
		 */
		eup->eu_midway[MIDWAY_ID] = MIDWAY_RESET;

		/*
		 * Delay to allow everything to terminate
		 */
		DELAY ( MIDWAY_DELAY );
	}

	return;
}

#if BSD < 199506
/*
 * Device shutdown routine
 *
 * Arguments:
 *	kdc		pointer to device's configuration table
 *	force		forced shutdown flag
 *
 * Returns:
 * 	none
 *
 */
static int
eni_pci_shutdown ( kdc, force )
	struct kern_devconf	*kdc;
	int			force;
{
	Eni_unit	*eup = NULL;

	if ( kdc->kdc_unit < eni_nunits ) {

		eup = eni_units[kdc->kdc_unit];
		if ( eup != NULL ) {
			/* Do device reset */
			eni_pci_reset ( eup );
		}
	}

	(void) dev_detach ( kdc );
	return ( 0 );
}
#else
/*
 * Device shutdown routine
 *
 * Arguments:
 *	howto		type of shutdown
 *	eup		pointer to device unit structure
 *
 * Returns:
 *	none
 *
 */
static void
eni_pci_shutdown ( eup, howto )
	void	*eup;
	int	howto;
{

	/* Do device reset */
	eni_pci_reset ( eup );

}
#endif	/* BSD < 199506 */
