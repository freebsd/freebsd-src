/*
 * WPA definitions shared between hostapd and wpa_supplicant
 * Copyright (c) 2002-2018, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef WPA_COMMON_H
#define WPA_COMMON_H

#include "common/defs.h"

/* IEEE 802.11i */
#define PMKID_LEN 16
#define PMK_LEN 32
#define PMK_LEN_SUITE_B_192 48
#define PMK_LEN_MAX 64
#define WPA_REPLAY_COUNTER_LEN 8
#define RSN_PN_LEN 6
#define WPA_NONCE_LEN 32
#define WPA_KEY_RSC_LEN 8
#define WPA_GMK_LEN 32
#define WPA_GTK_MAX_LEN 32
#define WPA_PASN_PMK_LEN 32
#define WPA_PASN_MAX_MIC_LEN 24
#define WPA_MAX_RSNXE_LEN 4

#define OWE_DH_GROUP 19

#ifdef CONFIG_NO_TKIP
#define WPA_ALLOWED_PAIRWISE_CIPHERS \
(WPA_CIPHER_CCMP | WPA_CIPHER_GCMP | WPA_CIPHER_NONE | \
WPA_CIPHER_GCMP_256 | WPA_CIPHER_CCMP_256)
#define WPA_ALLOWED_GROUP_CIPHERS \
(WPA_CIPHER_CCMP | WPA_CIPHER_GCMP | \
WPA_CIPHER_GCMP_256 | WPA_CIPHER_CCMP_256 | \
WPA_CIPHER_GTK_NOT_USED)
#else /* CONFIG_NO_TKIP */
#define WPA_ALLOWED_PAIRWISE_CIPHERS \
(WPA_CIPHER_CCMP | WPA_CIPHER_GCMP | WPA_CIPHER_TKIP | WPA_CIPHER_NONE | \
WPA_CIPHER_GCMP_256 | WPA_CIPHER_CCMP_256)
#define WPA_ALLOWED_GROUP_CIPHERS \
(WPA_CIPHER_CCMP | WPA_CIPHER_GCMP | WPA_CIPHER_TKIP | \
WPA_CIPHER_GCMP_256 | WPA_CIPHER_CCMP_256 | \
WPA_CIPHER_GTK_NOT_USED)
#endif /* CONFIG_NO_TKIP */
#define WPA_ALLOWED_GROUP_MGMT_CIPHERS \
(WPA_CIPHER_AES_128_CMAC | WPA_CIPHER_BIP_GMAC_128 | WPA_CIPHER_BIP_GMAC_256 | \
WPA_CIPHER_BIP_CMAC_256)

#define WPA_SELECTOR_LEN 4
#define WPA_VERSION 1
#define RSN_SELECTOR_LEN 4
#define RSN_VERSION 1

#define RSN_SELECTOR(a, b, c, d) \
	((((u32) (a)) << 24) | (((u32) (b)) << 16) | (((u32) (c)) << 8) | \
	 (u32) (d))

#define WPA_AUTH_KEY_MGMT_NONE RSN_SELECTOR(0x00, 0x50, 0xf2, 0)
#define WPA_AUTH_KEY_MGMT_UNSPEC_802_1X RSN_SELECTOR(0x00, 0x50, 0xf2, 1)
#define WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X RSN_SELECTOR(0x00, 0x50, 0xf2, 2)
#define WPA_AUTH_KEY_MGMT_CCKM RSN_SELECTOR(0x00, 0x40, 0x96, 0)
#define WPA_CIPHER_SUITE_NONE RSN_SELECTOR(0x00, 0x50, 0xf2, 0)
#define WPA_CIPHER_SUITE_TKIP RSN_SELECTOR(0x00, 0x50, 0xf2, 2)
#define WPA_CIPHER_SUITE_CCMP RSN_SELECTOR(0x00, 0x50, 0xf2, 4)


#define RSN_AUTH_KEY_MGMT_UNSPEC_802_1X RSN_SELECTOR(0x00, 0x0f, 0xac, 1)
#define RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X RSN_SELECTOR(0x00, 0x0f, 0xac, 2)
#define RSN_AUTH_KEY_MGMT_FT_802_1X RSN_SELECTOR(0x00, 0x0f, 0xac, 3)
#define RSN_AUTH_KEY_MGMT_FT_PSK RSN_SELECTOR(0x00, 0x0f, 0xac, 4)
#define RSN_AUTH_KEY_MGMT_802_1X_SHA256 RSN_SELECTOR(0x00, 0x0f, 0xac, 5)
#define RSN_AUTH_KEY_MGMT_PSK_SHA256 RSN_SELECTOR(0x00, 0x0f, 0xac, 6)
#define RSN_AUTH_KEY_MGMT_TPK_HANDSHAKE RSN_SELECTOR(0x00, 0x0f, 0xac, 7)
#define RSN_AUTH_KEY_MGMT_SAE RSN_SELECTOR(0x00, 0x0f, 0xac, 8)
#define RSN_AUTH_KEY_MGMT_FT_SAE RSN_SELECTOR(0x00, 0x0f, 0xac, 9)
#define RSN_AUTH_KEY_MGMT_802_1X_SUITE_B RSN_SELECTOR(0x00, 0x0f, 0xac, 11)
#define RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192 RSN_SELECTOR(0x00, 0x0f, 0xac, 12)
#define RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384 RSN_SELECTOR(0x00, 0x0f, 0xac, 13)
#define RSN_AUTH_KEY_MGMT_FILS_SHA256 RSN_SELECTOR(0x00, 0x0f, 0xac, 14)
#define RSN_AUTH_KEY_MGMT_FILS_SHA384 RSN_SELECTOR(0x00, 0x0f, 0xac, 15)
#define RSN_AUTH_KEY_MGMT_FT_FILS_SHA256 RSN_SELECTOR(0x00, 0x0f, 0xac, 16)
#define RSN_AUTH_KEY_MGMT_FT_FILS_SHA384 RSN_SELECTOR(0x00, 0x0f, 0xac, 17)
#define RSN_AUTH_KEY_MGMT_OWE RSN_SELECTOR(0x00, 0x0f, 0xac, 18)
#define RSN_AUTH_KEY_MGMT_FT_PSK_SHA384 RSN_SELECTOR(0x00, 0x0f, 0xac, 19)
#define RSN_AUTH_KEY_MGMT_PSK_SHA384 RSN_SELECTOR(0x00, 0x0f, 0xac, 20)
#define RSN_AUTH_KEY_MGMT_PASN RSN_SELECTOR(0x00, 0x0f, 0xac, 21)
#define RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384_UNRESTRICTED \
	RSN_SELECTOR(0x00, 0x0f, 0xac, 22)
