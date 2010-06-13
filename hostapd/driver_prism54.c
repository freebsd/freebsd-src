/*
 * hostapd / Driver interaction with Prism54 PIMFOR interface
 * Copyright (c) 2004, Bell Kin <bell_kin@pek.com.tw>
 * based on hostap driver.c, ieee802_11.c
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
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

#include "includes.h"
#include <sys/ioctl.h>
#include <sys/select.h>

#ifdef USE_KERNEL_HEADERS
/* compat-wireless does not include linux/compiler.h to define __user, so
 * define it here */
#ifndef __user
#define __user
#endif /* __user */
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#include <linux/if_arp.h>
#include <linux/wireless.h>
#else /* USE_KERNEL_HEADERS */
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include "wireless_copy.h"
#endif /* USE_KERNEL_HEADERS */

#include "hostapd.h"
#include "driver.h"
#include "ieee802_1x.h"
#include "eloop.h"
#include "ieee802_11.h"
#include "prism54.h"
#include "wpa.h"
#include "radius/radius.h"
#include "sta_info.h"
#include "accounting.h"

const int PIM_BUF_SIZE = 4096;

struct prism54_driver_data {
	struct hostapd_data *hapd;
	char iface[IFNAMSIZ + 1];
	int sock; /* raw packet socket for 802.3 access */
	int pim_sock; /* socket for pimfor packet */
	char macs[2007][6];
};


static int mac_id_refresh(struct prism54_driver_data *data, int id, char *mac)
{
	if (id < 0 || id > 2006) {
		return -1;
	}
	memcpy(&data->macs[id][0], mac, ETH_ALEN);
	return 0;
}


static char * mac_id_get(struct prism54_driver_data *data, int id)
{
	if (id < 0 || id > 2006) {
		return NULL;
	}
	return &data->macs[id][0];
}


/* wait for a specific pimfor, timeout in 10ms resolution */
/* pim_sock must be non-block to prevent dead lock from no response */
/* or same response type in series */
static int prism54_waitpim(void *priv, unsigned long oid, void *buf, int len,
			   int timeout)
{
	struct prism54_driver_data *drv = priv;
	struct timeval tv, stv, ctv;
	fd_set pfd;
	int rlen;
	pimdev_hdr *pkt;

	pkt = malloc(8192);
	if (pkt == NULL)
		return -1;

	FD_ZERO(&pfd);
	gettimeofday(&stv, NULL);
	do {
		FD_SET(drv->pim_sock, &pfd);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		if (select(drv->pim_sock + 1, &pfd, NULL, NULL, &tv)) {
			rlen = recv(drv->pim_sock, pkt, 8192, 0);
			if (rlen > 0) {
				if (pkt->oid == htonl(oid)) {
					if (rlen <= len) {
						if (buf != NULL) {
							memcpy(buf, pkt, rlen);
						}
						free(pkt);
						return rlen;
					} else {
						printf("buffer too small\n");
						free(pkt);
						return -1;
					}
				} else {
					gettimeofday(&ctv, NULL);
					continue;
				}
			}
		}
		gettimeofday(&ctv, NULL);
	} while (((ctv.tv_sec - stv.tv_sec) * 100 +
		  (ctv.tv_usec - stv.tv_usec) / 10000) > timeout);
	free(pkt);
	return 0;
}


/* send an eapol packet */
static int prism54_send_eapol(void *priv, const u8 *addr,
			      const u8 *data, size_t data_len, int encrypt,
			      const u8 *own_addr)
{
	struct prism54_driver_data *drv = priv;
	ieee802_3_hdr *hdr;
	size_t len;
	u8 *pos;
	int res;

	len = sizeof(*hdr) + data_len;
	hdr = os_zalloc(len);
	if (hdr == NULL) {
		printf("malloc() failed for prism54_send_data(len=%lu)\n",
		       (unsigned long) len);
		return -1;
	}

	memcpy(&hdr->da[0], addr, ETH_ALEN);
	memcpy(&hdr->sa[0], own_addr, ETH_ALEN);
	hdr->type = htons(ETH_P_PAE);
	pos = (u8 *) (hdr + 1);
	memcpy(pos, data, data_len);

	res = send(drv->sock, hdr, len, 0);
	free(hdr);

	if (res < 0) {
		perror("hostapd_send_eapol: send");
		printf("hostapd_send_eapol - packet len: %lu - failed\n",
		       (unsigned long) len);
	}

	return res;
}


