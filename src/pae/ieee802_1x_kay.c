/*
 * IEEE 802.1X-2010 Key Agreement Protocol of PAE state machine
 * Copyright (c) 2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <time.h>
#include "includes.h"
#include "common.h"
#include "list.h"
#include "eloop.h"
#include "wpabuf.h"
#include "state_machine.h"
#include "l2_packet/l2_packet.h"
#include "common/eapol_common.h"
#include "crypto/aes_wrap.h"
#include "ieee802_1x_cp.h"
#include "ieee802_1x_key.h"
#include "ieee802_1x_kay.h"
#include "ieee802_1x_kay_i.h"
#include "ieee802_1x_secy_ops.h"


#define DEFAULT_SA_KEY_LEN	16
#define DEFAULT_ICV_LEN		16
#define MAX_ICV_LEN		32  /* 32 bytes, 256 bits */

#define MAX_MISSING_SAK_USE 10  /* Accept up to 10 inbound MKPDUs without
				 * SAK-USE before dropping */

#define PENDING_PN_EXHAUSTION 0xC0000000

#define MKA_ALIGN_LENGTH(len) (((len) + 0x3) & ~0x3)

/* IEEE Std 802.1X-2010, Table 9-1 - MKA Algorithm Agility */
#define MKA_ALGO_AGILITY_2009 { 0x00, 0x80, 0xC2, 0x01 }
static u8 mka_algo_agility[4] = MKA_ALGO_AGILITY_2009;

/* IEEE802.1AE-2006 Table 14-1 MACsec Cipher Suites */
static struct macsec_ciphersuite cipher_suite_tbl[] = {
	/* GCM-AES-128 */
	{
		.id = CS_ID_GCM_AES_128,
		.name = CS_NAME_GCM_AES_128,
		.capable = MACSEC_CAP_INTEG_AND_CONF_0_30_50,
		.sak_len = DEFAULT_SA_KEY_LEN,
	},
	/* GCM-AES-256 */
	{
		.id = CS_ID_GCM_AES_256,
		.name = CS_NAME_GCM_AES_256,
		.capable = MACSEC_CAP_INTEG_AND_CONF_0_30_50,
		.sak_len = 32,
	},
};
#define CS_TABLE_SIZE (ARRAY_SIZE(cipher_suite_tbl))
#define DEFAULT_CS_INDEX  0

static struct mka_alg mka_alg_tbl[] = {
	{
		.parameter = MKA_ALGO_AGILITY_2009,

		.icv_len = DEFAULT_ICV_LEN,

		.cak_trfm = ieee802_1x_cak_aes_cmac,
		.ckn_trfm = ieee802_1x_ckn_aes_cmac,
		.kek_trfm = ieee802_1x_kek_aes_cmac,
		.ick_trfm = ieee802_1x_ick_aes_cmac,
		.icv_hash = ieee802_1x_icv_aes_cmac,
	},
};
#define MKA_ALG_TABLE_SIZE (ARRAY_SIZE(mka_alg_tbl))


static int is_ki_equal(struct ieee802_1x_mka_ki *ki1,
		       struct ieee802_1x_mka_ki *ki2)
{
	return os_memcmp(ki1->mi, ki2->mi, MI_LEN) == 0 &&
		ki1->kn == ki2->kn;
}


static void set_mka_param_body_len(void *body, unsigned int len)
{
	struct ieee802_1x_mka_hdr *hdr = body;
	hdr->length = (len >> 8) & 0x0f;
	hdr->length1 = len & 0xff;
}


static unsigned int get_mka_param_body_len(const void *body)
{
	const struct ieee802_1x_mka_hdr *hdr = body;
	return (hdr->length << 8) | hdr->length1;
}


static u8 get_mka_param_body_type(const void *body)
{
	const struct ieee802_1x_mka_hdr *hdr = body;
	return hdr->type;
}


static const char * mi_txt(const u8 *mi)
{
	static char txt[MI_LEN * 2 + 1];

	wpa_snprintf_hex(txt, sizeof(txt), mi, MI_LEN);
	return txt;
}


static const char * sci_txt(const struct ieee802_1x_mka_sci *sci)
{
	static char txt[ETH_ALEN * 3 + 1 + 5 + 1];

	os_snprintf(txt, sizeof(txt), MACSTR "@%u",
		    MAC2STR(sci->addr), be_to_host16(sci->port));
	return txt;
}


static const char * algo_agility_txt(const u8 *algo_agility)
{
	static char txt[4 * 2 + 1];

	wpa_snprintf_hex(txt, sizeof(txt), algo_agility, 4);
	return txt;
}


/**
 * ieee802_1x_mka_dump_basic_body -
 */
static void
ieee802_1x_mka_dump_basic_body(struct ieee802_1x_mka_basic_body *body)
{
	size_t body_len;

	if (!body)
		return;

	/* IEEE Std 802.1X-2010, Figure 11-8 */
	body_len = get_mka_param_body_len(body);
	wpa_printf(MSG_DEBUG, "MKA Basic Parameter Set");
	wpa_printf(MSG_DEBUG, "\tMKA Version Identifier: %d", body->version);
	wpa_printf(MSG_DEBUG, "\tKey Server Priority: %d", body->priority);
	wpa_printf(MSG_DEBUG, "\tKey Server: %d", body->key_server);
	wpa_printf(MSG_DEBUG, "\tMACsec Desired: %d", body->macsec_desired);
	wpa_printf(MSG_DEBUG, "\tMACsec Capability: %d",
		   body->macsec_capability);
	wpa_printf(MSG_DEBUG, "\tParameter set body length: %zu", body_len);
	wpa_printf(MSG_DEBUG, "\tSCI: %s", sci_txt(&body->actor_sci));
	wpa_printf(MSG_DEBUG, "\tActor's Member Identifier: %s",
		   mi_txt(body->actor_mi));
	wpa_printf(MSG_DEBUG, "\tActor's Message Number: %d",
		   be_to_host32(body->actor_mn));
	wpa_printf(MSG_DEBUG, "\tAlgorithm Agility: %s",
		   algo_agility_txt(body->algo_agility));
	wpa_hexdump(MSG_DEBUG, "\tCAK Name", body->ckn,
		    body_len + MKA_HDR_LEN - sizeof(*body));
}


/**
 * ieee802_1x_mka_dump_peer_body -
 */
static void
ieee802_1x_mka_dump_peer_body(struct ieee802_1x_mka_peer_body *body)
{
	size_t body_len;
	size_t i;
	u8 *mi;
	be32 mn;

	if (body == NULL)
		return;

	/* IEEE Std 802.1X-2010, Figure 11-9 */
	body_len = get_mka_param_body_len(body);
	if (body->type == MKA_LIVE_PEER_LIST) {
		wpa_printf(MSG_DEBUG, "Live Peer List parameter set");
		wpa_printf(MSG_DEBUG, "\tBody Length: %zu", body_len);
	} else if (body->type == MKA_POTENTIAL_PEER_LIST) {
		wpa_printf(MSG_DEBUG, "Potential Peer List parameter set");
		wpa_printf(MSG_DEBUG, "\tBody Length: %zu", body_len);
	}

	for (i = 0; i < body_len; i += MI_LEN + sizeof(mn)) {
		mi = body->peer + i;
		os_memcpy(&mn, mi + MI_LEN, sizeof(mn));
		wpa_printf(MSG_DEBUG, "\tMember Id: %s  Message Number: %d",
			   mi_txt(mi), be_to_host32(mn));
	}
}


/**
 * ieee802_1x_mka_dump_dist_sak_body -
 */
static void
ieee802_1x_mka_dump_dist_sak_body(struct ieee802_1x_mka_dist_sak_body *body)
{
	size_t body_len;

	if (body == NULL)
		return;

	/* IEEE Std 802.1X-2010, Figure 11-11 and 11-12 */
	body_len = get_mka_param_body_len(body);
	wpa_printf(MSG_DEBUG, "Distributed SAK parameter set");
	wpa_printf(MSG_DEBUG, "\tDistributed AN........: %d", body->dan);
	wpa_printf(MSG_DEBUG, "\tConfidentiality Offset: %d",
		   body->confid_offset);
	wpa_printf(MSG_DEBUG, "\tBody Length...........: %zu", body_len);
	if (!body_len)
		return;

	wpa_printf(MSG_DEBUG, "\tKey Number............: %d",
		   be_to_host32(body->kn));
	/* TODO: Other than GCM-AES-128 case: MACsec Cipher Suite */
	wpa_hexdump(MSG_DEBUG, "\tAES Key Wrap of SAK...:", body->sak, 24);
}


static const char * yes_no(int val)
{
	return val ? "Yes" : "No";
}


/**
 * ieee802_1x_mka_dump_sak_use_body -
 */
static void
ieee802_1x_mka_dump_sak_use_body(struct ieee802_1x_mka_sak_use_body *body)
{
	int body_len;

	if (body == NULL)
		return;

	/* IEEE Std 802.1X-2010, Figure 11-10 */
	body_len = get_mka_param_body_len(body);
	wpa_printf(MSG_DEBUG, "MACsec SAK Use parameter set");
	wpa_printf(MSG_DEBUG, "\tLatest Key AN....: %d", body->lan);
	wpa_printf(MSG_DEBUG, "\tLatest Key Tx....: %s", yes_no(body->ltx));
	wpa_printf(MSG_DEBUG, "\tLatest Key Rx....: %s", yes_no(body->lrx));
	wpa_printf(MSG_DEBUG, "\tOld Key AN.......: %d", body->oan);
	wpa_printf(MSG_DEBUG, "\tOld Key Tx.......: %s", yes_no(body->otx));
	wpa_printf(MSG_DEBUG, "\tOld Key Rx.......: %s", yes_no(body->orx));
	wpa_printf(MSG_DEBUG, "\tPlain Tx.........: %s", yes_no(body->ptx));
	wpa_printf(MSG_DEBUG, "\tPlain Rx.........: %s", yes_no(body->prx));
	wpa_printf(MSG_DEBUG, "\tDelay Protect....: %s",
		   yes_no(body->delay_protect));
	wpa_printf(MSG_DEBUG, "\tBody Length......: %d", body_len);
	if (!body_len)
		return;

	wpa_printf(MSG_DEBUG, "\tKey Server MI....: %s", mi_txt(body->lsrv_mi));
	wpa_printf(MSG_DEBUG, "\tKey Number.......: %u",
		   be_to_host32(body->lkn));
	wpa_printf(MSG_DEBUG, "\tLowest PN........: %u",
		   be_to_host32(body->llpn));
	wpa_printf(MSG_DEBUG, "\tOld Key Server MI: %s", mi_txt(body->osrv_mi));
	wpa_printf(MSG_DEBUG, "\tOld Key Number...: %u",
		   be_to_host32(body->okn));
	wpa_printf(MSG_DEBUG, "\tOld Lowest PN....: %u",
		   be_to_host32(body->olpn));
}


/**
 * ieee802_1x_kay_get_participant -
 */
static struct ieee802_1x_mka_participant *
ieee802_1x_kay_get_participant(struct ieee802_1x_kay *kay, const u8 *ckn,
			       size_t len)
{
	struct ieee802_1x_mka_participant *participant;

	dl_list_for_each(participant, &kay->participant_list,
			 struct ieee802_1x_mka_participant, list) {
		if (participant->ckn.len == len &&
		    os_memcmp(participant->ckn.name, ckn,
			      participant->ckn.len) == 0)
			return participant;
	}

	wpa_printf(MSG_DEBUG, "KaY: participant is not found");

	return NULL;
}


/**
 * ieee802_1x_kay_get_principal_participant -
 */
static struct ieee802_1x_mka_participant *
ieee802_1x_kay_get_principal_participant(struct ieee802_1x_kay *kay)
{
	struct ieee802_1x_mka_participant *participant;

	dl_list_for_each(participant, &kay->participant_list,
			 struct ieee802_1x_mka_participant, list) {
		if (participant->principal)
			return participant;
	}

	wpa_printf(MSG_DEBUG, "KaY: principal participant is not found");
	return NULL;
}


static struct ieee802_1x_kay_peer * get_peer_mi(struct dl_list *peers,
						const u8 *mi)
{
	struct ieee802_1x_kay_peer *peer;

	dl_list_for_each(peer, peers, struct ieee802_1x_kay_peer, list) {
		if (os_memcmp(peer->mi, mi, MI_LEN) == 0)
			return peer;
	}

	return NULL;
}


/**
 * ieee802_1x_kay_get_potential_peer
 */
static struct ieee802_1x_kay_peer *
ieee802_1x_kay_get_potential_peer(
	struct ieee802_1x_mka_participant *participant, const u8 *mi)
{
	return get_peer_mi(&participant->potential_peers, mi);
}


/**
 * ieee802_1x_kay_get_live_peer
 */
static struct ieee802_1x_kay_peer *
ieee802_1x_kay_get_live_peer(struct ieee802_1x_mka_participant *participant,
			     const u8 *mi)
{
	return get_peer_mi(&participant->live_peers, mi);
}


/**
 * ieee802_1x_kay_is_in_potential_peer
 */
static Boolean
ieee802_1x_kay_is_in_potential_peer(
	struct ieee802_1x_mka_participant *participant, const u8 *mi)
{
	return ieee802_1x_kay_get_potential_peer(participant, mi) != NULL;
}


/**
 * ieee802_1x_kay_is_in_live_peer
 */
static Boolean
ieee802_1x_kay_is_in_live_peer(
	struct ieee802_1x_mka_participant *participant, const u8 *mi)
{
	return ieee802_1x_kay_get_live_peer(participant, mi) != NULL;
}


/**
 * ieee802_1x_kay_get_peer
 */
static struct ieee802_1x_kay_peer *
ieee802_1x_kay_get_peer(struct ieee802_1x_mka_participant *participant,
			const u8 *mi)
{
	struct ieee802_1x_kay_peer *peer;

	peer = ieee802_1x_kay_get_live_peer(participant, mi);
	if (peer)
		return peer;

	return ieee802_1x_kay_get_potential_peer(participant, mi);
}


/**
 * ieee802_1x_kay_get_cipher_suite
 */
static struct macsec_ciphersuite *
ieee802_1x_kay_get_cipher_suite(struct ieee802_1x_mka_participant *participant,
				const u8 *cs_id, unsigned int *idx)
{
	unsigned int i;
	u64 cs;
	be64 _cs;

	os_memcpy(&_cs, cs_id, CS_ID_LEN);
	cs = be_to_host64(_cs);

	for (i = 0; i < CS_TABLE_SIZE; i++) {
		if (cipher_suite_tbl[i].id == cs) {
			*idx = i;
			return &cipher_suite_tbl[i];
		}
	}

	return NULL;
}


u64 mka_sci_u64(struct ieee802_1x_mka_sci *sci)
{
	struct ieee802_1x_mka_sci tmp;

	os_memcpy(tmp.addr, sci->addr, ETH_ALEN);
	tmp.port = sci->port;

	return *((u64 *) &tmp);
}


static Boolean sci_equal(const struct ieee802_1x_mka_sci *a,
			 const struct ieee802_1x_mka_sci *b)
{
	return os_memcmp(a, b, sizeof(struct ieee802_1x_mka_sci)) == 0;
}


/**
 * ieee802_1x_kay_get_peer_sci
 */
static struct ieee802_1x_kay_peer *
ieee802_1x_kay_get_peer_sci(struct ieee802_1x_mka_participant *participant,
			    const struct ieee802_1x_mka_sci *sci)
{
	struct ieee802_1x_kay_peer *peer;

	dl_list_for_each(peer, &participant->live_peers,
			 struct ieee802_1x_kay_peer, list) {
		if (sci_equal(&peer->sci, sci))
			return peer;
	}

	dl_list_for_each(peer, &participant->potential_peers,
			 struct ieee802_1x_kay_peer, list) {
		if (sci_equal(&peer->sci, sci))
			return peer;
	}

	return NULL;
}


static void ieee802_1x_kay_use_data_key(struct data_key *pkey);

/**
 * ieee802_1x_kay_init_receive_sa -
 */
static struct receive_sa *
ieee802_1x_kay_init_receive_sa(struct receive_sc *psc, u8 an, u32 lowest_pn,
			       struct data_key *key)
{
	struct receive_sa *psa;

	if (!psc || !key)
		return NULL;

	psa = os_zalloc(sizeof(*psa));
	if (!psa) {
		wpa_printf(MSG_ERROR, "%s: out of memory", __func__);
		return NULL;
	}

	ieee802_1x_kay_use_data_key(key);
	psa->pkey = key;
	psa->lowest_pn = lowest_pn;
	psa->next_pn = lowest_pn;
	psa->an = an;
	psa->sc = psc;

	os_get_time(&psa->created_time);
	psa->in_use = FALSE;

	dl_list_add(&psc->sa_list, &psa->list);
	wpa_printf(MSG_DEBUG,
		   "KaY: Create receive SA(an: %hhu lowest_pn: %u) of SC",
		   an, lowest_pn);

	return psa;
}


static void ieee802_1x_kay_deinit_data_key(struct data_key *pkey);

/**
 * ieee802_1x_kay_deinit_receive_sa -
 */
static void ieee802_1x_kay_deinit_receive_sa(struct receive_sa *psa)
{
	ieee802_1x_kay_deinit_data_key(psa->pkey);
	psa->pkey = NULL;
	wpa_printf(MSG_DEBUG,
		   "KaY: Delete receive SA(an: %hhu) of SC",
		   psa->an);
	dl_list_del(&psa->list);
	os_free(psa);
}


