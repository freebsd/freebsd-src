/*
 * Copyright (c) 2025 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef	__IF_RGE_HW_H__
#define	__IF_RGE_HW_H__

struct rge_softc;

extern	int rge_chipinit(struct rge_softc *);
extern	void rge_mac_config_mcu(struct rge_softc *, enum rge_mac_type);
extern	void rge_write_mac_ocp(struct rge_softc *, uint16_t, uint16_t);
extern	uint16_t rge_read_mac_ocp(struct rge_softc *, uint16_t);
extern	void rge_ephy_config(struct rge_softc *);
extern	int rge_phy_config(struct rge_softc *);
extern	void rge_set_macaddr(struct rge_softc *, const uint8_t *);
extern	void rge_get_macaddr(struct rge_softc *, uint8_t *);
extern	void rge_hw_reset(struct rge_softc *);
extern	void rge_config_imtype(struct rge_softc *, int);
extern	void rge_disable_aspm_clkreq(struct rge_softc *);
extern	void rge_setup_intr(struct rge_softc *, int);
extern	void rge_write_csi(struct rge_softc *, uint32_t, uint32_t);
extern	uint32_t rge_read_csi(struct rge_softc *, uint32_t);
extern	void rge_write_phy(struct rge_softc *, uint16_t, uint16_t, uint16_t);
extern	uint16_t rge_read_phy(struct rge_softc *, uint16_t, uint16_t);
extern	void rge_write_phy_ocp(struct rge_softc *, uint16_t, uint16_t);
extern	uint16_t rge_read_phy_ocp(struct rge_softc *sc, uint16_t reg);
extern	int rge_get_link_status(struct rge_softc *);

#endif	/* __IF_RGE_HW_H__ */
