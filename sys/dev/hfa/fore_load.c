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
 *	@(#) $FreeBSD: src/sys/dev/hfa/fore_load.c,v 1.13 1999/09/25 18:23:49 phk Exp $
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Loadable kernel module and device identification support
 *
 */

#include <dev/hfa/fore_include.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/dev/hfa/fore_load.c,v 1.13 1999/09/25 18:23:49 phk Exp $");
#endif


/*
 * Local functions
 */
static int	fore_start __P((void));
#ifdef sun
static int	fore_stop __P((void));
static int	fore_doload __P((void));
static int	fore_dounload __P((void));
static int	fore_identify __P((char *));
static int	fore_attach __P((struct devinfo *));
#endif
#ifdef __FreeBSD__
static const char *	fore_pci_probe __P((pcici_t, pcidi_t));
static void	fore_pci_attach __P((pcici_t, int));
#if BSD < 199506
static int	fore_pci_shutdown __P((struct kern_devconf *, int));
#else
static void	fore_pci_shutdown __P((void *, int));
#endif
#endif
static void	fore_unattach __P((Fore_unit *));
static void	fore_reset __P((Fore_unit *));


/*
 * Local variables
 */
static int	fore_inited = 0;

/*
 * Driver entry points
 */
#ifdef sun
static struct dev_ops	fore_ops = {
	1,		/* revision */
	fore_identify,	/* identify */
	fore_attach,	/* attach */
	NULL,		/* open */
	NULL,		/* close */
	NULL,		/* read */
	NULL,		/* write */
	NULL,		/* strategy */
	NULL,		/* dump */
	NULL,		/* psize */
	NULL,		/* ioctl */
	NULL,		/* reset */
	NULL		/* mmap */
};
#endif

#ifdef __FreeBSD__
static	u_long	fore_pci_count = 0;

static struct pci_device fore_pci_device = {
	FORE_DEV_NAME,
	fore_pci_probe,
	fore_pci_attach,
	&fore_pci_count,
#if BSD < 199506
	fore_pci_shutdown
#else
	NULL
#endif
};

COMPAT_PCI_DRIVER(fore_pci, fore_pci_device);
#endif


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


#ifdef sun

/*
 * Halt driver processing 
 * 
 * This will be called just prior to unloading the module from memory.
 * Everything we've setup since we've been loaded must be undone here.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	shutdown was successful 
 *	errno	shutdown failed - reason indicated
 *
 */
static int
fore_stop()
{
	int	err = 0;
	int	s = splimp();
	int	i;

	/*
	 * Stop the watchdog timer
	 */
	(void) atm_untimeout(&fore_timer);

	/*
	 * Clean up each device (if any)
	 */
	for ( i = 0; i < fore_nunits; i++ ) {
		Fore_unit	*fup = fore_units[i];

		if (fup == NULL)
			continue;

		/*
		 * Deregister device from kernel services
		 */
		if (err = atm_physif_deregister((Cmn_unit *)fup)) {
			(void) splx(s);
			return (err);
		}

		/*
		 * Unattach the device from the system
		 */
		fore_unattach(fup);

		/*
		 * Free any Fore-specific device resources
		 */
		fore_interface_free(fup);

		/*
		 * Free the unit structure
		 */
		atm_dev_free(fup);
		fore_units[i] = NULL;
	}

	fore_nunits = 0;

	/*
	 * Now free our global resources
	 */

	/*
	 * Release our storage pools
	 */
	atm_release_pool(&fore_vcc_pool);
	atm_release_pool(&fore_nif_pool);

	/*
	 * Release all DMA mappings
	 */
	DMA_RELEASE();

	fore_inited = 0;

	(void) splx(s);

	return (0);
}

/*
 * Device identify routine
 * 
 * Determine if this driver will support the named device.  If we claim to
 * support the device, our attach routine will (later) be called for the
 * device.
 *
 * Arguments:
 *	name	pointer to identifier string from device
 *
 * Returns:
 *	1 	driver claims support for this device
 *	0	device not claimed by this driver
 *
 */
static int
fore_identify(name)
	char	*name;
{
	int	ret = 0;
	int	i = 0;

	/*
	 * Initialize driver stuff
	 */
	if (fore_inited == 0) {
		if (fore_start())
			return (0);
	}

	while (fore_devices[i].fd_name) {
		if (strcmp(fore_devices[i].fd_name, name) == 0) {

			/*
			 * We support this device!!
			 */
			if (fore_nunits < FORE_MAX_UNITS) {
				fore_nunits++;
				ret = 1;
			} else {
				log(LOG_ERR,
					"fore_identify: Too many devices\n");
			}
			break;
		}
		i++;
	}
	return (ret);
}


/*
 * Device attach routine
 * 
 * Attach a device we've previously claimed to support.  Walk through its
 * register set and map, as required.  Determine what level the device will
 * be interrupting at and then register an interrupt handler for it.  If we
 * succeed, then reset the adapter and read useful info from its PROM.
 * Last, register the interface with the kernel ATM services.
 *
 * Arguments:
 *	devinfo_p	pointer to device information structure
 *
 * Returns:
 *	0 	attach was successful
 *	-1	attach failed
 *
 */
static int
fore_attach(devinfo_p)
	struct dev_info	*devinfo_p;
{
	struct dev_reg	*dev_reg_p;
	struct dev_intr	*dev_intr_p;
	Fore_unit	*fup;
	Atm_config	*fcp;
	addr_t		valp;
	int		val;
	int		i;
	int		err_count = BOOT_LOOPS;
	static int	unit = 0;

	/*
	 * Sanity check
	 */
	if (devinfo_p == NULL)
		return (-1);

	/*
	 * Make sure this isn't a duplicate unit
	 */
	if (fore_units[unit] != NULL)
		return (-1);

	/*
	 * Allocate a new unit structure
	 */
	fup = (Fore_unit *) atm_dev_alloc(sizeof(Fore_unit), sizeof(int), 0);
	if (fup == NULL)
		return (-1);

	/*
	 * Start initializing it
	 */
	fup->fu_unit = unit;
	fup->fu_mtu = FORE_IFF_MTU;
	fup->fu_devinfo = devinfo_p;
	fup->fu_vcc_pool = &fore_vcc_pool;
	fup->fu_nif_pool = &fore_nif_pool;
	fup->fu_ioctl = fore_atm_ioctl;
	fup->fu_instvcc = fore_instvcc;
	fup->fu_openvcc = fore_openvcc;
	fup->fu_closevcc = fore_closevcc;
	fup->fu_output = fore_output;

	/*
	 * Consider this unit assigned
	 */
	fore_units[unit] = fup;
	unit++;

	ATM_DEBUG1("fore_attach: fup=%p\n", fup);
	ATM_DEBUG2("\tfu_xmit_q=%p fu_xmit_head=%p\n",
		fup->fu_xmit_q, &fup->fu_xmit_head);
	ATM_DEBUG2("\tfu_recv_q=%p fu_recv_head=%p\n",
		fup->fu_recv_q, &fup->fu_recv_head);
	ATM_DEBUG2("\tfu_buf1s_q=%p fu_buf1s_head=%p\n",
		fup->fu_buf1s_q, &fup->fu_buf1s_head);
	ATM_DEBUG2("\tfu_buf1l_q=%p fu_buf1l_head=%p\n",
		fup->fu_buf1l_q, &fup->fu_buf1l_head);
	ATM_DEBUG2("\tfu_cmd_q=%p fu_cmd_head=%p\n",
		fup->fu_cmd_q, &fup->fu_cmd_head);
	ATM_DEBUG1("\tfu_stats=%p\n",
		&fup->fu_stats);

	/*
	 * Tell kernel our unit number
	 */
	devinfo_p->devi_unit = fup->fu_unit;

	/*
	 * Figure out what type of device we've got.  This should always
	 * work since we've already done this at identify time!
	 */
	i = 0;
	while (fore_devices[i].fd_name) {
		if (strcmp(fore_devices[i].fd_name, devinfo_p->devi_name) == 0)
			break;
		i++;
	}
	if (fore_devices[i].fd_name == NULL)
		return (-1);

	fup->fu_config.ac_device = fore_devices[i].fd_devtyp;

	/*
	 * Walk through the OPENPROM register information
	 * mapping register banks as they are found.
	 */
	for ( dev_reg_p = devinfo_p->devi_reg, i = 1;
		i <= devinfo_p->devi_nreg; i++, ++dev_reg_p )
	{
		if ( dev_reg_p == NULL )
		{
			/*
			 * Can't happen...
			 */
			return ( -1 );
		}

		/*
		 * Each device type has different register sets
		 */
		switch (fup->fu_config.ac_device) {

#ifdef FORE_SBUS
		case DEV_FORE_SBA200E:

			switch ( i )
			{
			/*
			 * Host Control Register (HCR)
			 */
			case 1:
				if ( sizeof(Fore_reg) != dev_reg_p->reg_size )
				{
					return ( -1 );
				}
				fup->fu_ctlreg = (Fore_reg *)
					map_regs ( dev_reg_p->reg_addr,
						sizeof(Fore_reg),
						dev_reg_p->reg_bustype );
				if ( fup->fu_ctlreg == NULL )
				{
					return ( -1 );
				}
				break;

			/*
			 * SBus Burst Transfer Configuration Register
			 */
			case 2:
				/*
				 * Not used
				 */
				break;

			/*
			 * SBus Interrupt Level Select Register
			 */
			case 3:
				if ( sizeof (Fore_reg) != dev_reg_p->reg_size )
				{
					return ( -1 );
				}
				fup->fu_intlvl = (Fore_reg *)
					map_regs ( dev_reg_p->reg_addr,
						sizeof(Fore_reg),
						dev_reg_p->reg_bustype );
				if ( fup->fu_intlvl == NULL )
				{
					return ( -1 );
				}
				break;

			/*
			 * i960 RAM
			 */
			case 4:
				fup->fu_ram = (Fore_mem *)
					map_regs ( dev_reg_p->reg_addr,
						dev_reg_p->reg_size,
						dev_reg_p->reg_bustype );
				if ( fup->fu_ram == NULL )
				{
					return ( -1 );
				}
				fup->fu_ramsize = dev_reg_p->reg_size;

				/*
				 * Various versions of the Sun PROM mess with 
				 * the reg_addr value in unpredictable (to me,
				 * at least) ways, so just use the "memoffset"
				 * property, which should give us the RAM 
				 * offset directly.
				 */
				val = getprop(devinfo_p->devi_nodeid, 
							"memoffset", -1);
				if (val == -1) {
					return (-1);
				}
				fup->fu_config.ac_ram = val;
				fup->fu_config.ac_ramsize = fup->fu_ramsize;

				/*
				 * Set monitor interface for initializing
				 */
				fup->fu_mon = (Mon960 *)
					(fup->fu_ram + MON960_BASE);
				break;

			default:
				log(LOG_ERR, 
					"fore_attach: Too many registers\n");
				return ( -1 );
			}
			break;

		case DEV_FORE_SBA200:

			switch ( i )
			{
			/*
			 * Board Control Register (BCR)
			 */
			case 1:
				if ( sizeof(Fore_reg) != dev_reg_p->reg_size )
				{
					return ( -1 );
				}
				fup->fu_ctlreg = (Fore_reg *)
					map_regs ( dev_reg_p->reg_addr,
						sizeof(Fore_reg),
						dev_reg_p->reg_bustype );
				if ( fup->fu_ctlreg == NULL )
				{
					return ( -1 );
				}
				break;

			/*
			 * i960 RAM
			 */
			case 2:
				fup->fu_ram = (Fore_mem *)
					map_regs ( dev_reg_p->reg_addr,
						dev_reg_p->reg_size,
						dev_reg_p->reg_bustype );
				if ( fup->fu_ram == NULL )
				{
					return ( -1 );
				}
				fup->fu_ramsize = dev_reg_p->reg_size;

				/*
				 * Various versions of the Sun PROM mess with 
				 * the reg_addr value in unpredictable (to me,
				 * at least) ways, so just use the "memoffset"
				 * property, which should give us the RAM 
				 * offset directly.
				 */
				val = getprop(devinfo_p->devi_nodeid, 
							"memoffset", -1);
				if (val == -1) {
					return (-1);
				}
				fup->fu_config.ac_ram = val;
				fup->fu_config.ac_ramsize = fup->fu_ramsize;

				/*
				 * Set monitor interface for initializing
				 */
				fup->fu_mon = (Mon960 *)
					(fup->fu_ram + MON960_BASE);
				break;

			default:
				log(LOG_ERR, 
					"fore_attach: Too many registers\n");
				return ( -1 );
			}
			break;
#endif	/* FORE_SBUS */

		default:
			log(LOG_ERR, 
				"fore_attach: Unsupported device type %d\n",
				fup->fu_config.ac_device);
			return (-1);
		}
	}

	/*
	 * Install the device in the interrupt chain.
	 *
	 * dev_intr_p may be null IFF devi_nintr is zero.
	 */
	dev_intr_p = devinfo_p->devi_intr;
	for ( i = devinfo_p->devi_nintr; i > 0; --i, ++dev_intr_p )
	{

		if ( dev_intr_p == NULL )
		{
			/*
			 * Can't happen.
			 */
			return ( -1 );
		}

		/*
		 * Convert hardware ipl (0-15) into spl level.
		 */
		if ( ipltospl ( dev_intr_p->int_pri ) > fup->fu_intrpri )
		{
			fup->fu_intrpri = ipltospl ( dev_intr_p->int_pri );

			/*
			 * If SBA-200E card, set SBus interrupt level
			 * into board register
			 */
			if ( fup->fu_intlvl ) {
#if defined(sun4c)
				*(fup->fu_intlvl) = dev_intr_p->int_pri;
#elif defined(sun4m)
				extern int	svimap[];

				*(fup->fu_intlvl) = 
					svimap[dev_intr_p->int_pri & 0xf];
#else
				#error PORT ME;
#endif
			}
		}

		DEVICE_LOCK((Cmn_unit *)fup);

		/*
		 * Register our interrupt routine.
		 */
	        (void) addintr ( dev_intr_p->int_pri, fore_poll,
		    devinfo_p->devi_name, devinfo_p->devi_unit );

		/*
		 * If we can do DMA (we can), then DVMA routines need
		 * to know the highest IPL level we will interrupt at.
		 */
		adddma ( dev_intr_p->int_pri );

		DEVICE_UNLOCK((Cmn_unit *)fup);
	}

	/*
	 * Poke the hardware...boot the CP and prepare it for downloading
	 */
	fore_reset(fup);

	switch (fup->fu_config.ac_device) {

#ifdef FORE_SBUS
	case DEV_FORE_SBA200E:
		/*
		 * Enable interrupts
		 */
		SBA200E_HCR_SET(*fup->fu_ctlreg, SBA200E_SBUS_ENA);
		break;
#endif	/* FORE_SBUS */
	}

	/*
	 * Wait for monitor to perform self-test
	 */
	while (CP_READ(fup->fu_mon->mon_bstat) != BOOT_MONREADY) {
		if (CP_READ(fup->fu_mon->mon_bstat) == BOOT_FAILTEST) {
			log(LOG_ERR, "fore_attach: Unit %d failed self-test\n",
				fup->fu_unit);
			return (-1);

		} else if ( --err_count == 0 ) {
			log(LOG_ERR, "fore_attach: Unit %d unable to boot\n",
				fup->fu_unit);
			return (-1);
		}
		DELAY ( BOOT_DELAY );
	}

	/*
	 * Write a one line message to the console informing
	 * that we've attached the device.
	 */
	report_dev ( devinfo_p );

	/*
	 * Get the mac address from the card PROM
	 */
	val = getprop ( devinfo_p->devi_nodeid, "macaddress1", -1 );
	if ( val != -1 ) {
		fup->fu_pif.pif_macaddr.ma_data[0] = val & 0xff;
		val = getprop ( devinfo_p->devi_nodeid, "macaddress2", -1 );
		fup->fu_pif.pif_macaddr.ma_data[1] = val & 0xff;
		val = getprop ( devinfo_p->devi_nodeid, "macaddress3", -1 );
		fup->fu_pif.pif_macaddr.ma_data[2] = val & 0xff;
		val = getprop ( devinfo_p->devi_nodeid, "macaddress4", -1 );
		fup->fu_pif.pif_macaddr.ma_data[3] = val & 0xff;
		val = getprop ( devinfo_p->devi_nodeid, "macaddress5", -1 );
		fup->fu_pif.pif_macaddr.ma_data[4] = val & 0xff;
		val = getprop ( devinfo_p->devi_nodeid, "macaddress6", -1 );
		fup->fu_pif.pif_macaddr.ma_data[5] = val & 0xff;
	} else {
		/*
		 * Newer PROM - mac addresses have been combined. Also,
		 * macaddrlo2 reflects the board serial number.
		 */
		val = htonl(getprop(devinfo_p->devi_nodeid, "macaddrlo2", -1));
		KM_COPY ( (caddr_t)&val, 
			(caddr_t)&fup->fu_pif.pif_macaddr.ma_data[2],
			sizeof(val) );
		val = htonl(getprop(devinfo_p->devi_nodeid, "macaddrhi4", -1));
		KM_COPY ( (caddr_t)&val,
			(caddr_t)fup->fu_pif.pif_macaddr.ma_data,
			sizeof(val) );
	}

	/*
	 * Setup the adapter config info
	 */
	fcp = &fup->fu_config;
	fcp->ac_vendor = VENDOR_FORE;
	fcp->ac_vendapi = VENDAPI_FORE_1;
	fcp->ac_macaddr = fup->fu_pif.pif_macaddr;
	val = getprop ( devinfo_p->devi_nodeid, "promversion", -1 );
	if ( val == -1 ) {
		val = getprop ( devinfo_p->devi_nodeid, "hw-version", -1 );
	}
	if (val != -1) {
		snprintf(fcp->ac_hard_vers,
		    sizeof(fcp->ac_hard_vers), "%d.%d.%d",
			(val >> 16) & 0xff, (val >> 8) & 0xff, val & 0xff);
	} else
		snprintf(fcp->ac_hard_vers,
		    sizeof(fcp->ac_hard_vers), "Unknown");

	val = getprop ( devinfo_p->devi_nodeid, "serialnumber", -1 );
	if ( val != -1 )
		fcp->ac_serial = val;

	valp = (addr_t)getlongprop ( devinfo_p->devi_nodeid, "model" );
	if ( valp )
	{
		/*
		 * Media Type
		 */
		switch (fcp->ac_device) {

#ifdef FORE_SBUS
		case DEV_FORE_SBA200E:
			fcp->ac_media = MEDIA_OC3C;
			fup->fu_pif.pif_pcr = ATM_PCR_OC3C;
			break;

		case DEV_FORE_SBA200:
			/*
			 * Look at the /SSS trailer to determine 4B5B speed
			 * 	TAXI-100 = 125; TAXI-140 = 175
			 * Assume that OC3 has no /SSS speed identifier.
			 */
			while (*valp && *valp != '/')
				valp++;
			if (*valp == NULL) {
				fcp->ac_media = MEDIA_OC3C;
				fup->fu_pif.pif_pcr = ATM_PCR_OC3C;
			} else if (strcmp(valp, "/125") == 0) {
				fcp->ac_media = MEDIA_TAXI_100;
				fup->fu_pif.pif_pcr = ATM_PCR_TAXI100;
			} else {
				fcp->ac_media = MEDIA_TAXI_140;
				fup->fu_pif.pif_pcr = ATM_PCR_TAXI140;
			}
			break;
#endif	/* FORE_SBUS */
		}

		/*
		 * Free property space
		 */
		KM_FREE(valp, getproplen(devinfo_p->devi_nodeid, "model"), 0);
	}

	/*
	 * Bus information
	 */
	fcp->ac_busslot = 
#ifdef SBUS_SIZE
		(long)(devinfo_p->devi_reg->reg_addr - SBUS_BASE) / SBUS_SIZE;
#else
		sbusslot((u_long)devinfo_p->devi_reg->reg_addr);
#endif

	val = getprop(devinfo_p->devi_parent->devi_nodeid, "burst-sizes", 0);
	if (val & SBUS_BURST32)
		fcp->ac_bustype = BUS_SBUS_B32;
	else
		fcp->ac_bustype = BUS_SBUS_B16;

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
		/*
		 * Modload calls UNLOAD if it get's a failure - don't
		 * call fore_unload() here.
		 */
		return ( -1 );
	}

	/*
	 * Initialize the CP microcode program.
	 */
	fore_initialize(fup);

	return (0);
}
#endif	/* sun */