/* open data channel(auth-1) or eapol only(unauth-0) */
static int prism54_set_sta_authorized(void *priv, const u8 *addr,
				      int authorized)
{
	struct prism54_driver_data *drv = priv;
	pimdev_hdr *hdr;
	char *pos;

	hdr = malloc(sizeof(*hdr) + ETH_ALEN);
	if (hdr == NULL)
		return -1;
	hdr->op = htonl(PIMOP_SET);
	if (authorized) {
		hdr->oid = htonl(DOT11_OID_EAPAUTHSTA);
	} else {
		hdr->oid = htonl(DOT11_OID_EAPUNAUTHSTA);
	}
	pos = (char *) (hdr + 1);
	memcpy(pos, addr, ETH_ALEN);
	send(drv->pim_sock, hdr, sizeof(*hdr) + ETH_ALEN, 0);
	prism54_waitpim(priv, hdr->oid, hdr, sizeof(*hdr) + ETH_ALEN, 10);
	free(hdr);
	return 0;
}


static int
prism54_sta_set_flags(void *priv, const u8 *addr, int total_flags,
		      int flags_or, int flags_and)
{
	/* For now, only support setting Authorized flag */
	if (flags_or & WLAN_STA_AUTHORIZED)
		return prism54_set_sta_authorized(priv, addr, 1);
	if (flags_and & WLAN_STA_AUTHORIZED)
		return prism54_set_sta_authorized(priv, addr, 0);
	return 0;
}


/* set per station key */
static int prism54_set_encryption(const char *ifname, void *priv,
				  const char *alg, const u8 *addr,
				  int idx, const u8 *key, size_t key_len,
				  int txkey)
{
	struct prism54_driver_data *drv = priv;
	pimdev_hdr *hdr;
	struct obj_stakey *keys;
	u8 *buf;
	size_t blen;
	int ret = 0;

	blen = sizeof(struct obj_stakey) + sizeof(pimdev_hdr);
	hdr = malloc(blen);
	if (hdr == NULL) {
		printf("memory low\n");
		return -1;
	}
	keys = (struct obj_stakey *) &hdr[1];
	if (!addr) {
		memset(&keys->address[0], 0xff, ETH_ALEN);
	} else {
		memcpy(&keys->address[0], addr, ETH_ALEN);
	}
	if (!strcmp(alg, "WEP")) {
		keys->type = DOT11_PRIV_WEP;
	} else if (!strcmp(alg, "TKIP")) {
		keys->type = DOT11_PRIV_TKIP;
	} else if (!strcmp(alg, "none")) {
		/* the only way to clear the key is to deauth it */
		/* and prism54 is capable to receive unencrypted packet */
		/* so we do nothing here */
		free(hdr);
		return 0;
	} else {
		printf("bad auth type: %s\n", alg);
	}
	buf = (u8 *) &keys->key[0];
	keys->length = key_len;
	keys->keyid = idx;
	keys->options = htons(DOT11_STAKEY_OPTION_DEFAULTKEY);
	keys->reserved = 0;

	hdr->op = htonl(PIMOP_SET);
	hdr->oid = htonl(DOT11_OID_STAKEY);

	memcpy(buf, key, key_len);
	
	ret = send(drv->pim_sock, hdr, blen, 0);
	if (ret < 0) {
		free(hdr);
		return ret;
	}
	prism54_waitpim(priv, hdr->oid, hdr, blen, 10);

	free(hdr);

	return 0;
}


/* get TKIP station sequence counter, prism54 is only 6 bytes */
static int prism54_get_seqnum(const char *ifname, void *priv, const u8 *addr,
			      int idx, u8 *seq)
{
	struct prism54_driver_data *drv = priv;
	struct obj_stasc *stasc;
	pimdev_hdr *hdr;
	size_t blen;
	int ret = 0;

	blen = sizeof(*stasc) + sizeof(*hdr);
	hdr = malloc(blen);
	if (hdr == NULL)
		return -1;

	stasc = (struct obj_stasc *) &hdr[1];
	
	if (addr == NULL)
		memset(&stasc->address[0], 0xff, ETH_ALEN);
	else
		memcpy(&stasc->address[0], addr, ETH_ALEN);

	hdr->oid = htonl(DOT11_OID_STASC);
	hdr->op = htonl(PIMOP_GET);
	stasc->keyid = idx;
	if (send(drv->pim_sock,hdr,blen,0) <= 0) {
		free(hdr);
		return -1;
	}
	if (prism54_waitpim(priv, DOT11_OID_STASC, hdr, blen, 10) <= 0) {
		ret = -1;
	} else {
		if (hdr->op == (int) htonl(PIMOP_RESPONSE)) {
			memcpy(seq + 2, &stasc->sc_high, ETH_ALEN);
			memset(seq, 0, 2);
		} else {
			ret = -1;
		}
	}
	free(hdr);

	return ret;
}


