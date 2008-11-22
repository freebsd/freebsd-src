/*
 * WPA Supplicant - Windows/NDIS driver interface
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

#include <stdlib.h>
#include <Packet32.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <ntddndis.h>

#include "common.h"
#include "driver.h"
#include "wpa_supplicant.h"
#include "l2_packet.h"
#include "eloop.h"
#include "wpa.h"
#include "driver_ndis.h"

int wpa_driver_register_event_cb(struct wpa_driver_ndis_data *drv);

static void wpa_driver_ndis_poll(void *drv);


/* FIX: to be removed once this can be compiled with the complete NDIS
 * header files */
#ifndef OID_802_11_BSSID
#define OID_802_11_BSSID 			0x0d010101
#define OID_802_11_SSID 			0x0d010102
#define OID_802_11_INFRASTRUCTURE_MODE		0x0d010108
#define OID_802_11_ADD_WEP			0x0D010113
#define OID_802_11_REMOVE_WEP			0x0D010114
#define OID_802_11_DISASSOCIATE			0x0D010115
#define OID_802_11_BSSID_LIST 			0x0d010217
#define OID_802_11_AUTHENTICATION_MODE		0x0d010118
#define OID_802_11_PRIVACY_FILTER		0x0d010119
#define OID_802_11_BSSID_LIST_SCAN 		0x0d01011A
#define OID_802_11_WEP_STATUS	 		0x0d01011B
#define OID_802_11_ENCRYPTION_STATUS OID_802_11_WEP_STATUS
#define OID_802_11_ADD_KEY 			0x0d01011D
#define OID_802_11_REMOVE_KEY 			0x0d01011E
#define OID_802_11_ASSOCIATION_INFORMATION	0x0d01011F
#define OID_802_11_TEST 			0x0d010120
#define OID_802_11_CAPABILITY 			0x0d010122
#define OID_802_11_PMKID 			0x0d010123

#define NDIS_802_11_LENGTH_SSID 32
#define NDIS_802_11_LENGTH_RATES 8
#define NDIS_802_11_LENGTH_RATES_EX 16

typedef UCHAR NDIS_802_11_MAC_ADDRESS[6];

typedef struct NDIS_802_11_SSID {
	ULONG SsidLength;
	UCHAR Ssid[NDIS_802_11_LENGTH_SSID];
} NDIS_802_11_SSID;

typedef LONG NDIS_802_11_RSSI;

typedef enum NDIS_802_11_NETWORK_TYPE {
	Ndis802_11FH,
	Ndis802_11DS,
	Ndis802_11OFDM5,
	Ndis802_11OFDM24,
	Ndis802_11NetworkTypeMax
} NDIS_802_11_NETWORK_TYPE;

typedef struct NDIS_802_11_CONFIGURATION_FH {
	ULONG Length;
	ULONG HopPattern;
	ULONG HopSet;
	ULONG DwellTime;
} NDIS_802_11_CONFIGURATION_FH;

typedef struct NDIS_802_11_CONFIGURATION {
	ULONG Length;
	ULONG BeaconPeriod;
	ULONG ATIMWindow;
	ULONG DSConfig;
	NDIS_802_11_CONFIGURATION_FH FHConfig;
} NDIS_802_11_CONFIGURATION;

typedef enum NDIS_802_11_NETWORK_INFRASTRUCTURE {
	Ndis802_11IBSS,
	Ndis802_11Infrastructure,
	Ndis802_11AutoUnknown,
	Ndis802_11InfrastructureMax
} NDIS_802_11_NETWORK_INFRASTRUCTURE;

typedef enum NDIS_802_11_AUTHENTICATION_MODE {
	Ndis802_11AuthModeOpen,
	Ndis802_11AuthModeShared,
	Ndis802_11AuthModeAutoSwitch,
	Ndis802_11AuthModeWPA,
	Ndis802_11AuthModeWPAPSK,
	Ndis802_11AuthModeWPANone,
	Ndis802_11AuthModeWPA2,
	Ndis802_11AuthModeWPA2PSK,
	Ndis802_11AuthModeMax
} NDIS_802_11_AUTHENTICATION_MODE;

typedef enum NDIS_802_11_WEP_STATUS {
	Ndis802_11WEPEnabled,
	Ndis802_11Encryption1Enabled = Ndis802_11WEPEnabled,
	Ndis802_11WEPDisabled,
	Ndis802_11EncryptionDisabled = Ndis802_11WEPDisabled,
	Ndis802_11WEPKeyAbsent,
	Ndis802_11Encryption1KeyAbsent = Ndis802_11WEPKeyAbsent,
	Ndis802_11WEPNotSupported,
	Ndis802_11EncryptionNotSupported = Ndis802_11WEPNotSupported,
	Ndis802_11Encryption2Enabled,
	Ndis802_11Encryption2KeyAbsent,
	Ndis802_11Encryption3Enabled,
	Ndis802_11Encryption3KeyAbsent
} NDIS_802_11_WEP_STATUS, NDIS_802_11_ENCRYPTION_STATUS;

typedef enum NDIS_802_11_PRIVACY_FILTER {
	Ndis802_11PrivFilterAcceptAll,
	Ndis802_11PrivFilter8021xWEP
} NDIS_802_11_PRIVACY_FILTER;

typedef UCHAR NDIS_802_11_RATES[NDIS_802_11_LENGTH_RATES];
typedef UCHAR NDIS_802_11_RATES_EX[NDIS_802_11_LENGTH_RATES_EX];

typedef struct NDIS_WLAN_BSSID_EX {
	ULONG Length;
	NDIS_802_11_MAC_ADDRESS MacAddress; /* BSSID */
	UCHAR Reserved[2];
	NDIS_802_11_SSID Ssid;
	ULONG Privacy;
	NDIS_802_11_RSSI Rssi;
	NDIS_802_11_NETWORK_TYPE NetworkTypeInUse;
	NDIS_802_11_CONFIGURATION Configuration;
	NDIS_802_11_NETWORK_INFRASTRUCTURE InfrastructureMode;
	NDIS_802_11_RATES_EX SupportedRates;
	ULONG IELength;
	UCHAR IEs[1];
} NDIS_WLAN_BSSID_EX;

typedef struct NDIS_802_11_BSSID_LIST_EX {
	ULONG NumberOfItems;
	NDIS_WLAN_BSSID_EX Bssid[1];
} NDIS_802_11_BSSID_LIST_EX;

typedef struct NDIS_802_11_FIXED_IEs {
	UCHAR Timestamp[8];
	USHORT BeaconInterval;
	USHORT Capabilities;
} NDIS_802_11_FIXED_IEs;

typedef struct NDIS_802_11_WEP {
	ULONG Length;
	ULONG KeyIndex;
	ULONG KeyLength;
	UCHAR KeyMaterial[1];
} NDIS_802_11_WEP;

typedef ULONG NDIS_802_11_KEY_INDEX;
typedef ULONGLONG NDIS_802_11_KEY_RSC;

typedef struct NDIS_802_11_KEY {
	ULONG Length;
	ULONG KeyIndex;
	ULONG KeyLength;
	NDIS_802_11_MAC_ADDRESS BSSID;
	NDIS_802_11_KEY_RSC KeyRSC;
	UCHAR KeyMaterial[1];
} NDIS_802_11_KEY;

typedef struct NDIS_802_11_REMOVE_KEY {
	ULONG Length;
	ULONG KeyIndex;
	NDIS_802_11_MAC_ADDRESS BSSID;
} NDIS_802_11_REMOVE_KEY;

typedef struct NDIS_802_11_AI_REQFI {
	USHORT Capabilities;
	USHORT ListenInterval;
	NDIS_802_11_MAC_ADDRESS CurrentAPAddress;
} NDIS_802_11_AI_REQFI;

