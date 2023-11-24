/*
 * IEEE 802.1X-2010 Key Hierarchy
 * Copyright (c) 2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef IEEE802_1X_KEY_H
#define IEEE802_1X_KEY_H

int ieee802_1x_cak_aes_cmac(const u8 *msk, size_t msk_bytes, const u8 *mac1,
			    const u8 *mac2, u8 *cak, size_t cak_bytes);
int ieee802_1x_ckn_aes_cmac(const u8 *msk, size_t msk_bytes, const u8 *mac1,
			    const u8 *mac2, const u8 *sid,
			    size_t sid_bytes, u8 *ckn);
int ieee802_1x_kek_aes_cmac(const u8 *cak, size_t cak_bytes, const u8 *ckn,
			    size_t ckn_bytes, u8 *kek, size_t kek_bytes);
int ieee802_1x_ick_aes_cmac(const u8 *cak, size_t cak_bytes, const u8 *ckn,
			    size_t ckn_bytes, u8 *ick, size_t ick_bytes);
int ieee802_1x_icv_aes_cmac(const u8 *ick, size_t ick_bytes, const u8 *msg,
			    size_t msg_bytes, u8 *icv);
int ieee802_1x_sak_aes_cmac(const u8 *cak, size_t cak_bytes, const u8 *ctx,
			    size_t ctx_bytes, u8 *sak, size_t sak_bytes);

#endif /* IEEE802_1X_KEY_H */