/* include unencrypted, set mlme autolevel to extended */
static int prism54_init_1x(void *priv)
{
	struct prism54_driver_data *drv = priv;
	pimdev_hdr *hdr;
	unsigned long *ul;
	int blen = sizeof(*hdr) + sizeof(*ul);

	hdr = malloc(blen);
	if (hdr == NULL)
		return -1;

	ul = (unsigned long *) &hdr[1];
	hdr->op = htonl(PIMOP_SET);
	hdr->oid = htonl(DOT11_OID_EXUNENCRYPTED);
	*ul = htonl(DOT11_BOOL_TRUE); /* not accept */
	send(drv->pim_sock, hdr, blen, 0);
	prism54_waitpim(priv, DOT11_OID_EXUNENCRYPTED, hdr, blen, 10);
	hdr->op = htonl(PIMOP_SET);
	hdr->oid = htonl(DOT11_OID_MLMEAUTOLEVEL);
	*ul = htonl(DOT11_MLME_EXTENDED);
	send(drv->pim_sock, hdr, blen, 0);
	prism54_waitpim(priv, DOT11_OID_MLMEAUTOLEVEL, hdr, blen, 10);
	hdr->op = htonl(PIMOP_SET);
	hdr->oid = htonl(DOT11_OID_DOT1XENABLE);
	*ul = htonl(DOT11_BOOL_TRUE);
	send(drv->pim_sock, hdr, blen, 0);
	prism54_waitpim(priv, DOT11_OID_DOT1XENABLE, hdr, blen, 10);
	hdr->op = htonl(PIMOP_SET);
	hdr->oid = htonl(DOT11_OID_AUTHENABLE);
	*ul = htonl(DOT11_AUTH_OS); /* OS */
	send(drv->pim_sock, hdr, blen, 0);
	prism54_waitpim(priv, DOT11_OID_AUTHENABLE, hdr, blen, 10);
	free(hdr);
	return 0;
}


static int prism54_set_privacy_invoked(const char *ifname, void *priv,
				       int flag)
{
	struct prism54_driver_data *drv = priv;
	pimdev_hdr *hdr;
	unsigned long *ul;
	int ret;
	int blen = sizeof(*hdr) + sizeof(*ul);
	hdr = malloc(blen);
	if (hdr == NULL)
		return -1;
	ul = (unsigned long *) &hdr[1];
	hdr->op = htonl(PIMOP_SET);
	hdr->oid = htonl(DOT11_OID_PRIVACYINVOKED);
	if (flag) {
		*ul = htonl(DOT11_BOOL_TRUE); /* has privacy */
	} else {
		*ul = 0;
	}
	ret = send(drv->pim_sock, hdr, blen, 0);
	if (ret >= 0) {
		ret = prism54_waitpim(priv, DOT11_OID_PRIVACYINVOKED, hdr,
				      blen, 10);
	}
	free(hdr);
	return ret;
}

 
static int prism54_ioctl_setiwessid(const char *ifname, void *priv,
				    const u8 *buf, int len)
{
#if 0
	struct prism54_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.flags = 1; /* SSID active */
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len + 1;

	if (ioctl(drv->pim_sock, SIOCSIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCSIWESSID]");
		printf("len=%d\n", len);
		return -1;
	}
#endif
	return 0;
}


/* kick all stations */
/* does not work during init, but at least it won't crash firmware */
static int prism54_flush(void *priv)
{
	struct prism54_driver_data *drv = priv;
	struct obj_mlmeex *mlme;
	pimdev_hdr *hdr;
	int ret;
	unsigned int i;
	long *nsta;
	int blen = sizeof(*hdr) + sizeof(*mlme);
	char *mac_id;

	hdr = os_zalloc(blen);
	if (hdr == NULL)
		return -1;

	mlme = (struct obj_mlmeex *) &hdr[1];
	nsta = (long *) &hdr[1];
	hdr->op = htonl(PIMOP_GET);
	hdr->oid = htonl(DOT11_OID_CLIENTS);
	ret = send(drv->pim_sock, hdr, sizeof(*hdr) + sizeof(long), 0);
	ret = prism54_waitpim(priv, DOT11_OID_CLIENTS, hdr, blen, 10);
	if ((ret < 0) || (hdr->op != (int) htonl(PIMOP_RESPONSE)) ||
	    (le_to_host32(*nsta) > 2007)) {
		free(hdr);
		return 0;
	}
	for (i = 0; i < le_to_host32(*nsta); i++) {
		mlme->id = -1;
		mac_id = mac_id_get(drv, i);
		if (mac_id)
			memcpy(&mlme->address[0], mac_id, ETH_ALEN);
		mlme->code = host_to_le16(WLAN_REASON_UNSPECIFIED);
		mlme->state = htons(DOT11_STATE_NONE);
		mlme->size = 0;
		hdr->op = htonl(PIMOP_SET);
		hdr->oid = htonl(DOT11_OID_DISASSOCIATEEX);
		ret = send(drv->pim_sock, hdr, blen, 0);
		prism54_waitpim(priv, DOT11_OID_DISASSOCIATEEX, hdr, blen,
				100);
	}
	for (i = 0; i < le_to_host32(*nsta); i++) {
		mlme->id = -1;
		mac_id = mac_id_get(drv, i);
		if (mac_id)
			memcpy(&mlme->address[0], mac_id, ETH_ALEN);
		mlme->code = host_to_le16(WLAN_REASON_UNSPECIFIED);
		mlme->state = htons(DOT11_STATE_NONE);
		mlme->size = 0;
		hdr->op = htonl(PIMOP_SET);
		hdr->oid = htonl(DOT11_OID_DEAUTHENTICATEEX);
		ret = send(drv->pim_sock, hdr, blen, 0);
		prism54_waitpim(priv, DOT11_OID_DEAUTHENTICATEEX, hdr, blen,
				100);
	}
	free(hdr);
	return 0;
}