#define RSN_AUTH_KEY_MGMT_802_1X_SHA384 RSN_SELECTOR(0x00, 0x0f, 0xac, 23)
#define RSN_AUTH_KEY_MGMT_SAE_EXT_KEY RSN_SELECTOR(0x00, 0x0f, 0xac, 24)
#define RSN_AUTH_KEY_MGMT_FT_SAE_EXT_KEY RSN_SELECTOR(0x00, 0x0f, 0xac, 25)

#define RSN_AUTH_KEY_MGMT_CCKM RSN_SELECTOR(0x00, 0x40, 0x96, 0x00)
#define RSN_AUTH_KEY_MGMT_OSEN RSN_SELECTOR(0x50, 0x6f, 0x9a, 0x01)
#define RSN_AUTH_KEY_MGMT_DPP RSN_SELECTOR(0x50, 0x6f, 0x9a, 0x02)

#define RSN_CIPHER_SUITE_NONE RSN_SELECTOR(0x00, 0x0f, 0xac, 0)
#define RSN_CIPHER_SUITE_WEP40 RSN_SELECTOR(0x00, 0x0f, 0xac, 1)
#define RSN_CIPHER_SUITE_TKIP RSN_SELECTOR(0x00, 0x0f, 0xac, 2)
#if 0
#define RSN_CIPHER_SUITE_WRAP RSN_SELECTOR(0x00, 0x0f, 0xac, 3)
#endif
#define RSN_CIPHER_SUITE_CCMP RSN_SELECTOR(0x00, 0x0f, 0xac, 4)
#define RSN_CIPHER_SUITE_WEP104 RSN_SELECTOR(0x00, 0x0f, 0xac, 5)
#define RSN_CIPHER_SUITE_AES_128_CMAC RSN_SELECTOR(0x00, 0x0f, 0xac, 6)
#define RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED RSN_SELECTOR(0x00, 0x0f, 0xac, 7)
#define RSN_CIPHER_SUITE_GCMP RSN_SELECTOR(0x00, 0x0f, 0xac, 8)
#define RSN_CIPHER_SUITE_GCMP_256 RSN_SELECTOR(0x00, 0x0f, 0xac, 9)
#define RSN_CIPHER_SUITE_CCMP_256 RSN_SELECTOR(0x00, 0x0f, 0xac, 10)
#define RSN_CIPHER_SUITE_BIP_GMAC_128 RSN_SELECTOR(0x00, 0x0f, 0xac, 11)
#define RSN_CIPHER_SUITE_BIP_GMAC_256 RSN_SELECTOR(0x00, 0x0f, 0xac, 12)
#define RSN_CIPHER_SUITE_BIP_CMAC_256 RSN_SELECTOR(0x00, 0x0f, 0xac, 13)
#define RSN_CIPHER_SUITE_SMS4 RSN_SELECTOR(0x00, 0x14, 0x72, 1)
#define RSN_CIPHER_SUITE_CKIP RSN_SELECTOR(0x00, 0x40, 0x96, 0)
#define RSN_CIPHER_SUITE_CKIP_CMIC RSN_SELECTOR(0x00, 0x40, 0x96, 1)
#define RSN_CIPHER_SUITE_CMIC RSN_SELECTOR(0x00, 0x40, 0x96, 2)
/* KRK is defined for nl80211 use only */
#define RSN_CIPHER_SUITE_KRK RSN_SELECTOR(0x00, 0x40, 0x96, 255)

/* EAPOL-Key Key Data Encapsulation
 * GroupKey and PeerKey require encryption, otherwise, encryption is optional.
 */