typedef struct NDIS_802_11_AI_RESFI {
	USHORT Capabilities;
	USHORT StatusCode;
	USHORT AssociationId;
} NDIS_802_11_AI_RESFI;

typedef struct NDIS_802_11_ASSOCIATION_INFORMATION {
	ULONG Length;
	USHORT AvailableRequestFixedIEs;
	NDIS_802_11_AI_REQFI RequestFixedIEs;
	ULONG RequestIELength;
	ULONG OffsetRequestIEs;
	USHORT AvailableResponseFixedIEs;
	NDIS_802_11_AI_RESFI ResponseFixedIEs;
	ULONG ResponseIELength;
	ULONG OffsetResponseIEs;
} NDIS_802_11_ASSOCIATION_INFORMATION;

typedef struct NDIS_802_11_AUTHENTICATION_ENCRYPTION {
	NDIS_802_11_AUTHENTICATION_MODE AuthModeSupported;
	NDIS_802_11_ENCRYPTION_STATUS EncryptStatusSupported;
} NDIS_802_11_AUTHENTICATION_ENCRYPTION;

typedef struct NDIS_802_11_CAPABILITY {
	ULONG Length;
	ULONG Version;
	ULONG NoOfPMKIDs;
	ULONG NoOfAuthEncryptPairSupported;
	NDIS_802_11_AUTHENTICATION_ENCRYPTION
		AuthenticationEncryptionSupported[1];
} NDIS_802_11_CAPABILITY;

typedef UCHAR NDIS_802_11_PMKID_VALUE[16];

typedef struct BSSID_INFO {
	NDIS_802_11_MAC_ADDRESS BSSID;
	NDIS_802_11_PMKID_VALUE PMKID;
} BSSID_INFO;

typedef struct NDIS_802_11_PMKID {
	ULONG Length;
	ULONG BSSIDInfoCount;
	BSSID_INFO BSSIDInfo[1];
} NDIS_802_11_PMKID;

typedef enum NDIS_802_11_STATUS_TYPE {
	Ndis802_11StatusType_Authentication,
	Ndis802_11StatusType_PMKID_CandidateList = 2,
	Ndis802_11StatusTypeMax
} NDIS_802_11_STATUS_TYPE;

typedef struct NDIS_802_11_STATUS_INDICATION {
	NDIS_802_11_STATUS_TYPE StatusType;
} NDIS_802_11_STATUS_INDICATION;

typedef struct PMKID_CANDIDATE {
	NDIS_802_11_MAC_ADDRESS BSSID;
	ULONG Flags;
} PMKID_CANDIDATE;

#define NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED 0x01

typedef struct NDIS_802_11_PMKID_CANDIDATE_LIST {
	ULONG Version;
	ULONG NumCandidates;
	PMKID_CANDIDATE CandidateList[1];
} NDIS_802_11_PMKID_CANDIDATE_LIST;

typedef struct NDIS_802_11_AUTHENTICATION_REQUEST {
	ULONG Length;
	NDIS_802_11_MAC_ADDRESS Bssid;
	ULONG Flags;
} NDIS_802_11_AUTHENTICATION_REQUEST;

#define NDIS_802_11_AUTH_REQUEST_REAUTH			0x01
#define NDIS_802_11_AUTH_REQUEST_KEYUPDATE		0x02
#define NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR		0x06
#define NDIS_802_11_AUTH_REQUEST_GROUP_ERROR		0x0E

#endif


static int ndis_get_oid(struct wpa_driver_ndis_data *drv, unsigned int oid,
			char *data, int len)
{
	char *buf;
	PACKET_OID_DATA *o;
	int ret;

	buf = malloc(sizeof(*o) + len);
	if (buf == NULL)
		return -1;
	memset(buf, 0, sizeof(*o) + len);
	o = (PACKET_OID_DATA *) buf;
	o->Oid = oid;
	o->Length = len;

	if (!PacketRequest(drv->adapter, FALSE, o)) {
		wpa_printf(MSG_DEBUG, "%s: oid=0x%x len (%d) failed",
			   __func__, oid, len);
		free(buf);
		return -1;
	}
	if (o->Length > len) {
		wpa_printf(MSG_DEBUG, "%s: oid=0x%x Length (%d) > len (%d)",
			   __func__, oid, (unsigned int) o->Length, len);
		free(buf);
		return -1;
	}
	memcpy(data, o->Data, o->Length);
	ret = o->Length;
	free(buf);
	return ret;
}


static int ndis_set_oid(struct wpa_driver_ndis_data *drv, unsigned int oid,
			char *data, int len)
{
	char *buf;
	PACKET_OID_DATA *o;

	buf = malloc(sizeof(*o) + len);
	if (buf == NULL)
		return -1;
	memset(buf, 0, sizeof(*o) + len);
	o = (PACKET_OID_DATA *) buf;
	o->Oid = oid;
	o->Length = len;
	if (data)
		memcpy(o->Data, data, len);

	if (!PacketRequest(drv->adapter, TRUE, o)) {
		wpa_printf(MSG_DEBUG, "%s: oid=0x%x len (%d) failed",
			   __func__, oid, len);
		free(buf);
		return -1;
	}
	free(buf);
	return 0;
}


static int ndis_set_auth_mode(struct wpa_driver_ndis_data *drv, int mode)
{
	u32 auth_mode = mode;
	if (ndis_set_oid(drv, OID_802_11_AUTHENTICATION_MODE,
			 (char *) &auth_mode, sizeof(auth_mode)) < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: Failed to set "
			   "OID_802_11_AUTHENTICATION_MODE (%d)",
			   (int) auth_mode);
		return -1;
	}
	return 0;
}


static int ndis_get_auth_mode(struct wpa_driver_ndis_data *drv)
{
	u32 auth_mode;
	int res;
	res = ndis_get_oid(drv, OID_802_11_AUTHENTICATION_MODE,
			   (char *) &auth_mode, sizeof(auth_mode));
	if (res != sizeof(auth_mode)) {
		wpa_printf(MSG_DEBUG, "NDIS: Failed to get "
			   "OID_802_11_AUTHENTICATION_MODE");
		return -1;
	}
	return auth_mode;
}


static int ndis_set_encr_status(struct wpa_driver_ndis_data *drv, int encr)
{
	u32 encr_status = encr;
	if (ndis_set_oid(drv, OID_802_11_ENCRYPTION_STATUS,
			 (char *) &encr_status, sizeof(encr_status)) < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: Failed to set "
			   "OID_802_11_ENCRYPTION_STATUS (%d)", encr);
		return -1;
	}
	return 0;
}


static int ndis_get_encr_status(struct wpa_driver_ndis_data *drv)
{
	u32 encr;
	int res;
	res = ndis_get_oid(drv, OID_802_11_ENCRYPTION_STATUS,
			   (char *) &encr, sizeof(encr));
	if (res != sizeof(encr)) {
		wpa_printf(MSG_DEBUG, "NDIS: Failed to get "
			   "OID_802_11_ENCRYPTION_STATUS");
		return -1;
	}
	return encr;
}


static int wpa_driver_ndis_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_ndis_data *drv = priv;

	if (drv->wired) {
		/*
		 * Report PAE group address as the "BSSID" for wired
		 * connection.
		 */
		bssid[0] = 0x01;
		bssid[1] = 0x80;
		bssid[2] = 0xc2;
		bssid[3] = 0x00;
		bssid[4] = 0x00;
		bssid[5] = 0x03;
		return 0;
	}

	return ndis_get_oid(drv, OID_802_11_BSSID, bssid, ETH_ALEN) < 0 ?
		-1 : 0;
}



