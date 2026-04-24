/*-
 * Copyright (c) 2012 Semihalf.
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
 */

#ifndef DPAA_ETH_H_
#define DPAA_ETH_H_

struct dpaa_eth_softc {
	/* XXX MII bus requires that struct ifnet is first!!! */
	if_t				sc_ifnet;

	device_t			sc_dev;
	struct resource			*sc_mem;
	struct mtx			sc_lock;

	int				sc_mac_enet_mode;

	/* RX Pool */
	struct bman_pool		*sc_rx_pool;
	uint8_t				sc_rx_bpid;
	uma_zone_t			sc_rx_zone;
	char				sc_rx_zname[64];

	/* RX Frame Queue */
	struct qman_fq			*sc_rx_fq;
	uint32_t			sc_rx_fqid;

	/* TX Frame Queue */
	struct qman_fq			*sc_tx_fq;
	bool				sc_tx_fq_full;
	struct qman_fq			*sc_tx_conf_fq;
	uint32_t			sc_tx_conf_fqid;

	/* Methods */
	int				(*sc_port_rx_init)
	    (struct dpaa_eth_softc *sc, int unit);
	int				(*sc_port_tx_init)
	    (struct dpaa_eth_softc *sc, int unit);
	void				(*sc_start_locked)
	    (struct dpaa_eth_softc *sc);

	/* dTSEC data */
	uint8_t				sc_eth_id; /* Ethernet ID within its frame manager */
	uintptr_t			sc_mac_mem_offset;
	int				sc_mac_mdio_irq;
	uint8_t				sc_mac_addr[6];
	int				sc_port_rx_hw_id;
	int				sc_port_tx_hw_id;
	uint32_t			sc_port_tx_qman_chan;
	int				sc_phy_addr;
	bool				sc_hidden;
	device_t			sc_mdio;
	int				sc_rev_major;
	int				sc_rev_minor;

	device_t			sc_rx_port;
	device_t			sc_tx_port;

	int				sc_rx_channel;

	/* MII data */
	struct mii_data			*sc_mii;
	device_t			sc_mii_dev;
	struct mtx			sc_mii_lock;

	struct callout			sc_tick_callout;

	/* Frame Info Zone */
	uma_zone_t			sc_fi_zone;
	char				sc_fi_zname[64];
};

/**
 * @group dTSEC Regular Mode API.
 * @{
 */
int	dpaa_eth_fm_port_rx_init(struct dpaa_eth_softc *sc);
int	dpaa_eth_fm_port_tx_init(struct dpaa_eth_softc *sc);

void	dpaa_eth_if_start_locked(struct dpaa_eth_softc *sc);

int	dpaa_eth_pool_rx_init(struct dpaa_eth_softc *sc);
void	dpaa_eth_pool_rx_free(struct dpaa_eth_softc *sc);

int	dpaa_eth_fi_pool_init(struct dpaa_eth_softc *sc);
void	dpaa_eth_fi_pool_free(struct dpaa_eth_softc *sc);

int	dpaa_eth_fq_rx_init(struct dpaa_eth_softc *sc);
int	dpaa_eth_fq_tx_init(struct dpaa_eth_softc *sc);
void	dpaa_eth_fq_rx_free(struct dpaa_eth_softc *sc);
void	dpaa_eth_fq_tx_free(struct dpaa_eth_softc *sc);
/** @} */

#endif /* DPAA_ETH_H_ */