#define RSN_KEY_DATA_GROUPKEY RSN_SELECTOR(0x00, 0x0f, 0xac, 1)
#if 0
#define RSN_KEY_DATA_STAKEY RSN_SELECTOR(0x00, 0x0f, 0xac, 2)
#endif
#define RSN_KEY_DATA_MAC_ADDR RSN_SELECTOR(0x00, 0x0f, 0xac, 3)
#define RSN_KEY_DATA_PMKID RSN_SELECTOR(0x00, 0x0f, 0xac, 4)
#define RSN_KEY_DATA_IGTK RSN_SELECTOR(0x00, 0x0f, 0xac, 9)
#define RSN_KEY_DATA_KEYID RSN_SELECTOR(0x00, 0x0f, 0xac, 10)
#define RSN_KEY_DATA_MULTIBAND_GTK RSN_SELECTOR(0x00, 0x0f, 0xac, 11)
#define RSN_KEY_DATA_MULTIBAND_KEYID RSN_SELECTOR(0x00, 0x0f, 0xac, 12)
#define RSN_KEY_DATA_OCI RSN_SELECTOR(0x00, 0x0f, 0xac, 13)
#define RSN_KEY_DATA_BIGTK RSN_SELECTOR(0x00, 0x0f, 0xac, 14)
#define RSN_KEY_DATA_MLO_GTK RSN_SELECTOR(0x00, 0x0f, 0xac, 16)
#define RSN_KEY_DATA_MLO_IGTK RSN_SELECTOR(0x00, 0x0f, 0xac, 17)
#define RSN_KEY_DATA_MLO_BIGTK RSN_SELECTOR(0x00, 0x0f, 0xac, 18)
#define RSN_KEY_DATA_MLO_LINK RSN_SELECTOR(0x00, 0x0f, 0xac, 19)

#define WFA_KEY_DATA_IP_ADDR_REQ RSN_SELECTOR(0x50, 0x6f, 0x9a, 4)
#define WFA_KEY_DATA_IP_ADDR_ALLOC RSN_SELECTOR(0x50, 0x6f, 0x9a, 5)
#define WFA_KEY_DATA_TRANSITION_DISABLE RSN_SELECTOR(0x50, 0x6f, 0x9a, 0x20)
#define WFA_KEY_DATA_DPP RSN_SELECTOR(0x50, 0x6f, 0x9a, 0x21)

#define WPA_OUI_TYPE RSN_SELECTOR(0x00, 0x50, 0xf2, 1)

#define RSN_SELECTOR_PUT(a, val) WPA_PUT_BE32((u8 *) (a), (val))
#define RSN_SELECTOR_GET(a) WPA_GET_BE32((const u8 *) (a))

#define RSN_NUM_REPLAY_COUNTERS_1 0
#define RSN_NUM_REPLAY_COUNTERS_2 1
#define RSN_NUM_REPLAY_COUNTERS_4 2
#define RSN_NUM_REPLAY_COUNTERS_16 3


#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

#define WPA_IGTK_LEN 16
#define WPA_IGTK_MAX_LEN 32
#define WPA_BIGTK_LEN 16
#define WPA_BIGTK_MAX_LEN 32


/* IEEE 802.11, 7.3.2.25.3 RSN Capabilities */
#define WPA_CAPABILITY_PREAUTH BIT(0)
#define WPA_CAPABILITY_NO_PAIRWISE BIT(1)
/* B2-B3: PTKSA Replay Counter */
/* B4-B5: GTKSA Replay Counter */
#define WPA_CAPABILITY_MFPR BIT(6)
#define WPA_CAPABILITY_MFPC BIT(7)
/* B8: Reserved */
#define WPA_CAPABILITY_PEERKEY_ENABLED BIT(9)
#define WPA_CAPABILITY_SPP_A_MSDU_CAPABLE BIT(10)
#define WPA_CAPABILITY_SPP_A_MSDU_REQUIRED BIT(11)
#define WPA_CAPABILITY_PBAC BIT(12)
#define WPA_CAPABILITY_EXT_KEY_ID_FOR_UNICAST BIT(13)
#define WPA_CAPABILITY_OCVC BIT(14)
/* B15: Reserved */


/* IEEE 802.11r */
#define MOBILITY_DOMAIN_ID_LEN 2
#define FT_R0KH_ID_MAX_LEN 48
#define FT_R1KH_ID_LEN 6
#define WPA_PMK_NAME_LEN 16

/* FTE - MIC Control - RSNXE Used */
#define FTE_MIC_CTRL_RSNXE_USED BIT(0)
#define FTE_MIC_CTRL_MIC_LEN_MASK (BIT(1) | BIT(2) | BIT(3))
#define FTE_MIC_CTRL_MIC_LEN_SHIFT 1

/* FTE - MIC Length subfield values */
enum ft_mic_len_subfield {
	FTE_MIC_LEN_16 = 0,
	FTE_MIC_LEN_24 = 1,
	FTE_MIC_LEN_32 = 2,
};


/* IEEE 802.11, 8.5.2 EAPOL-Key frames */
#define WPA_KEY_INFO_TYPE_MASK ((u16) (BIT(0) | BIT(1) | BIT(2)))
#define WPA_KEY_INFO_TYPE_AKM_DEFINED 0
#define WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 BIT(0)
#define WPA_KEY_INFO_TYPE_HMAC_SHA1_AES BIT(1)
#define WPA_KEY_INFO_TYPE_AES_128_CMAC 3
#define WPA_KEY_INFO_KEY_TYPE BIT(3) /* 1 = Pairwise, 0 = Group key */
/* bit4..5 is used in WPA, but is reserved in IEEE 802.11i/RSN */
#define WPA_KEY_INFO_KEY_INDEX_MASK (BIT(4) | BIT(5))
#define WPA_KEY_INFO_KEY_INDEX_SHIFT 4
#define WPA_KEY_INFO_INSTALL BIT(6) /* pairwise */
#define WPA_KEY_INFO_TXRX BIT(6) /* group */
#define WPA_KEY_INFO_ACK BIT(7)
#define WPA_KEY_INFO_MIC BIT(8)
#define WPA_KEY_INFO_SECURE BIT(9)
#define WPA_KEY_INFO_ERROR BIT(10)
#define WPA_KEY_INFO_REQUEST BIT(11)
#define WPA_KEY_INFO_ENCR_KEY_DATA BIT(12) /* IEEE 802.11i/RSN only */
#define WPA_KEY_INFO_SMK_MESSAGE BIT(13)


