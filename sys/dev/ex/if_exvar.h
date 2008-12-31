/*-
 * Copyright (c) 1996, Javier Mart^mn Rueda (jmrueda@diatel.upm.es)
 * All rights reserved.
 *
 * Copyright (c) 2000 Matthew N. Dodd
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
 *	$FreeBSD: src/sys/dev/ex/if_exvar.h,v 1.7.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

struct ex_softc {
  	struct ifnet	*ifp;
	struct ifmedia	ifmedia;
	u_char		enaddr[6];

	device_t	dev;
	struct resource *ioport;
	int		ioport_rid;
	struct resource *irq;
	int		irq_rid;
	void *		ih;

	bus_space_tag_t	bst;
	bus_space_handle_t bsh;
	u_short		irq_no;		/* IRQ number. */

	char *		irq2ee;		/* irq <-> internal		*/
	u_char *	ee2irq;		/* representation conversion	*/

	u_int		mem_size;	/* Total memory size, in bytes. */
	u_int		rx_mem_size;	/* Rx memory size (by default,	*/
					/* first 3/4 of total memory).	*/

	u_int		rx_lower_limit;	/* Lower and upper limits of	*/
	u_int		rx_upper_limit;	/* receive buffer.		*/

	u_int		rx_head;	/* Head of receive ring buffer. */
	u_int		tx_mem_size;	/* Tx memory size (by default,	*/
					/* last quarter of total memory).*/

	u_int		tx_lower_limit;	/* Lower and upper limits of	*/
	u_int		tx_upper_limit;	/* transmit buffer.		*/

	u_int		tx_head;	/* Head and tail of 		*/
	u_int		tx_tail;	/* transmit ring buffer.	*/

	u_int		tx_last;	/* Pointer to beginning of last	*/
					/* frame in the chain.		*/
};

extern devclass_t ex_devclass;

extern char	irq2eemap[];
extern u_char	ee2irqmap[];
extern char	plus_irq2eemap[];
extern u_char	plus_ee2irqmap[];

int		ex_alloc_resources(device_t);
void		ex_release_resources(device_t);
int		ex_attach(device_t);
int		ex_detach(device_t);

driver_intr_t	ex_intr;

u_int16_t	ex_eeprom_read(struct ex_softc *, int);
void		ex_get_address(struct ex_softc *, u_char *);
int		ex_card_type(u_char *);

void		ex_stop(struct ex_softc *);

#define CSR_READ_1(sc, off) (bus_space_read_1((sc)->bst, (sc)->bsh, off))
#define CSR_READ_2(sc, off) (bus_space_read_2((sc)->bst, (sc)->bsh, off))
#define CSR_WRITE_1(sc, off, val) \
	bus_space_write_1((sc)->bst, (sc)->bsh, off, val)
#define CSR_WRITE_2(sc, off, val) \
	bus_space_write_2((sc)->bst, (sc)->bsh, off, val)
#define CSR_WRITE_MULTI_1(sc, off, addr, count) \
	bus_space_write_multi_1((sc)->bst, (sc)->bsh, off, addr, count)
#define CSR_WRITE_MULTI_2(sc, off, addr, count) \
	bus_space_write_multi_2((sc)->bst, (sc)->bsh, off, addr, count)
#define CSR_WRITE_MULTI_4(sc, off, addr, count) \
	bus_space_write_multi_4((sc)->bst, (sc)->bsh, off, addr, count)
#define CSR_READ_MULTI_1(sc, off, addr, count) \
	bus_space_read_multi_1((sc)->bst, (sc)->bsh, off, addr, count)
#define CSR_READ_MULTI_2(sc, off, addr, count) \
	bus_space_read_multi_2((sc)->bst, (sc)->bsh, off, addr, count)
#define CSR_READ_MULTI_4(sc, off, addr, count) \
	bus_space_read_multi_4((sc)->bst, (sc)->bsh, off, addr, count)

#define EX_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	EX_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define EX_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    MTX_NETWORK_LOCK, MTX_DEF)
#define EX_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define EX_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define EX_ASSERT_UNLOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);