/**
 * ieee802_1x_kay_init_receive_sc -
 */
static struct receive_sc *
ieee802_1x_kay_init_receive_sc(const struct ieee802_1x_mka_sci *psci)
{
	struct receive_sc *psc;

	if (!psci)
		return NULL;

	psc = os_zalloc(sizeof(*psc));
	if (!psc) {
		wpa_printf(MSG_ERROR, "%s: out of memory", __func__);
		return NULL;
	}

	os_memcpy(&psc->sci, psci, sizeof(psc->sci));

	os_get_time(&psc->created_time);
	psc->receiving = FALSE;

	dl_list_init(&psc->sa_list);
	wpa_printf(MSG_DEBUG, "KaY: Create receive SC: SCI %s",
		   sci_txt(&psc->sci));

	return psc;
}


static void ieee802_1x_delete_receive_sa(struct ieee802_1x_kay *kay,
					 struct receive_sa *sa)
{
	secy_disable_receive_sa(kay, sa);
	secy_delete_receive_sa(kay, sa);
	ieee802_1x_kay_deinit_receive_sa(sa);
}


/**
 * ieee802_1x_kay_deinit_receive_sc -
 **/
static void
ieee802_1x_kay_deinit_receive_sc(
	struct ieee802_1x_mka_participant *participant, struct receive_sc *psc)
{
	struct receive_sa *psa, *pre_sa;

	wpa_printf(MSG_DEBUG, "KaY: Delete receive SC");
	dl_list_for_each_safe(psa, pre_sa, &psc->sa_list, struct receive_sa,
			      list)
		ieee802_1x_delete_receive_sa(participant->kay, psa);

	dl_list_del(&psc->list);
	secy_delete_receive_sc(participant->kay, psc);
	os_free(psc);
}


static void ieee802_1x_kay_dump_peer(struct ieee802_1x_kay_peer *peer)
{
	wpa_printf(MSG_DEBUG, "\tMI: %s  MN: %d  SCI: %s",
		   mi_txt(peer->mi), peer->mn, sci_txt(&peer->sci));
}


static struct ieee802_1x_kay_peer *
ieee802_1x_kay_create_peer(const u8 *mi, u32 mn)
{
	struct ieee802_1x_kay_peer *peer;

	peer = os_zalloc(sizeof(*peer));
	if (!peer) {
		wpa_printf(MSG_ERROR, "KaY-%s: out of memory", __func__);
		return NULL;
	}

	os_memcpy(peer->mi, mi, MI_LEN);
	peer->mn = mn;
	peer->expire = time(NULL) + MKA_LIFE_TIME / 1000;
	peer->sak_used = FALSE;
	peer->missing_sak_use_count = 0;

	return peer;
}


/**
 * ieee802_1x_kay_create_live_peer
 */
static struct ieee802_1x_kay_peer *
ieee802_1x_kay_create_live_peer(struct ieee802_1x_mka_participant *participant,
				const u8 *mi, u32 mn)
{
	struct ieee802_1x_kay_peer *peer;
	struct receive_sc *rxsc;

	peer = ieee802_1x_kay_create_peer(mi, mn);
	if (!peer)
		return NULL;

	os_memcpy(&peer->sci, &participant->current_peer_sci,
		  sizeof(peer->sci));

	rxsc = ieee802_1x_kay_init_receive_sc(&peer->sci);
	if (!rxsc) {
		os_free(peer);
		return NULL;
	}

	if (secy_create_receive_sc(participant->kay, rxsc)) {
		os_free(rxsc);
		os_free(peer);
		return NULL;
	}
	dl_list_add(&participant->live_peers, &peer->list);
	dl_list_add(&participant->rxsc_list, &rxsc->list);

	wpa_printf(MSG_DEBUG, "KaY: Live peer created");
	ieee802_1x_kay_dump_peer(peer);

	return peer;
}


/**
 * ieee802_1x_kay_create_potential_peer
 */
static struct ieee802_1x_kay_peer *
ieee802_1x_kay_create_potential_peer(
	struct ieee802_1x_mka_participant *participant, const u8 *mi, u32 mn)
{
	struct ieee802_1x_kay_peer *peer;

	peer = ieee802_1x_kay_create_peer(mi, mn);
	if (!peer)
		return NULL;

	dl_list_add(&participant->potential_peers, &peer->list);

	wpa_printf(MSG_DEBUG, "KaY: Potential peer created");
	ieee802_1x_kay_dump_peer(peer);

	return peer;
}


/**
 * ieee802_1x_kay_move_live_peer
 */
static struct ieee802_1x_kay_peer *
ieee802_1x_kay_move_live_peer(struct ieee802_1x_mka_participant *participant,
			      u8 *mi, u32 mn)
{
	struct ieee802_1x_kay_peer *peer;
	struct receive_sc *rxsc;

	peer = ieee802_1x_kay_get_potential_peer(participant, mi);
	if (!peer)
		return NULL;

	rxsc = ieee802_1x_kay_init_receive_sc(&participant->current_peer_sci);
	if (!rxsc)
		return NULL;

	os_memcpy(&peer->sci, &participant->current_peer_sci,
		  sizeof(peer->sci));
	peer->mn = mn;
	peer->expire = time(NULL) + MKA_LIFE_TIME / 1000;

	wpa_printf(MSG_DEBUG, "KaY: Move potential peer to live peer");
	ieee802_1x_kay_dump_peer(peer);

	dl_list_del(&peer->list);
	if (secy_create_receive_sc(participant->kay, rxsc)) {
		wpa_printf(MSG_ERROR, "KaY: Can't create SC, discard peer");
		os_free(rxsc);
		os_free(peer);
		return NULL;
	}
	dl_list_add_tail(&participant->live_peers, &peer->list);

	dl_list_add(&participant->rxsc_list, &rxsc->list);

	return peer;
}



/**
 *  ieee802_1x_mka_basic_body_present -
 */
static Boolean
ieee802_1x_mka_basic_body_present(
	struct ieee802_1x_mka_participant *participant)
{
	return TRUE;
}


/**
 * ieee802_1x_mka_basic_body_length -
 */
static int
ieee802_1x_mka_basic_body_length(struct ieee802_1x_mka_participant *participant)
{
	int length;

	length = sizeof(struct ieee802_1x_mka_basic_body);
	length += participant->ckn.len;
	return MKA_ALIGN_LENGTH(length);
}


/**
 * ieee802_1x_mka_encode_basic_body
 */
static int
ieee802_1x_mka_encode_basic_body(
	struct ieee802_1x_mka_participant *participant,
	struct wpabuf *buf)
{
	struct ieee802_1x_mka_basic_body *body;
	struct ieee802_1x_kay *kay = participant->kay;
	unsigned int length = sizeof(struct ieee802_1x_mka_basic_body);

	length += participant->ckn.len;
	body = wpabuf_put(buf, MKA_ALIGN_LENGTH(length));

	body->version = kay->mka_version;
	body->priority = kay->actor_priority;
	/* The Key Server flag is set if and only if the participant has not
	 * decided that another participant is or will be the Key Server. */
	if (participant->is_elected)
		body->key_server = participant->is_key_server;
	else
		body->key_server = participant->can_be_key_server;

	body->macsec_desired = kay->macsec_desired;
	body->macsec_capability = kay->macsec_capable;
	set_mka_param_body_len(body, length - MKA_HDR_LEN);

	os_memcpy(body->actor_sci.addr, kay->actor_sci.addr,
		  sizeof(kay->actor_sci.addr));
	body->actor_sci.port = kay->actor_sci.port;

	os_memcpy(body->actor_mi, participant->mi, sizeof(body->actor_mi));
	participant->mn = participant->mn + 1;
	body->actor_mn = host_to_be32(participant->mn);
	os_memcpy(body->algo_agility, kay->algo_agility,
		  sizeof(body->algo_agility));

	os_memcpy(body->ckn, participant->ckn.name, participant->ckn.len);

	ieee802_1x_mka_dump_basic_body(body);

	return 0;
}


static Boolean
reset_participant_mi(struct ieee802_1x_mka_participant *participant)
{
	if (os_get_random(participant->mi, sizeof(participant->mi)) < 0)
		return FALSE;
	participant->mn = 0;

	return TRUE;
}


/**
 * ieee802_1x_mka_decode_basic_body -
 */
static struct ieee802_1x_mka_participant *
ieee802_1x_mka_decode_basic_body(struct ieee802_1x_kay *kay, const u8 *mka_msg,
				 size_t msg_len)
{
	struct ieee802_1x_mka_participant *participant;
	const struct ieee802_1x_mka_basic_body *body;
	struct ieee802_1x_kay_peer *peer;
	size_t ckn_len;
	size_t body_len;

	body = (const struct ieee802_1x_mka_basic_body *) mka_msg;

	if (body->version > MKA_VERSION_ID) {
		wpa_printf(MSG_DEBUG,
			   "KaY: Peer's version(%d) greater than MKA current version(%d)",
			   body->version, MKA_VERSION_ID);
	}
	if (kay->is_obliged_key_server && body->key_server) {
		wpa_printf(MSG_DEBUG, "KaY: I must be key server - ignore MKPDU claiming to be from a key server");
		return NULL;
	}

	body_len = get_mka_param_body_len(body);
	if (body_len < sizeof(struct ieee802_1x_mka_basic_body) - MKA_HDR_LEN) {
		wpa_printf(MSG_DEBUG, "KaY: Too small body length %zu",
			   body_len);
		return NULL;
	}
	ckn_len = body_len -
	    (sizeof(struct ieee802_1x_mka_basic_body) - MKA_HDR_LEN);
	participant = ieee802_1x_kay_get_participant(kay, body->ckn, ckn_len);
	if (!participant) {
		wpa_printf(MSG_DEBUG,
			   "KaY: Peer is not included in my CA - ignore MKPDU");
		return NULL;
	}

	/* If the peer's MI is my MI, I will choose new MI */
	if (os_memcmp(body->actor_mi, participant->mi, MI_LEN) == 0) {
		if (!reset_participant_mi(participant))
			return NULL;
		wpa_printf(MSG_DEBUG,
			   "KaY: Peer using my MI - selected a new random MI: %s",
			   mi_txt(participant->mi));
	}

	os_memcpy(participant->current_peer_id.mi, body->actor_mi, MI_LEN);
	participant->current_peer_id.mn = body->actor_mn;
	os_memcpy(participant->current_peer_sci.addr, body->actor_sci.addr,
		  sizeof(participant->current_peer_sci.addr));
	participant->current_peer_sci.port = body->actor_sci.port;

	/* handler peer */
	peer = ieee802_1x_kay_get_peer(participant, body->actor_mi);
	if (!peer) {
		/* Check duplicated SCI
		 *
		 * A duplicated SCI indicates either an active attacker or
		 * a valid peer whose MI is being changed. The latter scenario
		 * is more likely because to have gotten this far the received
		 * MKPDU must have had a valid ICV, indicating the peer holds
		 * the same CAK as our participant.
		 *
		 * Before creating a new peer object for the new MI we must
		 * clean up the resources (SCs and SAs) associated with the
		 * old peer. An easy way to do this is to ignore MKPDUs with
		 * the new MI's for now and just wait for the old peer to
		 * time out and clean itself up (within MKA_LIFE_TIME).
		 *
		 * This method is preferable to deleting the old peer here
		 * and now and continuing on with processing because if this
		 * MKPDU is from an attacker it's better to ignore the MKPDU
		 * than to process it (and delete a valid peer as well).
		 */
		peer = ieee802_1x_kay_get_peer_sci(participant,
						   &body->actor_sci);
		if (peer) {
			time_t new_expire;

			wpa_printf(MSG_WARNING,
				   "KaY: duplicated SCI detected - maybe active attacker or peer selected new MI - ignore MKPDU");
			/* Reduce timeout to speed up this process but left the
			 * chance for old one to prove aliveness. */
			new_expire = time(NULL) + MKA_HELLO_TIME * 1.5 / 1000;
			if (peer->expire > new_expire)
				peer->expire = new_expire;
			return NULL;
		}

		peer = ieee802_1x_kay_create_potential_peer(
			participant, body->actor_mi,
			be_to_host32(body->actor_mn));
		if (!peer) {
			wpa_printf(MSG_DEBUG,
				   "KaY: No potential peer entry found - ignore MKPDU");
			return NULL;
		}

		peer->macsec_desired = body->macsec_desired;
		peer->macsec_capability = body->macsec_capability;
		peer->is_key_server = (Boolean) body->key_server;
		peer->key_server_priority = body->priority;
	} else if (peer->mn < be_to_host32(body->actor_mn)) {
		peer->mn = be_to_host32(body->actor_mn);
		peer->macsec_desired = body->macsec_desired;
		peer->macsec_capability = body->macsec_capability;
		peer->is_key_server = (Boolean) body->key_server;
		peer->key_server_priority = body->priority;
	} else {
		wpa_printf(MSG_WARNING,
			   "KaY: The peer MN did not increase - ignore MKPDU");
		return NULL;
	}

	return participant;
}


/**
 * ieee802_1x_mka_live_peer_body_present
 */
static Boolean
ieee802_1x_mka_live_peer_body_present(
	struct ieee802_1x_mka_participant *participant)
{
	return !dl_list_empty(&participant->live_peers);
}


/**
 * ieee802_1x_kay_get_live_peer_length
 */
static int
ieee802_1x_mka_get_live_peer_length(
	struct ieee802_1x_mka_participant *participant)
{
	int len = MKA_HDR_LEN;
	struct ieee802_1x_kay_peer *peer;

	dl_list_for_each(peer, &participant->live_peers,
			 struct ieee802_1x_kay_peer, list)
		len += sizeof(struct ieee802_1x_mka_peer_id);

	return MKA_ALIGN_LENGTH(len);
}


/**
 * ieee802_1x_mka_encode_live_peer_body -
 */
static int
ieee802_1x_mka_encode_live_peer_body(
	struct ieee802_1x_mka_participant *participant,
	struct wpabuf *buf)
{
	struct ieee802_1x_mka_peer_body *body;
	struct ieee802_1x_kay_peer *peer;
	unsigned int length;
	struct ieee802_1x_mka_peer_id *body_peer;

	length = ieee802_1x_mka_get_live_peer_length(participant);
	body = wpabuf_put(buf, sizeof(struct ieee802_1x_mka_peer_body));

	body->type = MKA_LIVE_PEER_LIST;
	set_mka_param_body_len(body, length - MKA_HDR_LEN);

	dl_list_for_each(peer, &participant->live_peers,
			 struct ieee802_1x_kay_peer, list) {
		body_peer = wpabuf_put(buf,
				       sizeof(struct ieee802_1x_mka_peer_id));
		os_memcpy(body_peer->mi, peer->mi, MI_LEN);
		body_peer->mn = host_to_be32(peer->mn);
	}

	ieee802_1x_mka_dump_peer_body(body);
	return 0;
}

/**
 * ieee802_1x_mka_potential_peer_body_present
 */
static Boolean
ieee802_1x_mka_potential_peer_body_present(
	struct ieee802_1x_mka_participant *participant)
{
	return !dl_list_empty(&participant->potential_peers);
}


/**
 * ieee802_1x_kay_get_potential_peer_length
 */
static int
ieee802_1x_mka_get_potential_peer_length(
	struct ieee802_1x_mka_participant *participant)
{
	int len = MKA_HDR_LEN;
	struct ieee802_1x_kay_peer *peer;

	dl_list_for_each(peer, &participant->potential_peers,
			 struct ieee802_1x_kay_peer, list)
		len += sizeof(struct ieee802_1x_mka_peer_id);

	return MKA_ALIGN_LENGTH(len);
}


/**
 * ieee802_1x_mka_encode_potential_peer_body -
 */
static int
ieee802_1x_mka_encode_potential_peer_body(
	struct ieee802_1x_mka_participant *participant,
	struct wpabuf *buf)
{
	struct ieee802_1x_mka_peer_body *body;
	struct ieee802_1x_kay_peer *peer;
	unsigned int length;
	struct ieee802_1x_mka_peer_id *body_peer;

	length = ieee802_1x_mka_get_potential_peer_length(participant);
	body = wpabuf_put(buf, sizeof(struct ieee802_1x_mka_peer_body));

	body->type = MKA_POTENTIAL_PEER_LIST;
	set_mka_param_body_len(body, length - MKA_HDR_LEN);

	dl_list_for_each(peer, &participant->potential_peers,
			 struct ieee802_1x_kay_peer, list) {
		body_peer = wpabuf_put(buf,
				       sizeof(struct ieee802_1x_mka_peer_id));
		os_memcpy(body_peer->mi, peer->mi, MI_LEN);
		body_peer->mn = host_to_be32(peer->mn);
	}

	ieee802_1x_mka_dump_peer_body(body);
	return 0;
}


/**
 * ieee802_1x_mka_i_in_peerlist -
 */