struct wpa_eapol_key {
	u8 type;
	/* Note: key_info, key_length, and key_data_length are unaligned */
	u8 key_info[2]; /* big endian */
	u8 key_length[2]; /* big endian */
	u8 replay_counter[WPA_REPLAY_COUNTER_LEN];
	u8 key_nonce[WPA_NONCE_LEN];
	u8 key_iv[16];
	u8 key_rsc[WPA_KEY_RSC_LEN];
	u8 key_id[8]; /* Reserved in IEEE 802.11i/RSN */
	/* variable length Key MIC field */
	/* big endian 2-octet Key Data Length field */
	/* followed by Key Data Length bytes of Key Data */
} STRUCT_PACKED;

#define WPA_EAPOL_KEY_MIC_MAX_LEN 32
#define WPA_KCK_MAX_LEN 32
#define WPA_KEK_MAX_LEN 64
#define WPA_TK_MAX_LEN 32
#define WPA_KDK_MAX_LEN 32
#define FILS_ICK_MAX_LEN 48
#define FILS_FT_MAX_LEN 48
#define WPA_PASN_KCK_LEN 32
#define WPA_PASN_MIC_MAX_LEN 24
#define WPA_LTF_KEYSEED_MAX_LEN 48

/**
 * struct wpa_ptk - WPA Pairwise Transient Key
 * IEEE Std 802.11i-2004 - 8.5.1.2 Pairwise key hierarchy
 */
struct wpa_ptk {
	u8 kck[WPA_KCK_MAX_LEN]; /* EAPOL-Key Key Confirmation Key (KCK) */
	u8 kek[WPA_KEK_MAX_LEN]; /* EAPOL-Key Key Encryption Key (KEK) */
	u8 tk[WPA_TK_MAX_LEN]; /* Temporal Key (TK) */
	u8 kck2[WPA_KCK_MAX_LEN]; /* FT reasoc Key Confirmation Key (KCK2) */
	u8 kek2[WPA_KEK_MAX_LEN]; /* FT reassoc Key Encryption Key (KEK2) */
	u8 kdk[WPA_KDK_MAX_LEN]; /* Key Derivation Key */
	u8 ltf_keyseed[WPA_LTF_KEYSEED_MAX_LEN]; /* LTF Key seed */
	size_t kck_len;
	size_t kek_len;
	size_t tk_len;
	size_t kck2_len;
	size_t kek2_len;
	size_t kdk_len;
	size_t ltf_keyseed_len;
	int installed; /* 1 if key has already been installed to driver */
};

struct wpa_gtk {
	u8 gtk[WPA_GTK_MAX_LEN];
	size_t gtk_len;
};

struct wpa_igtk {
	u8 igtk[WPA_IGTK_MAX_LEN];
	size_t igtk_len;
};

struct wpa_bigtk {
	u8 bigtk[WPA_BIGTK_MAX_LEN];
	size_t bigtk_len;
};

/* WPA IE version 1
 * 00-50-f2:1 (OUI:OUI type)
 * 0x01 0x00 (version; little endian)
 * (all following fields are optional:)
 * Group Suite Selector (4 octets) (default: TKIP)
 * Pairwise Suite Count (2 octets, little endian) (default: 1)
 * Pairwise Suite List (4 * n octets) (default: TKIP)
 * Authenticated Key Management Suite Count (2 octets, little endian)
 *    (default: 1)
 * Authenticated Key Management Suite List (4 * n octets)
 *    (default: unspec 802.1X)
 * WPA Capabilities (2 octets, little endian) (default: 0)
 */

struct wpa_ie_hdr {
	u8 elem_id;
	u8 len;
	u8 oui[4]; /* 24-bit OUI followed by 8-bit OUI type */
	u8 version[2]; /* little endian */
} STRUCT_PACKED;


/* 1/4: PMKID
 * 2/4: RSN IE
 * 3/4: one or two RSN IEs + GTK IE (encrypted)
 * 4/4: empty
 * 1/2: GTK IE (encrypted)
 * 2/2: empty
 */

/* RSN IE version 1
 * 0x01 0x00 (version; little endian)
 * (all following fields are optional:)
 * Group Suite Selector (4 octets) (default: CCMP)
 * Pairwise Suite Count (2 octets, little endian) (default: 1)
 * Pairwise Suite List (4 * n octets) (default: CCMP)
 * Authenticated Key Management Suite Count (2 octets, little endian)
 *    (default: 1)
 * Authenticated Key Management Suite List (4 * n octets)
 *    (default: unspec 802.1X)
 * RSN Capabilities (2 octets, little endian) (default: 0)
 * PMKID Count (2 octets) (default: 0)
 * PMKID List (16 * n octets)
 * Management Group Cipher Suite (4 octets) (default: AES-128-CMAC)
 */