static int wpa_driver_ndis_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_ndis_data *drv = priv;
	NDIS_802_11_SSID buf;
	int res;

	res = ndis_get_oid(drv, OID_802_11_SSID, (char *) &buf, sizeof(buf));
	if (res < 4) {
		wpa_printf(MSG_DEBUG, "NDIS: Failed to get SSID");
		if (drv->wired) {
			wpa_printf(MSG_DEBUG, "NDIS: Allow get_ssid failure "
				   "with a wired interface");
			return 0;
		}
		return -1;
	}
	memcpy(ssid, buf.Ssid, buf.SsidLength);
	return buf.SsidLength;
}


static int wpa_driver_ndis_set_ssid(struct wpa_driver_ndis_data *drv,
				    const u8 *ssid, size_t ssid_len)
{
	NDIS_802_11_SSID buf;

	memset(&buf, 0, sizeof(buf));
	buf.SsidLength = ssid_len;
	memcpy(buf.Ssid, ssid, ssid_len);
	/*
	 * Make sure radio is marked enabled here so that scan request will not
	 * force SSID to be changed to a random one in order to enable radio at
	 * that point.
	 */
	drv->radio_enabled = 1;
	return ndis_set_oid(drv, OID_802_11_SSID, (char *) &buf, sizeof(buf));
}


/* Disconnect using OID_802_11_DISASSOCIATE. This will also turn the radio off.
 */
static int wpa_driver_ndis_radio_off(struct wpa_driver_ndis_data *drv)
{
	drv->radio_enabled = 0;
	return ndis_set_oid(drv, OID_802_11_DISASSOCIATE, "    ", 4);
}


/* Disconnect by setting SSID to random (i.e., likely not used). */
static int wpa_driver_ndis_disconnect(struct wpa_driver_ndis_data *drv)
{
	char ssid[32];
	int i;
	for (i = 0; i < 32; i++)
		ssid[i] = rand() & 0xff;
	return wpa_driver_ndis_set_ssid(drv, ssid, 32);
}


static int wpa_driver_ndis_deauthenticate(void *priv, const u8 *addr,
					  int reason_code)
{
	struct wpa_driver_ndis_data *drv = priv;
	return wpa_driver_ndis_disconnect(drv);
}


static int wpa_driver_ndis_disassociate(void *priv, const u8 *addr,
					int reason_code)
{
	struct wpa_driver_ndis_data *drv = priv;
	return wpa_driver_ndis_disconnect(drv);
}


static int wpa_driver_ndis_set_wpa(void *priv, int enabled)
{
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);
	return 0;
}


static void wpa_driver_ndis_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	wpa_printf(MSG_DEBUG, "Scan timeout - try to get results");
	wpa_supplicant_event(timeout_ctx, EVENT_SCAN_RESULTS, NULL);
}


static int wpa_driver_ndis_scan(void *priv, const u8 *ssid, size_t ssid_len)
{
	struct wpa_driver_ndis_data *drv = priv;
	int res;

	if (!drv->radio_enabled) {
		wpa_printf(MSG_DEBUG, "NDIS: turning radio on before the first"
			   " scan");
		if (wpa_driver_ndis_disconnect(drv) < 0) {
			wpa_printf(MSG_DEBUG, "NDIS: failed to enable radio");
		}
		drv->radio_enabled = 1;
	}

	res = ndis_set_oid(drv, OID_802_11_BSSID_LIST_SCAN, "    ", 4);
	eloop_register_timeout(3, 0, wpa_driver_ndis_scan_timeout, drv,
			       drv->ctx);
	return res;
}


static void wpa_driver_ndis_get_ies(struct wpa_scan_result *res, u8 *ie,
				    size_t ie_len)
{
	u8 *pos = ie;
	u8 *end = ie + ie_len;

	if (ie_len < sizeof(NDIS_802_11_FIXED_IEs))
		return;

	pos += sizeof(NDIS_802_11_FIXED_IEs);
	/* wpa_hexdump(MSG_MSGDUMP, "IEs", pos, end - pos); */
	while (pos + 1 < end && pos + 2 + pos[1] <= end) {
		u8 ielen = 2 + pos[1];
		if (ielen > SSID_MAX_WPA_IE_LEN) {
			pos += ielen;
			continue;
		}
		if (pos[0] == GENERIC_INFO_ELEM && pos[1] >= 4 &&
		    memcmp(pos + 2, "\x00\x50\xf2\x01", 4) == 0) {
			memcpy(res->wpa_ie, pos, ielen);
			res->wpa_ie_len = ielen;
		} else if (pos[0] == RSN_INFO_ELEM) {
			memcpy(res->rsn_ie, pos, ielen);
			res->rsn_ie_len = ielen;
		}
		pos += ielen;
	}
}


static int wpa_driver_ndis_get_scan_results(void *priv,
					    struct wpa_scan_result *results,
					    size_t max_size)
{
	struct wpa_driver_ndis_data *drv = priv;
	NDIS_802_11_BSSID_LIST_EX *b;
	size_t blen;
	int len, count, i, j;
	char *pos;

	blen = 65535;
	b = malloc(blen);
	if (b == NULL)
		return -1;
	memset(b, 0, blen);
	len = ndis_get_oid(drv, OID_802_11_BSSID_LIST, (char *) b, blen);
	if (len < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: failed to get scan results");
		free(b);
		return -1;
	}
	count = b->NumberOfItems;

	if (count > max_size)
		count = max_size;

	memset(results, 0, max_size * sizeof(struct wpa_scan_result));
	pos = (char *) &b->Bssid[0];
	for (i = 0; i < count; i++) {
		NDIS_WLAN_BSSID_EX *bss = (NDIS_WLAN_BSSID_EX *) pos;
		memcpy(results[i].bssid, bss->MacAddress, ETH_ALEN);
		memcpy(results[i].ssid, bss->Ssid.Ssid, bss->Ssid.SsidLength);
		results[i].ssid_len = bss->Ssid.SsidLength;
		if (bss->Privacy)
			results[i].caps |= IEEE80211_CAP_PRIVACY;
		results[i].level = (int) bss->Rssi;
		results[i].freq = bss->Configuration.DSConfig / 1000;
		for (j = 0; j < sizeof(bss->SupportedRates); j++) {
			if ((bss->SupportedRates[j] & 0x7f) >
			    results[i].maxrate) {
				results[i].maxrate =
					bss->SupportedRates[j] & 0x7f;
			}
		}
		wpa_driver_ndis_get_ies(&results[i], bss->IEs, bss->IELength);
		pos += bss->Length;
		if (pos > (char *) b + blen)
			break;
	}

	free(b);
	return count;
}


static int wpa_driver_ndis_remove_key(struct wpa_driver_ndis_data *drv,
				      int key_idx, const u8 *addr,
				      const u8 *bssid, int pairwise)
{
	NDIS_802_11_REMOVE_KEY rkey;
	NDIS_802_11_KEY_INDEX index;
	int res, res2;

	memset(&rkey, 0, sizeof(rkey));

	rkey.Length = sizeof(rkey);
	rkey.KeyIndex = key_idx;
	if (pairwise)
		rkey.KeyIndex |= 1 << 30;
	memcpy(rkey.BSSID, bssid, ETH_ALEN);

	res = ndis_set_oid(drv, OID_802_11_REMOVE_KEY, (char *) &rkey,
			   sizeof(rkey));
	if (!pairwise) {
		res2 = ndis_set_oid(drv, OID_802_11_REMOVE_WEP,
				    (char *) &index, sizeof(index));
	} else
		res2 = 0;

	if (res < 0 && res2 < 0)
		return res;
	return 0;
}