#ifdef __FreeBSD__
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

#if BSD >= 199506
	/*
	 * Add hook to our shutdown function
	 */
	EVENTHANDLER_REGISTER(shutdown_post_sync, fore_pci_shutdown, fup,
			      SHUTDOWN_PRI_DEFAULT);
#endif

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


#if BSD < 199506
/*
 * Device shutdown routine
 * 
 * Arguments:
 *	kdc		pointer to device's configuration table
 *	force		forced shutdown flag
 *
 * Returns:
 *	none
 *
 */
static int
fore_pci_shutdown(kdc, force)
	struct kern_devconf	*kdc;
	int			force;
{
	Fore_unit	*fup;

	if (kdc->kdc_unit < fore_nunits) {

		fup = fore_units[kdc->kdc_unit];
		if (fup != NULL) {
			fore_reset(fup);
		}
	}

	(void) dev_detach(kdc);
	return (0);
}
#else
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
#endif	/* BSD < 199506 */
#endif	/* __FreeBSD__ */


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
#ifdef sun
	struct dev_info		*devinfo_p = fup->fu_devinfo;
	struct dev_reg		*dev_reg_p;
	struct dev_intr		*dev_intr_p;
	int			i;
#endif


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
	(void)untimeout((KTimeout_ret(*) __P((void *)))fore_initialize,
		(void *)fup, fup->fu_thandle);

