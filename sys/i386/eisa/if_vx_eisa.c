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
 */

#include "eisa.h"
#if NEISA > 0

#include "vx.h"
#if NVX > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <i386/eisa/eisaconf.h>

#include <dev/vx/if_vxreg.h>

#define EISA_DEVICE_ID_3COM_3C592	0x506d5920
#define EISA_DEVICE_ID_3COM_3C597_TX	0x506d5970
#define EISA_DEVICE_ID_3COM_3C597_T4	0x506d5971
#define EISA_DEVICE_ID_3COM_3C597_MII	0x506d5972


#define	VX_EISA_SLOT_OFFSET		0x0c80
#define	VX_EISA_IOSIZE			0x000a
#define VX_RESOURCE_CONFIG		0x0008


static char *vx_match __P((eisa_id_t type));
static int vx_eisa_probe __P((void));
static int vx_eisa_attach __P((struct eisa_device *));

struct eisa_driver vx_eisa_driver = {
    "vx",
    vx_eisa_probe,
    vx_eisa_attach,
     /* shutdown */ NULL,
    &vx_count
};

DATA_SET(eisadriver_set, vx_eisa_driver);

static char*
vx_match(type)
    eisa_id_t       type;
{
    switch (type) {
      case EISA_DEVICE_ID_3COM_3C592:
	return "3Com 3C592 Network Adapter";
	break;
      case EISA_DEVICE_ID_3COM_3C597_TX:
	return "3Com 3C597-TX Network Adapter";
	break;
      case EISA_DEVICE_ID_3COM_3C597_T4:
	return "3Com 3C597-T4 Network Adapter";
	break;
      case EISA_DEVICE_ID_3COM_3C597_MII:
	return "3Com 3C597-MII Network Adapter";
	break;
      default:
	break;
    }
    return (NULL);
}

static int
vx_eisa_probe(void)
{
    u_long          iobase;
    struct eisa_device *e_dev = NULL;
    int             count;

    count = 0;
    while ((e_dev = eisa_match_dev(e_dev, vx_match))) {
	u_long          port;

	port = e_dev->ioconf.slot * EISA_SLOT_SIZE;
	iobase = port + VX_EISA_SLOT_OFFSET;

	eisa_add_iospace(e_dev, iobase, VX_EISA_IOSIZE, RESVADDR_NONE);
	eisa_add_iospace(e_dev, port, VX_IOSIZE, RESVADDR_NONE);

	/* Set irq */
	eisa_add_intr(e_dev, inw(iobase + VX_RESOURCE_CONFIG) >> 12);
	eisa_registerdev(e_dev, &vx_eisa_driver);
	count++;
    }
    return count;
}

static int
vx_eisa_attach(e_dev)
    struct eisa_device *e_dev;
{
    struct vx_softc *sc;
    int             unit = e_dev->unit;
    int             irq = ffs(e_dev->ioconf.irq) - 1;
    resvaddr_t     *ioport;
    resvaddr_t     *eisa_ioport;
    u_char          level_intr;

    ioport = e_dev->ioconf.ioaddrs.lh_first;

    if (!ioport)
	return -1;

    eisa_ioport = ioport->links.le_next;

    if (!eisa_ioport)
	return -1;

    eisa_reg_start(e_dev);
    if (eisa_reg_iospace(e_dev, ioport))
	return -1;

    if (eisa_reg_iospace(e_dev, eisa_ioport))
	return -1;

    if ((sc = vxalloc(unit)) == NULL)
	return -1;

    sc->vx_io_addr = ioport->addr;

    level_intr = FALSE;

    if (eisa_reg_intr(e_dev, irq, vxintr, (void *) sc, &net_imask,
		       /* shared == */ level_intr)) {
	vxfree(sc);
	return -1;
    }
    eisa_reg_end(e_dev);

    /* Now the registers are availible through the lower ioport */

    vxattach(sc);

    if (eisa_enable_intr(e_dev, irq)) {
	vxfree(sc);
	eisa_release_intr(e_dev, irq, vxintr);
	return -1;
    }
    return 0;
}

#endif	/* NVX > 0 */
#endif	/* NEISA > 0 */
