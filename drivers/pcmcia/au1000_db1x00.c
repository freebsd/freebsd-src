/*
 *
 * Alchemy Semi Db1x00 boards specific pcmcia routines.
 *
 * Copyright 2002 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * 
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/tqueue.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/types.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/bus_ops.h>
#include "cs_internal.h"

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <asm/au1000.h>
#include <asm/au1000_pcmcia.h>

#include <asm/db1x00.h>

static BCSR * const bcsr = (BCSR *)0xAE000000;

static int db1x00_pcmcia_init(struct pcmcia_init *init)
{
	bcsr->pcmcia = 0; /* turn off power */
	au_sync_delay(2);
	return PCMCIA_NUM_SOCKS;
}

static int db1x00_pcmcia_shutdown(void)
{
	bcsr->pcmcia = 0; /* turn off power */
	au_sync_delay(2);
	return 0;
}

static int 
db1x00_pcmcia_socket_state(unsigned sock, struct pcmcia_state *state)
{
	u32 inserted;
	unsigned char vs;

	if(sock > PCMCIA_MAX_SOCK) return -1;

	state->ready = 0;
	state->vs_Xv = 0;
	state->vs_3v = 0;
	state->detect = 0;

	if (sock == 0) {
		vs = bcsr->status & 0x3;
		inserted = !(bcsr->status & (1<<4));
	}
	else {
		vs = (bcsr->status & 0xC)>>2;
		inserted = !(bcsr->status & (1<<5));
	}

	DEBUG(KERN_DEBUG "db1x00 socket %d: inserted %d, vs %d\n", 
			sock, inserted, vs);

	if (inserted) {
		switch (vs) {
			case 0:
			case 2:
				state->vs_3v=1;
				break;
			case 3: /* 5V */
				break;
			default:
				/* return without setting 'detect' */
				printk(KERN_ERR "db1x00 bad VS (%d)\n",
						vs);
				return -1;
		}
		state->detect = 1;
		state->ready = 1;
	}
	else {
		/* if the card was previously inserted and then ejected,
		 * we should turn off power to it
		 */
		if ((sock == 0) && (bcsr->pcmcia & BCSR_PCMCIA_PC0RST)) {
			bcsr->pcmcia &= ~(BCSR_PCMCIA_PC0RST | 
					BCSR_PCMCIA_PC0DRVEN |
					BCSR_PCMCIA_PC0VPP |
					BCSR_PCMCIA_PC0VCC);
		}
		else if ((sock == 1) && (bcsr->pcmcia & BCSR_PCMCIA_PC1RST)) {
			bcsr->pcmcia &= ~(BCSR_PCMCIA_PC1RST | 
					BCSR_PCMCIA_PC1DRVEN |
					BCSR_PCMCIA_PC1VPP |
					BCSR_PCMCIA_PC1VCC);
		}
	}

	state->bvd1=1;
	state->bvd2=1;
	state->wrprot=0; 
	return 1;
}


static int db1x00_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{
	if(info->sock > PCMCIA_MAX_SOCK) return -1;

	if(info->sock == 0) {
		info->irq = AU1000_GPIO_2;
	}
	else 
		info->irq = AU1000_GPIO_5;

	return 0;
}


static int 
db1x00_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{
	u16 pwr;
	int sock = configure->sock;

	if(sock > PCMCIA_MAX_SOCK) return -1;

	DEBUG(KERN_DEBUG "socket %d Vcc %dV Vpp %dV, reset %d\n", 
			sock, configure->vcc, configure->vpp, configure->reset);

	/* pcmcia reg was set to zero at init time. Be careful when
	 * initializing a socket not to wipe out the settings of the 
	 * other socket.
	 */
	pwr = bcsr->pcmcia;
	pwr &= ~(0xf << sock*8); /* clear voltage settings */

	switch(configure->vcc){
		case 0:  /* Vcc 0 */
			pwr |= SET_VCC_VPP(0,0,sock);
			break;
		case 50: /* Vcc 5V */
			switch(configure->vpp) {
				case 0:
					pwr |= SET_VCC_VPP(2,0,sock);
					break;
				case 50:
					pwr |= SET_VCC_VPP(2,1,sock);
					break;
				case 12:
					pwr |= SET_VCC_VPP(2,2,sock);
					break;
				case 33:
				default:
					pwr |= SET_VCC_VPP(0,0,sock);
					printk("%s: bad Vcc/Vpp (%d:%d)\n", 
							__FUNCTION__, 
							configure->vcc, 
							configure->vpp);
					break;
			}
			break;
		case 33: /* Vcc 3.3V */
			switch(configure->vpp) {
				case 0:
					pwr |= SET_VCC_VPP(1,0,sock);
					break;
				case 12:
					pwr |= SET_VCC_VPP(1,2,sock);
					break;
				case 33:
					pwr |= SET_VCC_VPP(1,1,sock);
					break;
				case 50:
				default:
					pwr |= SET_VCC_VPP(0,0,sock);
					printk("%s: bad Vcc/Vpp (%d:%d)\n", 
							__FUNCTION__, 
							configure->vcc, 
							configure->vpp);
					break;
			}
			break;
		default: /* what's this ? */
			pwr |= SET_VCC_VPP(0,0,sock);
			printk(KERN_ERR "%s: bad Vcc %d\n", 
					__FUNCTION__, configure->vcc);
			break;
	}

	bcsr->pcmcia = pwr;
	au_sync_delay(300);

	if (sock == 0) {
		if (!configure->reset) {
			pwr |= BCSR_PCMCIA_PC0DRVEN;
			bcsr->pcmcia = pwr;
			au_sync_delay(300);
			pwr |= BCSR_PCMCIA_PC0RST;
			bcsr->pcmcia = pwr;
			au_sync_delay(100);
		}
		else {
			pwr &= ~(BCSR_PCMCIA_PC0RST | BCSR_PCMCIA_PC0DRVEN);
			bcsr->pcmcia = pwr;
			au_sync_delay(100);
		}
	}
	else {
		if (!configure->reset) {
			pwr |= BCSR_PCMCIA_PC1DRVEN;
			bcsr->pcmcia = pwr;
			au_sync_delay(300);
			pwr |= BCSR_PCMCIA_PC1RST;
			bcsr->pcmcia = pwr;
			au_sync_delay(100);
		}
		else {
			pwr &= ~(BCSR_PCMCIA_PC1RST | BCSR_PCMCIA_PC1DRVEN);
			bcsr->pcmcia = pwr;
			au_sync_delay(100);
		}
	}
	return 0;
}

struct pcmcia_low_level db1x00_pcmcia_ops = { 
	db1x00_pcmcia_init,
	db1x00_pcmcia_shutdown,
	db1x00_pcmcia_socket_state,
	db1x00_pcmcia_get_irq_info,
	db1x00_pcmcia_configure_socket
};
