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

#ifdef COMPILING_LINT 
#warning "The fore pci driver is broken and is not compiled with LINT"
#else 

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Loadable kernel module and device identification support
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/eventhandler.h>
#include <net/if.h>
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
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <dev/hfa/fore.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>
#include <dev/hfa/fore_var.h>
#include <dev/hfa/fore_include.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static int	fore_start(void);
static const char *	fore_pci_probe(pcici_t, pcidi_t);
static void	fore_pci_attach(pcici_t, int);
static void	fore_pci_shutdown(void *, int);
static void	fore_unattach(Fore_unit *);
static void	fore_reset(Fore_unit *);

#ifndef COMPAT_OLDPCI
#error "The fore device requires the old pci compatibility shims"
#endif

/*
 * Local variables
 */
static int	fore_inited = 0;

/*
 * Driver entry points
 */

static	u_long	fore_pci_count = 0;

static struct pci_device fore_pci_device = {
	FORE_DEV_NAME,
	fore_pci_probe,
	fore_pci_attach,
	&fore_pci_count,
	NULL
};

COMPAT_PCI_DRIVER(fore_pci, fore_pci_device);


/*
 * Initialize driver processing
 * 
 * This will be called during module loading.  Not much to do here, as
 * we must wait for our identify/attach routines to get called before
 * we know what we're in for.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	startup was successful 
 *	errno	startup failed - reason indicated
 *
 */
static int
fore_start()
{

	/*
	 * Verify software version
	 */
	if (atm_version != ATM_VERSION) {
		log(LOG_ERR, "version mismatch: fore=%d.%d kernel=%d.%d\n",
			ATM_VERS_MAJ(ATM_VERSION), ATM_VERS_MIN(ATM_VERSION),
			ATM_VERS_MAJ(atm_version), ATM_VERS_MIN(atm_version));
		return (EINVAL);
	}

	/*
	 * Initialize DMA mapping
	 */
	DMA_INIT();

	/*
	 * Start up watchdog timer
	 */
	atm_timeout(&fore_timer, ATM_HZ * FORE_TIME_TICK, fore_timeout);

	fore_inited = 1;

	return (0);
}




/*
 * Device probe routine
 * 
 * Determine if this driver will support the identified device.  If we claim
 * to support the device, our attach routine will (later) be called for the
 * device.
 *
 * Arguments:
 *	config_id	device's PCI configuration ID
 *	device_id	device's PCI Vendor/Device ID
 *
 * Returns:
 *	name 	device identification string
 *	NULL	device not claimed by this driver
 *
 */
static const char *
fore_pci_probe(config_id, device_id)
	pcici_t	config_id;
	pcidi_t	device_id;
{

	/*
	 * Initialize driver stuff
	 */
	if (fore_inited == 0) {
		if (fore_start())
			return (NULL);
	}

	if ((device_id & 0xffff) != FORE_VENDOR_ID)
		return (NULL);

	if (((device_id >> 16) & 0xffff) == FORE_PCA200E_ID)
                return ("FORE Systems PCA-200E ATM");

	return (NULL);
}


/*
 * Device attach routine
 * 
 * Attach a device we've previously claimed to support.  Walk through its
 * register set and map, as required.  Determine what level the device will
 * be interrupting at and then register an interrupt handler for it.  If we
 * succeed, then reset the adapter and initialize the microcode.
 * Last, register the interface with the kernel ATM services.
 *
 * Arguments:
 *	config_id	device's PCI configuration ID
 *	unit		device unit number
 *
 * Returns:
 *	none
 *
 */