static int wpa_driver_ndis_add_wep(struct wpa_driver_ndis_data *drv,
				   int pairwise, int key_idx, int set_tx,
				   const u8 *key, size_t key_len)
{
	NDIS_802_11_WEP *wep;
	size_t len;
	int res;

	len = 12 + key_len;
	wep = malloc(len);
	if (wep == NULL)
		return -1;
	memset(wep, 0, len);
	wep->Length = len;
	wep->KeyIndex = key_idx;
	if (set_tx)
		wep->KeyIndex |= 1 << 31;
#if 0 /* Setting bit30 does not seem to work with some NDIS drivers */
	if (pairwise)
		wep->KeyIndex |= 1 << 30;
#endif
	wep->KeyLength = key_len;
	memcpy(wep->KeyMaterial, key, key_len);

	wpa_hexdump_key(MSG_MSGDUMP, "NDIS: OIS_802_11_ADD_WEP",
			(char *) wep, len);
	res = ndis_set_oid(drv, OID_802_11_ADD_WEP, (char *) wep, len);

	free(wep);

	return res;
}

static int wpa_driver_ndis_set_key(void *priv, wpa_alg alg, const u8 *addr,
				   int key_idx, int set_tx,
				   const u8 *seq, size_t seq_len,
				   const u8 *key, size_t key_len)
{
	struct wpa_driver_ndis_data *drv = priv;
	size_t len;
	NDIS_802_11_KEY *nkey;
	int i, res, pairwise;
	u8 bssid[ETH_ALEN];

	if (addr == NULL || memcmp(addr, "\xff\xff\xff\xff\xff\xff",
				   ETH_ALEN) == 0) {
		/* Group Key */
		pairwise = 0;
		wpa_driver_ndis_get_bssid(drv, bssid);
	} else {
		/* Pairwise Key */
		pairwise = 1;
		memcpy(bssid, addr, ETH_ALEN);
	}

	if (alg == WPA_ALG_NONE || key_len == 0) {
		return wpa_driver_ndis_remove_key(drv, key_idx, addr, bssid,
						  pairwise);
	}

	if (alg == WPA_ALG_WEP) {
		return wpa_driver_ndis_add_wep(drv, pairwise, key_idx, set_tx,
					       key, key_len);
	}

	len = 12 + 6 + 6 + 8 + key_len;

	nkey = malloc(len);
	if (nkey == NULL)
		return -1;
	memset(nkey, 0, len);

	nkey->Length = len;
	nkey->KeyIndex = key_idx;
	if (set_tx)
		nkey->KeyIndex |= 1 << 31;
	if (pairwise)
		nkey->KeyIndex |= 1 << 30;
	if (seq && seq_len)
		nkey->KeyIndex |= 1 << 29;
	nkey->KeyLength = key_len;
	memcpy(nkey->BSSID, bssid, ETH_ALEN);
	if (seq && seq_len) {
		for (i = 0; i < seq_len; i++)
			nkey->KeyRSC |= seq[i] << (i * 8);
	}
	if (alg == WPA_ALG_TKIP && key_len == 32) {
		memcpy(nkey->KeyMaterial, key, 16);
		memcpy(nkey->KeyMaterial + 16, key + 24, 8);
		memcpy(nkey->KeyMaterial + 24, key + 16, 8);
	} else {
		memcpy(nkey->KeyMaterial, key, key_len);
	}

	wpa_hexdump_key(MSG_MSGDUMP, "NDIS: OIS_802_11_ADD_KEY",
			(char *) nkey, len);
	res = ndis_set_oid(drv, OID_802_11_ADD_KEY, (char *) nkey, len);
	free(nkey);

	return res;
}


static int
wpa_driver_ndis_associate(void *priv,
			  struct wpa_driver_associate_params *params)
{
	struct wpa_driver_ndis_data *drv = priv;
	u32 auth_mode, encr, priv_mode, mode;

	/* Note: Setting OID_802_11_INFRASTRUCTURE_MODE clears current keys,
	 * so static WEP keys needs to be set again after this. */
	if (params->mode == IEEE80211_MODE_IBSS)
		mode = Ndis802_11IBSS;
	else
		mode = Ndis802_11Infrastructure;
	if (ndis_set_oid(drv, OID_802_11_INFRASTRUCTURE_MODE,
			 (char *) &mode, sizeof(mode)) < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: Failed to set "
			   "OID_802_11_INFRASTRUCTURE_MODE (%d)",
			   (int) mode);
		/* Try to continue anyway */
	}

	if (params->wpa_ie == NULL || params->wpa_ie_len == 0) {
		if (params->auth_alg & AUTH_ALG_SHARED_KEY) {
			if (params->auth_alg & AUTH_ALG_OPEN_SYSTEM)
				auth_mode = Ndis802_11AuthModeAutoSwitch;
			else
				auth_mode = Ndis802_11AuthModeShared;
		} else
			auth_mode = Ndis802_11AuthModeOpen;
		priv_mode = Ndis802_11PrivFilterAcceptAll;
	} else if (params->wpa_ie[0] == RSN_INFO_ELEM) {
		priv_mode = Ndis802_11PrivFilter8021xWEP;
		if (params->key_mgmt_suite == KEY_MGMT_PSK)
			auth_mode = Ndis802_11AuthModeWPA2PSK;
		else
			auth_mode = Ndis802_11AuthModeWPA2;
	} else {
		priv_mode = Ndis802_11PrivFilter8021xWEP;
		if (params->key_mgmt_suite == KEY_MGMT_WPA_NONE)
			auth_mode = Ndis802_11AuthModeWPANone;
		else if (params->key_mgmt_suite == KEY_MGMT_PSK)
			auth_mode = Ndis802_11AuthModeWPAPSK;
		else
			auth_mode = Ndis802_11AuthModeWPA;
	}

	switch (params->pairwise_suite) {
	case CIPHER_CCMP:
		encr = Ndis802_11Encryption3Enabled;
		break;
	case CIPHER_TKIP:
		encr = Ndis802_11Encryption2Enabled;
		break;
	case CIPHER_WEP40:
	case CIPHER_WEP104:
		encr = Ndis802_11Encryption1Enabled;
		break;
	case CIPHER_NONE:
		if (params->group_suite == CIPHER_CCMP)
			encr = Ndis802_11Encryption3Enabled;
		else
			encr = Ndis802_11Encryption2Enabled;
		break;
	default:
		encr = Ndis802_11EncryptionDisabled;
	};

	if (ndis_set_oid(drv, OID_802_11_PRIVACY_FILTER,
			 (char *) &priv_mode, sizeof(priv_mode)) < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: Failed to set "
			   "OID_802_11_PRIVACY_FILTER (%d)",
			   (int) priv_mode);
		/* Try to continue anyway */
	}

	ndis_set_auth_mode(drv, auth_mode);
	ndis_set_encr_status(drv, encr);

	return wpa_driver_ndis_set_ssid(drv, params->ssid, params->ssid_len);
}


static int wpa_driver_ndis_set_pmkid(struct wpa_driver_ndis_data *drv)
{
	int len, count, i, ret;
	struct ndis_pmkid_entry *entry;
	NDIS_802_11_PMKID *p;

	count = 0;
	entry = drv->pmkid;
	while (entry) {
		count++;
		if (count >= drv->no_of_pmkid)
			break;
		entry = entry->next;
	}
	len = 8 + count * sizeof(BSSID_INFO);
	p = malloc(len);
	if (p == NULL)
		return -1;
	memset(p, 0, len);
	p->Length = len;
	p->BSSIDInfoCount = count;
	entry = drv->pmkid;
	for (i = 0; i < count; i++) {
		memcpy(&p->BSSIDInfo[i].BSSID, entry->bssid, ETH_ALEN);
		memcpy(&p->BSSIDInfo[i].PMKID, entry->pmkid, 16);
		entry = entry->next;
	}
	wpa_hexdump(MSG_MSGDUMP, "NDIS: OID_802_11_PMKID", (char *) p, len);
	ret = ndis_set_oid(drv, OID_802_11_PMKID, (char *) p, len);
	free(p);
	return ret;
}