struct rsn_ie_hdr {
	u8 elem_id; /* WLAN_EID_RSN */
	u8 len;
	u8 version[2]; /* little endian */
} STRUCT_PACKED;


#define KDE_HDR_LEN (1 + 1 + RSN_SELECTOR_LEN)

struct rsn_error_kde {
	be16 mui;
	be16 error_type;
} STRUCT_PACKED;

#define WPA_IGTK_KDE_PREFIX_LEN (2 + RSN_PN_LEN)
struct wpa_igtk_kde {
	u8 keyid[2];
	u8 pn[RSN_PN_LEN];
	u8 igtk[WPA_IGTK_MAX_LEN];
} STRUCT_PACKED;

#define WPA_BIGTK_KDE_PREFIX_LEN (2 + RSN_PN_LEN)
struct wpa_bigtk_kde {
	u8 keyid[2];
	u8 pn[RSN_PN_LEN];
	u8 bigtk[WPA_BIGTK_MAX_LEN];
} STRUCT_PACKED;

#define RSN_MLO_GTK_KDE_PREFIX_LENGTH		(1 + RSN_PN_LEN)
#define RSN_MLO_GTK_KDE_PREFIX0_KEY_ID_MASK	0x03
#define RSN_MLO_GTK_KDE_PREFIX0_TX		0x04
#define RSN_MLO_GTK_KDE_PREFIX0_LINK_ID_SHIFT	4
#define RSN_MLO_GTK_KDE_PREFIX0_LINK_ID_MASK	0xF0

#define RSN_MLO_IGTK_KDE_PREFIX_LENGTH		(2 + RSN_PN_LEN + 1)
#define RSN_MLO_IGTK_KDE_PREFIX8_LINK_ID_SHIFT	4
#define RSN_MLO_IGTK_KDE_PREFIX8_LINK_ID_MASK	0xF0
struct rsn_mlo_igtk_kde {
	u8 keyid[2];
	u8 pn[RSN_PN_LEN];
	u8 prefix8;
	u8 igtk[WPA_IGTK_MAX_LEN];
} STRUCT_PACKED;

#define RSN_MLO_BIGTK_KDE_PREFIX_LENGTH		(2 + RSN_PN_LEN + 1)
#define RSN_MLO_BIGTK_KDE_PREFIX8_LINK_ID_SHIFT	4
#define RSN_MLO_BIGTK_KDE_PREFIX8_LINK_ID_MASK	0xF0
struct rsn_mlo_bigtk_kde {
	u8 keyid[2];
	u8 pn[RSN_PN_LEN];
	u8 prefix8;
	u8 bigtk[WPA_BIGTK_MAX_LEN];
} STRUCT_PACKED;

#define RSN_MLO_LINK_KDE_FIXED_LENGTH		(1 + ETH_ALEN)
#define RSN_MLO_LINK_KDE_LINK_INFO_INDEX	0
#define RSN_MLO_LINK_KDE_LI_LINK_ID_SHIFT	0
#define RSN_MLO_LINK_KDE_LI_LINK_ID_MASK	0x0F
#define RSN_MLO_LINK_KDE_LI_RSNE_INFO		0x10
#define RSN_MLO_LINK_KDE_LI_RSNXE_INFO		0x20
#define RSN_MLO_LINK_KDE_LINK_MAC_INDEX		1

struct rsn_mdie {
	u8 mobility_domain[MOBILITY_DOMAIN_ID_LEN];
	u8 ft_capab;
} STRUCT_PACKED;

#define RSN_FT_CAPAB_FT_OVER_DS BIT(0)
#define RSN_FT_CAPAB_FT_RESOURCE_REQ_SUPP BIT(1)

struct rsn_ftie {
	u8 mic_control[2];
	u8 mic[16];
	u8 anonce[WPA_NONCE_LEN];
	u8 snonce[WPA_NONCE_LEN];
	/* followed by optional parameters */
} STRUCT_PACKED;

struct rsn_ftie_sha384 {
	u8 mic_control[2];
	u8 mic[24];
	u8 anonce[WPA_NONCE_LEN];
	u8 snonce[WPA_NONCE_LEN];
	/* followed by optional parameters */
} STRUCT_PACKED;

struct rsn_ftie_sha512 {
	u8 mic_control[2];
	u8 mic[32];
	u8 anonce[WPA_NONCE_LEN];
	u8 snonce[WPA_NONCE_LEN];
	/* followed by optional parameters */
} STRUCT_PACKED;

#define FTIE_SUBELEM_R1KH_ID 1
#define FTIE_SUBELEM_GTK 2
#define FTIE_SUBELEM_R0KH_ID 3
#define FTIE_SUBELEM_IGTK 4
#define FTIE_SUBELEM_OCI 5
#define FTIE_SUBELEM_BIGTK 6
#define FTIE_SUBELEM_MLO_GTK 8
#define FTIE_SUBELEM_MLO_IGTK 9
#define FTIE_SUBELEM_MLO_BIGTK 10

struct rsn_rdie {
	u8 id;
	u8 descr_count;
	le16 status_code;
} STRUCT_PACKED;

