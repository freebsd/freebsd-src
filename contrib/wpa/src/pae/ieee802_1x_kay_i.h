/*
 * IEEE 802.1X-2010 Key Agree Protocol of PAE state machine
 * Copyright (c) 2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef IEEE802_1X_KAY_I_H
#define IEEE802_1X_KAY_I_H

#include "utils/list.h"
#include "common/defs.h"
#include "common/ieee802_1x_defs.h"

#define MKA_VERSION_ID              1

/* IEEE Std 802.1X-2010, 11.11.1, Table 11-7 */
enum mka_packet_type {
	MKA_BASIC_PARAMETER_SET = MKA_VERSION_ID,
	MKA_LIVE_PEER_LIST = 1,
	MKA_POTENTIAL_PEER_LIST = 2,
	MKA_SAK_USE = 3,
	MKA_DISTRIBUTED_SAK = 4,
	MKA_DISTRIBUTED_CAK = 5,
	MKA_KMD = 6,
	MKA_ANNOUNCEMENT = 7,
	MKA_ICV_INDICATOR = 255
};

#define ICV_LEN                         16  /* 16 bytes */
#define SAK_WRAPPED_LEN                 24
/* KN + Wrapper SAK */
#define DEFAULT_DIS_SAK_BODY_LENGTH     (SAK_WRAPPED_LEN + 4)
#define MAX_RETRY_CNT                   5

struct ieee802_1x_kay;

struct ieee802_1x_mka_peer_id {
	u8 mi[MI_LEN];
	u32 mn;
};

struct ieee802_1x_kay_peer {
	struct ieee802_1x_mka_sci sci;
	u8 mi[MI_LEN];
	u32 mn;
	time_t expire;
	Boolean is_key_server;
	u8 key_server_priority;
	Boolean macsec_desired;
	enum macsec_cap macsec_capbility;
	Boolean sak_used;
	struct dl_list list;
};

struct key_conf {
	u8 *key;
	struct ieee802_1x_mka_ki ki;
	enum confidentiality_offset offset;
	u8 an;
	Boolean tx;
	Boolean rx;
	int key_len; /* unit: byte */
};

struct data_key {
	u8 *key;
	int key_len;
	struct ieee802_1x_mka_ki key_identifier;
	enum confidentiality_offset confidentiality_offset;
	u8 an;
	Boolean transmits;
	Boolean receives;
	struct os_time created_time;
	u32 next_pn;

	/* not defined data */
	Boolean rx_latest;
	Boolean tx_latest;

	int user;  /* FIXME: to indicate if it can be delete safely */

	struct dl_list list;
};

/* TransmitSC in IEEE Std 802.1AE-2006, Figure 10-6 */
struct transmit_sc {
	struct ieee802_1x_mka_sci sci; /* const SCI sci */
	Boolean transmitting; /* bool transmitting (read only) */

	struct os_time created_time; /* Time createdTime */

	u8 encoding_sa; /* AN encodingSA (read only) */
	u8 enciphering_sa; /* AN encipheringSA (read only) */

	/* not defined data */
	unsigned int channel;

	struct dl_list list;
	struct dl_list sa_list;
};

/* TransmitSA in IEEE Std 802.1AE-2006, Figure 10-6 */
struct transmit_sa {
	Boolean in_use; /* bool inUse (read only) */
	u32 next_pn; /* PN nextPN (read only) */
	struct os_time created_time; /* Time createdTime */

	Boolean enable_transmit; /* bool EnableTransmit */

	u8 an;
	Boolean confidentiality;
	struct data_key *pkey;

	struct transmit_sc *sc;
	struct dl_list list; /* list entry in struct transmit_sc::sa_list */
};

/* ReceiveSC in IEEE Std 802.1AE-2006, Figure 10-6 */
struct receive_sc {
	struct ieee802_1x_mka_sci sci; /* const SCI sci */
	Boolean receiving; /* bool receiving (read only) */

	struct os_time created_time; /* Time createdTime */

	unsigned int channel;

	struct dl_list list;
	struct dl_list sa_list;
};

/* ReceiveSA in IEEE Std 802.1AE-2006, Figure 10-6 */
struct receive_sa {
	Boolean enable_receive; /* bool enableReceive */
	Boolean in_use; /* bool inUse (read only) */

	u32 next_pn; /* PN nextPN (read only) */
	u32 lowest_pn; /* PN lowestPN (read only) */
	u8 an;
	struct os_time created_time;

	struct data_key *pkey;
	struct receive_sc *sc; /* list entry in struct receive_sc::sa_list */

	struct dl_list list;
};

struct macsec_ciphersuite {
	u8 id[CS_ID_LEN];
	char name[32];
	enum macsec_cap capable;
	int sak_len; /* unit: byte */

	u32 index;
};

struct mka_alg {
	u8 parameter[4];
	size_t cak_len;
	size_t kek_len;
	size_t ick_len;
	size_t icv_len;

	int (*cak_trfm)(const u8 *msk, const u8 *mac1, const u8 *mac2, u8 *cak);
	int (*ckn_trfm)(const u8 *msk, const u8 *mac1, const u8 *mac2,
			const u8 *sid, size_t sid_len, u8 *ckn);
	int (*kek_trfm)(const u8 *cak, const u8 *ckn, size_t ckn_len, u8 *kek);
	int (*ick_trfm)(const u8 *cak, const u8 *ckn, size_t ckn_len, u8 *ick);
	int (*icv_hash)(const u8 *ick, const u8 *msg, size_t msg_len, u8 *icv);

	int index; /* index for configuring */
};

#define DEFAULT_MKA_ALG_INDEX 0

/* See IEEE Std 802.1X-2010, 9.16 MKA management */
struct ieee802_1x_mka_participant {
	/* used for active and potential participant */
	struct mka_key_name ckn;
	struct mka_key cak;
	Boolean cached;