static Boolean
ieee802_1x_mka_i_in_peerlist(struct ieee802_1x_mka_participant *participant,
			     const u8 *mka_msg, size_t msg_len)
{
	struct ieee802_1x_mka_hdr *hdr;
	size_t body_len;
	size_t left_len;
	u8 body_type;
	const u8 *pos;
	size_t i;

	for (pos = mka_msg, left_len = msg_len;
	     left_len > MKA_HDR_LEN + DEFAULT_ICV_LEN;
	     left_len -= MKA_ALIGN_LENGTH(body_len) + MKA_HDR_LEN,
		     pos += MKA_ALIGN_LENGTH(body_len) + MKA_HDR_LEN) {
		hdr = (struct ieee802_1x_mka_hdr *) pos;
		body_len = get_mka_param_body_len(hdr);
		body_type = get_mka_param_body_type(hdr);

		if (left_len < (MKA_HDR_LEN + MKA_ALIGN_LENGTH(body_len) + DEFAULT_ICV_LEN)) {
			wpa_printf(MSG_ERROR,
				   "KaY: MKA Peer Packet Body Length (%zu bytes) is less than the Parameter Set Header Length (%zu bytes) + the Parameter Set Body Length (%zu bytes) + %d bytes of ICV",
				   left_len, MKA_HDR_LEN,
				   MKA_ALIGN_LENGTH(body_len),
				   DEFAULT_ICV_LEN);
			return FALSE;
		}

		if (body_type != MKA_LIVE_PEER_LIST &&
		    body_type != MKA_POTENTIAL_PEER_LIST)
			continue;

		if ((body_len % 16) != 0) {
			wpa_printf(MSG_ERROR,
				   "KaY: MKA Peer Packet Body Length (%zu bytes) should be a multiple of 16 octets",
				   body_len);
			continue;
		}

		ieee802_1x_mka_dump_peer_body(
			(struct ieee802_1x_mka_peer_body *)pos);

		for (i = 0; i < body_len;
		     i += sizeof(struct ieee802_1x_mka_peer_id)) {
			const struct ieee802_1x_mka_peer_id *peer_mi;

			peer_mi = (const struct ieee802_1x_mka_peer_id *)
				(pos + MKA_HDR_LEN + i);
			if (os_memcmp(peer_mi->mi, participant->mi,
				      MI_LEN) == 0) {
				u32 mn = be_to_host32(peer_mi->mn);

				wpa_printf(MSG_DEBUG,
					   "KaY: My MI - received MN %u, most recently transmitted MN %u",
					   mn, participant->mn);
				if (mn == participant->mn)
					return TRUE;
			}
		}
	}

	return FALSE;
}


/**
 * ieee802_1x_mka_decode_live_peer_body -
 */
static int ieee802_1x_mka_decode_live_peer_body(
	struct ieee802_1x_mka_participant *participant,
	const u8 *peer_msg, size_t msg_len)
{
	const struct ieee802_1x_mka_hdr *hdr;
	struct ieee802_1x_kay_peer *peer;
	size_t body_len;
	size_t i;
	Boolean is_included;

	is_included = ieee802_1x_kay_is_in_live_peer(
		participant, participant->current_peer_id.mi);

	hdr = (const struct ieee802_1x_mka_hdr *) peer_msg;
	body_len = get_mka_param_body_len(hdr);
	if (body_len % 16 != 0) {
		wpa_printf(MSG_ERROR,
			   "KaY: MKA Peer Packet Body Length (%zu bytes) should be a multiple of 16 octets",
			   body_len);
		return -1;
	}

	for (i = 0; i < body_len; i += sizeof(struct ieee802_1x_mka_peer_id)) {
		const struct ieee802_1x_mka_peer_id *peer_mi;
		u32 peer_mn;

		peer_mi = (const struct ieee802_1x_mka_peer_id *)
			(peer_msg + MKA_HDR_LEN + i);
		peer_mn = be_to_host32(peer_mi->mn);

		/* it is myself */
		if (os_memcmp(peer_mi, participant->mi, MI_LEN) == 0) {
			/* My message id is used by other participant */
			if (peer_mn > participant->mn &&
			    !reset_participant_mi(participant))
				wpa_printf(MSG_DEBUG, "KaY: Could not update mi");
			continue;
		}

		if (!is_included)
			continue;

		peer = ieee802_1x_kay_get_peer(participant, peer_mi->mi);
		if (peer) {
			peer->mn = peer_mn;
		} else if (!ieee802_1x_kay_create_potential_peer(
				participant, peer_mi->mi, peer_mn)) {
			return -1;
		}
	}

	return 0;
}


/**
 * ieee802_1x_mka_decode_potential_peer_body -
 */
static int
ieee802_1x_mka_decode_potential_peer_body(
	struct ieee802_1x_mka_participant *participant,
	const u8 *peer_msg, size_t msg_len)
{
	const struct ieee802_1x_mka_hdr *hdr;
	size_t body_len;
	size_t i;

	hdr = (const struct ieee802_1x_mka_hdr *) peer_msg;
	body_len = get_mka_param_body_len(hdr);
	if (body_len % 16 != 0) {
		wpa_printf(MSG_ERROR,
			   "KaY: MKA Peer Packet Body Length (%zu bytes) should be a multiple of 16 octets",
			   body_len);
		return -1;
	}

	for (i = 0; i < body_len; i += sizeof(struct ieee802_1x_mka_peer_id)) {
		const struct ieee802_1x_mka_peer_id *peer_mi;
		u32 peer_mn;

		peer_mi = (struct ieee802_1x_mka_peer_id *)
			(peer_msg + MKA_HDR_LEN + i);
		peer_mn = be_to_host32(peer_mi->mn);

		/* it is myself */
		if (os_memcmp(peer_mi, participant->mi, MI_LEN) == 0) {
			/* My message id is used by other participant */
			if (peer_mn > participant->mn &&
			    !reset_participant_mi(participant))
				wpa_printf(MSG_DEBUG, "KaY: Could not update mi");
			continue;
		}
	}

	return 0;
}


/**
 * ieee802_1x_mka_sak_use_body_present
 */
static Boolean
ieee802_1x_mka_sak_use_body_present(
	struct ieee802_1x_mka_participant *participant)
{
	return participant->to_use_sak;
}


/**
 * ieee802_1x_mka_get_sak_use_length
 */
static int
ieee802_1x_mka_get_sak_use_length(
	struct ieee802_1x_mka_participant *participant)
{
	int length = MKA_HDR_LEN;

	if (participant->kay->macsec_desired && participant->advised_desired)
		length = sizeof(struct ieee802_1x_mka_sak_use_body);

	return MKA_ALIGN_LENGTH(length);
}


/**
 * ieee802_1x_mka_get_lpn
 */
static u32
ieee802_1x_mka_get_lpn(struct ieee802_1x_mka_participant *principal,
		       struct ieee802_1x_mka_ki *ki)
{
	struct transmit_sa *txsa;
	u32 lpn = 0;

	dl_list_for_each(txsa, &principal->txsc->sa_list,
			 struct transmit_sa, list) {
		if (is_ki_equal(&txsa->pkey->key_identifier, ki)) {
			/* Per IEEE Std 802.1X-2010, Clause 9, "Each SecY uses
			 * MKA to communicate the lowest PN used for
			 * transmission with the SAK within the last two
			 * seconds".  Achieve this 2 second delay by setting the
			 * lpn using the transmit next PN (i.e., txsa->next_pn)
			 * that was read last time here (i.e., mka_hello_time
			 * 2 seconds ago).
			 *
			 * The lowest acceptable PN is the same as the last
			 * transmitted PN, which is one less than the next
			 * transmit PN.
			 *
			 * NOTE: This method only works if mka_hello_time is 2s.
			 */
			lpn = (txsa->next_pn > 0) ? (txsa->next_pn - 1) : 0;

			/* Now read the current transmit next PN for use next
			 * time through. */
			secy_get_transmit_next_pn(principal->kay, txsa);
			break;
		}
	}

	if (lpn == 0)
		lpn = 1;

	return lpn;
}


/**
 * ieee802_1x_mka_encode_sak_use_body -
 */
static int
ieee802_1x_mka_encode_sak_use_body(
	struct ieee802_1x_mka_participant *participant,
	struct wpabuf *buf)
{
	struct ieee802_1x_mka_sak_use_body *body;
	struct ieee802_1x_kay *kay = participant->kay;
	unsigned int length;
	u32 pn = 1;

	length = ieee802_1x_mka_get_sak_use_length(participant);
	body = wpabuf_put(buf, length);

	body->type = MKA_SAK_USE;
	set_mka_param_body_len(body, length - MKA_HDR_LEN);

	if (length == MKA_HDR_LEN) {
		body->ptx = TRUE;
		body->prx = TRUE;
		body->lan = 0;
		body->lrx = FALSE;
		body->ltx = FALSE;
		body->delay_protect = FALSE;
		return 0;
	}

	/* data delay protect */
	body->delay_protect = kay->mka_hello_time <= MKA_BOUNDED_HELLO_TIME;
	/* lowest accept packet number */
	pn = ieee802_1x_mka_get_lpn(participant, &participant->lki);
	if (pn > kay->pn_exhaustion) {
		wpa_printf(MSG_WARNING, "KaY: My LPN exhaustion");
		if (participant->is_key_server)
			participant->new_sak = TRUE;
	}

	body->llpn = host_to_be32(pn);
	pn = ieee802_1x_mka_get_lpn(participant, &participant->oki);
	body->olpn = host_to_be32(pn);

	/* plain tx, plain rx */
	body->ptx = !kay->macsec_protect;
	body->prx = kay->macsec_validate != Strict;

	/* latest key: rx, tx, key server member identifier key number */
	body->lan = participant->lan;
	os_memcpy(body->lsrv_mi, participant->lki.mi, sizeof(body->lsrv_mi));
	body->lkn = host_to_be32(participant->lki.kn);
	body->lrx = participant->lrx;
	body->ltx = participant->ltx;

	/* old key: rx, tx, key server member identifier key number */
	body->oan = participant->oan;
	if (participant->oki.kn != participant->lki.kn &&
	    participant->oki.kn != 0) {
		body->otx = TRUE;
		body->orx = TRUE;
		os_memcpy(body->osrv_mi, participant->oki.mi,
			  sizeof(body->osrv_mi));
		body->okn = host_to_be32(participant->oki.kn);
	} else {
		body->otx = FALSE;
		body->orx = FALSE;
	}

	/* set CP's variable */
	if (body->ltx) {
		kay->tx_enable = TRUE;
		kay->port_enable = TRUE;
	}
	if (body->lrx)
		kay->rx_enable = TRUE;

	ieee802_1x_mka_dump_sak_use_body(body);
	return 0;
}


/**
 * ieee802_1x_mka_decode_sak_use_body -
 */
static int
ieee802_1x_mka_decode_sak_use_body(
	struct ieee802_1x_mka_participant *participant,
	const u8 *mka_msg, size_t msg_len)
{
	struct ieee802_1x_mka_hdr *hdr;
	struct ieee802_1x_mka_sak_use_body *body;
	struct ieee802_1x_kay_peer *peer;
	struct receive_sc *rxsc;
	struct receive_sa *rxsa;
	struct data_key *sa_key = NULL;
	size_t body_len;
	struct ieee802_1x_mka_ki ki;
	u32 lpn;
	Boolean all_receiving;
	Boolean found;
	struct ieee802_1x_kay *kay = participant->kay;

	if (!participant->principal) {
		wpa_printf(MSG_WARNING, "KaY: Participant is not principal");
		return -1;
	}
	peer = ieee802_1x_kay_get_live_peer(participant,
					    participant->current_peer_id.mi);
	if (!peer) {
		wpa_printf(MSG_WARNING,
			   "KaY: The peer (%s) is not my live peer - ignore MACsec SAK Use parameter set",
			   mi_txt(participant->current_peer_id.mi));
		return -1;
	}

	hdr = (struct ieee802_1x_mka_hdr *) mka_msg;
	body_len = get_mka_param_body_len(hdr);
	body = (struct ieee802_1x_mka_sak_use_body *) mka_msg;
	ieee802_1x_mka_dump_sak_use_body(body);

	if ((body_len != 0) && (body_len < 40)) {
		wpa_printf(MSG_ERROR,
			   "KaY: MKA Use SAK Packet Body Length (%zu bytes) should be 0, 40, or more octets",
			   body_len);
		return -1;
	}

	/* TODO: what action should I take when peer does not support MACsec */
	if (body_len == 0) {
		wpa_printf(MSG_WARNING, "KaY: Peer does not support MACsec");
		return 0;
	}

	/* TODO: when the plain tx or rx of peer is true, should I change
	 * the attribute of controlled port
	 */
	if (body->prx)
		wpa_printf(MSG_WARNING, "KaY: peer's plain rx are TRUE");

	if (body->ptx)
		wpa_printf(MSG_WARNING, "KaY: peer's plain tx are TRUE");

	/* check latest key is valid */
	if (body->ltx || body->lrx) {
		found = FALSE;
		os_memcpy(ki.mi, body->lsrv_mi, sizeof(ki.mi));
		ki.kn = be_to_host32(body->lkn);
		dl_list_for_each(sa_key, &participant->sak_list,
				 struct data_key, list) {
			if (is_ki_equal(&sa_key->key_identifier, &ki)) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			wpa_printf(MSG_INFO, "KaY: Latest key is invalid");
			return -1;
		}
		if (os_memcmp(participant->lki.mi, body->lsrv_mi,
			      sizeof(participant->lki.mi)) == 0 &&
		    be_to_host32(body->lkn) == participant->lki.kn &&
		    body->lan == participant->lan) {
			peer->sak_used = TRUE;
		}
		if (body->ltx && peer->is_key_server) {
			ieee802_1x_cp_set_servertransmitting(kay->cp, TRUE);
			ieee802_1x_cp_sm_step(kay->cp);
		}
	}

	/* check old key is valid (but only if we remember our old key) */
	if (participant->oki.kn != 0 && (body->otx || body->orx)) {
		if (os_memcmp(participant->oki.mi, body->osrv_mi,
			      sizeof(participant->oki.mi)) != 0 ||
		    be_to_host32(body->okn) != participant->oki.kn ||
		    body->oan != participant->oan) {
			wpa_printf(MSG_WARNING, "KaY: Old key is invalid");
			return -1;
		}
	}

	/* TODO: how to set the MACsec hardware when delay_protect is true */
	if (body->delay_protect &&
	    (!be_to_host32(body->llpn) || !be_to_host32(body->olpn))) {
		wpa_printf(MSG_WARNING,
			   "KaY: Lowest packet number should be greater than 0 when delay_protect is TRUE");
		return -1;
	}

	/* check all live peer have used the sak for receiving sa */
	all_receiving = TRUE;
	dl_list_for_each(peer, &participant->live_peers,
			 struct ieee802_1x_kay_peer, list) {
		if (!peer->sak_used) {
			all_receiving = FALSE;
			break;
		}
	}
	if (all_receiving) {
		participant->to_dist_sak = FALSE;
		ieee802_1x_cp_set_allreceiving(kay->cp, TRUE);
		ieee802_1x_cp_sm_step(kay->cp);
	}

	/* if I'm key server, and detects peer member pn exhaustion, rekey. */
	lpn = be_to_host32(body->llpn);
	if (lpn > kay->pn_exhaustion) {
		if (participant->is_key_server) {
			participant->new_sak = TRUE;
			wpa_printf(MSG_WARNING, "KaY: Peer LPN exhaustion");
		}
	}

	if (sa_key)
		sa_key->next_pn = lpn;
	found = FALSE;
	dl_list_for_each(rxsc, &participant->rxsc_list, struct receive_sc,
			 list) {
		dl_list_for_each(rxsa, &rxsc->sa_list, struct receive_sa,
				 list) {
			if (sa_key && rxsa->pkey == sa_key) {
				found = TRUE;
				break;
			}
		}
		if (found)
			break;
	}
	if (!found) {
		wpa_printf(MSG_WARNING, "KaY: Can't find rxsa");
		return -1;
	}

	if (body->delay_protect) {
		secy_get_receive_lowest_pn(participant->kay, rxsa);
		if (lpn > rxsa->lowest_pn) {
			/* Delay protect window (communicated via MKA) is
			 * tighter than SecY's current replay protect window,
			 * so tell SecY the new (and higher) lpn. */
			rxsa->lowest_pn = lpn;
			secy_set_receive_lowest_pn(participant->kay, rxsa);
			wpa_printf(MSG_DEBUG, "KaY: update lpn =0x%x", lpn);
		}
		/* FIX: Delay protection for olpn not implemented.
		 * Note that Old Key is only active for MKA_SAK_RETIRE_TIME
		 * (3 seconds) and delay protection does allow PN's within
		 * a 2 seconds window, so olpn would be a lot of work for
		 * just 1 second's worth of protection. */
	}

	return 0;
}


/**
 * ieee802_1x_mka_dist_sak_body_present
 */
static Boolean
ieee802_1x_mka_dist_sak_body_present(
	struct ieee802_1x_mka_participant *participant)
{
	return participant->is_key_server && participant->to_dist_sak &&
		participant->new_key;
}


/**
 * ieee802_1x_kay_get_dist_sak_length
 */
static int
ieee802_1x_mka_get_dist_sak_length(
	struct ieee802_1x_mka_participant *participant)
{
	int length = MKA_HDR_LEN;
	unsigned int cs_index = participant->kay->macsec_csindex;

	if (participant->advised_desired && cs_index < CS_TABLE_SIZE) {
		length = sizeof(struct ieee802_1x_mka_dist_sak_body);
		if (cs_index != DEFAULT_CS_INDEX)
			length += CS_ID_LEN;

		length += cipher_suite_tbl[cs_index].sak_len + 8;
	}

