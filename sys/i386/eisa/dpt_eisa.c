/**
 *       Copyright (c) 1997 by Matthew N. Dodd <winter@jurai.net>
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Credits:  Based on and part of the DPT driver for FreeBSD written and
 *           maintained by Simon Shapiro <shimon@simon-shapiro.org>
 */

/*
 * $Id$
 */

#include "opt_dpt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#include <i386/eisa/eisaconf.h>

#include <sys/dpt.h>
#include <i386/eisa/dpt_eisa.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

/* Function Prototypes */

int		dpt_eisa_probe(void);
int		dpt_eisa_attach(struct eisa_device*);
int		dpt_eisa_shutdown(int);

static const char	*dpt_eisa_match(eisa_id_t);

static struct eisa_driver dpt_eisa_driver = 
{
	"dpt",
	dpt_eisa_probe,
	dpt_eisa_attach,
	dpt_eisa_shutdown,
	&dpt_unit
};

DATA_SET (eisadriver_set, dpt_eisa_driver);

int
dpt_eisa_probe(void)
{
	static int		already_announced = 0;
	u_int32_t		io_base;
	u_int32_t		irq;
	struct eisa_device	*e_dev = NULL;
	dpt_conf_t		*config;
	dpt_softc_t		*dpt;
        int			count = 0;

	if ( !already_announced ) {
		printf("DPT:  EISA SCSI HBA Driver, version %d.%d.%d\n",
			DPT_RELEASE, DPT_VERSION, DPT_PATCH);
		++already_announced;
	}

	if ((dpt = (dpt_softc_t *) malloc(sizeof(dpt_softc_t), 
					  M_DEVBUF, M_NOWAIT)) == NULL) {
		printf("dpt_eisa_probe() : Failed to allocate %d bytes for a DPT softc\n", sizeof(dpt_softc_t));
		return -1;
	}

	bzero(dpt, sizeof(dpt_softc_t));

	TAILQ_INIT(&dpt->free_ccbs);
	TAILQ_INIT(&dpt->waiting_ccbs);
	TAILQ_INIT(&dpt->submitted_ccbs);
	TAILQ_INIT(&dpt->completed_ccbs);

	dpt->queue_status = DPT_QUEUES_NONE_ACTIVE;
	dpt->commands_processed = 0;
	dpt->handle_interrupts = 0;
	dpt->v_membase = NULL;
	dpt->p_membase = NULL;

	dpt->unit = -1;

	while ((e_dev = eisa_match_dev(e_dev, dpt_eisa_match))) {
		io_base = (e_dev->ioconf.slot * EISA_SLOT_SIZE)
			 + DPT_EISA_SLOT_OFFSET;

		eisa_add_iospace(e_dev, io_base, 
				 DPT_EISA_IOSIZE, RESVADDR_NONE);
	
		dpt->io_base = io_base;

		if ((config = dpt_get_conf(dpt, 0xc1, 7,
				      sizeof(dpt_conf_t), 1)) == NULL) {
#ifdef DPT_DEBUG_ERROR
			printf("eisa0:%d dpt_eisa_probe() : Failed to get board configuration.\n",
				e_dev->ioconf.slot);
#endif
			continue;
		}

		irq = config->IRQ;

		eisa_add_intr(e_dev, irq);
		eisa_registerdev(e_dev, &dpt_eisa_driver);

		count++;
		
	}

	free(dpt, M_DEVBUF);
	return count;
}

int
dpt_eisa_attach(e_dev)
	struct eisa_device	*e_dev;
{
	int		result;
	int		ndx;

	dpt_conf_t	*config;
	dpt_softc_t	*dpt;

        int		unit = e_dev->unit;
        int		irq;
	resvaddr_t	*io_space;

	if (TAILQ_FIRST(&e_dev->ioconf.irqs) == NULL) {
#ifdef DPT_DEBUG_ERROR
		printf("dpt%d: Can't retrieve irq from EISA config struct.\n", 
			unit);
#endif
		return -1;
	}

	irq = TAILQ_FIRST(&e_dev->ioconf.irqs)->irq_no;
	io_space = e_dev->ioconf.ioaddrs.lh_first;

	if (!io_space) {
#ifdef DPT_DEBUG_ERROR
		printf("dpt%d: No I/O space?!\n", unit);
#endif
		return -1;
	}

	if ( dpt_controllers_present >= DPT_MAX_ADAPTERS ){
		printf("dpt%d: More than %d Adapters found!  Adapter rejected\n",
			unit, DPT_MAX_ADAPTERS);
		return -1;
	}

	if ((dpt = (dpt_softc_t *) malloc(sizeof(dpt_softc_t),
					  M_DEVBUF, M_NOWAIT)) == NULL) {
		printf("dpt%d: Failed to allocate %d bytes for a DPT softc\n",
			unit, sizeof(dpt_softc_t));
		return -1;
	}

	/*
	 * Initialize the queues.  See dpt.h for details. We do this here,
	 * as we may get hit with interrupts at any moment and we want to
	 * have a minimal structure in place to handle them. We also want to
	 * register interrupts correctly. To do so, we need a valid dpt
	 * structure. To have that, we need this  minimal setup here.
	 */

