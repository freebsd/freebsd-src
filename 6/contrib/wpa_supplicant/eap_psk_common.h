/*
 * WPA Supplicant / EAP-PSK shared routines
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef EAP_PSK_COMMON_H
#define EAP_PSK_COMMON_H


#define EAP_PSK_RAND_LEN 16
#define EAP_PSK_MAC_LEN 16
#define EAP_PSK_TEK_LEN 16
#define EAP_PSK_MSK_LEN 64
#define EAP_PSK_PSK_LEN 16
#define EAP_PSK_AK_LEN 16
#define EAP_PSK_KDK_LEN 16

#define EAP_PSK_R_FLAG_CONT 1
#define EAP_PSK_R_FLAG_DONE_SUCCESS 2
#define EAP_PSK_R_FLAG_DONE_FAILURE 3
#define EAP_PSK_E_FLAG 0x20

/* Shared prefix for all EAP-PSK frames */
struct eap_psk_hdr {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PSK */
	u8 flags;
} __attribute__ ((packed));

/* EAP-PSK First Message (AS -> Supplicant) */
struct eap_psk_hdr_1 {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PSK */
	u8 flags;
	u8 rand_s[EAP_PSK_RAND_LEN];
	/* Followed by variable length ID_S */
} __attribute__ ((packed));

/* EAP-PSK Second Message (Supplicant -> AS) */
struct eap_psk_hdr_2 {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PSK */
	u8 flags;
	u8 rand_s[EAP_PSK_RAND_LEN];
	u8 rand_p[EAP_PSK_RAND_LEN];
	u8 mac_p[EAP_PSK_MAC_LEN];
	/* Followed by variable length ID_P */
} __attribute__ ((packed));

/* EAP-PSK Third Message (AS -> Supplicant) */
struct eap_psk_hdr_3 {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PSK */
	u8 flags;
	u8 rand_s[EAP_PSK_RAND_LEN];
	u8 mac_s[EAP_PSK_MAC_LEN];
	/* Followed by variable length PCHANNEL */
} __attribute__ ((packed));

/* EAP-PSK Fourth Message (Supplicant -> AS) */
struct eap_psk_hdr_4 {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PSK */
	u8 flags;
	u8 rand_s[EAP_PSK_RAND_LEN];
	/* Followed by variable length PCHANNEL */
} __attribute__ ((packed));


void eap_psk_key_setup(const u8 *psk, u8 *ak, u8 *kdk);
void eap_psk_derive_keys(const u8 *kdk, const u8 *rand_p, u8 *tek, u8 *msk);

#endif /* EAP_PSK_COMMON_H */