/* WFA Transition Disable KDE (using OUI_WFA) */
/* Transition Disable Bitmap bits */
#define TRANSITION_DISABLE_WPA3_PERSONAL BIT(0)
#define TRANSITION_DISABLE_SAE_PK BIT(1)
#define TRANSITION_DISABLE_WPA3_ENTERPRISE BIT(2)
#define TRANSITION_DISABLE_ENHANCED_OPEN BIT(3)

/* DPP KDE Flags */
#define DPP_KDE_PFS_ALLOWED BIT(0)
#define DPP_KDE_PFS_REQUIRED BIT(1)

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */


int wpa_eapol_key_mic(const u8 *key, size_t key_len, int akmp, int ver,
		      const u8 *buf, size_t len, u8 *mic);
int wpa_pmk_to_ptk(const u8 *pmk, size_t pmk_len, const char *label,
		   const u8 *addr1, const u8 *addr2,
		   const u8 *nonce1, const u8 *nonce2,
		   struct wpa_ptk *ptk, int akmp, int cipher,
		   const u8 *z, size_t z_len, size_t kdk_len);
int fils_rmsk_to_pmk(int akmp, const u8 *rmsk, size_t rmsk_len,
		     const u8 *snonce, const u8 *anonce, const u8 *dh_ss,
		     size_t dh_ss_len, u8 *pmk, size_t *pmk_len);
int fils_pmkid_erp(int akmp, const u8 *reauth, size_t reauth_len,
		   u8 *pmkid);
int fils_pmk_to_ptk(const u8 *pmk, size_t pmk_len, const u8 *spa, const u8 *aa,
		    const u8 *snonce, const u8 *anonce, const u8 *dhss,
		    size_t dhss_len, struct wpa_ptk *ptk,
		    u8 *ick, size_t *ick_len, int akmp, int cipher,
		    u8 *fils_ft, size_t *fils_ft_len, size_t kdk_len);
int fils_key_auth_sk(const u8 *ick, size_t ick_len, const u8 *snonce,
		     const u8 *anonce, const u8 *sta_addr, const u8 *bssid,
		     const u8 *g_sta, size_t g_sta_len,
		     const u8 *g_ap, size_t g_ap_len,
		     int akmp, u8 *key_auth_sta, u8 *key_auth_ap,
		     size_t *key_auth_len);

#ifdef CONFIG_IEEE80211R
int wpa_ft_mic(int key_mgmt, const u8 *kck, size_t kck_len, const u8 *sta_addr,
	       const u8 *ap_addr, u8 transaction_seqnum,
	       const u8 *mdie, size_t mdie_len,
	       const u8 *ftie, size_t ftie_len,
	       const u8 *rsnie, size_t rsnie_len,
	       const u8 *ric, size_t ric_len,
	       const u8 *rsnxe, size_t rsnxe_len,
	       const struct wpabuf *extra,
	       u8 *mic);
int wpa_derive_pmk_r0(const u8 *xxkey, size_t xxkey_len,
		      const u8 *ssid, size_t ssid_len,
		      const u8 *mdid, const u8 *r0kh_id, size_t r0kh_id_len,
		      const u8 *s0kh_id, u8 *pmk_r0, u8 *pmk_r0_name,
		      int key_mgmt);
int wpa_derive_pmk_r1_name(const u8 *pmk_r0_name, const u8 *r1kh_id,
			   const u8 *s1kh_id, u8 *pmk_r1_name,
			   size_t pmk_r1_len);
int wpa_derive_pmk_r1(const u8 *pmk_r0, size_t pmk_r0_len,
		      const u8 *pmk_r0_name,
		      const u8 *r1kh_id, const u8 *s1kh_id,
		      u8 *pmk_r1, u8 *pmk_r1_name);
int wpa_pmk_r1_to_ptk(const u8 *pmk_r1, size_t pmk_r1_len, const u8 *snonce,
		      const u8 *anonce, const u8 *sta_addr, const u8 *bssid,
		      const u8 *pmk_r1_name,
		      struct wpa_ptk *ptk, u8 *ptk_name, int akmp, int cipher,
		      size_t kdk_len);
#endif /* CONFIG_IEEE80211R */

struct wpa_ie_data {
	int proto;
	int pairwise_cipher;
	int has_pairwise;
	int group_cipher;
	int has_group;
	int key_mgmt;
	int capabilities;
	size_t num_pmkid;
	const u8 *pmkid;
	int mgmt_group_cipher;
};


int wpa_parse_wpa_ie_rsn(const u8 *rsn_ie, size_t rsn_ie_len,
			 struct wpa_ie_data *data);
int wpa_parse_wpa_ie_wpa(const u8 *wpa_ie, size_t wpa_ie_len,
			 struct wpa_ie_data *data);
int wpa_default_rsn_cipher(int freq);

void rsn_pmkid(const u8 *pmk, size_t pmk_len, const u8 *aa, const u8 *spa,
	       u8 *pmkid, int akmp);
#ifdef CONFIG_SUITEB
int rsn_pmkid_suite_b(const u8 *kck, size_t kck_len, const u8 *aa,
		       const u8 *spa, u8 *pmkid);
#else /* CONFIG_SUITEB */
static inline int rsn_pmkid_suite_b(const u8 *kck, size_t kck_len, const u8 *aa,
				    const u8 *spa, u8 *pmkid)
{
	return -1;
}
#endif /* CONFIG_SUITEB */
#ifdef CONFIG_SUITEB192
int rsn_pmkid_suite_b_192(const u8 *kck, size_t kck_len, const u8 *aa,
			  const u8 *spa, u8 *pmkid);