static int wpa_driver_ndis_add_pmkid(void *priv, const u8 *bssid,
				     const u8 *pmkid)
{
	struct wpa_driver_ndis_data *drv = priv;
	struct ndis_pmkid_entry *entry, *prev;

	if (drv->no_of_pmkid == 0)
		return 0;

	prev = NULL;
	entry = drv->pmkid;
	while (entry) {
		if (memcmp(entry->bssid, bssid, ETH_ALEN) == 0)
			break;
		prev = entry;
		entry = entry->next;
	}

	if (entry) {
		/* Replace existing entry for this BSSID and move it into the
		 * beginning of the list. */
		memcpy(entry->pmkid, pmkid, 16);
		if (prev) {
			prev->next = entry->next;
			entry->next = drv->pmkid;
			drv->pmkid = entry;
		}
	} else {
		entry = malloc(sizeof(*entry));
		if (entry) {
			memcpy(entry->bssid, bssid, ETH_ALEN);
			memcpy(entry->pmkid, pmkid, 16);
			entry->next = drv->pmkid;
			drv->pmkid = entry;
		}
	}

	return wpa_driver_ndis_set_pmkid(drv);
}


static int wpa_driver_ndis_remove_pmkid(void *priv, const u8 *bssid,
		 			const u8 *pmkid)
{
	struct wpa_driver_ndis_data *drv = priv;
	struct ndis_pmkid_entry *entry, *prev;

	if (drv->no_of_pmkid == 0)
		return 0;

	entry = drv->pmkid;
	prev = NULL;
	drv->pmkid = NULL;
	while (entry) {
		if (memcmp(entry->bssid, bssid, ETH_ALEN) == 0 &&
		    memcmp(entry->pmkid, pmkid, 16) == 0) {
			if (prev)
				prev->next = entry->next;
			else
				drv->pmkid = entry->next;
			free(entry);
			break;
		}
		prev = entry;
		entry = entry->next;
	}
	return wpa_driver_ndis_set_pmkid(drv);
}


static int wpa_driver_ndis_flush_pmkid(void *priv)
{
	struct wpa_driver_ndis_data *drv = priv;
	NDIS_802_11_PMKID p;
	struct ndis_pmkid_entry *pmkid, *prev;

	if (drv->no_of_pmkid == 0)
		return 0;

	pmkid = drv->pmkid;
	drv->pmkid = NULL;
	while (pmkid) {
		prev = pmkid;
		pmkid = pmkid->next;
		free(prev);
	}

	memset(&p, 0, sizeof(p));
	p.Length = 8;
	p.BSSIDInfoCount = 0;
	wpa_hexdump(MSG_MSGDUMP, "NDIS: OID_802_11_PMKID (flush)",
		    (char *) &p, 8);
	return ndis_set_oid(drv, OID_802_11_PMKID, (char *) &p, 8);
}


static int wpa_driver_ndis_get_associnfo(struct wpa_driver_ndis_data *drv)
{
	char buf[512], *pos;
	NDIS_802_11_ASSOCIATION_INFORMATION *ai;
	int len, i;
	union wpa_event_data data;
	NDIS_802_11_BSSID_LIST_EX *b;
	size_t blen;

	len = ndis_get_oid(drv, OID_802_11_ASSOCIATION_INFORMATION, buf,
			   sizeof(buf));
	if (len < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: failed to get association "
			   "information");
		return -1;
	}
	if (len > sizeof(buf)) {
		/* Some drivers seem to be producing incorrect length for this
		 * data. Limit the length to the current buffer size to avoid
		 * crashing in hexdump. The data seems to be otherwise valid,
		 * so better try to use it. */
		wpa_printf(MSG_DEBUG, "NDIS: ignored bogus association "
			   "information length %d", len);
		len = ndis_get_oid(drv, OID_802_11_ASSOCIATION_INFORMATION,
				   buf, sizeof(buf));
		if (len < -1) {
			wpa_printf(MSG_DEBUG, "NDIS: re-reading association "
				   "information failed");
			return -1;
		}
		if (len > sizeof(buf)) {
			wpa_printf(MSG_DEBUG, "NDIS: ignored bogus association"
				   " information length %d (re-read)", len);
			len = sizeof(buf);
		}
	}
	wpa_hexdump(MSG_MSGDUMP, "NDIS: association information", buf, len);
	if (len < sizeof(*ai)) {
		wpa_printf(MSG_DEBUG, "NDIS: too short association "
			   "information");
		return -1;
	}
	ai = (NDIS_802_11_ASSOCIATION_INFORMATION *) buf;
	wpa_printf(MSG_DEBUG, "NDIS: ReqFixed=0x%x RespFixed=0x%x off_req=%d "
		   "off_resp=%d len_req=%d len_resp=%d",
		   ai->AvailableRequestFixedIEs, ai->AvailableResponseFixedIEs,
		   (int) ai->OffsetRequestIEs, (int) ai->OffsetResponseIEs,
		   (int) ai->RequestIELength, (int) ai->ResponseIELength);

	if (ai->OffsetRequestIEs + ai->RequestIELength > len ||
	    ai->OffsetResponseIEs + ai->ResponseIELength > len) {
		wpa_printf(MSG_DEBUG, "NDIS: association information - "
			   "IE overflow");
		return -1;
	}

	wpa_hexdump(MSG_MSGDUMP, "NDIS: Request IEs", 
		    buf + ai->OffsetRequestIEs, ai->RequestIELength);
	wpa_hexdump(MSG_MSGDUMP, "NDIS: Response IEs", 
		    buf + ai->OffsetResponseIEs, ai->ResponseIELength);

	memset(&data, 0, sizeof(data));
	data.assoc_info.req_ies = buf + ai->OffsetRequestIEs;
	data.assoc_info.req_ies_len = ai->RequestIELength;
	data.assoc_info.resp_ies = buf + ai->OffsetResponseIEs;
	data.assoc_info.resp_ies_len = ai->ResponseIELength;

	blen = 65535;
	b = malloc(blen);
	if (b == NULL)
		goto skip_scan_results;
	memset(b, 0, blen);
	len = ndis_get_oid(drv, OID_802_11_BSSID_LIST, (char *) b, blen);
	if (len < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: failed to get scan results");
		free(b);
		b = NULL;
		goto skip_scan_results;
	}
	wpa_printf(MSG_DEBUG, "NDIS: %d BSSID items to process for AssocInfo",
		   (unsigned int) b->NumberOfItems);

	pos = (char *) &b->Bssid[0];
	for (i = 0; i < b->NumberOfItems; i++) {
		NDIS_WLAN_BSSID_EX *bss = (NDIS_WLAN_BSSID_EX *) pos;
		if (memcmp(drv->bssid, bss->MacAddress, ETH_ALEN) == 0 &&
		    bss->IELength > sizeof(NDIS_802_11_FIXED_IEs)) {
			data.assoc_info.beacon_ies =
				((u8 *) bss->IEs) +
				sizeof(NDIS_802_11_FIXED_IEs);
			data.assoc_info.beacon_ies_len =
				bss->IELength - sizeof(NDIS_802_11_FIXED_IEs);
			wpa_hexdump(MSG_MSGDUMP, "NDIS: Beacon IEs",
				    data.assoc_info.beacon_ies,
				    data.assoc_info.beacon_ies_len);
			break;
		}
		pos += bss->Length;
		if (pos > (char *) b + blen)
			break;
	}

skip_scan_results:
	wpa_supplicant_event(drv->ctx, EVENT_ASSOCINFO, &data);

	free(b);

	return 0;
}