	return MKA_ALIGN_LENGTH(length);
}


/**
 * ieee802_1x_mka_encode_dist_sak_body -
 */
static int
ieee802_1x_mka_encode_dist_sak_body(
	struct ieee802_1x_mka_participant *participant,
	struct wpabuf *buf)
{
	struct ieee802_1x_mka_dist_sak_body *body;
	struct data_key *sak;
	unsigned int length;
	unsigned int cs_index;
	int sak_pos;

	length = ieee802_1x_mka_get_dist_sak_length(participant);
	body = wpabuf_put(buf, length);
	body->type = MKA_DISTRIBUTED_SAK;
	set_mka_param_body_len(body, length - MKA_HDR_LEN);
	if (length == MKA_HDR_LEN) {
		body->confid_offset = 0;
		body->dan = 0;
		return 0;
	}

	sak = participant->new_key;
	if (!sak) {
		wpa_printf(MSG_DEBUG,
			   "KaY: No SAK available to build Distributed SAK parameter set");
		return -1;
	}
	body->confid_offset = sak->confidentiality_offset;
	body->dan = sak->an;
	body->kn = host_to_be32(sak->key_identifier.kn);
	cs_index = participant->kay->macsec_csindex;
	sak_pos = 0;
	if (cs_index >= CS_TABLE_SIZE)
		return -1;
	if (cs_index != DEFAULT_CS_INDEX) {
		be64 cs;

		cs = host_to_be64(cipher_suite_tbl[cs_index].id);
		os_memcpy(body->sak, &cs, CS_ID_LEN);
		sak_pos = CS_ID_LEN;
	}
	if (aes_wrap(participant->kek.key, participant->kek.len,
		     cipher_suite_tbl[cs_index].sak_len / 8,
		     sak->key, body->sak + sak_pos)) {
		wpa_printf(MSG_ERROR, "KaY: AES wrap failed");
		return -1;
	}

	ieee802_1x_mka_dump_dist_sak_body(body);

	return 0;
}


/**
 * ieee802_1x_kay_init_data_key -
 */
static void ieee802_1x_kay_init_data_key(struct data_key *pkey)
{
	pkey->transmits = TRUE;
	pkey->receives = TRUE;
	os_get_time(&pkey->created_time);

	pkey->next_pn = 1;
	pkey->user = 1;
}


/**
 * ieee802_1x_kay_decode_dist_sak_body -
 */
static int
ieee802_1x_mka_decode_dist_sak_body(
	struct ieee802_1x_mka_participant *participant,
	const u8 *mka_msg, size_t msg_len)
{
	struct ieee802_1x_mka_hdr *hdr;
	struct ieee802_1x_mka_dist_sak_body *body;
	struct ieee802_1x_kay_peer *peer;
	struct macsec_ciphersuite *cs;
	size_t body_len;
	struct data_key *sa_key = NULL;
	int sak_len;
	u8 *wrap_sak;
	u8 *unwrap_sak;
	struct ieee802_1x_kay *kay = participant->kay;

	hdr = (struct ieee802_1x_mka_hdr *) mka_msg;
	body_len = get_mka_param_body_len(hdr);
	if ((body_len != 0) && (body_len != 28) && (body_len < 36)) {
		wpa_printf(MSG_ERROR,
			   "KaY: MKA Use SAK Packet Body Length (%zu bytes) should be 0, 28, 36, or more octets",
			   body_len);
		return -1;
	}

	if (!participant->principal) {
		wpa_printf(MSG_ERROR,
			   "KaY: I can't accept the distributed SAK as I am not principal");
		return -1;
	}
	if (participant->is_key_server) {
		wpa_printf(MSG_ERROR,
			   "KaY: Reject distributed SAK since I'm a key server");
		return -1;
	}
	if (!kay->macsec_desired ||
	    kay->macsec_capable == MACSEC_CAP_NOT_IMPLEMENTED) {
		wpa_printf(MSG_ERROR,
			   "KaY: I am not MACsec-desired or without MACsec capable");
		return -1;
	}

	peer = ieee802_1x_kay_get_live_peer(participant,
					    participant->current_peer_id.mi);
	if (!peer) {
		wpa_printf(MSG_ERROR,
			   "KaY: The key server is not in my live peers list");
		return -1;
	}
	if (!sci_equal(&kay->key_server_sci, &peer->sci)) {
		wpa_printf(MSG_ERROR, "KaY: The key server is not elected");
		return -1;
	}

	if (body_len == 0) {
		kay->authenticated = TRUE;
		kay->secured = FALSE;
		kay->failed = FALSE;
		participant->advised_desired = FALSE;
		ieee802_1x_cp_connect_authenticated(kay->cp);
		ieee802_1x_cp_sm_step(kay->cp);
		wpa_printf(MSG_WARNING, "KaY: The Key server advise no MACsec");
		participant->to_use_sak = FALSE;
		return 0;
	}

	participant->advised_desired = TRUE;
	kay->authenticated = FALSE;
	kay->secured = TRUE;
	kay->failed = FALSE;
	ieee802_1x_cp_connect_secure(kay->cp);
	ieee802_1x_cp_sm_step(kay->cp);

	body = (struct ieee802_1x_mka_dist_sak_body *)mka_msg;
	ieee802_1x_mka_dump_dist_sak_body(body);
	dl_list_for_each(sa_key, &participant->sak_list, struct data_key, list)
	{
		if (os_memcmp(sa_key->key_identifier.mi,
			      participant->current_peer_id.mi, MI_LEN) == 0 &&
		    sa_key->key_identifier.kn == be_to_host32(body->kn)) {
			wpa_printf(MSG_DEBUG,
				   "KaY: SAK has already been installed - do not set it again");
			return 0;
		}
	}

	if (body_len == 28) {
		sak_len = DEFAULT_SA_KEY_LEN;
		wrap_sak =  body->sak;
		kay->macsec_csindex = DEFAULT_CS_INDEX;
		cs = &cipher_suite_tbl[kay->macsec_csindex];
	} else {
		unsigned int idx;

		cs = ieee802_1x_kay_get_cipher_suite(participant, body->sak,
						     &idx);
		if (!cs) {
			wpa_printf(MSG_ERROR,
				   "KaY: I can't support the Cipher Suite advised by key server");
			return -1;
		}
		sak_len = cs->sak_len;
		wrap_sak = body->sak + CS_ID_LEN;
		kay->macsec_csindex = idx;
	}

	unwrap_sak = os_zalloc(sak_len);
	if (!unwrap_sak) {
		wpa_printf(MSG_ERROR, "KaY-%s: Out of memory", __func__);
		return -1;
	}
	if (aes_unwrap(participant->kek.key, participant->kek.len,
		       sak_len >> 3, wrap_sak, unwrap_sak)) {
		wpa_printf(MSG_ERROR, "KaY: AES unwrap failed");
		os_free(unwrap_sak);
		return -1;
	}
	wpa_hexdump_key(MSG_DEBUG, "\tAES Key Unwrap of SAK.:",
			unwrap_sak, sak_len);

	sa_key = os_zalloc(sizeof(*sa_key));
	if (!sa_key) {
		os_free(unwrap_sak);
		return -1;
	}

	os_memcpy(&sa_key->key_identifier.mi, &participant->current_peer_id.mi,
		  MI_LEN);
	sa_key->key_identifier.kn = be_to_host32(body->kn);

	sa_key->key = unwrap_sak;
	sa_key->key_len = sak_len;

	sa_key->confidentiality_offset = body->confid_offset;
	sa_key->an = body->dan;
	ieee802_1x_kay_init_data_key(sa_key);

	ieee802_1x_kay_use_data_key(sa_key);
	dl_list_add(&participant->sak_list, &sa_key->list);

	ieee802_1x_cp_set_ciphersuite(kay->cp, cs->id);
	ieee802_1x_cp_sm_step(kay->cp);
	ieee802_1x_cp_set_offset(kay->cp, body->confid_offset);
	ieee802_1x_cp_sm_step(kay->cp);
	ieee802_1x_cp_set_distributedki(kay->cp, &sa_key->key_identifier);
	ieee802_1x_cp_set_distributedan(kay->cp, body->dan);
	ieee802_1x_cp_signal_newsak(kay->cp);
	ieee802_1x_cp_sm_step(kay->cp);

	kay->rcvd_keys++;
	participant->to_use_sak = TRUE;

	return 0;
}


/**
 * ieee802_1x_mka_icv_body_present
 */
static Boolean
ieee802_1x_mka_icv_body_present(struct ieee802_1x_mka_participant *participant)
{
	return TRUE;
}


/**
 * ieee802_1x_kay_get_icv_length
 */
static int
ieee802_1x_mka_get_icv_length(struct ieee802_1x_mka_participant *participant)
{
	int length;

	/* Determine if we need space for the ICV Indicator */
	if (mka_alg_tbl[participant->kay->mka_algindex].icv_len !=
	    DEFAULT_ICV_LEN)
		length = sizeof(struct ieee802_1x_mka_icv_body);
	else
		length = 0;
	length += mka_alg_tbl[participant->kay->mka_algindex].icv_len;

	return MKA_ALIGN_LENGTH(length);
}


/**
 * ieee802_1x_mka_encode_icv_body -
 */
static int
ieee802_1x_mka_encode_icv_body(struct ieee802_1x_mka_participant *participant,
			       struct wpabuf *buf)
{
	struct ieee802_1x_mka_icv_body *body;
	unsigned int length;
	u8 cmac[MAX_ICV_LEN];

	length = ieee802_1x_mka_get_icv_length(participant);
	if (mka_alg_tbl[participant->kay->mka_algindex].icv_len !=
	    DEFAULT_ICV_LEN)  {
		wpa_printf(MSG_DEBUG, "KaY: ICV Indicator");
		body = wpabuf_put(buf, MKA_HDR_LEN);
		body->type = MKA_ICV_INDICATOR;
		length -= MKA_HDR_LEN;
		set_mka_param_body_len(body, length);
	}

	if (mka_alg_tbl[participant->kay->mka_algindex].icv_hash(
		    participant->ick.key, participant->ick.len,
		    wpabuf_head(buf), wpabuf_len(buf), cmac)) {
		wpa_printf(MSG_ERROR, "KaY: failed to calculate ICV");
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "KaY: ICV", cmac, length);

	os_memcpy(wpabuf_put(buf, length), cmac, length);

	return 0;
}

/**
 * ieee802_1x_mka_decode_icv_body -
 */
static const u8 *
ieee802_1x_mka_decode_icv_body(struct ieee802_1x_mka_participant *participant,
			       const u8 *mka_msg, size_t msg_len)
{
	const struct ieee802_1x_mka_hdr *hdr;
	const struct ieee802_1x_mka_icv_body *body;
	size_t body_len;
	size_t left_len;
	u8 body_type;
	const u8 *pos;

	pos = mka_msg;
	left_len = msg_len;
	while (left_len > MKA_HDR_LEN + DEFAULT_ICV_LEN) {
		hdr = (const struct ieee802_1x_mka_hdr *) pos;
		body_len = MKA_ALIGN_LENGTH(get_mka_param_body_len(hdr));
		body_type = get_mka_param_body_type(hdr);

		if (left_len < body_len + MKA_HDR_LEN)
			break;

		if (body_type != MKA_ICV_INDICATOR) {
			left_len -= MKA_HDR_LEN + body_len;
			pos += MKA_HDR_LEN + body_len;
			continue;
		}

		body = (const struct ieee802_1x_mka_icv_body *) pos;
		if (body_len
		    < mka_alg_tbl[participant->kay->mka_algindex].icv_len)
			return NULL;

		return body->icv;
	}

	return mka_msg + msg_len - DEFAULT_ICV_LEN;
}


/**
 * ieee802_1x_mka_decode_dist_cak_body-
 */
static int
ieee802_1x_mka_decode_dist_cak_body(
	struct ieee802_1x_mka_participant *participant,
	const u8 *mka_msg, size_t msg_len)
{
	struct ieee802_1x_mka_hdr *hdr;
	size_t body_len;

	hdr = (struct ieee802_1x_mka_hdr *) mka_msg;
	body_len = get_mka_param_body_len(hdr);
	if (body_len < 28) {
		wpa_printf(MSG_ERROR,
			   "KaY: MKA Use CAK Packet Body Length (%zu bytes) should be 28 or more octets",
			   body_len);
		return -1;
	}

	return 0;
}


/**
 * ieee802_1x_mka_decode_kmd_body -
 */
static int
ieee802_1x_mka_decode_kmd_body(
	struct ieee802_1x_mka_participant *participant,
	const u8 *mka_msg, size_t msg_len)
{
	struct ieee802_1x_mka_hdr *hdr;
	size_t body_len;

	hdr = (struct ieee802_1x_mka_hdr *) mka_msg;
	body_len = get_mka_param_body_len(hdr);
	if (body_len < 5) {
		wpa_printf(MSG_ERROR,
			   "KaY: MKA Use KMD Packet Body Length (%zu bytes) should be 5 or more octets",
			   body_len);
		return -1;
	}

	return 0;
}


/**
 * ieee802_1x_mka_decode_announce_body -
 */
static int ieee802_1x_mka_decode_announce_body(
	struct ieee802_1x_mka_participant *participant,
	const u8 *mka_msg, size_t msg_len)
{
	return 0;
}


struct mka_param_body_handler {
	int (*body_tx)(struct ieee802_1x_mka_participant *participant,
		       struct wpabuf *buf);
	int (*body_rx)(struct ieee802_1x_mka_participant *participant,
		       const u8 *mka_msg, size_t msg_len);
	int (*body_length)(struct ieee802_1x_mka_participant *participant);
	Boolean (*body_present)(struct ieee802_1x_mka_participant *participant);
};


static struct mka_param_body_handler mka_body_handler[] = {
	/* Basic parameter set */
	{
		.body_tx      = ieee802_1x_mka_encode_basic_body,
		.body_rx      = NULL,
		.body_length  = ieee802_1x_mka_basic_body_length,
		.body_present = ieee802_1x_mka_basic_body_present
	},

	/* Live Peer List parameter set */
	{
		.body_tx      = ieee802_1x_mka_encode_live_peer_body,
		.body_rx      = ieee802_1x_mka_decode_live_peer_body,
		.body_length  = ieee802_1x_mka_get_live_peer_length,
		.body_present = ieee802_1x_mka_live_peer_body_present
	},

	/* Potential Peer List parameter set */
	{
		.body_tx      = ieee802_1x_mka_encode_potential_peer_body,
		.body_rx      = ieee802_1x_mka_decode_potential_peer_body,
		.body_length  = ieee802_1x_mka_get_potential_peer_length,
		.body_present = ieee802_1x_mka_potential_peer_body_present
	},

	/* MACsec SAK Use parameter set */
	{
		.body_tx      = ieee802_1x_mka_encode_sak_use_body,
		.body_rx      = ieee802_1x_mka_decode_sak_use_body,
		.body_length  = ieee802_1x_mka_get_sak_use_length,
		.body_present = ieee802_1x_mka_sak_use_body_present
	},

	/* Distributed SAK parameter set */
	{
		.body_tx      = ieee802_1x_mka_encode_dist_sak_body,
		.body_rx      = ieee802_1x_mka_decode_dist_sak_body,
		.body_length  = ieee802_1x_mka_get_dist_sak_length,
		.body_present = ieee802_1x_mka_dist_sak_body_present
	},

	/* Distribute CAK parameter set */
	{
		.body_tx      = NULL,
		.body_rx      = ieee802_1x_mka_decode_dist_cak_body,
		.body_length  = NULL,
		.body_present = NULL
	},

	/* KMD parameter set */
	{
		.body_tx      = NULL,
		.body_rx      = ieee802_1x_mka_decode_kmd_body,
		.body_length  = NULL,
		.body_present = NULL
	},

	/* Announcement parameter set */
	{
		.body_tx      = NULL,
		.body_rx      = ieee802_1x_mka_decode_announce_body,
		.body_length  = NULL,
		.body_present = NULL
	},

	/* ICV Indicator parameter set */
	{
		.body_tx      = ieee802_1x_mka_encode_icv_body,
		.body_rx      = NULL,
		.body_length  = ieee802_1x_mka_get_icv_length,
		.body_present = ieee802_1x_mka_icv_body_present
	},
};


/**
 * ieee802_1x_kay_use_data_key - Take reference on a key
 */
static void ieee802_1x_kay_use_data_key(struct data_key *pkey)
{
	pkey->user++;
}


/**
 * ieee802_1x_kay_deinit_data_key - Release reference on a key and
 * free if there are no remaining users
 */
static void ieee802_1x_kay_deinit_data_key(struct data_key *pkey)
{
	if (!pkey)
		return;

	pkey->user--;
	if (pkey->user > 1)
		return;

	os_free(pkey->key);
	os_free(pkey);
}


/**
 * ieee802_1x_kay_generate_new_sak -
 */