	bzero(dpt, sizeof(dpt_softc_t));
    
	TAILQ_INIT(&dpt->free_ccbs);
	TAILQ_INIT(&dpt->waiting_ccbs);
	TAILQ_INIT(&dpt->submitted_ccbs);
	TAILQ_INIT(&dpt->completed_ccbs);

	if (TAILQ_EMPTY(&dpt_softc_list)) {
		TAILQ_INIT(&dpt_softc_list);
	}

	TAILQ_INSERT_TAIL(&dpt_softc_list, dpt, links);
	dpt->queue_status       = DPT_QUEUES_NONE_ACTIVE;
	dpt->commands_processed = 0;

#ifdef DPT_MEASURE_PERFORMANCE
	/* Zero out all command counters */
	bzero((void *)&dpt->performance, sizeof(dpt_perf_t));
#endif		/* DPT_MEASURE_PERFORMANCE */

	dpt->handle_interrupts = 0;	/* 
					 * Do not set to 1 until all
					 * initialization is done 
					 */
	dpt->v_membase = NULL;
	dpt->p_membase = NULL;

	dpt->unit = unit;
	dpt->io_base = (e_dev->ioconf.slot * EISA_SLOT_SIZE)
			 + DPT_EISA_SLOT_OFFSET;

	eisa_reg_start(e_dev);

	if (eisa_reg_iospace(e_dev, io_space)) {
#ifdef DPT_DEBUG_ERROR
		printf("dpt%d: eisa_reg_iospace() failed.\n", unit);
#endif
		free(dpt, M_DEVBUF);
		return -1;
	}

	/* reset the card? */

	/* If the DPT is mapped as an IDE controller, let it be IDE controller */
	if (dpt->io_base == ISA_PRIMARY_WD_ADDRESS) {
#ifdef DPT_DEBUG_WARN
		printf("dpt%d: Mapped as an IDE controller.  "
			"Disabling SCSI setup\n", unit);
#endif
		free(dpt, M_DEVBUF);
		return -1;
	} else {
		if ((config = dpt_get_conf(dpt, 0xc1, 7,
					   sizeof(dpt_conf_t), 1)) == NULL) {
#ifdef DPT_DEBUG_ERROR
			printf("dpt%d: Failed to get board configuration (%x)\n",
				 unit, BaseRegister(dpt));
#endif
			free(dpt, M_DEVBUF);
			return -1;
		}
	}

	if(eisa_reg_intr(e_dev, irq, dpt_intr, (void *)dpt, &cam_imask,
			/* shared == */ config->IRQ_TR)) {
#ifdef DPT_DEBUG_ERROR
		printf("dpt%d: eisa_reg_intr() failed.\n", unit);
#endif
		free(dpt, M_DEVBUF);
		return -1;
	}
	eisa_reg_end(e_dev);

	/* Enable our interrupt handler. */
	if (eisa_enable_intr(e_dev, irq)) {
#ifdef DPT_DEBUG_ERROR
		printf("dpt%d: eisa_enable_intr() failed.\n", unit);
#endif
		free(dpt, M_DEVBUF);
		eisa_release_intr(e_dev, irq, dpt_intr);
		return -1;
	}

	dpt->max_id      = config->MAX_ID;
	dpt->max_lun     = config->MAX_LUN;
	dpt->irq         = config->IRQ;
	dpt->channels    = config->MAX_CHAN;
	dpt->dma_channel = (8 - config->DMA_channel) & 7;

#ifdef DPT_DEBUG_SETUP
	printf("dpt%d: max_id = %d, max_chan = %d, max_lun = %d\n",
		dpt->unit, dpt->max_id, dpt->channels, dpt->max_lun);
#endif