#else /* CONFIG_SUITEB192 */
static inline int rsn_pmkid_suite_b_192(const u8 *kck, size_t kck_len,
					const u8 *aa, const u8 *spa, u8 *pmkid)
{
	return -1;
}
#endif /* CONFIG_SUITEB192 */

const char * wpa_cipher_txt(int cipher);
const char * wpa_key_mgmt_txt(int key_mgmt, int proto);
u32 wpa_akm_to_suite(int akm);
int wpa_compare_rsn_ie(int ft_initial_assoc,
		       const u8 *ie1, size_t ie1len,
		       const u8 *ie2, size_t ie2len);
int wpa_insert_pmkid(u8 *ies, size_t *ies_len, const u8 *pmkid, bool replace);

struct wpa_ft_ies {
	const u8 *mdie;
	size_t mdie_len;
	const u8 *ftie;
	size_t ftie_len;
	const u8 *r1kh_id;
	const u8 *gtk;
	size_t gtk_len;
	const u8 *r0kh_id;
	size_t r0kh_id_len;
	const u8 *fte_anonce;
	const u8 *fte_snonce;
	bool fte_rsnxe_used;
	unsigned int fte_elem_count;
	const u8 *fte_mic;
	size_t fte_mic_len;
	const u8 *rsn;
	size_t rsn_len;
	u16 rsn_capab;
	const u8 *rsn_pmkid;
	const u8 *tie;
	size_t tie_len;
	const u8 *igtk;
	size_t igtk_len;
	const u8 *bigtk;
	size_t bigtk_len;
#ifdef CONFIG_OCV
	const u8 *oci;
	size_t oci_len;
#endif /* CONFIG_OCV */
	const u8 *ric;
	size_t ric_len;
	int key_mgmt;
	int pairwise_cipher;
	const u8 *rsnxe;
	size_t rsnxe_len;
	u16 valid_mlo_gtks; /* bitmap of valid link GTK subelements */
	const u8 *mlo_gtk[MAX_NUM_MLD_LINKS];
	size_t mlo_gtk_len[MAX_NUM_MLD_LINKS];
	u16 valid_mlo_igtks; /* bitmap of valid link IGTK subelements */
	const u8 *mlo_igtk[MAX_NUM_MLD_LINKS];
	size_t mlo_igtk_len[MAX_NUM_MLD_LINKS];
	u16 valid_mlo_bigtks; /* bitmap of valid link BIGTK subelements */
	const u8 *mlo_bigtk[MAX_NUM_MLD_LINKS];
	size_t mlo_bigtk_len[MAX_NUM_MLD_LINKS];

	struct wpabuf *fte_buf;
};

/* IEEE P802.11az/D2.6 - 9.4.2.303 PASN Parameters element */
#define WPA_PASN_CTRL_COMEBACK_INFO_PRESENT BIT(0)
#define WPA_PASN_CTRL_GROUP_AND_KEY_PRESENT BIT(1)

#define WPA_PASN_WRAPPED_DATA_NO      0
#define WPA_PASN_WRAPPED_DATA_FT      1
#define WPA_PASN_WRAPPED_DATA_FILS_SK 2
#define WPA_PASN_WRAPPED_DATA_SAE     3

struct pasn_parameter_ie {
	u8 id;
	u8 len;
	u8 id_ext;
	u8 control; /* WPA_PASN_CTRL_* */
	u8 wrapped_data_format; /* WPA_PASN_WRAPPED_DATA_* */
} STRUCT_PACKED;

struct wpa_pasn_params_data {
	u8 wrapped_data_format;
	u16 after;
	u8 comeback_len;
	const u8 *comeback;
	u16 group;
	u8 pubkey_len;
	const u8 *pubkey;
};

/* See RFC 5480 section 2.2 */
#define WPA_PASN_PUBKEY_COMPRESSED_0 0x02
#define WPA_PASN_PUBKEY_COMPRESSED_1 0x03
#define WPA_PASN_PUBKEY_UNCOMPRESSED 0x04

int wpa_ft_parse_ies(const u8 *ies, size_t ies_len, struct wpa_ft_ies *parse,
		     int key_mgmt, bool reassoc_resp);
void wpa_ft_parse_ies_free(struct wpa_ft_ies *parse);