static int
ieee802_1x_kay_generate_new_sak(struct ieee802_1x_mka_participant *participant)
{
	struct data_key *sa_key = NULL;
	struct ieee802_1x_kay_peer *peer;
	struct ieee802_1x_kay *kay = participant->kay;
	int ctx_len, ctx_offset;
	u8 *context;
	unsigned int key_len;
	u8 *key;
	struct macsec_ciphersuite *cs;

	/* check condition for generating a fresh SAK:
	 * must have one live peer
	 * and MKA life time elapse since last distribution
	 * or potential peer is empty
	 */
	if (dl_list_empty(&participant->live_peers)) {
		wpa_printf(MSG_ERROR,
			   "KaY: Live peers list must not be empty when generating fresh SAK");
		return -1;
	}

	/* FIXME: A fresh SAK not generated until
	 * the live peer list contains at least one peer and
	 * MKA life time has elapsed since the prior SAK was first distributed,
	 * or the Key server's potential peer is empty
	 * but I can't understand the second item, so
	 * here only check first item and ingore
	 *   && (!dl_list_empty(&participant->potential_peers))) {
	 */
	if ((time(NULL) - kay->dist_time) < MKA_LIFE_TIME / 1000) {
		wpa_printf(MSG_ERROR,
			   "KaY: Life time has not elapsed since prior SAK distributed");
		return -1;
	}

	cs = &cipher_suite_tbl[kay->macsec_csindex];
	key_len = cs->sak_len;
	key = os_zalloc(key_len);
	if (!key) {
		wpa_printf(MSG_ERROR, "KaY-%s: Out of memory", __func__);
		return -1;
	}

	ctx_len = key_len + sizeof(kay->dist_kn);
	dl_list_for_each(peer, &participant->live_peers,
			 struct ieee802_1x_kay_peer, list)
		ctx_len += sizeof(peer->mi);
	ctx_len += sizeof(participant->mi);

	context = os_zalloc(ctx_len);
	if (!context)
		goto fail;

	ctx_offset = 0;
	if (os_get_random(context + ctx_offset, key_len) < 0)
		goto fail;

	ctx_offset += key_len;
	dl_list_for_each(peer, &participant->live_peers,
			 struct ieee802_1x_kay_peer, list) {
		os_memcpy(context + ctx_offset, peer->mi, sizeof(peer->mi));
		ctx_offset += sizeof(peer->mi);
	}
	os_memcpy(context + ctx_offset, participant->mi,
		  sizeof(participant->mi));
	ctx_offset += sizeof(participant->mi);
	os_memcpy(context + ctx_offset, &kay->dist_kn, sizeof(kay->dist_kn));

	if (key_len == 16 || key_len == 32) {
		if (ieee802_1x_sak_aes_cmac(participant->cak.key,
					    participant->cak.len,
					    context, ctx_len,
					    key, key_len)) {
			wpa_printf(MSG_ERROR, "KaY: Failed to generate SAK");
			goto fail;
		}
	} else {
		wpa_printf(MSG_ERROR, "KaY: SAK Length(%u) not supported",
			   key_len);
		goto fail;
	}
	wpa_hexdump_key(MSG_DEBUG, "KaY: generated new SAK", key, key_len);
	os_free(context);
	context = NULL;

	sa_key = os_zalloc(sizeof(*sa_key));
	if (!sa_key) {
		wpa_printf(MSG_ERROR, "KaY-%s: Out of memory", __func__);
		goto fail;
	}

	sa_key->key = key;
	sa_key->key_len = key_len;
	os_memcpy(sa_key->key_identifier.mi, participant->mi, MI_LEN);
	sa_key->key_identifier.kn = kay->dist_kn;

	sa_key->confidentiality_offset = kay->macsec_confidentiality;
	sa_key->an = kay->dist_an;
	ieee802_1x_kay_init_data_key(sa_key);

	participant->new_key = sa_key;

	ieee802_1x_kay_use_data_key(sa_key);
	dl_list_add(&participant->sak_list, &sa_key->list);

	ieee802_1x_cp_set_ciphersuite(kay->cp, cs->id);
	ieee802_1x_cp_sm_step(kay->cp);
	ieee802_1x_cp_set_offset(kay->cp, kay->macsec_confidentiality);
	ieee802_1x_cp_sm_step(kay->cp);
	ieee802_1x_cp_set_distributedki(kay->cp, &sa_key->key_identifier);
	ieee802_1x_cp_set_distributedan(kay->cp, sa_key->an);
	ieee802_1x_cp_signal_newsak(kay->cp);
	ieee802_1x_cp_sm_step(kay->cp);

	dl_list_for_each(peer, &participant->live_peers,
			 struct ieee802_1x_kay_peer, list)
		peer->sak_used = FALSE;

	kay->dist_kn++;
	kay->dist_an++;
	if (kay->dist_an > 3)
		kay->dist_an = 0;

	kay->dist_time = time(NULL);

	return 0;

fail:
	os_free(key);
	os_free(context);
	return -1;
}


static int compare_priorities(const struct ieee802_1x_kay_peer *peer,
			      const struct ieee802_1x_kay_peer *other)
{
	if (peer->key_server_priority < other->key_server_priority)
		return -1;
	if (other->key_server_priority < peer->key_server_priority)
		return 1;

	return os_memcmp(peer->sci.addr, other->sci.addr, ETH_ALEN);
}


/**
 * ieee802_1x_kay_elect_key_server - elect the key server
 * when to elect: whenever the live peers list changes
 */
static int
ieee802_1x_kay_elect_key_server(struct ieee802_1x_mka_participant *participant)
{
	struct ieee802_1x_kay_peer *peer;
	struct ieee802_1x_kay_peer *key_server = NULL;
	struct ieee802_1x_kay *kay = participant->kay;
	Boolean i_is_key_server;
	int priority_comparison;

	if (participant->is_obliged_key_server) {
		participant->new_sak = TRUE;
		participant->to_dist_sak = FALSE;
		ieee802_1x_cp_set_electedself(kay->cp, TRUE);
		return 0;
	}

	/* elect the key server among the peers */
	dl_list_for_each(peer, &participant->live_peers,
			 struct ieee802_1x_kay_peer, list) {
		if (!peer->is_key_server)
			continue;

		if (!key_server) {
			key_server = peer;
			continue;
		}

		if (compare_priorities(peer, key_server) < 0)
			key_server = peer;
	}

	/* elect the key server between me and the above elected peer */
	i_is_key_server = FALSE;
	if (key_server && participant->can_be_key_server) {
		struct ieee802_1x_kay_peer tmp;

		tmp.key_server_priority = kay->actor_priority;
		os_memcpy(&tmp.sci, &kay->actor_sci, sizeof(tmp.sci));
		priority_comparison = compare_priorities(&tmp, key_server);
		if (priority_comparison < 0) {
			i_is_key_server = TRUE;
		} else if (priority_comparison == 0) {
			wpa_printf(MSG_WARNING,
				   "KaY: Cannot elect key server between me and peer, duplicate MAC detected");
			key_server = NULL;
		}
	} else if (participant->can_be_key_server) {
		i_is_key_server = TRUE;
	}

	if (i_is_key_server) {
		ieee802_1x_cp_set_electedself(kay->cp, TRUE);
		if (!sci_equal(&kay->key_server_sci, &kay->actor_sci)) {
			ieee802_1x_cp_signal_chgdserver(kay->cp);
			ieee802_1x_cp_sm_step(kay->cp);
		}

		participant->is_key_server = TRUE;
		participant->principal = TRUE;
		participant->new_sak = TRUE;
		wpa_printf(MSG_DEBUG, "KaY: I am elected as key server");
		participant->to_dist_sak = FALSE;
		participant->is_elected = TRUE;

		os_memcpy(&kay->key_server_sci, &kay->actor_sci,
			  sizeof(kay->key_server_sci));
		kay->key_server_priority = kay->actor_priority;
	} else if (key_server) {
		wpa_printf(MSG_DEBUG,
			   "KaY: Peer %s was elected as the key server",
			   mi_txt(key_server->mi));
		ieee802_1x_cp_set_electedself(kay->cp, FALSE);
		if (!sci_equal(&kay->key_server_sci, &key_server->sci)) {
			ieee802_1x_cp_signal_chgdserver(kay->cp);
			ieee802_1x_cp_sm_step(kay->cp);
		}

		participant->is_key_server = FALSE;
		participant->principal = TRUE;
		participant->is_elected = TRUE;

		os_memcpy(&kay->key_server_sci, &key_server->sci,
			  sizeof(kay->key_server_sci));
		kay->key_server_priority = key_server->key_server_priority;
	} else {
		participant->principal = FALSE;
		participant->is_key_server = FALSE;
		participant->is_elected = FALSE;
	}

	return 0;
}


/**
 * ieee802_1x_kay_decide_macsec_use - the key server determinate
 *		 how to use MACsec: whether use MACsec and its capability
 * protectFrames will be advised if the key server and one of its live peers are
 * MACsec capable and one of those request MACsec protection
 */
static int
ieee802_1x_kay_decide_macsec_use(
	struct ieee802_1x_mka_participant *participant)
{
	struct ieee802_1x_kay *kay = participant->kay;
	struct ieee802_1x_kay_peer *peer;
	enum macsec_cap less_capability;
	Boolean has_peer;

	if (!participant->is_key_server)
		return -1;

	/* key server self is MACsec-desired and requesting MACsec */
	if (!kay->macsec_desired) {
		participant->advised_desired = FALSE;
		return -1;
	}
	if (kay->macsec_capable == MACSEC_CAP_NOT_IMPLEMENTED) {
		participant->advised_desired = FALSE;
		return -1;
	}
	less_capability = kay->macsec_capable;

	/* at least one of peers is MACsec-desired and requesting MACsec */
	has_peer = FALSE;
	dl_list_for_each(peer, &participant->live_peers,
			 struct ieee802_1x_kay_peer, list) {
		if (!peer->macsec_desired)
			continue;

		if (peer->macsec_capability == MACSEC_CAP_NOT_IMPLEMENTED)
			continue;

		less_capability = (less_capability < peer->macsec_capability) ?
			less_capability : peer->macsec_capability;
		has_peer = TRUE;
	}

	if (has_peer) {
		participant->advised_desired = TRUE;
		participant->advised_capability = less_capability;
		kay->authenticated = FALSE;
		kay->secured = TRUE;
		kay->failed = FALSE;
		ieee802_1x_cp_connect_secure(kay->cp);
		ieee802_1x_cp_sm_step(kay->cp);
	} else {
		participant->advised_desired = FALSE;
		participant->advised_capability = MACSEC_CAP_NOT_IMPLEMENTED;
		participant->to_use_sak = FALSE;
		kay->authenticated = TRUE;
		kay->secured = FALSE;
		kay->failed = FALSE;
		kay->ltx_kn = 0;
		kay->ltx_an = 0;
		kay->lrx_kn = 0;
		kay->lrx_an = 0;
		kay->otx_kn = 0;
		kay->otx_an = 0;
		kay->orx_kn = 0;
		kay->orx_an = 0;
		ieee802_1x_cp_connect_authenticated(kay->cp);
		ieee802_1x_cp_sm_step(kay->cp);
	}

	return 0;
}

static const u8 pae_group_addr[ETH_ALEN] = {
	0x01, 0x80, 0xc2, 0x00, 0x00, 0x03
};


/**
 * ieee802_1x_kay_encode_mkpdu -
 */
static int
ieee802_1x_kay_encode_mkpdu(struct ieee802_1x_mka_participant *participant,
			    struct wpabuf *pbuf)
{
	unsigned int i;
	struct ieee8023_hdr *ether_hdr;
	struct ieee802_1x_hdr *eapol_hdr;

	ether_hdr = wpabuf_put(pbuf, sizeof(*ether_hdr));
	os_memcpy(ether_hdr->dest, pae_group_addr, sizeof(ether_hdr->dest));
	os_memcpy(ether_hdr->src, participant->kay->actor_sci.addr,
		  sizeof(ether_hdr->dest));
	ether_hdr->ethertype = host_to_be16(ETH_P_EAPOL);
	wpa_printf(MSG_DEBUG, "KaY: Ethernet header: DA=" MACSTR " SA=" MACSTR
		   " Ethertype=0x%x",
		   MAC2STR(ether_hdr->dest), MAC2STR(ether_hdr->src),
		   be_to_host16(ether_hdr->ethertype));

	eapol_hdr = wpabuf_put(pbuf, sizeof(*eapol_hdr));
	eapol_hdr->version = EAPOL_VERSION;
	eapol_hdr->type = IEEE802_1X_TYPE_EAPOL_MKA;
	eapol_hdr->length = host_to_be16(wpabuf_tailroom(pbuf));
	wpa_printf(MSG_DEBUG,
		   "KaY: Common EAPOL PDU structure: Protocol Version=%u Packet Type=%u Packet Body Length=%u",
		   eapol_hdr->version, eapol_hdr->type,
		   be_to_host16(eapol_hdr->length));

	for (i = 0; i < ARRAY_SIZE(mka_body_handler); i++) {
		if (mka_body_handler[i].body_present &&
		    mka_body_handler[i].body_present(participant)) {
			if (mka_body_handler[i].body_tx(participant, pbuf))
				return -1;
		}
	}

	return 0;
}


/**
 * ieee802_1x_participant_send_mkpdu -
 */
static int
ieee802_1x_participant_send_mkpdu(
	struct ieee802_1x_mka_participant *participant)
{
	struct wpabuf *buf;
	struct ieee802_1x_kay *kay = participant->kay;
	size_t length = 0;
	unsigned int i;

	wpa_printf(MSG_DEBUG, "KaY: Encode and send an MKPDU (ifname=%s)",
		   kay->if_name);
	length += sizeof(struct ieee802_1x_hdr) + sizeof(struct ieee8023_hdr);
	for (i = 0; i < ARRAY_SIZE(mka_body_handler); i++) {
		if (mka_body_handler[i].body_present &&
		    mka_body_handler[i].body_present(participant))
			length += mka_body_handler[i].body_length(participant);
	}

	buf = wpabuf_alloc(length);
	if (!buf) {
		wpa_printf(MSG_ERROR, "KaY: out of memory");
		return -1;
	}

	if (ieee802_1x_kay_encode_mkpdu(participant, buf)) {
		wpa_printf(MSG_ERROR, "KaY: encode mkpdu fail");
		return -1;
	}

	wpa_hexdump_buf(MSG_MSGDUMP, "KaY: Outgoing MKPDU", buf);
	l2_packet_send(kay->l2_mka, NULL, 0, wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);

	kay->active = TRUE;
	participant->active = TRUE;

	return 0;
}


static void ieee802_1x_kay_deinit_transmit_sa(struct transmit_sa *psa);

static void ieee802_1x_delete_transmit_sa(struct ieee802_1x_kay *kay,
					  struct transmit_sa *sa)
{
	secy_disable_transmit_sa(kay, sa);
	secy_delete_transmit_sa(kay, sa);
	ieee802_1x_kay_deinit_transmit_sa(sa);
}


/**
 * ieee802_1x_participant_timer -
 */
static void ieee802_1x_participant_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct ieee802_1x_mka_participant *participant;
	struct ieee802_1x_kay *kay;
	struct ieee802_1x_kay_peer *peer, *pre_peer;
	time_t now = time(NULL);
	Boolean lp_changed;
	struct receive_sc *rxsc, *pre_rxsc;
	struct transmit_sa *txsa, *pre_txsa;

	participant = (struct ieee802_1x_mka_participant *)eloop_ctx;
	kay = participant->kay;
	wpa_printf(MSG_DEBUG, "KaY: Participant timer (ifname=%s)",
		   kay->if_name);
	if (participant->cak_life) {
		if (now > participant->cak_life)
			goto delete_mka;
	}

	/* should delete MKA instance if there are not live peers
	 * when the MKA life elapsed since its creating */
	if (participant->mka_life) {
		if (dl_list_empty(&participant->live_peers)) {
			if (now > participant->mka_life)
				goto delete_mka;
		} else {
			participant->mka_life = 0;
		}
	}

	lp_changed = FALSE;
	dl_list_for_each_safe(peer, pre_peer, &participant->live_peers,
			      struct ieee802_1x_kay_peer, list) {
		if (now > peer->expire) {
			wpa_printf(MSG_DEBUG, "KaY: Live peer removed");
			wpa_hexdump(MSG_DEBUG, "\tMI: ", peer->mi,
				    sizeof(peer->mi));
			wpa_printf(MSG_DEBUG, "\tMN: %d", peer->mn);
			dl_list_for_each_safe(rxsc, pre_rxsc,
					      &participant->rxsc_list,
					      struct receive_sc, list) {
				if (sci_equal(&rxsc->sci, &peer->sci)) {
					ieee802_1x_kay_deinit_receive_sc(
						participant, rxsc);
				}
			}
			dl_list_del(&peer->list);
			os_free(peer);
			lp_changed = TRUE;
		}
	}

	if (lp_changed) {
		if (dl_list_empty(&participant->live_peers)) {
			participant->advised_desired = FALSE;
			participant->advised_capability =
				MACSEC_CAP_NOT_IMPLEMENTED;
			participant->to_use_sak = FALSE;
			participant->ltx = FALSE;
			participant->lrx = FALSE;
			participant->otx = FALSE;
			participant->orx = FALSE;
			participant->is_key_server = FALSE;
			participant->is_elected = FALSE;
			kay->authenticated = FALSE;
			kay->secured = FALSE;
			kay->failed = FALSE;
			kay->ltx_kn = 0;
			kay->ltx_an = 0;
			kay->lrx_kn = 0;
			kay->lrx_an = 0;
			kay->otx_kn = 0;
			kay->otx_an = 0;
			kay->orx_kn = 0;
			kay->orx_an = 0;
			dl_list_for_each_safe(txsa, pre_txsa,
					      &participant->txsc->sa_list,
					      struct transmit_sa, list) {
				ieee802_1x_delete_transmit_sa(kay, txsa);
			}

			ieee802_1x_cp_connect_pending(kay->cp);
			ieee802_1x_cp_sm_step(kay->cp);
		} else {
			ieee802_1x_kay_elect_key_server(participant);
			ieee802_1x_kay_decide_macsec_use(participant);
		}
	}

	dl_list_for_each_safe(peer, pre_peer, &participant->potential_peers,
			      struct ieee802_1x_kay_peer, list) {
		if (now > peer->expire) {
			wpa_printf(MSG_DEBUG, "KaY: Potential peer removed");
			wpa_hexdump(MSG_DEBUG, "\tMI: ", peer->mi,
				    sizeof(peer->mi));
			wpa_printf(MSG_DEBUG, "\tMN: %d", peer->mn);
			dl_list_del(&peer->list);
			os_free(peer);
		}
	}

	if (participant->new_sak && participant->is_key_server) {
		if (!ieee802_1x_kay_generate_new_sak(participant))
			participant->to_dist_sak = TRUE;

		participant->new_sak = FALSE;
	}

	if (participant->retry_count < MAX_RETRY_CNT ||
	    participant->mode == PSK) {
		ieee802_1x_participant_send_mkpdu(participant);
		participant->retry_count++;
	}

	eloop_register_timeout(kay->mka_hello_time / 1000, 0,
			       ieee802_1x_participant_timer,
			       participant, NULL);

	return;

