/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@novatel.ca>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/clock.h>

#include <i386/isa/isa_device.h>

#include <dev/ep/if_epreg.h>
#include <dev/ep/if_epvar.h>
#include <i386/isa/elink.h>

static int		ep_isa_probe		(struct isa_device *);
static int		ep_isa_attach		(struct isa_device *);
static struct ep_board *ep_look_for_board_at	(struct isa_device *is);
static int		get_eeprom_data		(int, int);
static void		epintr			(int);

#if 0
static int		send_ID_sequence	(int);
#endif

static int		ep_current_tag = EP_LAST_TAG + 1;

struct isa_driver epdriver = {
    ep_isa_probe,
    ep_isa_attach,
    "ep",
    0
};

int
ep_isa_probe(is)
    struct isa_device *is;
{
    struct ep_softc *sc;
    struct ep_board *epb;
    u_short k;

    if ((epb = ep_look_for_board_at(is)) == 0)
        return (0);

    /*
     * Allocate a storage area for us
     */
    sc = ep_alloc(ep_unit, epb);
    if (!sc)
        return (0);

    is->id_unit = ep_unit++;

    /*
     * The iobase was found and MFG_ID was 0x6d50. PROD_ID should be
     * 0x9[0-f]50       (IBM-PC)
     * 0x9[0-f]5[0-f]   (PC-98)
     */
    GO_WINDOW(0);
    k = sc->epb->prod_id;
#ifdef PC98
    if ((k & 0xf0f0) != (PROD_ID & 0xf0f0)) {
#else
    if ((k & 0xf0ff) != (PROD_ID & 0xf0ff)) {
#endif
        printf("ep_isa_probe: ignoring model %04x\n", k);
        ep_free(sc);
        return (0);
    }

    k = sc->epb->res_cfg;

    k >>= 12;

    /* Now we have two cases again:
     *
     *  1. Device was configured with 'irq?'
     *      In this case we use irq read from the board
     *
     *  2. Device was configured with 'irq xxx'
     *      In this case we set up the board to use specified interrupt
     *
     */

    if (is->id_irq == 0) { /* irq? */
        is->id_irq = 1 << ((k == 2) ? 9 : k);
    }

    sc->stat = 0;       /* 16 bit access */

    /* By now, the adapter is already activated */

    return (EP_IOSIZE);         /* 16 bytes of I/O space used. */
}

static int
ep_isa_attach(is)
    struct isa_device *is;
{
    struct ep_softc *sc = ep_softc[is->id_unit];
    u_short config;
    int irq;

    is->id_ointr = epintr;
    sc->ep_connectors = 0;
    config = inw(IS_BASE + EP_W0_CONFIG_CTRL);
    if (config & IS_AUI) {
        sc->ep_connectors |= AUI;
    }
    if (config & IS_BNC) {
        sc->ep_connectors |= BNC;
    }
    if (config & IS_UTP) {
        sc->ep_connectors |= UTP;
    }
    if (!(sc->ep_connectors & 7))
        printf("no connectors!");
    sc->ep_connector = inw(BASE + EP_W0_ADDRESS_CFG) >> ACF_CONNECTOR_BITS;
    /*
     * Write IRQ value to board
     */

    irq = ffs(is->id_irq) - 1;
    if (irq == -1) {
        printf(" invalid irq... cannot attach\n");
        return 0;
    }

    GO_WINDOW(0);
    SET_IRQ(BASE, irq);

    ep_attach(sc);
    return 1;
}

static struct ep_board * 
ep_look_for_board_at(is)
    struct isa_device *is;
{
    int data, i, j, id_port = ELINK_ID_PORT;
    int count = 0;