static int prism54_sta_deauth(void *priv, const u8 *addr, int reason)
{
	struct prism54_driver_data *drv = priv;
	pimdev_hdr *hdr;
	struct obj_mlmeex *mlme;
	int ret;
	int blen = sizeof(*hdr) + sizeof(*mlme);
	hdr = malloc(blen);
	if (hdr == NULL)
		return -1;
	mlme = (struct obj_mlmeex *) &hdr[1];
	hdr->op = htonl(PIMOP_SET);
	hdr->oid = htonl(DOT11_OID_DEAUTHENTICATEEX);
	memcpy(&mlme->address[0], addr, ETH_ALEN);
	mlme->id = -1;
	mlme->state = htons(DOT11_STATE_NONE);
	mlme->code = host_to_le16(reason);
	mlme->size = 0;
	ret = send(drv->pim_sock, hdr, blen, 0);
	prism54_waitpim(priv, DOT11_OID_DEAUTHENTICATEEX, hdr, blen, 10);
	free(hdr);
	return ret;
}


static int prism54_sta_disassoc(void *priv, const u8 *addr, int reason)
{
	struct prism54_driver_data *drv = priv;
        pimdev_hdr *hdr;
        struct obj_mlmeex *mlme;
	int ret;
        int blen = sizeof(*hdr) + sizeof(*mlme);
        hdr = malloc(blen);
	if (hdr == NULL)
		return -1;
        mlme = (struct obj_mlmeex *) &hdr[1];
        hdr->op = htonl(PIMOP_SET);
        hdr->oid = htonl(DOT11_OID_DISASSOCIATEEX);
        memcpy(&mlme->address[0], addr, ETH_ALEN);
        mlme->id = -1;
        mlme->state = htons(DOT11_STATE_NONE);
        mlme->code = host_to_le16(reason);
	mlme->size = 0;
        ret = send(drv->pim_sock, hdr, blen, 0);
        prism54_waitpim(priv, DOT11_OID_DISASSOCIATEEX, hdr, blen, 10);
        free(hdr);
        return ret;
}


static int prism54_get_inact_sec(void *priv, const u8 *addr)
{
	struct prism54_driver_data *drv = priv;
	pimdev_hdr *hdr;
	struct obj_sta *sta;
	int blen = sizeof(*hdr) + sizeof(*sta);
	int ret;

	hdr = malloc(blen);
	if (hdr == NULL)
		return -1;
	hdr->op = htonl(PIMOP_GET);
	hdr->oid = htonl(DOT11_OID_CLIENTFIND);
	sta = (struct obj_sta *) &hdr[1];
	memcpy(&sta->address[0], addr, ETH_ALEN);
	ret = send(drv->pim_sock, hdr, blen, 0);
	ret = prism54_waitpim(priv, DOT11_OID_CLIENTFIND, hdr, blen, 10);
	if (ret != blen) {
		printf("get_inact_sec: bad return %d\n", ret);
		free(hdr);
		return -1;
	}
	if (hdr->op != (int) htonl(PIMOP_RESPONSE)) {
		printf("get_inact_sec: bad resp\n");
		free(hdr);
		return -1;
	}
	free(hdr);
	return le_to_host16(sta->age);
}