struct wpa_eapol_ie_parse {
	const u8 *wpa_ie;
	size_t wpa_ie_len;
	const u8 *rsn_ie;
	size_t rsn_ie_len;
	const u8 *pmkid;
	const u8 *key_id;
	const u8 *gtk;
	size_t gtk_len;
	const u8 *mac_addr;
	const u8 *igtk;
	size_t igtk_len;
	const u8 *bigtk;
	size_t bigtk_len;
	const u8 *mdie;
	size_t mdie_len;
	const u8 *ftie;
	size_t ftie_len;
	const u8 *ip_addr_req;
	const u8 *ip_addr_alloc;
	const u8 *transition_disable;
	size_t transition_disable_len;
	const u8 *dpp_kde;
	size_t dpp_kde_len;
	const u8 *oci;
	size_t oci_len;
	const u8 *osen;
	size_t osen_len;
	const u8 *rsnxe;
	size_t rsnxe_len;
	const u8 *reassoc_deadline;
	const u8 *key_lifetime;
	const u8 *lnkid;
	size_t lnkid_len;
	const u8 *ext_capab;
	size_t ext_capab_len;
	const u8 *supp_rates;
	size_t supp_rates_len;
	const u8 *ext_supp_rates;
	size_t ext_supp_rates_len;
	const u8 *ht_capabilities;
	const u8 *vht_capabilities;
	const u8 *he_capabilities;
	size_t he_capab_len;
	const u8 *he_6ghz_capabilities;
	const u8 *eht_capabilities;
	size_t eht_capab_len;
	const u8 *supp_channels;
	size_t supp_channels_len;
	const u8 *supp_oper_classes;
	size_t supp_oper_classes_len;
	const u8 *ssid;
	size_t ssid_len;
	u8 qosinfo;
	u16 aid;
	const u8 *wmm;
	size_t wmm_len;
	u16 valid_mlo_gtks; /* bitmap of valid link GTK KDEs */
	const u8 *mlo_gtk[MAX_NUM_MLD_LINKS];
	size_t mlo_gtk_len[MAX_NUM_MLD_LINKS];
	u16 valid_mlo_igtks; /* bitmap of valid link IGTK KDEs */
	const u8 *mlo_igtk[MAX_NUM_MLD_LINKS];
	size_t mlo_igtk_len[MAX_NUM_MLD_LINKS];
	u16 valid_mlo_bigtks; /* bitmap of valid link BIGTK KDEs */
	const u8 *mlo_bigtk[MAX_NUM_MLD_LINKS];
	size_t mlo_bigtk_len[MAX_NUM_MLD_LINKS];
	u16 valid_mlo_links; /* bitmap of valid MLO link KDEs */
	const u8 *mlo_link[MAX_NUM_MLD_LINKS];
	size_t mlo_link_len[MAX_NUM_MLD_LINKS];
};

int wpa_parse_kde_ies(const u8 *buf, size_t len, struct wpa_eapol_ie_parse *ie);
static inline int wpa_supplicant_parse_ies(const u8 *buf, size_t len,
					   struct wpa_eapol_ie_parse *ie)
{
	return wpa_parse_kde_ies(buf, len, ie);
}


int wpa_cipher_key_len(int cipher);
int wpa_cipher_rsc_len(int cipher);
enum wpa_alg wpa_cipher_to_alg(int cipher);
int wpa_cipher_valid_group(int cipher);
int wpa_cipher_valid_pairwise(int cipher);
int wpa_cipher_valid_mgmt_group(int cipher);
u32 wpa_cipher_to_suite(int proto, int cipher);
int rsn_cipher_put_suites(u8 *pos, int ciphers);
int wpa_cipher_put_suites(u8 *pos, int ciphers);
int wpa_pick_pairwise_cipher(int ciphers, int none_allowed);
int wpa_pick_group_cipher(int ciphers);
int wpa_parse_cipher(const char *value);
int wpa_write_ciphers(char *start, char *end, int ciphers, const char *delim);
int wpa_select_ap_group_cipher(int wpa, int wpa_pairwise, int rsn_pairwise);
unsigned int wpa_mic_len(int akmp, size_t pmk_len);
int wpa_use_akm_defined(int akmp);
int wpa_use_cmac(int akmp);
int wpa_use_aes_key_wrap(int akmp);
int fils_domain_name_hash(const char *domain, u8 *hash);

bool pasn_use_sha384(int akmp, int cipher);
int pasn_pmk_to_ptk(const u8 *pmk, size_t pmk_len,
		    const u8 *spa, const u8 *bssid,
		    const u8 *dhss, size_t dhss_len,
		    struct wpa_ptk *ptk, int akmp, int cipher,
		    size_t kdk_len);

u8 pasn_mic_len(int akmp, int cipher);

int pasn_mic(const u8 *kck, int akmp, int cipher,
	     const u8 *addr1, const u8 *addr2,
	     const u8 *data, size_t data_len,
	     const u8 *frame, size_t frame_len, u8 *mic);

int wpa_ltf_keyseed(struct wpa_ptk *ptk, int akmp, int cipher);

int pasn_auth_frame_hash(int akmp, int cipher, const u8 *data, size_t len,
			 u8 *hash);

void wpa_pasn_build_auth_header(struct wpabuf *buf, const u8 *bssid,
				const u8 *src, const u8 *dst,
				u8 trans_seq, u16 status);

int wpa_pasn_add_rsne(struct wpabuf *buf, const u8 *pmkid,
		      int akmp, int cipher);

void wpa_pasn_add_parameter_ie(struct wpabuf *buf, u16 pasn_group,
			       u8 wrapped_data_format,
			       const struct wpabuf *pubkey, bool compressed,
			       const struct wpabuf *comeback, int after);

int wpa_pasn_add_wrapped_data(struct wpabuf *buf,
			      struct wpabuf *wrapped_data_buf);

int wpa_pasn_validate_rsne(const struct wpa_ie_data *data);
int wpa_pasn_parse_parameter_ie(const u8 *data, u8 len, bool from_ap,
				struct wpa_pasn_params_data *pasn_params);

void wpa_pasn_add_rsnxe(struct wpabuf *buf, u16 capab);
int wpa_pasn_add_extra_ies(struct wpabuf *buf, const u8 *extra_ies, size_t len);

#endif /* WPA_COMMON_H */
