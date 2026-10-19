/*
 * Copyright (c) 2026 Mir Saheb Ali <mirsahebali@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef RTL8188F_H
#define RTL8188F_H

#include <dev/rtwn/if_rtwn_ridx.h>

/*
 * Global Definitions.
 */
#define R88F_HQ_NPAGES		12
#define R88F_LQ_NPAGES		2
#define R88F_NQ_NPAGES		2
#define R88F_TXPKTBUF_COUNT	177
#define R88F_TX_PAGE_COUNT	247
#define R88F_RX_DMA_BUFFER_SIZE	0x3f80
#define R88F_MAX_FW_SIZE	21020
#define R88F_RTWN_POWER_COUNT	28

/*
 * Function declarations.
 */

/* r88f_rom.c */
int	r88f_efuse_preread(struct rtwn_softc *);
void	r88f_parse_rom(struct rtwn_softc *, uint8_t *);

/* r88f_rx.c */
int8_t	r88f_get_rssi_cck(struct rtwn_softc *, void *);
int8_t	r88f_get_rssi_ofdm(struct rtwn_softc *, void *);
void	r88f_get_rx_stats(struct rtwn_softc *, struct ieee80211_rx_stats *,
		       const void *, const void *);

/* r88f_init.c */
int	r88f_set_page_size(struct rtwn_softc *);
void	r88f_init_ampdu(struct rtwn_softc *);
int	r88f_llt_init(struct rtwn_softc *);
void	r88f_init_rf(struct rtwn_softc *);

/* r88f_calib.c */
void	r88f_lc_calib(struct rtwn_softc *);
void	r88f_iq_calib(struct rtwn_softc *);

/* r88f_chan.c */
void	r88f_set_chan(struct rtwn_softc *, struct ieee80211_channel *);
void	r88f_get_txpower(struct rtwn_softc *, int, struct ieee80211_channel *,
		      uint8_t[RTWN_RIDX_COUNT]);

/* r88f_fw.c */
void	r88f_fw_reset(struct rtwn_softc *, int);
void	r88f_fw_download_enable(struct rtwn_softc *, int);

#endif /* RTL8188F_H */