#ifdef sun
	/*
	 * Remove any mappings of the device
	 */
	for ( dev_reg_p = devinfo_p->devi_reg, i = 1;
		i <= devinfo_p->devi_nreg; i++, ++dev_reg_p )
	{
		if ( dev_reg_p == NULL )
		{
			/*
			 * Can't happen...
			 */
			break;
		}

		/*
		 * Each device type has different register sets
		 */
		switch (fup->fu_config.ac_device) {

#ifdef FORE_SBUS
		case DEV_FORE_SBA200E:

			switch ( i )
			{
			/*
			 * Host Control Register (HCR)
			 */
			case 1:
				unmap_regs((addr_t)fup->fu_ctlreg,
					sizeof(Fore_reg));
				break;

			/*
			 * SBus Burst Transfer Configuration Register
			 */
			case 2:
				/*
				 * Not used
				 */
				break;

			/*
			 * SBus Interrupt Level Select Register
			 */
			case 3:
				unmap_regs((addr_t)fup->fu_intlvl,
					sizeof(Fore_reg));
				break;

			/*
			 * i960 RAM
			 */
			case 4:
				unmap_regs((addr_t)fup->fu_ram,
					fup->fu_ramsize);
				break;
			}
			break;

		case DEV_FORE_SBA200:

			switch ( i )
			{
			/*
			 * Board Control Register (BCR)
			 */
			case 1:
				unmap_regs((addr_t)fup->fu_ctlreg,
					sizeof(Fore_reg));
				break;

			/*
			 * i960 RAM
			 */
			case 2:
				unmap_regs((addr_t)fup->fu_ram,
					fup->fu_ramsize);
				break;
			}
			break;
#endif	/* FORE_SBUS */
		}
	}

	/*
	 * Remove the interrupt vector(s)
	 */
	dev_intr_p = devinfo_p->devi_intr;
	for ( i = devinfo_p->devi_nintr; i > 0; --i, ++dev_intr_p )
	{
		if ( dev_intr_p == NULL )
		{
			/*
			 * Can't happen...
			 */
			break;
		}
		(void) remintr ( dev_intr_p->int_pri, fore_poll );
	}
