/*	$NetBSD: lsi64854var.h,v 1.6 2005/02/04 02:10:36 perry Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*	$FreeBSD: src/sys/sparc64/sbus/lsi64854var.h,v 1.6 2006/01/31 12:50:02 marius Exp $ */

struct lsi64854_softc {
	device_t		sc_dev;

	int			sc_rid;
	struct resource		*sc_res;
	bus_space_handle_t	sc_regh;
	bus_space_tag_t		sc_regt;
	u_int			sc_rev;		/* revision */
	int			sc_burst;	/* max suported burst size */

	int			sc_channel;
#define L64854_CHANNEL_SCSI	1
#define L64854_CHANNEL_ENET	2
#define L64854_CHANNEL_PP	3
	void			*sc_client;

	int			sc_active;	/* DMA active ? */
	bus_dmamap_t		sc_dmamap;	/* DMA map for bus_dma_* */

	bus_dma_tag_t		sc_parent_dmat;
	bus_dma_tag_t		sc_buffer_dmat;
	int			sc_datain;
	size_t			sc_dmasize;
	caddr_t			*sc_dmaaddr;
	size_t			*sc_dmalen;

	void	(*reset)(struct lsi64854_softc *);/* reset routine */
	int	(*setup)(struct lsi64854_softc *, caddr_t *, size_t *,
			 int, size_t *);	/* DMA setup */
	int	(*intr)(void *);		/* interrupt handler */

	u_int 			sc_dmactl;
	int			sc_dodrain;
};

#define L64854_GCSR(sc)	\
	(bus_space_read_4((sc)->sc_regt, (sc)->sc_regh, L64854_REG_CSR))

#define L64854_SCSR(sc, csr)	\
	bus_space_write_4((sc)->sc_regt, (sc)->sc_regh, L64854_REG_CSR, csr)


/*
 * DMA engine interface functions.
 */
#define DMA_RESET(sc)			(((sc)->reset)(sc))
#define DMA_INTR(sc)			(((sc)->intr)(sc))
#define DMA_SETUP(sc, a, l, d, s)	(((sc)->setup)(sc, a, l, d, s))

#define DMA_ISACTIVE(sc)		((sc)->sc_active)

#define DMA_ENINTR(sc) do {			\
	uint32_t csr = L64854_GCSR(sc);		\
	csr |= L64854_INT_EN;			\
	L64854_SCSR(sc, csr);			\
} while (0)

#define DMA_ISINTR(sc)	(L64854_GCSR(sc) & (D_INT_PEND|D_ERR_PEND))

#define DMA_GO(sc) do {				\
	uint32_t csr = L64854_GCSR(sc);		\
	csr |= D_EN_DMA;			\
	L64854_SCSR(sc, csr);			\
	sc->sc_active = 1;			\
} while (0)


int	lsi64854_attach(struct lsi64854_softc *);
int	lsi64854_detach(struct lsi64854_softc *);