static void wpa_driver_ndis_poll_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_driver_ndis_data *drv = eloop_ctx;
	u8 bssid[ETH_ALEN];

	if (drv->wired)
		return;

	if (wpa_driver_ndis_get_bssid(drv, bssid)) {
		/* Disconnected */
		if (memcmp(drv->bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN)
		    != 0) {
			memset(drv->bssid, 0, ETH_ALEN);
			wpa_supplicant_event(drv->ctx, EVENT_DISASSOC, NULL);
		}
	} else {
		/* Connected */
		if (memcmp(drv->bssid, bssid, ETH_ALEN) != 0) {
			memcpy(drv->bssid, bssid, ETH_ALEN);
			wpa_driver_ndis_get_associnfo(drv);
			wpa_supplicant_event(drv->ctx, EVENT_ASSOC, NULL);
		}
	}
	eloop_register_timeout(1, 0, wpa_driver_ndis_poll_timeout, drv, NULL);
}


static void wpa_driver_ndis_poll(void *priv)
{
	struct wpa_driver_ndis_data *drv = priv;
	eloop_cancel_timeout(wpa_driver_ndis_poll_timeout, drv, NULL);
	wpa_driver_ndis_poll_timeout(drv, NULL);
}


/* Called when driver generates Media Connect Event by calling
 * NdisMIndicateStatus() with NDIS_STATUS_MEDIA_CONNECT */
void wpa_driver_ndis_event_connect(struct wpa_driver_ndis_data *drv)
{
	wpa_printf(MSG_DEBUG, "NDIS: Media Connect Event");
	if (wpa_driver_ndis_get_bssid(drv, drv->bssid) == 0) {
		wpa_driver_ndis_get_associnfo(drv);
		wpa_supplicant_event(drv->ctx, EVENT_ASSOC, NULL);
	}
}


/* Called when driver generates Media Disconnect Event by calling
 * NdisMIndicateStatus() with NDIS_STATUS_MEDIA_DISCONNECT */
void wpa_driver_ndis_event_disconnect(struct wpa_driver_ndis_data *drv)
{
	wpa_printf(MSG_DEBUG, "NDIS: Media Disconnect Event");
	memset(drv->bssid, 0, ETH_ALEN);
	wpa_supplicant_event(drv->ctx, EVENT_DISASSOC, NULL);
}


static void wpa_driver_ndis_event_auth(struct wpa_driver_ndis_data *drv,
				       const u8 *data, size_t data_len)
{
	NDIS_802_11_AUTHENTICATION_REQUEST *req;
	int pairwise = 0, group = 0;
	union wpa_event_data event;

	if (data_len < sizeof(*req)) {
		wpa_printf(MSG_DEBUG, "NDIS: Too short Authentication Request "
			   "Event (len=%d)", data_len);
		return;
	}
	req = (NDIS_802_11_AUTHENTICATION_REQUEST *) data;

	wpa_printf(MSG_DEBUG, "NDIS: Authentication Request Event: "
		   "Bssid " MACSTR " Flags 0x%x",
		   MAC2STR(req->Bssid), (int) req->Flags);

	if ((req->Flags & NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR) ==
	    NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR)
		pairwise = 1;
	else if ((req->Flags & NDIS_802_11_AUTH_REQUEST_GROUP_ERROR) ==
	    NDIS_802_11_AUTH_REQUEST_GROUP_ERROR)
		group = 1;

	if (pairwise || group) {
		memset(&event, 0, sizeof(event));
		event.michael_mic_failure.unicast = pairwise;
		wpa_supplicant_event(drv->ctx, EVENT_MICHAEL_MIC_FAILURE,
				     &event);
	}
}


static void wpa_driver_ndis_event_pmkid(struct wpa_driver_ndis_data *drv,
					const u8 *data, size_t data_len)
{
	NDIS_802_11_PMKID_CANDIDATE_LIST *pmkid;
	int i;
	union wpa_event_data event;

	if (data_len < 8) {
		wpa_printf(MSG_DEBUG, "NDIS: Too short PMKID Candidate List "
			   "Event (len=%d)", data_len);
		return;
	}
	pmkid = (NDIS_802_11_PMKID_CANDIDATE_LIST *) data;
	wpa_printf(MSG_DEBUG, "NDIS: PMKID Candidate List Event - Version %d "
		   "NumCandidates %d",
		   (int) pmkid->Version, (int) pmkid->NumCandidates);

	if (pmkid->Version != 1) {
		wpa_printf(MSG_DEBUG, "NDIS: Unsupported PMKID Candidate List "
			   "Version %d", (int) pmkid->Version);
		return;
	}

	if (data_len < 8 + pmkid->NumCandidates * sizeof(PMKID_CANDIDATE)) {
		wpa_printf(MSG_DEBUG, "NDIS: PMKID Candidate List underflow");
		return;
	}

	memset(&event, 0, sizeof(event));
	for (i = 0; i < pmkid->NumCandidates; i++) {
		PMKID_CANDIDATE *p = &pmkid->CandidateList[i];
		wpa_printf(MSG_DEBUG, "NDIS: %d: " MACSTR " Flags 0x%x",
			   i, MAC2STR(p->BSSID), (int) p->Flags);
		memcpy(event.pmkid_candidate.bssid, p->BSSID, ETH_ALEN);
		event.pmkid_candidate.index = i;
		event.pmkid_candidate.preauth =
			p->Flags & NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED;
		wpa_supplicant_event(drv->ctx, EVENT_PMKID_CANDIDATE,
				     &event);
	}
}


/* Called when driver calls NdisMIndicateStatus() with
 * NDIS_STATUS_MEDIA_SPECIFIC_INDICATION */
void wpa_driver_ndis_event_media_specific(struct wpa_driver_ndis_data *drv,
					  const u8 *data, size_t data_len)
{
	NDIS_802_11_STATUS_INDICATION *status;

	if (data == NULL || data_len < sizeof(*status))
		return;

	wpa_hexdump(MSG_DEBUG, "NDIS: Media Specific Indication",
		    data, data_len);

	status = (NDIS_802_11_STATUS_INDICATION *) data;
	data += sizeof(status);
	data_len -= sizeof(status);

	switch (status->StatusType) {
	case Ndis802_11StatusType_Authentication:
		wpa_driver_ndis_event_auth(drv, data, data_len);
		break;
	case Ndis802_11StatusType_PMKID_CandidateList:
		wpa_driver_ndis_event_pmkid(drv, data, data_len);
		break;
	default:
		wpa_printf(MSG_DEBUG, "NDIS: Unknown StatusType %d",
			   (int) status->StatusType);
		break;
	}
}


