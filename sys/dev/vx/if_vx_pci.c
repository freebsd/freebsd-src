/*
 * Copyright (C) 1996 Naoki Hamada <nao@tom-yam.or.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/vx/if_vx_pci.c,v 1.20 2000/01/29 14:50:32 peter Exp $
 */

#include "vx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>

#include <pci/pcivar.h>

#include <dev/vx/if_vxreg.h>

static void vx_pci_shutdown(void *, int);
static const char *vx_pci_probe(pcici_t, pcidi_t);
static void vx_pci_attach(pcici_t, int unit);

static void
vx_pci_shutdown(
	void *sc,
	int howto)
{
   vxstop(sc); 
   vxfree(sc);
}

static const char*
vx_pci_probe(
	pcici_t config_id,
	pcidi_t device_id)
{
   if(device_id == 0x590010b7ul)
      return "3COM 3C590 Etherlink III PCI";
   if(device_id == 0x595010b7ul || device_id == 0x595110b7ul ||
	device_id == 0x595210b7ul)
      return "3COM 3C595 Fast Etherlink III PCI";
	/*
	 * The (Fast) Etherlink XL adapters are now supported by
	 * the xl driver, which uses bus master DMA and is much
	 * faster. (And which also supports the 3c905B.
	 */
#ifdef VORTEX_ETHERLINK_XL
   if(device_id == 0x900010b7ul || device_id == 0x900110b7ul)
      return "3COM 3C900 Etherlink XL PCI";
   if(device_id == 0x905010b7ul || device_id == 0x905110b7ul)
      return "3COM 3C905 Fast Etherlink XL PCI";
#endif
   return NULL;
}

static void
vx_pci_attach(
	pcici_t config_id,
	int unit)
{
    struct vx_softc *sc;

    if (unit >= NVX) {
       printf("vx%d: not configured; kernel is built for only %d device%s.\n",
          unit, NVX, NVX == 1 ? "" : "s"); 
       return;
    }

    if ((sc = vxalloc(unit)) == NULL) {
	return;
    }

    sc->vx_io_addr = pci_conf_read(config_id, 0x10) & 0xffffffe0;

    if (vxattach(sc) == 0) {
	return;
    }

    /* defect check for 3C590 */
    if ((pci_conf_read(config_id, 0) >> 16) == 0x5900) {
	GO_WINDOW(0);
	if (vxbusyeeprom(sc))
	    return;
	outw(BASE + VX_W0_EEPROM_COMMAND, EEPROM_CMD_RD | EEPROM_SOFT_INFO_2);
	if (vxbusyeeprom(sc))
	    return;
	if (!(inw(BASE + VX_W0_EEPROM_DATA) & NO_RX_OVN_ANOMALY)) {
	    printf("Warning! Defective early revision adapter!\n");
	}
    }

    /*
     * Add shutdown hook so that DMA is disabled prior to reboot. Not
     * doing do could allow DMA to corrupt kernel memory during the
     * reboot before the driver initializes.
     */
    EVENTHANDLER_REGISTER(shutdown_post_sync, vx_pci_shutdown, sc,
			  SHUTDOWN_PRI_DEFAULT);

    pci_map_int(config_id, vxintr, (void *) sc, &net_imask);
}

static struct pci_device vxdevice = {
    "vx",
    vx_pci_probe,
    vx_pci_attach,
    &vx_count,
    NULL
};

COMPAT_PCI_DRIVER (vx, vxdevice);