/* set attachments */
static int prism54_set_generic_elem(const char *ifname, void *priv,
				    const u8 *elem, size_t elem_len)
{
	struct prism54_driver_data *drv = priv;
	pimdev_hdr *hdr;
	char *pos;
	struct obj_attachment_hdr *attach;
	size_t blen = sizeof(*hdr) + sizeof(*attach) + elem_len;
	hdr = os_zalloc(blen);
	if (hdr == NULL) {
		printf("%s: memory low\n", __func__);
		return -1;
	}
	hdr->op = htonl(PIMOP_SET);
	hdr->oid = htonl(DOT11_OID_ATTACHMENT);
	attach = (struct obj_attachment_hdr *)&hdr[1];
	attach->type = DOT11_PKT_BEACON;
	attach->id = -1;
	attach->size = host_to_le16((short)elem_len);
	pos = ((char*) attach) + sizeof(*attach);
	if (elem)
		memcpy(pos, elem, elem_len);
	send(drv->pim_sock, hdr, blen, 0);
	attach->type = DOT11_PKT_PROBE_RESP;
	send(drv->pim_sock, hdr, blen, 0);
	free(hdr);
	return 0;
}


/* tell the card to auth the sta */
static void prism54_handle_probe(struct prism54_driver_data *drv,
				 void *buf, size_t len)
{
	struct obj_mlmeex *mlme;
	pimdev_hdr *hdr;
	struct sta_info *sta;
	hdr = (pimdev_hdr *)buf;
	mlme = (struct obj_mlmeex *) &hdr[1];
	sta = ap_get_sta(drv->hapd, (u8 *) &mlme->address[0]);
	if (sta != NULL) {
		if (sta->flags & (WLAN_STA_AUTH | WLAN_STA_ASSOC))
			return;
	}
	if (len < sizeof(*mlme)) {
		printf("bad probe packet\n");
		return;
	}
	mlme->state = htons(DOT11_STATE_AUTHING);
	mlme->code = 0;
	hdr->op = htonl(PIMOP_SET);
	hdr->oid = htonl(DOT11_OID_AUTHENTICATEEX);
	mlme->size = 0;
	send(drv->pim_sock, hdr, sizeof(*hdr)+sizeof(*mlme), 0);
}


static void prism54_handle_deauth(struct prism54_driver_data *drv,
				  void *buf, size_t len)
{
	struct obj_mlme *mlme;
	pimdev_hdr *hdr;
	struct sta_info *sta;
	char *mac_id;

	hdr = (pimdev_hdr *) buf;
	mlme = (struct obj_mlme *) &hdr[1];
	sta = ap_get_sta(drv->hapd, (u8 *) &mlme->address[0]);
	mac_id = mac_id_get(drv, mlme->id);
	if (sta == NULL || mac_id == NULL)
		return;
	memcpy(&mlme->address[0], mac_id, ETH_ALEN);
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DEAUTH);
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	ap_free_sta(drv->hapd, sta);
}


static void prism54_handle_disassoc(struct prism54_driver_data *drv,
				    void *buf, size_t len)
{
	struct obj_mlme *mlme;
	pimdev_hdr *hdr;
	struct sta_info *sta;
	char *mac_id;

	hdr = (pimdev_hdr *) buf;
	mlme = (struct obj_mlme *) &hdr[1];
	mac_id = mac_id_get(drv, mlme->id);
	if (mac_id == NULL)
		return;
	memcpy(&mlme->address[0], mac_id, ETH_ALEN);
	sta = ap_get_sta(drv->hapd, (u8 *) &mlme->address[0]);
	if (sta == NULL) {
		return;
	}
	sta->flags &= ~WLAN_STA_ASSOC;
	wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	accounting_sta_stop(drv->hapd, sta);
	ieee802_1x_free_station(sta);
}