static void
wpa_driver_ndis_get_wpa_capability(struct wpa_driver_ndis_data *drv)
{
	wpa_printf(MSG_DEBUG, "NDIS: verifying driver WPA capability");

	if (ndis_set_auth_mode(drv, Ndis802_11AuthModeWPA) == 0 &&
	    ndis_get_auth_mode(drv) == Ndis802_11AuthModeWPA) {
		wpa_printf(MSG_DEBUG, "NDIS: WPA key management supported");
		drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA;
	}

	if (ndis_set_auth_mode(drv, Ndis802_11AuthModeWPAPSK) == 0 &&
	    ndis_get_auth_mode(drv) == Ndis802_11AuthModeWPAPSK) {
		wpa_printf(MSG_DEBUG, "NDIS: WPA-PSK key management "
			   "supported");
		drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK;
	}

	if (ndis_set_encr_status(drv, Ndis802_11Encryption3Enabled) == 0 &&
	    ndis_get_encr_status(drv) == Ndis802_11Encryption3KeyAbsent) {
		wpa_printf(MSG_DEBUG, "NDIS: CCMP encryption supported");
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_CCMP;
	}

	if (ndis_set_encr_status(drv, Ndis802_11Encryption2Enabled) == 0 &&
	    ndis_get_encr_status(drv) == Ndis802_11Encryption2KeyAbsent) {
		wpa_printf(MSG_DEBUG, "NDIS: TKIP encryption supported");
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_TKIP;
	}

	if (ndis_set_encr_status(drv, Ndis802_11Encryption1Enabled) == 0 &&
	    ndis_get_encr_status(drv) == Ndis802_11Encryption1KeyAbsent) {
		wpa_printf(MSG_DEBUG, "NDIS: WEP encryption supported");
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_WEP40 |
			WPA_DRIVER_CAPA_ENC_WEP104;
	}

	if (ndis_set_auth_mode(drv, Ndis802_11AuthModeShared) == 0 &&
	    ndis_get_auth_mode(drv) == Ndis802_11AuthModeShared) {
		drv->capa.auth |= WPA_DRIVER_AUTH_SHARED;
	}

	if (ndis_set_auth_mode(drv, Ndis802_11AuthModeOpen) == 0 &&
	    ndis_get_auth_mode(drv) == Ndis802_11AuthModeOpen) {
		drv->capa.auth |= WPA_DRIVER_AUTH_OPEN;
	}

	ndis_set_encr_status(drv, Ndis802_11EncryptionDisabled);

	/* Could also verify OID_802_11_ADD_KEY error reporting and
	 * support for OID_802_11_ASSOCIATION_INFORMATION. */

	if (drv->capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA &&
	    drv->capa.enc & (WPA_DRIVER_CAPA_ENC_TKIP |
			     WPA_DRIVER_CAPA_ENC_CCMP)) {
		wpa_printf(MSG_DEBUG, "NDIS: driver supports WPA");
		drv->has_capability = 1;
	} else {
		wpa_printf(MSG_DEBUG, "NDIS: no WPA support found");
	}

	wpa_printf(MSG_DEBUG, "NDIS: driver capabilities: key_mgmt 0x%x "
		   "enc 0x%x auth 0x%x",
		   drv->capa.key_mgmt, drv->capa.enc, drv->capa.auth);
}


static void wpa_driver_ndis_get_capability(struct wpa_driver_ndis_data *drv)
{
	char buf[512];
	int len, i;
	NDIS_802_11_CAPABILITY *c;

	drv->capa.flags = WPA_DRIVER_FLAGS_DRIVER_IE |
		WPA_DRIVER_FLAGS_SET_KEYS_AFTER_ASSOC;

	len = ndis_get_oid(drv, OID_802_11_CAPABILITY, buf, sizeof(buf));
	if (len < 0) {
		wpa_driver_ndis_get_wpa_capability(drv);
		return;
	}

	wpa_hexdump(MSG_MSGDUMP, "OID_802_11_CAPABILITY", buf, len);
	c = (NDIS_802_11_CAPABILITY *) buf;
	if (len < sizeof(*c) || c->Version != 2) {
		wpa_printf(MSG_DEBUG, "NDIS: unsupported "
			   "OID_802_11_CAPABILITY data");
		return;
	}
	wpa_printf(MSG_DEBUG, "NDIS: Driver supports OID_802_11_CAPABILITY - "
		   "NoOfPMKIDs %d NoOfAuthEncrPairs %d",
		   (int) c->NoOfPMKIDs, (int) c->NoOfAuthEncryptPairSupported);
	drv->has_capability = 1;
	drv->no_of_pmkid = c->NoOfPMKIDs;
	for (i = 0; i < c->NoOfAuthEncryptPairSupported; i++) {
		NDIS_802_11_AUTHENTICATION_ENCRYPTION *ae;
		ae = &c->AuthenticationEncryptionSupported[i];
		if ((char *) (ae + 1) > buf + len) {
			wpa_printf(MSG_DEBUG, "NDIS: auth/encr pair list "
				   "overflow");
			break;
		}
		wpa_printf(MSG_MSGDUMP, "NDIS: %d - auth %d encr %d",
			   i, (int) ae->AuthModeSupported,
			   (int) ae->EncryptStatusSupported);
		switch (ae->AuthModeSupported) {
		case Ndis802_11AuthModeOpen:
			drv->capa.auth |= WPA_DRIVER_AUTH_OPEN;
			break;
		case Ndis802_11AuthModeShared:
			drv->capa.auth |= WPA_DRIVER_AUTH_SHARED;
			break;
		case Ndis802_11AuthModeWPA:
			drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA;
			break;
		case Ndis802_11AuthModeWPAPSK:
			drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK;
			break;
		case Ndis802_11AuthModeWPA2:
			drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA2;
			break;
		case Ndis802_11AuthModeWPA2PSK:
			drv->capa.key_mgmt |=
				WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK;
			break;
		case Ndis802_11AuthModeWPANone:
			drv->capa.key_mgmt |=
				WPA_DRIVER_CAPA_KEY_MGMT_WPA_NONE;
			break;
		default:
			break;
		}
		switch (ae->EncryptStatusSupported) {
		case Ndis802_11Encryption1Enabled:
			drv->capa.enc |= WPA_DRIVER_CAPA_ENC_WEP40;
			drv->capa.enc |= WPA_DRIVER_CAPA_ENC_WEP104;
			break;
		case Ndis802_11Encryption2Enabled:
			drv->capa.enc |= WPA_DRIVER_CAPA_ENC_TKIP;
			break;
		case Ndis802_11Encryption3Enabled:
			drv->capa.enc |= WPA_DRIVER_CAPA_ENC_CCMP;
			break;
		default:
			break;
		}
	}

	wpa_printf(MSG_DEBUG, "NDIS: driver capabilities: key_mgmt 0x%x "
		   "enc 0x%x auth 0x%x",
		   drv->capa.key_mgmt, drv->capa.enc, drv->capa.auth);
}


static int wpa_driver_ndis_get_capa(void *priv, struct wpa_driver_capa *capa)
{
	struct wpa_driver_ndis_data *drv = priv;
	if (!drv->has_capability)
		return -1;
	memcpy(capa, &drv->capa, sizeof(*capa));
	return 0;
}


static const char * wpa_driver_ndis_get_ifname(void *priv)
{
	struct wpa_driver_ndis_data *drv = priv;
	return drv->ifname;
}


static const u8 * wpa_driver_ndis_get_mac_addr(void *priv)
{
	struct wpa_driver_ndis_data *drv = priv;
	return drv->own_addr;
}