delete_mka:
	kay->authenticated = FALSE;
	kay->secured = FALSE;
	kay->failed = TRUE;
	ieee802_1x_kay_delete_mka(kay, &participant->ckn);
}


/**
 * ieee802_1x_kay_init_transmit_sa -
 */
static struct transmit_sa *
ieee802_1x_kay_init_transmit_sa(struct transmit_sc *psc, u8 an, u32 next_PN,
				struct data_key *key)
{
	struct transmit_sa *psa;

	key->tx_latest = TRUE;
	key->rx_latest = TRUE;

	psa = os_zalloc(sizeof(*psa));
	if (!psa) {
		wpa_printf(MSG_ERROR, "%s: out of memory", __func__);
		return NULL;
	}

	if (key->confidentiality_offset >= CONFIDENTIALITY_OFFSET_0 &&
	    key->confidentiality_offset <= CONFIDENTIALITY_OFFSET_50)
		psa->confidentiality = TRUE;
	else
		psa->confidentiality = FALSE;

	psa->an = an;
	ieee802_1x_kay_use_data_key(key);
	psa->pkey = key;
	psa->next_pn = next_PN;
	psa->sc = psc;

	os_get_time(&psa->created_time);
	psa->in_use = FALSE;

	dl_list_add(&psc->sa_list, &psa->list);
	wpa_printf(MSG_DEBUG,
		   "KaY: Create transmit SA(an: %hhu, next_pn: %u) of SC",
		   an, next_PN);

	return psa;
}


/**
 * ieee802_1x_kay_deinit_transmit_sa -
 */
static void ieee802_1x_kay_deinit_transmit_sa(struct transmit_sa *psa)
{
	ieee802_1x_kay_deinit_data_key(psa->pkey);
	psa->pkey = NULL;
	wpa_printf(MSG_DEBUG,
		   "KaY: Delete transmit SA(an: %hhu) of SC",
		   psa->an);
	dl_list_del(&psa->list);
	os_free(psa);
}


/**
 * init_transmit_sc -
 */
static struct transmit_sc *
ieee802_1x_kay_init_transmit_sc(const struct ieee802_1x_mka_sci *sci)
{
	struct transmit_sc *psc;

	psc = os_zalloc(sizeof(*psc));
	if (!psc) {
		wpa_printf(MSG_ERROR, "%s: out of memory", __func__);
		return NULL;
	}
	os_memcpy(&psc->sci, sci, sizeof(psc->sci));

	os_get_time(&psc->created_time);
	psc->transmitting = FALSE;
	psc->encoding_sa = FALSE;
	psc->enciphering_sa = FALSE;

	dl_list_init(&psc->sa_list);
	wpa_printf(MSG_DEBUG, "KaY: Create transmit SC - SCI: %s",
		   sci_txt(&psc->sci));

	return psc;
}


/**
 * ieee802_1x_kay_deinit_transmit_sc -
 */
static void
ieee802_1x_kay_deinit_transmit_sc(
	struct ieee802_1x_mka_participant *participant, struct transmit_sc *psc)
{
	struct transmit_sa *psa, *tmp;

	wpa_printf(MSG_DEBUG, "KaY: Delete transmit SC");
	dl_list_for_each_safe(psa, tmp, &psc->sa_list, struct transmit_sa, list)
		ieee802_1x_delete_transmit_sa(participant->kay, psa);

	secy_delete_transmit_sc(participant->kay, psc);
	os_free(psc);
}


/****************** Interface between CP and KAY *********************/
/**
 * ieee802_1x_kay_set_latest_sa_attr -
 */
int ieee802_1x_kay_set_latest_sa_attr(struct ieee802_1x_kay *kay,
				      struct ieee802_1x_mka_ki *lki, u8 lan,
				      Boolean ltx, Boolean lrx)
{
	struct ieee802_1x_mka_participant *principal;

	principal = ieee802_1x_kay_get_principal_participant(kay);
	if (!principal)
		return -1;

	if (!lki)
		os_memset(&principal->lki, 0, sizeof(principal->lki));
	else
		os_memcpy(&principal->lki, lki, sizeof(principal->lki));

	principal->lan = lan;
	principal->ltx = ltx;
	principal->lrx = lrx;
	if (!lki) {
		kay->ltx_kn = 0;
		kay->lrx_kn = 0;
	} else {
		kay->ltx_kn = lki->kn;
		kay->lrx_kn = lki->kn;
	}
	kay->ltx_an = lan;
	kay->lrx_an = lan;

	return 0;
}


/**
 * ieee802_1x_kay_set_old_sa_attr -
 */
int ieee802_1x_kay_set_old_sa_attr(struct ieee802_1x_kay *kay,
				   struct ieee802_1x_mka_ki *oki,
				   u8 oan, Boolean otx, Boolean orx)
{
	struct ieee802_1x_mka_participant *principal;

	principal = ieee802_1x_kay_get_principal_participant(kay);
	if (!principal)
		return -1;

	if (!oki)
		os_memset(&principal->oki, 0, sizeof(principal->oki));
	else
		os_memcpy(&principal->oki, oki, sizeof(principal->oki));

	principal->oan = oan;
	principal->otx = otx;
	principal->orx = orx;

	if (!oki) {
		kay->otx_kn = 0;
		kay->orx_kn = 0;
	} else {
		kay->otx_kn = oki->kn;
		kay->orx_kn = oki->kn;
	}
	kay->otx_an = oan;
	kay->orx_an = oan;

	return 0;
}


static struct transmit_sa * lookup_txsa_by_an(struct transmit_sc *txsc, u8 an)
{
	struct transmit_sa *txsa;

	dl_list_for_each(txsa, &txsc->sa_list, struct transmit_sa, list) {
		if (txsa->an == an)
			return txsa;
	}

	return NULL;
}


static struct receive_sa * lookup_rxsa_by_an(struct receive_sc *rxsc, u8 an)
{
	struct receive_sa *rxsa;

	dl_list_for_each(rxsa, &rxsc->sa_list, struct receive_sa, list) {
		if (rxsa->an == an)
			return rxsa;
	}

	return NULL;
}


/**
 * ieee802_1x_kay_create_sas -
 */
int ieee802_1x_kay_create_sas(struct ieee802_1x_kay *kay,
			      struct ieee802_1x_mka_ki *lki)
{
	struct data_key *sa_key, *latest_sak;
	struct ieee802_1x_mka_participant *principal;
	struct receive_sc *rxsc;
	struct receive_sa *rxsa;
	struct transmit_sa *txsa;

	principal = ieee802_1x_kay_get_principal_participant(kay);
	if (!principal)
		return -1;

	latest_sak = NULL;
	dl_list_for_each(sa_key, &principal->sak_list, struct data_key, list) {
		if (is_ki_equal(&sa_key->key_identifier, lki)) {
			sa_key->rx_latest = TRUE;
			sa_key->tx_latest = TRUE;
			latest_sak = sa_key;
			principal->to_use_sak = TRUE;
		} else {
			sa_key->rx_latest = FALSE;
			sa_key->tx_latest = FALSE;
		}
	}
	if (!latest_sak) {
		wpa_printf(MSG_ERROR, "KaY: lki related sak not found");
		return -1;
	}

	dl_list_for_each(rxsc, &principal->rxsc_list, struct receive_sc, list) {
		while ((rxsa = lookup_rxsa_by_an(rxsc, latest_sak->an)) != NULL)
			ieee802_1x_delete_receive_sa(kay, rxsa);

		rxsa = ieee802_1x_kay_init_receive_sa(rxsc, latest_sak->an, 1,
						      latest_sak);
		if (!rxsa)
			return -1;

		secy_create_receive_sa(kay, rxsa);
	}

	while ((txsa = lookup_txsa_by_an(principal->txsc, latest_sak->an)) !=
	       NULL)
		ieee802_1x_delete_transmit_sa(kay, txsa);

	txsa = ieee802_1x_kay_init_transmit_sa(principal->txsc, latest_sak->an,
					       latest_sak->next_pn ?
					       latest_sak->next_pn : 1,
					       latest_sak);
	if (!txsa)
		return -1;

	secy_create_transmit_sa(kay, txsa);



	return 0;
}


/**
 * ieee802_1x_kay_delete_sas -
 */
int ieee802_1x_kay_delete_sas(struct ieee802_1x_kay *kay,
			      struct ieee802_1x_mka_ki *ki)
{
	struct data_key *sa_key, *pre_key;
	struct transmit_sa *txsa, *pre_txsa;
	struct receive_sa *rxsa, *pre_rxsa;
	struct receive_sc *rxsc;
	struct ieee802_1x_mka_participant *principal;

	wpa_printf(MSG_DEBUG, "KaY: Entry into %s", __func__);
	principal = ieee802_1x_kay_get_principal_participant(kay);
	if (!principal)
		return -1;

	/* remove the transmit sa */
	dl_list_for_each_safe(txsa, pre_txsa, &principal->txsc->sa_list,
			      struct transmit_sa, list) {
		if (is_ki_equal(&txsa->pkey->key_identifier, ki))
			ieee802_1x_delete_transmit_sa(kay, txsa);
	}

	/* remove the receive sa */
	dl_list_for_each(rxsc, &principal->rxsc_list, struct receive_sc, list) {
		dl_list_for_each_safe(rxsa, pre_rxsa, &rxsc->sa_list,
				      struct receive_sa, list) {
			if (is_ki_equal(&rxsa->pkey->key_identifier, ki))
				ieee802_1x_delete_receive_sa(kay, rxsa);
		}
	}

	/* remove the sak */
	dl_list_for_each_safe(sa_key, pre_key, &principal->sak_list,
			      struct data_key, list) {
		if (is_ki_equal(&sa_key->key_identifier, ki)) {
			if (principal->new_key == sa_key)
				principal->new_key = NULL;
			dl_list_del(&sa_key->list);
			ieee802_1x_kay_deinit_data_key(sa_key);
			break;
		}
	}

	return 0;
}


/**
 * ieee802_1x_kay_enable_tx_sas -
 */
int ieee802_1x_kay_enable_tx_sas(struct ieee802_1x_kay *kay,
				 struct ieee802_1x_mka_ki *lki)
{
	struct ieee802_1x_mka_participant *principal;
	struct transmit_sa *txsa;

	principal = ieee802_1x_kay_get_principal_participant(kay);
	if (!principal)
		return -1;

	dl_list_for_each(txsa, &principal->txsc->sa_list, struct transmit_sa,
			 list) {
		if (is_ki_equal(&txsa->pkey->key_identifier, lki)) {
			txsa->in_use = TRUE;
			secy_enable_transmit_sa(kay, txsa);
			ieee802_1x_cp_set_usingtransmitas(
				principal->kay->cp, TRUE);
			ieee802_1x_cp_sm_step(principal->kay->cp);
		}
	}

	return 0;
}


/**
 * ieee802_1x_kay_enable_rx_sas -
 */
int ieee802_1x_kay_enable_rx_sas(struct ieee802_1x_kay *kay,
				 struct ieee802_1x_mka_ki *lki)
{
	struct ieee802_1x_mka_participant *principal;
	struct receive_sa *rxsa;
	struct receive_sc *rxsc;

	principal = ieee802_1x_kay_get_principal_participant(kay);
	if (!principal)
		return -1;

	dl_list_for_each(rxsc, &principal->rxsc_list, struct receive_sc, list) {
		dl_list_for_each(rxsa, &rxsc->sa_list, struct receive_sa, list)
		{
			if (is_ki_equal(&rxsa->pkey->key_identifier, lki)) {
				rxsa->in_use = TRUE;
				secy_enable_receive_sa(kay, rxsa);
				ieee802_1x_cp_set_usingreceivesas(
					principal->kay->cp, TRUE);
				ieee802_1x_cp_sm_step(principal->kay->cp);
			}
		}
	}

	return 0;
}


/**
 * ieee802_1x_kay_enable_new_info -
 */
int ieee802_1x_kay_enable_new_info(struct ieee802_1x_kay *kay)
{
	struct ieee802_1x_mka_participant *principal;

	principal = ieee802_1x_kay_get_principal_participant(kay);
	if (!principal)
		return -1;

	if (principal->retry_count < MAX_RETRY_CNT || principal->mode == PSK) {
		ieee802_1x_participant_send_mkpdu(principal);
		principal->retry_count++;
	}

	return 0;
}


/**
 * ieee802_1x_kay_mkpdu_sanity_check -
 * Sanity checks specified in IEEE Std 802.1X-2010, 11.11.2 (Validation of
 * MKPDUs)
 */
static int ieee802_1x_kay_mkpdu_sanity_check(struct ieee802_1x_kay *kay,
					     const u8 *buf, size_t len)
{
	struct ieee8023_hdr *eth_hdr;
	struct ieee802_1x_hdr *eapol_hdr;
	struct ieee802_1x_mka_hdr *mka_hdr;
	struct ieee802_1x_mka_basic_body *body;
	size_t mka_msg_len;
	struct ieee802_1x_mka_participant *participant;
	size_t body_len;
	size_t ckn_len;
	u8 icv[MAX_ICV_LEN];
	const u8 *msg_icv;

	/* len > eth+eapol header already verified in kay_l2_receive();
	 * likewise, eapol_hdr->length validated there */
	eth_hdr = (struct ieee8023_hdr *) buf;
	eapol_hdr = (struct ieee802_1x_hdr *) (eth_hdr + 1);
	mka_hdr = (struct ieee802_1x_mka_hdr *) (eapol_hdr + 1);

	wpa_printf(MSG_DEBUG, "KaY: Ethernet header: DA=" MACSTR " SA=" MACSTR
		   " Ethertype=0x%x",
		   MAC2STR(eth_hdr->dest), MAC2STR(eth_hdr->src),
		   be_to_host16(eth_hdr->ethertype));

	/* the destination address shall not be an individual address */
	if (os_memcmp(eth_hdr->dest, pae_group_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG,
			   "KaY: ethernet destination address is not PAE group address");
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "KaY: Common EAPOL PDU structure: Protocol Version=%u Packet Type=%u Packet Body Length=%u",
		   eapol_hdr->version, eapol_hdr->type,
		   be_to_host16(eapol_hdr->length));

	/* MKPDU shall not be less than 32 octets */
	mka_msg_len = be_to_host16(eapol_hdr->length);
	if (mka_msg_len < 32) {
		wpa_printf(MSG_DEBUG, "KaY: MKPDU is less than 32 octets");
		return -1;
	}
	/* MKPDU shall be a multiple of 4 octets */
	if ((mka_msg_len % 4) != 0) {
		wpa_printf(MSG_DEBUG,
			   "KaY: MKPDU is not multiple of 4 octets");
		return -1;
	}

	wpa_hexdump(MSG_MSGDUMP, "KaY: EAPOL-MKA Packet Body (MKPDU)",
		    mka_hdr, mka_msg_len);

	/* Room for body_len already verified in kay_l2_receive() */
	body = (struct ieee802_1x_mka_basic_body *) mka_hdr;
	body_len = get_mka_param_body_len(body);
	/* EAPOL-MKA body should comprise basic parameter set and ICV */
	if (mka_msg_len < MKA_HDR_LEN + body_len + DEFAULT_ICV_LEN) {
		wpa_printf(MSG_ERROR,
			   "KaY: Received EAPOL-MKA Packet Body Length (%zu bytes) is less than the Basic Parameter Set Header Length (%zu bytes) + the Basic Parameter Set Body Length (%zu bytes) + %d bytes of ICV",
			   mka_msg_len, MKA_HDR_LEN,
			   body_len, DEFAULT_ICV_LEN);
		return -1;
	}

	if (body_len < sizeof(struct ieee802_1x_mka_basic_body) - MKA_HDR_LEN) {
		wpa_printf(MSG_DEBUG, "KaY: Too small body length %zu",
			   body_len);
		return -1;
	}
	ckn_len = body_len -
		(sizeof(struct ieee802_1x_mka_basic_body) - MKA_HDR_LEN);
	if (ckn_len < 1 || ckn_len > MAX_CKN_LEN) {
		wpa_printf(MSG_WARNING,
			   "KaY: Received EAPOL-MKA CKN Length (%zu bytes) is out of range (<= %u bytes)",
			   ckn_len, MAX_CKN_LEN);
		return -1;
	}

	ieee802_1x_mka_dump_basic_body(body);

	/* CKN should be owned by I */
	participant = ieee802_1x_kay_get_participant(kay, body->ckn, ckn_len);
	if (!participant) {
		wpa_printf(MSG_DEBUG, "KaY: CKN is not included in my CA");
		return -1;
	}

	/* algorithm agility check */
	if (os_memcmp(body->algo_agility, mka_algo_agility,
		      sizeof(body->algo_agility)) != 0) {
		wpa_printf(MSG_INFO,
			   "KaY: Peer's algorithm agility (%s) not supported",
			   algo_agility_txt(body->algo_agility));
		return -1;
	}

	/* ICV check */
	/*
	 * The ICV will comprise the final octets of the packet body, whatever
	 * its size, not the fixed length 16 octets, indicated by the EAPOL
	 * packet body length.
	 */
	if (len < mka_alg_tbl[kay->mka_algindex].icv_len ||
	    mka_alg_tbl[kay->mka_algindex].icv_hash(
		    participant->ick.key, participant->ick.len,
		    buf, len - mka_alg_tbl[kay->mka_algindex].icv_len, icv)) {
		wpa_printf(MSG_ERROR, "KaY: Failed to calculate ICV");
		return -1;
	}

	msg_icv = ieee802_1x_mka_decode_icv_body(participant,
						 (const u8 *) mka_hdr,
						 mka_msg_len);
	if (!msg_icv) {
		wpa_printf(MSG_WARNING, "KaY: No ICV in MKPDU - ignore it");
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "KaY: Received ICV",
		    msg_icv, mka_alg_tbl[kay->mka_algindex].icv_len);
	if (os_memcmp_const(msg_icv, icv,
			    mka_alg_tbl[kay->mka_algindex].icv_len) != 0) {
		wpa_printf(MSG_WARNING,
			   "KaY: Computed ICV is not equal to Received ICV");
		wpa_hexdump(MSG_DEBUG, "KaY: Calculated ICV",
			    icv, mka_alg_tbl[kay->mka_algindex].icv_len);
		return -1;
	}

	return 0;
}