/* to auth it, just allow it now, later for os/sk */
static void prism54_handle_auth(struct prism54_driver_data *drv,
				void *buf, size_t len)
{
	struct obj_mlmeex *mlme;
	pimdev_hdr *hdr;
	struct sta_info *sta;
	int resp;

	hdr = (pimdev_hdr *) buf;
	mlme = (struct obj_mlmeex *) &hdr[1];
	if (len < sizeof(*mlme)) {
		printf("bad auth packet\n");
		return;
	}

	if (mlme->state == htons(DOT11_STATE_AUTHING)) {
		sta = ap_sta_add(drv->hapd, (u8 *) &mlme->address[0]);
		if (drv->hapd->tkip_countermeasures) {
			resp = WLAN_REASON_MICHAEL_MIC_FAILURE;
			goto fail;
		}
		mac_id_refresh(drv, mlme->id, &mlme->address[0]);
		if (!sta) {
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		sta->flags &= ~WLAN_STA_PREAUTH;
		
		ieee802_1x_notify_pre_auth(sta->eapol_sm, 0);
		sta->flags |= WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
		mlme->code = 0;
		mlme->state=htons(DOT11_STATE_AUTH);
		hdr->op = htonl(PIMOP_SET);
		hdr->oid = htonl(DOT11_OID_AUTHENTICATEEX);
		mlme->size = 0;
		sta->timeout_next = STA_NULLFUNC;
		send(drv->pim_sock, hdr, sizeof(*hdr) + sizeof(*mlme), 0);
	}
	return;

fail:
	printf("auth fail: %x\n", resp);
	mlme->code = host_to_le16(resp);
	mlme->size = 0;
	if (sta)
		sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	hdr->oid = htonl(DOT11_OID_DEAUTHENTICATEEX);
	hdr->op = htonl(PIMOP_SET);
	send(drv->pim_sock, hdr, sizeof(*hdr)+sizeof(*mlme), 0);
}


/* do the wpa thing */
static void prism54_handle_assoc(struct prism54_driver_data *drv,
				 void *buf, size_t len)
{
	pimdev_hdr *hdr;
	struct obj_mlmeex *mlme;
	struct ieee802_11_elems elems;
	struct sta_info *sta;
	u8 *wpa_ie;
	u8 *cb;
	int ieofs = 0;
	size_t wpa_ie_len;
	int resp, new_assoc;
	char *mac_id;

	resp = 0;
	hdr = (pimdev_hdr *) buf;
	mlme = (struct obj_mlmeex *) &hdr[1];
	switch (ntohl(hdr->oid)) {
		case DOT11_OID_ASSOCIATE:
		case DOT11_OID_REASSOCIATE:
			mlme->size = 0;
		default:
			break;
	}
	if ((mlme->state == (int) htonl(DOT11_STATE_ASSOCING)) ||
	    (mlme->state == (int) htonl(DOT11_STATE_REASSOCING))) {
		if (len < sizeof(pimdev_hdr) + sizeof(struct obj_mlme)) {
			printf("bad assoc packet\n");
			return;
		}
		mac_id = mac_id_get(drv, mlme->id);
		if (mac_id == NULL)
			return;
		memcpy(&mlme->address[0], mac_id, ETH_ALEN);
		sta = ap_get_sta(drv->hapd, (u8 *) &mlme->address[0]);
		if (sta == NULL) {
			printf("cannot get sta\n");
			return;
		}
		cb = (u8 *) &mlme->data[0];
		if (hdr->oid == htonl(DOT11_OID_ASSOCIATEEX)) {
			ieofs = 4;
		} else if (hdr->oid == htonl(DOT11_OID_REASSOCIATEEX)) {
			ieofs = 10;
		}
		if (le_to_host16(mlme->size) <= ieofs) {
			printf("attach too small\n");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		if (ieee802_11_parse_elems(cb + ieofs,
					   le_to_host16(mlme->size) - ieofs,
					   &elems, 1) == ParseFailed) {
			printf("STA " MACSTR " sent invalid association "
			       "request\n", MAC2STR(sta->addr));
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		if ((drv->hapd->conf->wpa & WPA_PROTO_RSN) &&
		    elems.rsn_ie) {
			wpa_ie = elems.rsn_ie;
			wpa_ie_len = elems.rsn_ie_len;
		} else if ((drv->hapd->conf->wpa & WPA_PROTO_WPA) &&
			   elems.wpa_ie) {
			wpa_ie = elems.wpa_ie;
			wpa_ie_len = elems.wpa_ie_len;
		} else {
			wpa_ie = NULL;
			wpa_ie_len = 0;
		}
		if (drv->hapd->conf->wpa && wpa_ie == NULL) {
			printf("STA " MACSTR ": No WPA/RSN IE in association "
			       "request\n", MAC2STR(sta->addr));
			resp = WLAN_STATUS_INVALID_IE;
			goto fail;
		}
		if (drv->hapd->conf->wpa) {
			int res;
			wpa_ie -= 2;
			wpa_ie_len += 2;
			if (sta->wpa_sm == NULL)
				sta->wpa_sm = wpa_auth_sta_init(
					drv->hapd->wpa_auth, sta->addr);
			if (sta->wpa_sm == NULL) {
				printf("Failed to initialize WPA state "
				       "machine\n");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}
			res = wpa_validate_wpa_ie(drv->hapd->wpa_auth,
						  sta->wpa_sm,
						  wpa_ie, wpa_ie_len,
						  NULL, 0);
			if (res == WPA_INVALID_GROUP)
				resp = WLAN_STATUS_GROUP_CIPHER_NOT_VALID;
			else if (res == WPA_INVALID_PAIRWISE)
				resp = WLAN_STATUS_PAIRWISE_CIPHER_NOT_VALID;
			else if (res == WPA_INVALID_AKMP)
				resp = WLAN_STATUS_AKMP_NOT_VALID;
			else if (res == WPA_ALLOC_FAIL)
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			else if (res != WPA_IE_OK)
				resp = WLAN_STATUS_INVALID_IE;
			if (resp != WLAN_STATUS_SUCCESS)
				goto fail;
		}
		hdr->oid = (hdr->oid == htonl(DOT11_OID_ASSOCIATEEX)) ?
			htonl(DOT11_OID_ASSOCIATEEX) :
			htonl(DOT11_OID_REASSOCIATEEX);
		hdr->op = htonl(PIMOP_SET);
		mlme->code = 0;
		mlme->state = htons(DOT11_STATE_ASSOC);
		mlme->size = 0;
		send(drv->pim_sock, hdr, sizeof(*hdr) + sizeof(*mlme), 0);
		return;
	} else if (mlme->state==htons(DOT11_STATE_ASSOC)) {
		if (len < sizeof(pimdev_hdr) + sizeof(struct obj_mlme)) {
			printf("bad assoc packet\n");
			return;
		}
		mac_id = mac_id_get(drv, mlme->id);
		if (mac_id == NULL)
			return;
		memcpy(&mlme->address[0], mac_id, ETH_ALEN);
		sta = ap_get_sta(drv->hapd, (u8 *) &mlme->address[0]);
		if (sta == NULL) {
			printf("cannot get sta\n");
			return;
		}
		new_assoc = (sta->flags & WLAN_STA_ASSOC) == 0;
		sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);
		hostapd_new_assoc_sta(drv->hapd, sta, !new_assoc);
		ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);
		sta->timeout_next = STA_NULLFUNC;
		return;
	}
	return;

fail:
	printf("Prism54: assoc fail: %x\n", resp);
	mlme->code = host_to_le16(resp);
	mlme->size = 0;
	mlme->state = htons(DOT11_STATE_ASSOCING);
	hdr->oid = htonl(DOT11_OID_DISASSOCIATEEX);
	hdr->op = htonl(PIMOP_SET);
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	send(drv->pim_sock, hdr, sizeof(*hdr) + sizeof(*mlme), 0);
}


static void handle_pim(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct prism54_driver_data *drv = eloop_ctx;
	int len;
	pimdev_hdr *hdr;

	hdr = malloc(PIM_BUF_SIZE);
	if (hdr == NULL)
		return;
	len = recv(sock, hdr, PIM_BUF_SIZE, 0);
	if (len < 0) {
		perror("recv");
		free(hdr);
		return;
	}
	if (len < 8) {
		printf("handle_pim: too short (%d)\n", len);
		free(hdr);
		return;
	}

	if (hdr->op != (int) htonl(PIMOP_TRAP)) {
		free(hdr);
		return;
	}
	switch (ntohl(hdr->oid)) {
		case DOT11_OID_PROBE:
			prism54_handle_probe(drv, hdr, len);
			break;
		case DOT11_OID_DEAUTHENTICATEEX:
		case DOT11_OID_DEAUTHENTICATE:
			prism54_handle_deauth(drv, hdr, len);
			break;
		case DOT11_OID_DISASSOCIATEEX:
		case DOT11_OID_DISASSOCIATE:
			prism54_handle_disassoc(drv, hdr, len);
			break;
		case DOT11_OID_AUTHENTICATEEX:
		case DOT11_OID_AUTHENTICATE:
			prism54_handle_auth(drv, hdr, len);
			break;
		case DOT11_OID_ASSOCIATEEX:
		case DOT11_OID_REASSOCIATEEX:
		case DOT11_OID_ASSOCIATE:
		case DOT11_OID_REASSOCIATE:
			prism54_handle_assoc(drv, hdr, len);
		default:
			break;
	}

	free(hdr);
}


static void handle_802_3(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct hostapd_data *hapd = (struct hostapd_data *) eloop_ctx;
	int len;
	ieee802_3_hdr *hdr;

	hdr = malloc(PIM_BUF_SIZE);
	if (hdr == NULL)
		return;
	len = recv(sock, hdr, PIM_BUF_SIZE, 0);
	if (len < 0) {
		perror("recv");
		free(hdr);
		return;
	}
        if (len < 14) {
                wpa_printf(MSG_MSGDUMP, "handle_802_3: too short (%d)", len);
		free(hdr);
                return;
        }
        if (hdr->type == htons(ETH_P_PAE)) {
                ieee802_1x_receive(hapd, (u8 *) &hdr->sa[0], (u8 *) &hdr[1],
				   len - sizeof(*hdr));
        }
	free(hdr);
}


static int prism54_init_sockets(struct prism54_driver_data *drv)
{
	struct hostapd_data *hapd = drv->hapd;
	struct ifreq ifr;
	struct sockaddr_ll addr;

	drv->sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_PAE));
	if (drv->sock < 0) {
		perror("socket[PF_PACKET,SOCK_RAW]");
		return -1;
	}

	if (eloop_register_read_sock(drv->sock, handle_802_3, drv->hapd, NULL))
	{
		printf("Could not register read socket\n");
		return -1;
	}

        memset(&ifr, 0, sizeof(ifr));
	if (hapd->conf->bridge[0] != '\0') {
		printf("opening bridge: %s\n", hapd->conf->bridge);
		os_strlcpy(ifr.ifr_name, hapd->conf->bridge,
			   sizeof(ifr.ifr_name));
	} else {
		os_strlcpy(ifr.ifr_name, drv->iface, sizeof(ifr.ifr_name));
	}
        if (ioctl(drv->sock, SIOCGIFINDEX, &ifr) != 0) {
		perror("ioctl(SIOCGIFINDEX)");
		return -1;
        }

	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifr.ifr_ifindex;
	addr.sll_protocol = htons(ETH_P_PAE);
	wpa_printf(MSG_DEBUG, "Opening raw packet socket for ifindex %d",
		   addr.sll_ifindex);

	if (bind(drv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		return -1;
	}

        memset(&ifr, 0, sizeof(ifr));
        os_strlcpy(ifr.ifr_name, drv->iface, sizeof(ifr.ifr_name));
        if (ioctl(drv->sock, SIOCGIFHWADDR, &ifr) != 0) {
		perror("ioctl(SIOCGIFHWADDR)");
		return -1;
        }

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		printf("Invalid HW-addr family 0x%04x\n",
		       ifr.ifr_hwaddr.sa_family);
		return -1;
	}
	memcpy(drv->hapd->own_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	drv->pim_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (drv->pim_sock < 0) {
		perror("socket[PF_PACKET,SOCK_RAW]");
		return -1;
	}

	if (eloop_register_read_sock(drv->pim_sock, handle_pim, drv, NULL)) {
		printf("Could not register read socket\n");
		return -1;
	}

        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%sap", drv->iface);
        if (ioctl(drv->pim_sock, SIOCGIFINDEX, &ifr) != 0) {
		perror("ioctl(SIOCGIFINDEX)");
		return -1;
        }

	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifr.ifr_ifindex;
	addr.sll_protocol = htons(ETH_P_ALL);
	wpa_printf(MSG_DEBUG, "Opening raw packet socket for ifindex %d",
		   addr.sll_ifindex);

	if (bind(drv->pim_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		return -1;
	}

	return 0;
}


static void * prism54_driver_init(struct hostapd_data *hapd)
{
	struct prism54_driver_data *drv;

	drv = os_zalloc(sizeof(struct prism54_driver_data));
	if (drv == NULL) {
		printf("Could not allocate memory for hostapd Prism54 driver "
		       "data\n");
		return NULL;
	}

	drv->hapd = hapd;
	drv->pim_sock = drv->sock = -1;
	memcpy(drv->iface, hapd->conf->iface, sizeof(drv->iface));

	if (prism54_init_sockets(drv)) {
		free(drv);
		return NULL;
	}
	prism54_init_1x(drv);
	/* must clean previous elems */
	prism54_set_generic_elem(drv->iface, drv, NULL, 0);

	return drv;
}


static void prism54_driver_deinit(void *priv)
{
	struct prism54_driver_data *drv = priv;

	if (drv->pim_sock >= 0)
		close(drv->pim_sock);

	if (drv->sock >= 0)
		close(drv->sock);
	
	free(drv);
}


const struct wpa_driver_ops wpa_driver_prism54_ops = {
	.name = "prism54",
	.init = prism54_driver_init,
	.deinit = prism54_driver_deinit,
	/* .set_ieee8021x = prism54_init_1x, */
	.set_privacy = prism54_set_privacy_invoked,
	.set_encryption = prism54_set_encryption,
	.get_seqnum = prism54_get_seqnum,
	.flush = prism54_flush,
	.set_generic_elem = prism54_set_generic_elem,
	.send_eapol = prism54_send_eapol,
	.sta_set_flags = prism54_sta_set_flags,
	.sta_deauth = prism54_sta_deauth,
	.sta_disassoc = prism54_sta_disassoc,
	.set_ssid = prism54_ioctl_setiwessid,
	.get_inact_sec = prism54_get_inact_sec,
};
