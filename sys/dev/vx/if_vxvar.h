/*-
 * Copyright (c) 1993 Herb Peyerl (hpeyerl@novatel.ca) All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2. The name
 * of the author may not be used to endorse or promote products derived from
 * this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 October 2, 1994

 Modified by: Andres Vega Garcia

 INRIA - Sophia Antipolis, France
 e-mail: avega@sophia.inria.fr
 finger: avega@pax.inria.fr

 */

/*
 * Ethernet software status per interface.
 */
struct vx_softc {
    struct arpcom arpcom;	/* Ethernet common part		*/
    int unit;			/* unit number */
    bus_space_tag_t		bst;
    bus_space_handle_t		bsh;
    void			*vx_intrhand;
    struct resource		*vx_irq;
    struct resource		*vx_res;
#define MAX_MBS  8		/* # of mbufs we keep around	*/
    struct mbuf *mb[MAX_MBS];	/* spare mbuf storage.		*/
    int next_mb;		/* Which mbuf to use next. 	*/
    int last_mb;		/* Last mbuf.			*/
    char vx_connectors;		/* Connectors on this card.	*/
    char vx_connector;		/* Connector to use.		*/
    short tx_start_thresh;	/* Current TX_start_thresh.	*/
    int	tx_succ_ok;		/* # packets sent in sequence	*/
				/* w/o underrun			*/
    struct callout_handle ch;	/* Callout handle for timeouts  */
    int	buffill_pending;
};

#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->bst, sc->bsh, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->bst, sc->bsh, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->bst, sc->bsh, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->bst, sc->bsh, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->bst, sc->bsh, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->bst, sc->bsh, reg)

extern void vxfree(struct vx_softc *);
extern int vxattach(device_t);
extern void vxstop(struct vx_softc *);
extern void vxintr(void *);
extern int vxbusyeeprom(struct vx_softc *);