/**
 * ieee802_1x_kay_decode_mkpdu -
 */
static int ieee802_1x_kay_decode_mkpdu(struct ieee802_1x_kay *kay,
				       const u8 *buf, size_t len)
{
	struct ieee802_1x_mka_participant *participant;
	struct ieee802_1x_mka_hdr *hdr;
	struct ieee802_1x_kay_peer *peer;
	size_t body_len;
	size_t left_len;
	u8 body_type;
	int i;
	const u8 *pos;
	Boolean handled[256];
	Boolean bad_sak_use = FALSE; /* Error detected while processing SAK Use
				      * parameter set */
	Boolean i_in_peerlist, is_in_live_peer, is_in_potential_peer;

	wpa_printf(MSG_DEBUG, "KaY: Decode received MKPDU (ifname=%s)",
		   kay->if_name);
	if (ieee802_1x_kay_mkpdu_sanity_check(kay, buf, len))
		return -1;

	/* handle basic parameter set */
	pos = buf + sizeof(struct ieee8023_hdr) + sizeof(struct ieee802_1x_hdr);
	left_len = len - sizeof(struct ieee8023_hdr) -
		sizeof(struct ieee802_1x_hdr);
	participant = ieee802_1x_mka_decode_basic_body(kay, pos, left_len);
	if (!participant)
		return -1;

	/* to skip basic parameter set */
	hdr = (struct ieee802_1x_mka_hdr *) pos;
	body_len = MKA_ALIGN_LENGTH(get_mka_param_body_len(hdr));
	if (left_len < body_len + MKA_HDR_LEN)
		return -1;
	pos += body_len + MKA_HDR_LEN;
	left_len -= body_len + MKA_HDR_LEN;

	/* check i am in the peer's peer list */
	i_in_peerlist = ieee802_1x_mka_i_in_peerlist(participant, pos,
						     left_len);
	is_in_live_peer = ieee802_1x_kay_is_in_live_peer(
		participant, participant->current_peer_id.mi);
	wpa_printf(MSG_DEBUG, "KaY: i_in_peerlist=%s is_in_live_peer=%s",
		   yes_no(i_in_peerlist), yes_no(is_in_live_peer));
	if (i_in_peerlist && !is_in_live_peer) {
		/* accept the peer as live peer */
		is_in_potential_peer = ieee802_1x_kay_is_in_potential_peer(
			participant, participant->current_peer_id.mi);
		if (is_in_potential_peer) {
			if (!ieee802_1x_kay_move_live_peer(
				    participant,
				    participant->current_peer_id.mi,
				    be_to_host32(participant->
						 current_peer_id.mn)))
				return -1;
		} else if (!ieee802_1x_kay_create_live_peer(
				   participant, participant->current_peer_id.mi,
				   be_to_host32(participant->
						current_peer_id.mn))) {
				return -1;
		}

		ieee802_1x_kay_elect_key_server(participant);
		ieee802_1x_kay_decide_macsec_use(participant);
	}

	/*
	 * Handle other parameter set than basic parameter set.
	 * Each parameter set should be present only once.
	 */
	for (i = 0; i < 256; i++)
		handled[i] = FALSE;

	handled[0] = TRUE;
	for (; left_len > MKA_HDR_LEN + DEFAULT_ICV_LEN;
	     pos += body_len + MKA_HDR_LEN,
		     left_len -= body_len + MKA_HDR_LEN) {
		hdr = (struct ieee802_1x_mka_hdr *) pos;
		body_len = MKA_ALIGN_LENGTH(get_mka_param_body_len(hdr));
		body_type = get_mka_param_body_type(hdr);

		if (body_type == MKA_ICV_INDICATOR)
			return 0;

		if (left_len < (MKA_HDR_LEN + body_len + DEFAULT_ICV_LEN)) {
			wpa_printf(MSG_ERROR,
				   "KaY: MKA Peer Packet Body Length (%zu bytes) is less than the Parameter Set Header Length (%zu bytes) + the Parameter Set Body Length (%zu bytes) + %d bytes of ICV",
				   left_len, MKA_HDR_LEN,
				   body_len, DEFAULT_ICV_LEN);
			return -1;
		}

		if (handled[body_type]) {
			wpa_printf(MSG_DEBUG,
				   "KaY: Ignore duplicated body type %u",
				   body_type);
			continue;
		}

		handled[body_type] = TRUE;
		if (body_type < ARRAY_SIZE(mka_body_handler) &&
		    mka_body_handler[body_type].body_rx) {
			if (mka_body_handler[body_type].body_rx
				(participant, pos, left_len) != 0) {
				/* Handle parameter set failure */
				if (body_type != MKA_SAK_USE) {
					wpa_printf(MSG_INFO,
						   "KaY: Discarding Rx MKPDU: decode of parameter set type (%d) failed",
						   body_type);
					return -1;
				}

				/* Ideally DIST-SAK should be processed before
				 * SAK-USE. Unfortunately IEEE Std 802.1X-2010,
				 * 11.11.3 (Encoding MKPDUs) states SAK-USE(3)
				 * must always be encoded before DIST-SAK(4).
				 * Rather than redesigning mka_body_handler so
				 * that it somehow processes DIST-SAK before
				 * SAK-USE, just ignore SAK-USE failures if
				 * DIST-SAK is also present in this MKPDU. */
				bad_sak_use = TRUE;
			}
		} else {
			wpa_printf(MSG_ERROR,
				   "KaY: The body type %d is not supported in this MKA version %d",
				   body_type, MKA_VERSION_ID);
		}
	}

	if (bad_sak_use && !handled[MKA_DISTRIBUTED_SAK]) {
		wpa_printf(MSG_INFO,
			   "KaY: Discarding Rx MKPDU: decode of parameter set type (%d) failed",
			   MKA_SAK_USE);
		if (!reset_participant_mi(participant))
			wpa_printf(MSG_DEBUG, "KaY: Could not update mi");
		else
			wpa_printf(MSG_DEBUG,
				   "KaY: Selected a new random MI: %s",
				   mi_txt(participant->mi));
		return -1;
	}

	/* Detect missing parameter sets */
	peer = ieee802_1x_kay_get_live_peer(participant,
					    participant->current_peer_id.mi);
	if (peer) {
		/* MKPDU is from live peer */
		if (!handled[MKA_SAK_USE]) {
			/* Once a live peer starts sending SAK-USE, it should be
			 * sent every time. */
			if (peer->sak_used) {
				wpa_printf(MSG_INFO,
					   "KaY: Discarding Rx MKPDU: Live Peer stopped sending SAK-USE");
				return -1;
			}

			/* Live peer is probably hung if it hasn't sent SAK-USE
			 * after a reasonable number of MKPDUs. Drop the MKPDU,
			 * which will eventually force an timeout. */
			if (++peer->missing_sak_use_count >
			    MAX_MISSING_SAK_USE) {
				wpa_printf(MSG_INFO,
					   "KaY: Discarding Rx MKPDU: Live Peer not sending SAK-USE");
				return -1;
			}
		} else {
			peer->missing_sak_use_count = 0;

			/* Only update live peer watchdog after successful
			 * decode of all parameter sets */
			peer->expire = time(NULL) + MKA_LIFE_TIME / 1000;
		}
	} else {
		/* MKPDU is from new or potential peer */
		peer = ieee802_1x_kay_get_peer(participant,
					       participant->current_peer_id.mi);
		if (!peer) {
			wpa_printf(MSG_DEBUG, "KaY: No peer entry found");
			return -1;
		}

		/* Do not update potential peer watchdog. Per IEEE Std
		 * 802.1X-2010, 9.4.3, potential peers need to show liveness by
		 * including our MI/MN in their transmitted MKPDU (within
		 * potential or live parameter sets). Whena potential peer does
		 * include our MI/MN in an MKPDU, we respond by moving the peer
		 * from 'potential_peers' to 'live_peers'. */
	}

	kay->active = TRUE;
	participant->retry_count = 0;
	participant->active = TRUE;

	return 0;
}



static void kay_l2_receive(void *ctx, const u8 *src_addr, const u8 *buf,
			   size_t len)
{
	struct ieee802_1x_kay *kay = ctx;
	struct ieee8023_hdr *eth_hdr;
	struct ieee802_1x_hdr *eapol_hdr;
	size_t calc_len;

	/* IEEE Std 802.1X-2010, 11.4 (Validation of received EAPOL PDUs) */

	/* must contain at least ieee8023_hdr + ieee802_1x_hdr */
	if (len < sizeof(*eth_hdr) + sizeof(*eapol_hdr)) {
		wpa_printf(MSG_MSGDUMP, "KaY: EAPOL frame too short (%lu)",
			   (unsigned long) len);
		return;
	}

	eth_hdr = (struct ieee8023_hdr *) buf;
	eapol_hdr = (struct ieee802_1x_hdr *) (eth_hdr + 1);
	calc_len = sizeof(*eth_hdr) + sizeof(*eapol_hdr) +
		be_to_host16(eapol_hdr->length);
	if (len < calc_len) {
		wpa_printf(MSG_MSGDUMP, "KaY: EAPOL MPDU is invalid: (received len %lu, calculated len %lu, EAPOL length %u)",
			   (unsigned long) len,
			   (unsigned long) calc_len,
			   be_to_host16(eapol_hdr->length));
		return;
	}
	if (len > calc_len) {
		wpa_hexdump(MSG_DEBUG,
			    "KaY: Ignore extra octets following the Packey Body field",
			    &buf[calc_len], len - calc_len);
		len = calc_len;
	}

	if (eapol_hdr->version < EAPOL_VERSION) {
		wpa_printf(MSG_MSGDUMP, "KaY: version %d does not support MKA",
			   eapol_hdr->version);
		return;
	}
	if (be_to_host16(eth_hdr->ethertype) != ETH_P_PAE ||
	    eapol_hdr->type != IEEE802_1X_TYPE_EAPOL_MKA)
		return; /* ignore other EAPOL types silently here */

	wpa_hexdump(MSG_DEBUG, "KaY: RX EAPOL-MKA", buf, len);
	if (dl_list_empty(&kay->participant_list)) {
		wpa_printf(MSG_ERROR,
			   "KaY: No MKA participant instance - ignore EAPOL-MKA");
		return;
	}

	ieee802_1x_kay_decode_mkpdu(kay, buf, len);
}


/**
 * ieee802_1x_kay_init -
 */
struct ieee802_1x_kay *
ieee802_1x_kay_init(struct ieee802_1x_kay_ctx *ctx, enum macsec_policy policy,
		    Boolean macsec_replay_protect, u32 macsec_replay_window,
		    u16 port, u8 priority, const char *ifname, const u8 *addr)
{
	struct ieee802_1x_kay *kay;

	wpa_printf(MSG_DEBUG, "KaY: Initialize - ifname=%s addr=" MACSTR
		   " port=%u priority=%u",
		   ifname, MAC2STR(addr), port, priority);
	kay = os_zalloc(sizeof(*kay));
	if (!kay) {
		wpa_printf(MSG_ERROR, "KaY-%s: out of memory", __func__);
		os_free(ctx);
		return NULL;
	}

	kay->ctx = ctx;

	kay->enable = TRUE;
	kay->active = FALSE;

	kay->authenticated = FALSE;
	kay->secured = FALSE;
	kay->failed = FALSE;
	kay->policy = policy;

	os_strlcpy(kay->if_name, ifname, IFNAMSIZ);
	os_memcpy(kay->actor_sci.addr, addr, ETH_ALEN);
	kay->actor_sci.port = host_to_be16(port ? port : 0x0001);
	wpa_printf(MSG_DEBUG, "KaY: Generated SCI: %s",
		   sci_txt(&kay->actor_sci));
	kay->actor_priority = priority;

	/* While actor acts as a key server, shall distribute sakey */
	kay->dist_kn = 1;
	kay->dist_an = 0;
	kay->dist_time = 0;

	kay->pn_exhaustion = PENDING_PN_EXHAUSTION;
	kay->macsec_csindex = DEFAULT_CS_INDEX;
	kay->mka_algindex = DEFAULT_MKA_ALG_INDEX;
	kay->mka_version = MKA_VERSION_ID;

	os_memcpy(kay->algo_agility, mka_algo_agility,
		  sizeof(kay->algo_agility));

	dl_list_init(&kay->participant_list);

	if (policy != DO_NOT_SECURE &&
	    secy_get_capability(kay, &kay->macsec_capable) < 0)
		goto error;

	if (policy == DO_NOT_SECURE ||
	    kay->macsec_capable == MACSEC_CAP_NOT_IMPLEMENTED) {
		kay->macsec_capable = MACSEC_CAP_NOT_IMPLEMENTED;
		kay->macsec_desired = FALSE;
		kay->macsec_protect = FALSE;
		kay->macsec_encrypt = FALSE;
		kay->macsec_validate = Disabled;
		kay->macsec_replay_protect = FALSE;
		kay->macsec_replay_window = 0;
		kay->macsec_confidentiality = CONFIDENTIALITY_NONE;
		kay->mka_hello_time = MKA_HELLO_TIME;
	} else {
		kay->macsec_desired = TRUE;
		kay->macsec_protect = TRUE;
		if (kay->macsec_capable >= MACSEC_CAP_INTEG_AND_CONF &&
		    policy == SHOULD_ENCRYPT) {
			kay->macsec_encrypt = TRUE;
			kay->macsec_confidentiality = CONFIDENTIALITY_OFFSET_0;
		} else { /* SHOULD_SECURE */
			kay->macsec_encrypt = FALSE;
			kay->macsec_confidentiality = CONFIDENTIALITY_NONE;
		}
		kay->macsec_validate = Strict;
		kay->macsec_replay_protect = macsec_replay_protect;
		kay->macsec_replay_window = macsec_replay_window;
		kay->mka_hello_time = MKA_HELLO_TIME;
	}

	wpa_printf(MSG_DEBUG, "KaY: state machine created");

	/* Initialize the SecY must be prio to CP, as CP will control SecY */
	if (secy_init_macsec(kay) < 0) {
		wpa_printf(MSG_DEBUG, "KaY: Could not initialize MACsec");
		goto error;
	}

	wpa_printf(MSG_DEBUG, "KaY: secy init macsec done");

	/* init CP */
	kay->cp = ieee802_1x_cp_sm_init(kay);
	if (kay->cp == NULL)
		goto error;

	if (policy == DO_NOT_SECURE) {
		ieee802_1x_cp_connect_authenticated(kay->cp);
		ieee802_1x_cp_sm_step(kay->cp);
	} else {
		kay->l2_mka = l2_packet_init(kay->if_name, NULL, ETH_P_PAE,
					     kay_l2_receive, kay, 1);
		if (kay->l2_mka == NULL) {
			wpa_printf(MSG_WARNING,
				   "KaY: Failed to initialize L2 packet processing for MKA packet");
			goto error;
		}
	}

	return kay;

error:
	ieee802_1x_kay_deinit(kay);
	return NULL;
}


/**
 * ieee802_1x_kay_deinit -
 */
