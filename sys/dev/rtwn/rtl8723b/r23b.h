/*
 * Copyright (c) 2026 Ahmad Khalifa <vexeduxr@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef RTL8723B_H
#define RTL8723B_H

/*
 * Global definitions.
 */
#define R23B_PUBQ_NPAGES	231
#define R23B_HPQ_NPAGES		12
#define R23B_LPQ_NPAGES		2
#define R23B_NPQ_NPAGES		2

#define R23B_TX_PAGE_COUNT	247

#define R23B_RX_DMA_BUFFER_SIZE	0x3f80

#define R23B_CALIB_THRESHOLD	7

/*
 * Function declarations.
 */

/* r23b_calib.c */
void r23b_iq_calib(struct rtwn_softc *);
void r23b_lc_calib(struct rtwn_softc *);
void r23b_temp_measure(struct rtwn_softc *);

/* r23b_fw.c */
#ifndef RTWN_WITHOUT_UCODE
void r23b_fw_download_enable(struct rtwn_softc *, int);
void r23b_fw_reset(struct rtwn_softc *, int);
#endif

/* r23b_init.c */
void r23b_init_antsel(struct rtwn_softc *);
void r23b_init_bb_common(struct rtwn_softc *);
void r23b_init_rf(struct rtwn_softc *);
void r23b_post_init(struct rtwn_softc *);
int r23b_set_page_size(struct rtwn_softc *);

/* r23b_led.c */
void r23b_set_led(struct rtwn_softc *, int, int);

/* r23b_rf.c */
void r23b_rf_write(struct rtwn_softc *, int, uint8_t, uint32_t);

/* r23b_rom.c */
int r23b_efuse_preread(struct rtwn_softc *);
void r23b_parse_rom(struct rtwn_softc *, uint8_t *);

/* r23b_rx.c */
int8_t r23b_get_rssi_cck(struct rtwn_softc *, void *);

#endif /* RTL8723B_H */