static int wpa_driver_ndis_get_names(struct wpa_driver_ndis_data *drv)
{
	PTSTR names, pos, pos2;
	ULONG len;
	BOOLEAN res;
	const int MAX_ADAPTERS = 32;
	char *name[MAX_ADAPTERS];
	char *desc[MAX_ADAPTERS];
	int num_name, num_desc, i, found_name, found_desc;
	size_t dlen;

	wpa_printf(MSG_DEBUG, "NDIS: Packet.dll version: %s",
		   PacketGetVersion());

	len = 8192;
	names = malloc(len);
	if (names == NULL)
		return -1;
	memset(names, 0, len);

	res = PacketGetAdapterNames(names, &len);
	if (!res && len > 8192) {
		free(names);
		names = malloc(len);
		if (names == NULL)
			return -1;
		memset(names, 0, len);
		res = PacketGetAdapterNames(names, &len);
	}

	if (!res) {
		wpa_printf(MSG_ERROR, "NDIS: Failed to get adapter list "
			   "(PacketGetAdapterNames)");
		free(names);
		return -1;
	}

	/* wpa_hexdump_ascii(MSG_DEBUG, "NDIS: AdapterNames", names, len); */

	if (names[0] && names[1] == '\0' && names[2] && names[3] == '\0') {
		wpa_printf(MSG_DEBUG, "NDIS: Looks like adapter names are in "
			   "UNICODE");
		/* Convert to ASCII */
		pos2 = pos = names;
		while (pos2 < names + len) {
			if (pos2[0] == '\0' && pos2[1] == '\0' &&
			    pos2[2] == '\0' && pos2[3] == '\0') {
				pos2 += 4;
				break;
			}
			*pos++ = pos2[0];
			pos2 += 2;
		}
		memcpy(pos + 2, names, pos - names);
		pos += 2;
	} else
		pos = names;

	num_name = 0;
	while (pos < names + len) {
		name[num_name] = pos;
		while (*pos && pos < names + len)
			pos++;
		if (pos + 1 >= names + len) {
			free(names);
			return -1;
		}
		pos++;
		num_name++;
		if (num_name >= MAX_ADAPTERS) {
			wpa_printf(MSG_DEBUG, "NDIS: Too many adapters");
			free(names);
			return -1;
		}
		if (*pos == '\0') {
			wpa_printf(MSG_DEBUG, "NDIS: %d adapter names found",
				   num_name);
			pos++;
			break;
		}
	}

	num_desc = 0;
	while (pos < names + len) {
		desc[num_desc] = pos;
		while (*pos && pos < names + len)
			pos++;
		if (pos + 1 >= names + len) {
			free(names);
			return -1;
		}
		pos++;
		num_desc++;
		if (num_desc >= MAX_ADAPTERS) {
			wpa_printf(MSG_DEBUG, "NDIS: Too many adapter "
				   "descriptions");
			free(names);
			return -1;
		}
		if (*pos == '\0') {
			wpa_printf(MSG_DEBUG, "NDIS: %d adapter descriptions "
				   "found", num_name);
			pos++;
			break;
		}
	}

	/*
	 * Windows 98 with Packet.dll 3.0 alpha3 does not include adapter
	 * descriptions. Fill in dummy descriptors to work around this.
	 */
	while (num_desc < num_name)
		desc[num_desc++] = "dummy description";

	if (num_name != num_desc) {
		wpa_printf(MSG_DEBUG, "NDIS: mismatch in adapter name and "
			   "description counts (%d != %d)",
			   num_name, num_desc);
		free(names);
		return -1;
	}

	found_name = found_desc = -1;
	for (i = 0; i < num_name; i++) {
		wpa_printf(MSG_DEBUG, "NDIS: %d - %s - %s",
			   i, name[i], desc[i]);
		if (found_name == -1 && strcmp(name[i], drv->ifname) == 0) {
			found_name = i;
		}
		if (found_desc == -1 &&
		    strncmp(desc[i], drv->ifname, strlen(drv->ifname)) == 0) {
			found_desc = i;
		}
	}

	if (found_name < 0 && found_desc >= 0) {
		wpa_printf(MSG_DEBUG, "NDIS: Matched interface '%s' based on "
			   "description '%s'",
			   name[found_desc], desc[found_desc]);
		found_name = found_desc;
		strncpy(drv->ifname, name[found_desc], sizeof(drv->ifname));
	}

	if (found_name < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: Could not find interface '%s'",
			   drv->ifname);
		free(names);
		return -1;
	}

	i = found_name;
	pos = strchr(desc[i], '(');
	if (pos) {
		dlen = pos - desc[i];
		pos--;
		if (pos > desc[i] && *pos == ' ')
			dlen--;
	} else {
		dlen = strlen(desc[i]);
	}
	drv->adapter_desc = malloc(dlen + 1);
	if (drv->adapter_desc) {
		memcpy(drv->adapter_desc, desc[i], dlen);
		drv->adapter_desc[dlen] = '\0';
	}

	free(names);

	if (drv->adapter_desc == NULL)
		return -1;

	wpa_printf(MSG_DEBUG, "NDIS: Adapter description prefix '%s'",
		   drv->adapter_desc);

	return 0;
}


static void * wpa_driver_ndis_init(void *ctx, const char *ifname)
{
	struct wpa_driver_ndis_data *drv;
	u32 mode;

	drv = malloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	memset(drv, 0, sizeof(*drv));
	drv->ctx = ctx;
	strncpy(drv->ifname, ifname, sizeof(drv->ifname));
	drv->event_sock = -1;

	if (wpa_driver_ndis_get_names(drv) < 0) {
		free(drv);
		return NULL;
	}

	drv->adapter = PacketOpenAdapter(drv->ifname);
	if (drv->adapter == NULL) {
		wpa_printf(MSG_DEBUG, "NDIS: PacketOpenAdapter failed for "
			   "'%s'", drv->ifname);
		free(drv);
		return NULL;
	}

	if (ndis_get_oid(drv, OID_802_3_CURRENT_ADDRESS,
			 drv->own_addr, ETH_ALEN) < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: Get OID_802_3_CURRENT_ADDRESS "
			   "failed");
		PacketCloseAdapter(drv->adapter);
		free(drv);
		return NULL;
	}
	wpa_driver_ndis_get_capability(drv);

	/* Make sure that the driver does not have any obsolete PMKID entries.
	 */
	wpa_driver_ndis_flush_pmkid(drv);

	eloop_register_timeout(1, 0, wpa_driver_ndis_poll_timeout, drv, NULL);

	wpa_driver_register_event_cb(drv);

	/* Set mode here in case card was configured for ad-hoc mode
	 * previously. */
	mode = Ndis802_11Infrastructure;
	if (ndis_set_oid(drv, OID_802_11_INFRASTRUCTURE_MODE,
			 (char *) &mode, sizeof(mode)) < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: Failed to set "
			   "OID_802_11_INFRASTRUCTURE_MODE (%d)",
			   (int) mode);
		/* Try to continue anyway */

		if (!drv->has_capability && drv->capa.enc == 0) {
			wpa_printf(MSG_DEBUG, "NDIS: Driver did not provide "
				   "any wireless capabilities - assume it is "
				   "a wired interface");
			drv->wired = 1;
		}
	}

	return drv;
}


static void wpa_driver_ndis_deinit(void *priv)
{
	struct wpa_driver_ndis_data *drv = priv;
	eloop_cancel_timeout(wpa_driver_ndis_poll_timeout, drv, NULL);
	wpa_driver_ndis_flush_pmkid(drv);
	wpa_driver_ndis_disconnect(drv);
	if (wpa_driver_ndis_radio_off(drv) < 0) {
		wpa_printf(MSG_DEBUG, "NDIS: failed to disassociate and turn "
			   "radio off");
	}
	if (drv->event_sock >= 0) {
		eloop_unregister_read_sock(drv->event_sock);
		close(drv->event_sock);
	}
	if (drv->adapter)
		PacketCloseAdapter(drv->adapter);

	free(drv->adapter_desc);
	free(drv);
}


const struct wpa_driver_ops wpa_driver_ndis_ops = {
	.name = "ndis",
	.desc = "Windows NDIS driver",
	.init = wpa_driver_ndis_init,
	.deinit = wpa_driver_ndis_deinit,
	.set_wpa = wpa_driver_ndis_set_wpa,
	.scan = wpa_driver_ndis_scan,
	.get_scan_results = wpa_driver_ndis_get_scan_results,
	.get_bssid = wpa_driver_ndis_get_bssid,
	.get_ssid = wpa_driver_ndis_get_ssid,
	.set_key = wpa_driver_ndis_set_key,
	.associate = wpa_driver_ndis_associate,
	.deauthenticate = wpa_driver_ndis_deauthenticate,
	.disassociate = wpa_driver_ndis_disassociate,
	.poll = wpa_driver_ndis_poll,
	.add_pmkid = wpa_driver_ndis_add_pmkid,
	.remove_pmkid = wpa_driver_ndis_remove_pmkid,
	.flush_pmkid = wpa_driver_ndis_flush_pmkid,
	.get_capa = wpa_driver_ndis_get_capa,
	.get_ifname = wpa_driver_ndis_get_ifname,
	.get_mac_addr = wpa_driver_ndis_get_mac_addr,
};