	if (result = dpt_setup(dpt, config)) {
		free(config, M_TEMP);
		free(dpt, M_DEVBUF);
		printf("dpt%d: dpt_setup failed (%d).  Driver Disabled :-(\n",
		dpt->unit, result);
	} else {
		/* clean up the informational data, and display */
		char clean_vendor[9];
		char clean_model[17];
		char clean_firmware[5];
		char clean_protocol[5];
		char clean_other[7];

		int     ndx;

		strncpy(clean_other, dpt->board_data.otherData, 8);
		clean_other[6] = '\0';
		for (ndx = 5; ndx >= 0; ndx--) {
			if (clean_other[ndx] == ' ') {
				clean_other[ndx] = '\0';
			} else {
				break;
			}
		}

		strncpy(dpt->board_data.otherData, clean_other, 6);

		strncpy(clean_vendor, dpt->board_data.vendor, 8);
		clean_vendor[8] = '\0';

		for (ndx = 7; ndx >= 0; ndx--) {
			if (clean_vendor[ndx] == ' ') {
				clean_vendor[ndx] = '\0';
			} else {
				break;
			}
		}
	
		strncpy(dpt->board_data.vendor, clean_vendor, 8);

		strncpy(clean_model, dpt->board_data.modelNum, 16);
		clean_model[16] = '\0';

		for (ndx = 15; ndx >= 0; ndx--) {
			if (clean_model[ndx] == ' ') {
				clean_model[ndx] = '\0';
			} else {
				break;
			}
		}
	
		strncpy(dpt->board_data.modelNum, clean_model, 16);

		strncpy(clean_firmware, dpt->board_data.firmware, 4);
		clean_firmware[4] = '\0';

		for (ndx = 3; ndx >= 0; ndx--) {
			if (clean_firmware[ndx] == ' ')
				clean_firmware[ndx] = '\0';
			else
				break;
		}

		strncpy(dpt->board_data.firmware, clean_firmware, 4);

		strncpy(clean_protocol, dpt->board_data.protocol, 4);
		clean_protocol[4] = '\0';

		for (ndx = 3; ndx >= 0; ndx--) {
			if (clean_protocol[ndx] == ' ')
				clean_protocol[ndx] = '\0';
			else
				break;
		}

		strncpy(dpt->board_data.protocol, clean_protocol, 4);

		dpt_detect_cache(dpt);

		printf("dpt%d: %s type %x, model %s firmware %s, Protocol %s \n"
			"      on port %x with %dMB %s cache.  LED = %s\n",
			dpt->unit, clean_vendor, dpt->board_data.deviceType,
			clean_model, clean_firmware, clean_protocol, dpt->io_base,
			dpt->cache_size,
			(dpt->cache_type == DPT_NO_CACHE)
			? "Disabled"
			: (dpt->cache_type == DPT_CACHE_WRITETHROUGH)
			? "Write-Through"
			: "Write-Back",
			i2bin(dpt_blinking_led(dpt), 8));

		printf("dpt%d: Enabled Options:\n", dpt->unit);

#ifdef DPT_VERIFY_HINTR
		printf("      Verify Lost Transactions\n");
#endif
#ifdef DPT_RESTRICTED_FREELIST
		printf("      Restrict the Freelist Size\n");
#endif
#ifdef DPT_TRACK_CCB_STATES
		printf("      Precisely Track State Transitions\n");
#endif
#ifdef DPT_MEASURE_PERFORMANCE
		printf("      Collect Metrics\n");
#endif
#ifdef DPT_FREELIST_IS_STACK
		printf("      Optimize CPU Cache\n");
#endif
#ifdef DPT_HANDLE_TIMEOUTS
		printf("      Handle Timeouts\n");
#endif
#ifdef DPT_ALLOW_MEMIO
		printf("      Allow I/O to be Memeory Mapped\n");
#endif
#ifdef DPT_HINTR_CHECK_SOFTC
		printf("      Validate SoftC at Interrupt\n");
#endif

		/* register shutdown handlers */
		result = at_shutdown((bootlist_fn)dpt_shutdown, (void *)dpt,
						SHUTDOWN_POST_SYNC);
		switch ( result ) {
			case 0:
#ifdef DPT_DEBUG_SHUTDOWN
				printf("dpt%d: Shutdown handler registered\n", dpt->unit);
#endif
				break;
			default:
#ifdef DPT_DEBUG_WARN
				printf("dpt%d: Failed to register shutdown handler (%d)\n",
					dpt->unit, result);
#endif
				 break;
		}

		dpt_attach(dpt);
	}

	++dpt_controllers_present;

	return 0;
}

int
dpt_eisa_shutdown(foo)
	int	foo;
{
#ifdef DPT_DEBUG_WARN
	printf("dpt_pci_shutdown(%x)\n", foo);
#endif
	return (0);
}

static const char	*
dpt_eisa_match(type)
	eisa_id_t	type;
{
	switch (type) {
		case DPT_EISA_DPT2402 :
			return ("DPT PM2012A/9X");
			break;
		case DPT_EISA_DPTA401 :
			return ("DPT PM2012B/9X");
			break;
		case DPT_EISA_DPTA402 :
			return ("DPT PM2012B2/9X");
			break;
		case DPT_EISA_DPTA410 :
			return ("DPT PM2x22A/9X");
			break;
		case DPT_EISA_DPTA411 :
			return ("DPT Spectre");
			break;
		case DPT_EISA_DPTA412 :
			return ("DPT PM2021A/9X");
			break;
		case DPT_EISA_DPTA420 :
			return ("DPT Smart Cache IV (PM2042)");
			break;
		case DPT_EISA_DPTA501 :
			return ("DPT PM2012B1/9X");
			break;
		case DPT_EISA_DPTA502 :
			return ("DPT PM2012Bx/9X");
			break;
		case DPT_EISA_DPTA701 :
			return ("DPT PM2011B1/9X");
			break;
		case DPT_EISA_DPTBC01 :
			return ("DPT PM3011/7X ESDI");
			break;
		case DPT_EISA_NEC8200 :
			return ("NEC EATA SCSI");
			break;
		case DPT_EISA_ATT2408 :
			return ("ATT EATA SCSI");
			break;
		default:
			break;
	}
	
	return (NULL);
}