#endif	/* sun */

#ifdef __FreeBSD__
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
#endif	/* __FreeBSD__ */

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

		switch (fup->fu_config.ac_device) {

#ifdef FORE_SBUS
		case DEV_FORE_SBA200E:
			/*
			 * Reset i960 by setting and clearing RESET
			 */
			SBA200E_HCR_INIT(*fup->fu_ctlreg, SBA200E_RESET);
			SBA200E_HCR_CLR(*fup->fu_ctlreg, SBA200E_RESET);
			break;

		case DEV_FORE_SBA200:
			/*
			 * Reset i960 by setting and clearing RESET
			 *
			 * SBA200 will NOT reset if bit is OR'd in!
			 */
			*fup->fu_ctlreg = SBA200_RESET;
			*fup->fu_ctlreg = SBA200_RESET_CLR;
			break;
#endif	/* FORE_SBUS */
#ifdef FORE_PCI
		case DEV_FORE_PCA200E:
			/*
			 * Reset i960 by setting and clearing RESET
			 */
			PCA200E_HCR_INIT(*fup->fu_ctlreg, PCA200E_RESET);
			DELAY(10000);
			PCA200E_HCR_CLR(*fup->fu_ctlreg, PCA200E_RESET);
			break;

#endif
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

#ifdef sun
/*
 * Generic module load processing
 * 
 * This function is called by an OS-specific function when this
 * module is being loaded.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	load was successful 
 *	errno	load failed - reason indicated
 *
 */
static int
fore_doload()
{
	int	err = 0;

	/*
	 * Start us up
	 */
	err = fore_start();
	if (err)
		/* Problems, clean up */
		(void)fore_stop();

	return (err);
}


/*
 * Generic module unload processing
 * 
 * This function is called by an OS-specific function when this
 * module is being unloaded.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	unload was successful 
 *	errno	unload failed - reason indicated
 *
 */
static int
fore_dounload()
{
	int	err = 0;

	/*
	 * OK, try to clean up our mess
	 */
	err = fore_stop();

	return (err);
}


/*
 * Loadable driver description
 */
static struct vdldrv	fore_drv = {
	VDMAGIC_DRV,	/* Device Driver */
	"fore_mod",	/* name */
	&fore_ops,	/* dev_ops */
	NULL,		/* bdevsw */
	NULL,		/* cdevsw */
	0,		/* blockmajor */
	0		/* charmajor */
};


/*
 * Loadable module support entry point
 * 
 * This is the routine called by the vd driver for all loadable module
 * functions for this pseudo driver.  This routine name must be specified
 * on the modload(1) command.  This routine will be called whenever the
 * modload(1), modunload(1) or modstat(1) commands are issued for this
 * module.
 *
 * Arguments:
 *	cmd	vd command code
 *	vdp	pointer to vd driver's structure
 *	vdi	pointer to command-specific vdioctl_* structure
 *	vds	pointer to status structure (VDSTAT only)
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
int
fore_mod(cmd, vdp, vdi, vds)
	int		cmd;
	struct vddrv	*vdp;
	caddr_t		vdi;
	struct vdstat	*vds;
{
	int	err = 0;

	switch (cmd) {

	case VDLOAD:
		/*
		 * Module Load
		 *
		 * We dont support any user configuration
		 */
		err = fore_doload();
		if (err == 0)
			/* Let vd driver know about us */
			vdp->vdd_vdtab = (struct vdlinkage *)&fore_drv;
		break;

	case VDUNLOAD:
		/*
		 * Module Unload
		 */
		err = fore_dounload();
		break;

	case VDSTAT:
		/*
		 * Module Status
		 */

		/* Not much to say at the moment */

		break;

	default:
		log(LOG_ERR, "fore_mod: Unknown vd command 0x%x\n", cmd);
		err = EINVAL;
	}

	return (err);
}
#endif	/* sun */

#ifdef __FreeBSD__
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
	/* bmaj */	-1
};


/*
 * Loadable device driver module description
 */
#if BSD < 199506
MOD_DEV("fore_mod", LM_DT_CHAR, -1, (void *)&fore_cdev);
#else
MOD_DEV(fore, LM_DT_CHAR, -1, (void *)&fore_cdev);
#endif


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
#if BSD < 199506
	DISPATCH(lkmtp, cmd, ver, fore_load, fore_unload, nosys);
#else
	DISPATCH(lkmtp, cmd, ver, fore_load, fore_unload, lkm_nullcmd);
#endif
}
#endif	/* notdef */
#endif	/* __FreeBSD__ */

#endif	/* ATM_LINKED */