static void
fore_pci_attach(config_id, unit)
	pcici_t	config_id;
	int	unit;
{
	Fore_unit	*fup;
	vm_offset_t	va;
	vm_offset_t	pa;
	pcidi_t		device_id;
	long		val;
	int		err_count = BOOT_LOOPS;

	/*
	 * Just checking...
	 */
	if (unit >= FORE_MAX_UNITS) {
		log(LOG_ERR, "%s%d: too many devices\n", 
			FORE_DEV_NAME, unit);
		return;
	}

	/*
	 * Make sure this isn't a duplicate unit
	 */
	if (fore_units[unit] != NULL)
		return;

	/*
	 * Allocate a new unit structure
	 */
	fup = (Fore_unit *) atm_dev_alloc(sizeof(Fore_unit), sizeof(int), 0);
	if (fup == NULL)
		return;

	/*
	 * Start initializing it
	 */
	fup->fu_unit = unit;
	fup->fu_mtu = FORE_IFF_MTU;
	fup->fu_pcitag = config_id;
	fup->fu_vcc_pool = &fore_vcc_pool;
	fup->fu_nif_pool = &fore_nif_pool;
	fup->fu_ioctl = fore_atm_ioctl;
	fup->fu_instvcc = fore_instvcc;
	fup->fu_openvcc = fore_openvcc;
	fup->fu_closevcc = fore_closevcc;
	fup->fu_output = fore_output;
	callout_handle_init(&fup->fu_thandle);

	/*
	 * Get our device type
	 */
	device_id = pci_conf_read ( config_id, PCI_ID_REG );
	switch ((device_id >> 16) & 0xffff) {

	case FORE_PCA200E_ID:
		fup->fu_config.ac_device = DEV_FORE_PCA200E;
		break;

	default:
		fup->fu_config.ac_device = DEV_UNKNOWN;
	}

	/*
	 * Enable Memory Mapping / Bus Mastering 
	 */
	val = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	val |= (PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, val);

	/*
	 * Map RAM
	 */
	val = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	if ((val & PCIM_CMD_MEMEN) == 0) {
		log(LOG_ERR, "%s%d: memory mapping not enabled\n", 
			FORE_DEV_NAME, unit);
		goto failed;
	}
	if ((pci_map_mem(config_id, PCA200E_PCI_MEMBASE, &va, &pa)) == 0) {
		log(LOG_ERR, "%s%d: unable to map memory\n", 
			FORE_DEV_NAME, unit);
		goto failed;
	}
	fup->fu_ram = (Fore_mem *)va;
	fup->fu_ramsize = PCA200E_RAM_SIZE;
	fup->fu_mon = (Mon960 *)(fup->fu_ram + MON960_BASE);
	fup->fu_ctlreg = (Fore_reg *)(va + PCA200E_HCR_OFFSET);
	fup->fu_imask = (Fore_reg *)(va + PCA200E_IMASK_OFFSET);
	fup->fu_psr = (Fore_reg *)(va + PCA200E_PSR_OFFSET);

	/*
	 * Convert Endianess of Slave RAM accesses
	 */
	val = pci_conf_read(config_id, PCA200E_PCI_MCTL);
	val |= PCA200E_MCTL_SWAP;
	pci_conf_write(config_id, PCA200E_PCI_MCTL, val);

	/*
	 * Map interrupt in
	 */
	if ( !pci_map_int( config_id, fore_intr, fup, &net_imask ) ) {
		log(LOG_ERR, "%s%d: unable to map interrupt\n", 
			FORE_DEV_NAME, unit);
		goto failed;
	}

	/*
	 * Poke the hardware - boot the CP and prepare it for downloading
	 */
	fore_reset(fup);

	/*
	 * Wait for the monitor to perform self-test
	 */
	while (CP_READ(fup->fu_mon->mon_bstat) != BOOT_MONREADY) {
		if (CP_READ(fup->fu_mon->mon_bstat) == BOOT_FAILTEST) {
			log(LOG_ERR, "%s%d: failed self-test\n", 
				FORE_DEV_NAME, unit);
			goto failed;
		} else if ( --err_count == 0 ) {
			log(LOG_ERR, "%s%d: unable to boot - status=0x%lx\n", 
				FORE_DEV_NAME, unit,
				(u_long)CP_READ(fup->fu_mon->mon_bstat));
			goto failed;
		}
		DELAY ( BOOT_DELAY );
	}

	/*
	 * Setup the adapter config info - at least as much as we can
	 */
	fup->fu_config.ac_vendor = VENDOR_FORE;
	fup->fu_config.ac_vendapi = VENDAPI_FORE_1;
	fup->fu_config.ac_media = MEDIA_OC3C;
	fup->fu_pif.pif_pcr = ATM_PCR_OC3C;
	fup->fu_config.ac_bustype = BUS_PCI;
	fup->fu_config.ac_busslot = config_id->bus << 8 | config_id->slot;

	/*
	 * Save device ram info for user-level programs
	 */
	fup->fu_config.ac_ram = (long)fup->fu_ram;
	fup->fu_config.ac_ramsize = fup->fu_ramsize;

	/*
	 * Set device capabilities
	 */
	fup->fu_pif.pif_maxvpi = FORE_MAX_VPI;
	fup->fu_pif.pif_maxvci = FORE_MAX_VCI;

	/*
	 * Register this interface with ATM core services
	 */
        if ( atm_physif_register
			((Cmn_unit *)fup, FORE_DEV_NAME, fore_services) != 0 )
	{
		/*
		 * Registration failed - back everything out
		 */
		goto failed;
	}

	fore_units[unit] = fup;
	fore_nunits++;

	/*
	 * Add hook to our shutdown function
	 */
	EVENTHANDLER_REGISTER(shutdown_post_sync, fore_pci_shutdown, fup,
			      SHUTDOWN_PRI_DEFAULT);

	/*
	 * Initialize the CP microcode program.
	 */
	fore_initialize(fup);

	return;

failed:
	/*
	 * Unattach the device from the system
	 */
	fore_unattach(fup);

	/*
	 * Free any Fore-specific device resources
	 */
	fore_interface_free(fup);

	atm_dev_free(fup);

	return;
}