	/* used by management to monitor and control activation */
	Boolean active;
	Boolean participant;
	Boolean retain;

	enum { DEFAULT, DISABLED, ON_OPER_UP, ALWAYS } activate;

	/* used for active participant */
	Boolean principal;
	struct dl_list live_peers;
	struct dl_list potential_peers;

	/* not defined in IEEE 802.1X */
	struct dl_list list;

	struct mka_key kek;
	struct mka_key ick;

	struct ieee802_1x_mka_ki lki;
	u8 lan;
	Boolean ltx;
	Boolean lrx;

	struct ieee802_1x_mka_ki oki;
	u8 oan;
	Boolean otx;
	Boolean orx;

	Boolean is_key_server;
	Boolean is_obliged_key_server;
	Boolean can_be_key_server;
	Boolean is_elected;

	struct dl_list sak_list;
	struct dl_list rxsc_list;

	struct transmit_sc *txsc;

	u8 mi[MI_LEN];
	u32 mn;

	struct ieee802_1x_mka_peer_id current_peer_id;
	struct ieee802_1x_mka_sci current_peer_sci;
	time_t cak_life;
	time_t mka_life;
	Boolean to_dist_sak;
	Boolean to_use_sak;
	Boolean new_sak;

	Boolean advised_desired;
	enum macsec_cap advised_capability;

	struct data_key *new_key;
	u32 retry_count;

	struct ieee802_1x_kay *kay;
};

struct ieee802_1x_mka_hdr {
	/* octet 1 */
	u32 type:8;
	/* octet 2 */
	u32 reserve:8;
	/* octet 3 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	u32 length:4;
	u32 reserve1:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	u32 reserve1:4;
	u32 length:4;
#else
#error "Please fix <bits/endian.h>"
#endif
	/* octet 4 */
	u32 length1:8;
};

#define MKA_HDR_LEN sizeof(struct ieee802_1x_mka_hdr)

struct ieee802_1x_mka_basic_body {
	/* octet 1 */
	u32 version:8;
	/* octet 2 */
	u32 priority:8;
	/* octet 3 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	u32 length:4;
	u32 macsec_capbility:2;
	u32 macsec_desired:1;
	u32 key_server:1;
#elif __BYTE_ORDER == __BIG_ENDIAN
	u32 key_server:1;
	u32 macsec_desired:1;
	u32 macsec_capbility:2;
	u32 length:4;
#endif
	/* octet 4 */
	u32 length1:8;

	struct ieee802_1x_mka_sci actor_sci;
	u8 actor_mi[MI_LEN];
	u32 actor_mn;
	u8 algo_agility[4];

	/* followed by CAK Name*/
	u8 ckn[0];
};

struct ieee802_1x_mka_peer_body {
	/* octet 1 */
	u32 type:8;
	/* octet 2 */
	u32 reserve:8;
	/* octet 3 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	u32 length:4;
	u32 reserve1:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	u32 reserve1:4;
	u32 length:4;
#endif
	/* octet 4 */
	u32 length1:8;

	u8 peer[0];
	/* followed by Peers */
};

struct ieee802_1x_mka_sak_use_body {
	/* octet 1 */
	u32 type:8;
	/* octet 2 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	u32 orx:1;
	u32 otx:1;
	u32 oan:2;
	u32 lrx:1;
	u32 ltx:1;
	u32 lan:2;
#elif __BYTE_ORDER == __BIG_ENDIAN
	u32 lan:2;
	u32 ltx:1;
	u32 lrx:1;
	u32 oan:2;
	u32 otx:1;
	u32 orx:1;
#endif

	/* octet 3 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	u32 length:4;
	u32 delay_protect:1;
	u32 reserve:1;
	u32 prx:1;
	u32 ptx:1;
#elif __BYTE_ORDER == __BIG_ENDIAN
	u32 ptx:1;
	u32 prx:1;
	u32 reserve:1;
	u32 delay_protect:1;
	u32 length:4;
#endif

	/* octet 4 */
	u32 length1:8;

	/* octet 5 - 16 */
	u8 lsrv_mi[MI_LEN];
	/* octet 17 - 20 */
	u32 lkn;
	/* octet 21 - 24 */
	u32 llpn;

	/* octet 25 - 36 */
	u8 osrv_mi[MI_LEN];
	/* octet 37 - 40 */
	u32 okn;
	/* octet 41 - 44 */
	u32 olpn;
};


struct ieee802_1x_mka_dist_sak_body {
	/* octet 1 */
	u32 type:8;
	/* octet 2 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	u32 reserve:4;
	u32 confid_offset:2;
	u32 dan:2;
#elif __BYTE_ORDER == __BIG_ENDIAN
	u32 dan:2;
	u32 confid_offset:2;
	u32 reserve:4;
#endif
	/* octet 3 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	u32 length:4;
	u32 reserve1:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	u32 reserve1:4;
	u32 length:4;
#endif
	/* octet 4 */
	u32 length1:8;
	/* octet 5 - 8 */
	u32 kn;

	/* for GCM-AES-128: octet 9-32: SAK
	 * for other cipher suite: octet 9-16: cipher suite id, octet 17-: SAK
	 */
	u8 sak[0];
};


struct ieee802_1x_mka_icv_body {
	/* octet 1 */
	u32 type:8;
	/* octet 2 */
	u32 reserve:8;
	/* octet 3 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	u32 length:4;
	u32 reserve1:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	u32 reserve1:4;
	u32 length:4;
#endif
	/* octet 4 */
	u32 length1:8;

	/* octet 5 - */
	u8 icv[0];
};

#endif /* IEEE802_1X_KAY_I_H */