    if (ep_current_tag == (EP_LAST_TAG + 1)) {
        /* Come here just one time */

        ep_current_tag--;

        /* Look for the ISA boards. Init and leave them actived */
        outb(id_port, 0);
        outb(id_port, 0);

        elink_idseq(0xCF);

        elink_reset();
        DELAY(DELAY_MULTIPLE * 10000);
        for (i = 0; i < EP_MAX_BOARDS; i++) {
            outb(id_port, 0);
            outb(id_port, 0);
            elink_idseq(0xCF);

            data = get_eeprom_data(id_port, EEPROM_MFG_ID);
            if (data != MFG_ID)
                break;

            /* resolve contention using the Ethernet address */

            for (j = 0; j < 3; j++)
                 get_eeprom_data(id_port, j);

            /* and save this address for later use */

            for (j = 0; j < 3; j++)
                 ep_board[ep_boards].eth_addr[j] = get_eeprom_data(id_port, j);

            ep_board[ep_boards].res_cfg =
                get_eeprom_data(id_port, EEPROM_RESOURCE_CFG);

            ep_board[ep_boards].prod_id =
                get_eeprom_data(id_port, EEPROM_PROD_ID);
     
            ep_board[ep_boards].epb_used = 0;
#ifdef PC98
            ep_board[ep_boards].epb_addr =
                        (get_eeprom_data(id_port, EEPROM_ADDR_CFG) & 0x1f) *
			0x100 + 0x40d0;
#else
            ep_board[ep_boards].epb_addr =
                        (get_eeprom_data(id_port, EEPROM_ADDR_CFG) & 0x1f) *
			0x10 + 0x200;

            if (ep_board[ep_boards].epb_addr > 0x3E0)
                /* Board in EISA configuration mode */
                continue;
#endif /* PC98 */

            outb(id_port, ep_current_tag);      /* tags board */
            outb(id_port, ACTIVATE_ADAPTER_TO_CONFIG);
            ep_boards++;
            count++;
            ep_current_tag--;
        }

        ep_board[ep_boards].epb_addr = 0;
        if (count) {
            printf("%d 3C5x9 board(s) on ISA found at", count);
            for (j = 0; ep_board[j].epb_addr; j++)
                if (ep_board[j].epb_addr <= 0x3E0)
                    printf(" 0x%x", ep_board[j].epb_addr);
            printf("\n");
        }
    }

    /* we have two cases:
     *
     *  1. Device was configured with 'port ?'
     *      In this case we search for the first unused card in list
     *
     *  2. Device was configured with 'port xxx'
     *      In this case we search for the unused card with that address
     *
     */

    if (IS_BASE == -1) { /* port? */
        for (i = 0; ep_board[i].epb_addr && ep_board[i].epb_used; i++)
            ;
        if (ep_board[i].epb_addr == 0)
            return 0;

        IS_BASE = ep_board[i].epb_addr;
        ep_board[i].epb_used = 1;

        return &ep_board[i];
    } else {
        for (i = 0;
             ep_board[i].epb_addr && ep_board[i].epb_addr != IS_BASE;
             i++)
            ;

        if (ep_board[i].epb_used || ep_board[i].epb_addr != IS_BASE)
            return 0;

        if (inw(IS_BASE + EP_W0_EEPROM_COMMAND) & EEPROM_TST_MODE) {
            printf("ep%d: 3c5x9 at 0x%x in PnP mode. Disable PnP mode!\n",
                   is->id_unit, IS_BASE);
        }
        ep_board[i].epb_used = 1;

        return &ep_board[i];
    }
}

/*
 * We get eeprom data from the id_port given an offset into the eeprom.
 * Basically; after the ID_sequence is sent to all of the cards; they enter
 * the ID_CMD state where they will accept command requests. 0x80-0xbf loads
 * the eeprom data.  We then read the port 16 times and with every read; the
 * cards check for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle; each card
 * compares the data on the bus; if there is a difference then that card goes
 * into ID_WAIT state again). In the meantime; one bit of data is returned in
 * the AX register which is conveniently returned to us by inb().  Hence; we
 * read 16 times getting one bit of data with each read.
 */

static int
get_eeprom_data(id_port, offset)
    int id_port;
    int offset;
{
    int i, data = 0;
    outb(id_port, 0x80 + offset);
    for (i = 0; i < 16; i++) {
        DELAY(BIT_DELAY_MULTIPLE * 1000);
        data = (data << 1) | (inw(id_port) & 1);
    }
    return (data);
}

void
epintr(unit)
    int unit;
{
    register struct ep_softc *sc = ep_softc[unit];

    ep_intr(sc);

    return;
}

#if 0
static int
send_ID_sequence(port)
    int port;
{
    int cx, al;

    for (al = 0xff, cx = 0; cx < 255; cx++) {
        outb(port, al);
        al <<= 1;
        if (al & 0x100) 
            al ^= 0xcf;
    }
    return (1);
}
#endif