/*
 * Device shutdown routine
 * 
 * Arguments:
 *	howto		type of shutdown
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 *
 */
static void
fore_pci_shutdown(fup, howto)
	void		*fup;
	int		howto;
{

	fore_reset((Fore_unit *) fup);

	return;
}

/*
 * Device unattach routine
 * 
 * Reset the physical device, remove any pending timeouts, 
 * unmap any register sets, and unregister any interrupts.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */ 
static void
fore_unattach(fup)
	Fore_unit	*fup;
{


	/*
	 * Reset the board and return it to cold_start state.
	 * Hopefully, this will prevent use of resources as
	 * we're trying to free things up.
	 */
	fore_reset(fup);

	/*
	 * Lock out all device interrupts
	 */
	DEVICE_LOCK((Cmn_unit *)fup);

	/*
	 * Remove any pending timeout()'s
	 */
	(void)untimeout((KTimeout_ret(*)(void *))fore_initialize,
		(void *)fup, fup->fu_thandle);


	/*
	 * Unmap the device interrupt
	 */
	(void) pci_unmap_int(fup->fu_pcitag);

	/*
	 * Unmap memory
	 */
#ifdef notdef
	(void) pci_unmap_mem(fup->fu_pcitag, PCA200E_PCI_MEMBASE);
#endif

	DEVICE_UNLOCK((Cmn_unit *)fup);
}


/*
 * Device reset routine
 * 
 * Reset the physical device
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */ 
static void
fore_reset(fup)
	Fore_unit	*fup;
{
	int	s = splimp();

	/*
	 * Reset the board and return it to cold_start state
	 */
	if (fup->fu_mon)
		fup->fu_mon->mon_bstat = CP_WRITE(BOOT_COLDSTART);

	if (fup->fu_ctlreg) {

		if (fup->fu_config.ac_device == DEV_FORE_PCA200E) {
			/*
			 * Reset i960 by setting and clearing RESET
			 */
			PCA200E_HCR_INIT(*fup->fu_ctlreg, PCA200E_RESET);
			DELAY(10000);
			PCA200E_HCR_CLR(*fup->fu_ctlreg, PCA200E_RESET);
		 }
	}

	(void) splx(s);
	return;
}


#ifndef ATM_LINKED
/*
 *******************************************************************
 *
 * Loadable Module Support
 *
 *******************************************************************
 */


#ifdef notdef

/*
 * Driver entry points
 */
static struct cdevsw fore_cdev = {
	/* open */	noopen,
	/* close */	noclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	noname,
	/* maj */	-1,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};


/*
 * Loadable device driver module description
 */
MOD_DEV(fore, LM_DT_CHAR, -1, (void *)&fore_cdev);


/*
 * Loadable module support "load" entry point
 * 
 * This is the routine called by the lkm driver whenever the
 * modload(1) command is issued for this module.
 *
 * Arguments:
 *	lkmtp	pointer to lkm drivers's structure
 *	cmd	lkm command code
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
static int
fore_load(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(fore_doload());
}


/*
 * Loadable module support "unload" entry point
 * 
 * This is the routine called by the lkm driver whenever the
 * modunload(1) command is issued for this module.
 *
 * Arguments:
 *	lkmtp	pointer to lkm drivers's structure
 *	cmd	lkm command code
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
static int
fore_unload(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(fore_dounload());
}


/*
 * Loadable module support entry point
 * 
 * This is the routine called by the lkm driver for all loadable module
 * functions for this driver.  This routine name must be specified
 * on the modload(1) command.  This routine will be called whenever the
 * modload(1), modunload(1) or modstat(1) commands are issued for this
 * module.
 *
 * Arguments:
 *	lkmtp	pointer to lkm drivers's structure
 *	cmd	lkm command code
 *	ver	lkm version
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
int
fore_mod(lkmtp, cmd, ver)
	struct lkm_table	*lkmtp;
	int		cmd;
	int		ver;
{
	DISPATCH(lkmtp, cmd, ver, fore_load, fore_unload, lkm_nullcmd);
}
#endif	/* notdef */

#endif	/* ATM_LINKED */

#endif