void
ieee802_1x_kay_deinit(struct ieee802_1x_kay *kay)
{
	struct ieee802_1x_mka_participant *participant;

	if (!kay)
		return;

	wpa_printf(MSG_DEBUG, "KaY: state machine removed");

	while (!dl_list_empty(&kay->participant_list)) {
		participant = dl_list_entry(kay->participant_list.next,
					    struct ieee802_1x_mka_participant,
					    list);
		ieee802_1x_kay_delete_mka(kay, &participant->ckn);
	}

	ieee802_1x_cp_sm_deinit(kay->cp);
	secy_deinit_macsec(kay);

	if (kay->l2_mka) {
		l2_packet_deinit(kay->l2_mka);
		kay->l2_mka = NULL;
	}

	os_free(kay->ctx);
	os_free(kay);
}


static const char * mode_txt(enum mka_created_mode mode)
{
	switch (mode) {
	case PSK:
		return "PSK";
	case EAP_EXCHANGE:
		return "EAP";
	}

	return "?";
}


/**
 * ieee802_1x_kay_create_mka -
 */
struct ieee802_1x_mka_participant *
ieee802_1x_kay_create_mka(struct ieee802_1x_kay *kay,
			  const struct mka_key_name *ckn,
			  const struct mka_key *cak, u32 life,
			  enum mka_created_mode mode, Boolean is_authenticator)
{
	struct ieee802_1x_mka_participant *participant;
	unsigned int usecs;

	wpa_printf(MSG_DEBUG,
		   "KaY: Create MKA (ifname=%s mode=%s authenticator=%s)",
		   kay->if_name, mode_txt(mode), yes_no(is_authenticator));

	if (!kay || !ckn || !cak) {
		wpa_printf(MSG_ERROR, "KaY: ckn or cak is null");
		return NULL;
	}

	if (cak->len != 16 && cak->len != 32) {
		wpa_printf(MSG_ERROR, "KaY: Unexpected CAK length %u",
			   (unsigned int) cak->len);
		return NULL;
	}
	if (ckn->len > MAX_CKN_LEN) {
		wpa_printf(MSG_ERROR, "KaY: CKN is out of range (>32 bytes)");
		return NULL;
	}
	if (!kay->enable) {
		wpa_printf(MSG_ERROR, "KaY: Now is at disable state");
		return NULL;
	}

	participant = os_zalloc(sizeof(*participant));
	if (!participant) {
		wpa_printf(MSG_ERROR, "KaY-%s: out of memory", __func__);
		return NULL;
	}

	participant->ckn.len = ckn->len;
	os_memcpy(participant->ckn.name, ckn->name, ckn->len);
	wpa_hexdump(MSG_DEBUG, "KaY: CKN", participant->ckn.name,
		    participant->ckn.len);
	participant->cak.len = cak->len;
	os_memcpy(participant->cak.key, cak->key, cak->len);
	wpa_hexdump_key(MSG_DEBUG, "KaY: CAK", participant->cak.key,
			participant->cak.len);
	if (life)
		participant->cak_life = life + time(NULL);

	switch (mode) {
	case EAP_EXCHANGE:
		if (is_authenticator) {
			participant->is_obliged_key_server = TRUE;
			participant->can_be_key_server = TRUE;
			participant->is_key_server = TRUE;
			participant->principal = TRUE;

			os_memcpy(&kay->key_server_sci, &kay->actor_sci,
				  sizeof(kay->key_server_sci));
			kay->key_server_priority = kay->actor_priority;
			participant->is_elected = TRUE;
		} else {
			participant->is_obliged_key_server = FALSE;
			participant->can_be_key_server = FALSE;
			participant->is_key_server = FALSE;
			participant->is_elected = TRUE;
		}
		break;

	default:
		participant->is_obliged_key_server = FALSE;
		participant->can_be_key_server = TRUE;
		participant->is_key_server = TRUE;
		participant->is_elected = FALSE;
		break;
	}

	participant->cached = FALSE;

	participant->active = FALSE;
	participant->participant = FALSE;
	participant->retain = FALSE;
	participant->activate = DEFAULT;

	if (participant->is_key_server)
		participant->principal = TRUE;

	dl_list_init(&participant->live_peers);
	dl_list_init(&participant->potential_peers);

	participant->retry_count = 0;
	participant->kay = kay;

	if (!reset_participant_mi(participant))
		goto fail;
	wpa_printf(MSG_DEBUG, "KaY: Selected random MI: %s",
		   mi_txt(participant->mi));

	participant->lrx = FALSE;
	participant->ltx = FALSE;
	participant->orx = FALSE;
	participant->otx = FALSE;
	participant->to_dist_sak = FALSE;
	participant->to_use_sak = FALSE;
	participant->new_sak = FALSE;
	dl_list_init(&participant->sak_list);
	participant->new_key = NULL;
	dl_list_init(&participant->rxsc_list);
	participant->txsc = ieee802_1x_kay_init_transmit_sc(&kay->actor_sci);
	secy_cp_control_protect_frames(kay, kay->macsec_protect);
	secy_cp_control_replay(kay, kay->macsec_replay_protect,
			       kay->macsec_replay_window);
	if (secy_create_transmit_sc(kay, participant->txsc))
		goto fail;

	/* to derive KEK from CAK and CKN */
	participant->kek.len = participant->cak.len;
	if (mka_alg_tbl[kay->mka_algindex].kek_trfm(participant->cak.key,
						    participant->cak.len,
						    participant->ckn.name,
						    participant->ckn.len,
						    participant->kek.key,
						    participant->kek.len)) {
		wpa_printf(MSG_ERROR, "KaY: KEK derivation failed");
		goto fail;
	}
	wpa_hexdump_key(MSG_DEBUG, "KaY: Derived KEK",
			participant->kek.key, participant->kek.len);

	/* to derive ICK from CAK and CKN */
	participant->ick.len = participant->cak.len;
	if (mka_alg_tbl[kay->mka_algindex].ick_trfm(participant->cak.key,
						    participant->cak.len,
						    participant->ckn.name,
						    participant->ckn.len,
						    participant->ick.key,
						    participant->ick.len)) {
		wpa_printf(MSG_ERROR, "KaY: ICK derivation failed");
		goto fail;
	}
	wpa_hexdump_key(MSG_DEBUG, "KaY: Derived ICK",
			participant->ick.key, participant->ick.len);

	dl_list_add(&kay->participant_list, &participant->list);

	usecs = os_random() % (kay->mka_hello_time * 1000);
	eloop_register_timeout(0, usecs, ieee802_1x_participant_timer,
			       participant, NULL);

	/* Disable MKA lifetime for PSK mode.
	 * The peer(s) can take a long time to come up, because we
	 * create a "standby" MKA, and we need it to remain live until
	 * some peer appears.
	 */
	if (mode != PSK) {
		participant->mka_life = MKA_LIFE_TIME / 1000 + time(NULL) +
			usecs / 1000000;
	}
	participant->mode = mode;

	return participant;

fail:
	os_free(participant->txsc);
	os_free(participant);
	return NULL;
}


/**
 * ieee802_1x_kay_delete_mka -
 */
void
ieee802_1x_kay_delete_mka(struct ieee802_1x_kay *kay, struct mka_key_name *ckn)
{
	struct ieee802_1x_mka_participant *participant;
	struct ieee802_1x_kay_peer *peer;
	struct data_key *sak;
	struct receive_sc *rxsc;

	if (!kay || !ckn)
		return;

	wpa_printf(MSG_DEBUG, "KaY: participant removed");

	/* get the participant */
	participant = ieee802_1x_kay_get_participant(kay, ckn->name, ckn->len);
	if (!participant) {
		wpa_hexdump(MSG_DEBUG, "KaY: participant is not found",
			    ckn->name, ckn->len);
		return;
	}

	eloop_cancel_timeout(ieee802_1x_participant_timer, participant, NULL);
	dl_list_del(&participant->list);

	/* remove live peer */
	while (!dl_list_empty(&participant->live_peers)) {
		peer = dl_list_entry(participant->live_peers.next,
				     struct ieee802_1x_kay_peer, list);
		dl_list_del(&peer->list);
		os_free(peer);
	}

	/* remove potential peer */
	while (!dl_list_empty(&participant->potential_peers)) {
		peer = dl_list_entry(participant->potential_peers.next,
				     struct ieee802_1x_kay_peer, list);
		dl_list_del(&peer->list);
		os_free(peer);
	}

	/* remove sak */
	while (!dl_list_empty(&participant->sak_list)) {
		sak = dl_list_entry(participant->sak_list.next,
				    struct data_key, list);
		dl_list_del(&sak->list);
		ieee802_1x_kay_deinit_data_key(sak);
	}
	while (!dl_list_empty(&participant->rxsc_list)) {
		rxsc = dl_list_entry(participant->rxsc_list.next,
				     struct receive_sc, list);
		ieee802_1x_kay_deinit_receive_sc(participant, rxsc);
	}
	ieee802_1x_kay_deinit_transmit_sc(participant, participant->txsc);

	os_memset(&participant->cak, 0, sizeof(participant->cak));
	os_memset(&participant->kek, 0, sizeof(participant->kek));
	os_memset(&participant->ick, 0, sizeof(participant->ick));
	os_free(participant);
}


/**
 * ieee802_1x_kay_mka_participate -
 */
void ieee802_1x_kay_mka_participate(struct ieee802_1x_kay *kay,
				    struct mka_key_name *ckn,
				    Boolean status)
{
	struct ieee802_1x_mka_participant *participant;

	if (!kay || !ckn)
		return;

	participant = ieee802_1x_kay_get_participant(kay, ckn->name, ckn->len);
	if (!participant)
		return;

	participant->active = status;
}


/**
 * ieee802_1x_kay_new_sak -
 */
int
ieee802_1x_kay_new_sak(struct ieee802_1x_kay *kay)
{
	struct ieee802_1x_mka_participant *participant;

	if (!kay)
		return -1;

	participant = ieee802_1x_kay_get_principal_participant(kay);
	if (!participant)
		return -1;

	participant->new_sak = TRUE;
	wpa_printf(MSG_DEBUG, "KaY: new SAK signal");

	return 0;
}


/**
 * ieee802_1x_kay_change_cipher_suite -
 */
int
ieee802_1x_kay_change_cipher_suite(struct ieee802_1x_kay *kay,
				   unsigned int cs_index)
{
	struct ieee802_1x_mka_participant *participant;
	enum macsec_cap secy_cap;

	if (!kay)
		return -1;

	if (cs_index >= CS_TABLE_SIZE) {
		wpa_printf(MSG_ERROR,
			   "KaY: Configured cipher suite index is out of range");
		return -1;
	}
	if (kay->macsec_csindex == cs_index)
		return -2;

	if (cs_index == 0)
		kay->macsec_desired = FALSE;

	kay->macsec_csindex = cs_index;
	kay->macsec_capable = cipher_suite_tbl[kay->macsec_csindex].capable;

	if (secy_get_capability(kay, &secy_cap) < 0)
		return -3;

	if (kay->macsec_capable > secy_cap)
		kay->macsec_capable = secy_cap;

	participant = ieee802_1x_kay_get_principal_participant(kay);
	if (participant) {
		wpa_printf(MSG_INFO, "KaY: Cipher Suite changed");
		participant->new_sak = TRUE;
	}

	return 0;
}


#ifdef CONFIG_CTRL_IFACE

/**
 * ieee802_1x_kay_get_status - Get IEEE 802.1X KaY status details
 * @sm: Pointer to KaY allocated with ieee802_1x_kay_init()
 * @buf: Buffer for status information
 * @buflen: Maximum buffer length
 * @verbose: Whether to include verbose status information
 * Returns: Number of bytes written to buf.
 *
 * Query KaY status information. This function fills in a text area with current
 * status information. If the buffer (buf) is not large enough, status
 * information will be truncated to fit the buffer.
 */
int ieee802_1x_kay_get_status(struct ieee802_1x_kay *kay, char *buf,
			      size_t buflen)
{
	char *pos, *end;
	int res, count;
	struct ieee802_1x_mka_participant *p;

	if (!kay)
		return 0;

	pos = buf;
	end = buf + buflen;

	res = os_snprintf(pos, end - pos,
			  "PAE KaY status=%s\n"
			  "Authenticated=%s\n"
			  "Secured=%s\n"
			  "Failed=%s\n"
			  "Actor Priority=%u\n"
			  "Key Server Priority=%u\n"
			  "Is Key Server=%s\n"
			  "Number of Keys Distributed=%u\n"
			  "Number of Keys Received=%u\n"
			  "MKA Hello Time=%u\n",
			  kay->active ? "Active" : "Not-Active",
			  kay->authenticated ? "Yes" : "No",
			  kay->secured ? "Yes" : "No",
			  kay->failed ? "Yes" : "No",
			  kay->actor_priority,
			  kay->key_server_priority,
			  kay->is_key_server ? "Yes" : "No",
			  kay->dist_kn - 1,
			  kay->rcvd_keys,
			  kay->mka_hello_time);
	if (os_snprintf_error(buflen, res))
		return 0;
	pos += res;

	res = os_snprintf(pos, end - pos,
			  "actor_sci=%s\n", sci_txt(&kay->actor_sci));
	if (os_snprintf_error(buflen, res))
		return end - pos;
	pos += res;

	res = os_snprintf(pos, end - pos,
			  "key_server_sci=%s\n", sci_txt(&kay->key_server_sci));
	if (os_snprintf_error(buflen, res))
		return end - pos;
	pos += res;

	count = 0;
	dl_list_for_each(p, &kay->participant_list,
			 struct ieee802_1x_mka_participant, list) {
		char *pos2 = pos;

		res = os_snprintf(pos2, end - pos2, "participant_idx=%d\nckn=",
			count);
		if (os_snprintf_error(buflen, res))
			return end - pos;
		pos2 += res;
		count++;

		pos2 += wpa_snprintf_hex(pos2, end - pos2, p->ckn.name,
					 p->ckn.len);

		res = os_snprintf(pos2, end - pos2,
				  "\nmi=%s\n"
				  "mn=%u\n"
				  "active=%s\n"
				  "participant=%s\n"
				  "retain=%s\n"
				  "live_peers=%u\n"
				  "potential_peers=%u\n"
				  "is_key_server=%s\n"
				  "is_elected=%s\n",
				  mi_txt(p->mi), p->mn,
				  yes_no(p->active),
				  yes_no(p->participant),
				  yes_no(p->retain),
				  dl_list_len(&p->live_peers),
				  dl_list_len(&p->potential_peers),
				  yes_no(p->is_key_server),
				  yes_no(p->is_elected));
		if (os_snprintf_error(buflen, res))
			return end - pos;
		pos2 += res;
		pos = pos2;
	}

	return pos - buf;
}


static const char * true_false(Boolean val)
{
	return val ? "true" : "false";
}


static const char * activate_control_txt(enum activate_ctrl activate)
{
	switch (activate) {
	case DEFAULT:
		return "default";
	case DISABLED:
		return "disabled";
	case ON_OPER_UP:
		return "onOperUp";
	case ALWAYS:
		return "always";
	}

	return "?";
}


static char * mka_mib_peer(struct dl_list *peers, Boolean live, char *buf,
			   char *end)
{
	char *pos = buf;
	struct ieee802_1x_kay_peer *p;
	int res;

	dl_list_for_each(p, peers, struct ieee802_1x_kay_peer, list) {
		res = os_snprintf(pos, end - pos,
				  "ieee8021XKayMkaPeerListMI=%s\n"
				  "ieee8021XKayMkaPeerListMN=%u\n"
				  "ieee8021XKayMkaPeerListType=%u\n"
				  "ieee8021XKayMkaPeerListSCI=%s\n",
				  mi_txt(p->mi),
				  p->mn,
				  live ? 1 : 2,
				  sci_txt(&p->sci));
		if (os_snprintf_error(end - pos, res))
			return pos;
		pos += res;
	}

	return pos;
}


int ieee802_1x_kay_get_mib(struct ieee802_1x_kay *kay, char *buf,
			   size_t buflen)
{
	char *pos, *end;
	int res;
	struct ieee802_1x_mka_participant *p;

	if (!kay)
		return 0;

	pos = buf;
	end = buf + buflen;

	dl_list_for_each(p, &kay->participant_list,
			 struct ieee802_1x_mka_participant, list) {
		char *pos2 = pos;

		res = os_snprintf(pos2, end - pos2, "ieee8021XKayMkaPartCKN=");
		if (os_snprintf_error(buflen, res))
			return end - pos;
		pos2 += res;

		pos2 += wpa_snprintf_hex(pos2, end - pos2, p->ckn.name,
					 p->ckn.len);

		res = os_snprintf(pos2, end - pos2,
				  "\nieee8021XKayMkaPartCached=%s\n"
				  "ieee8021XKayMkaPartActive=%s\n"
				  "ieee8021XKayMkaPartRetain=%s\n"
				  "ieee8021XKayMkaPartActivateControl=%s\n"
				  "ieee8021XKayMkaPartPrincipal=%s\n",
				  true_false(p->cached),
				  true_false(p->active),
				  true_false(p->retain),
				  activate_control_txt(p->activate),
				  true_false(p->principal));
		if (os_snprintf_error(buflen, res))
			return end - pos;
		pos2 += res;
		pos = pos2;

		pos = mka_mib_peer(&p->live_peers, TRUE, pos, end);
		pos = mka_mib_peer(&p->potential_peers, FALSE, pos, end);
	}

	return pos - buf;
}

#endif /* CONFIG_CTRL_IFACE */
