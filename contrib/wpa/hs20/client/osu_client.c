/*
 * Hotspot 2.0 OSU client
 * Copyright (c) 2012-2014, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <time.h>
#include <sys/stat.h>

#include "common.h"
#include "utils/browser.h"
#include "utils/base64.h"
#include "utils/xml-utils.h"
#include "utils/http-utils.h"
#include "common/wpa_ctrl.h"
#include "common/wpa_helpers.h"
#include "eap_common/eap_defs.h"
#include "crypto/crypto.h"
#include "crypto/sha256.h"
#include "osu_client.h"

static void write_summary(struct hs20_osu_client *ctx, const char *fmt, ...);

static void write_result(struct hs20_osu_client *ctx, const char *fmt, ...)
{
	va_list ap;
	FILE *f;
	char buf[500];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	write_summary(ctx, "%s", buf);

	if (!ctx->result_file)
		return;

	f = fopen(ctx->result_file, "w");
	if (f == NULL)
		return;

	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fprintf(f, "\n");
	fclose(f);
}


static void write_summary(struct hs20_osu_client *ctx, const char *fmt, ...)
{
	va_list ap;
	FILE *f;

	if (!ctx->summary_file)
		return;

	f = fopen(ctx->summary_file, "a");
	if (f == NULL)
		return;

	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fprintf(f, "\n");
	fclose(f);
}


#define TMP_CERT_DL_FILE "tmp-cert-download"

static int download_cert(struct hs20_osu_client *ctx, xml_node_t *params,
			 const char *fname)
{
	xml_node_t *url_node, *hash_node;
	char *url, *hash;
	char *cert;
	size_t len;
	u8 digest1[SHA256_MAC_LEN], digest2[SHA256_MAC_LEN];
	int res;
	char *b64;
	FILE *f;

	url_node = get_node(ctx->xml, params, "CertURL");
	hash_node = get_node(ctx->xml, params, "CertSHA256Fingerprint");
	if (url_node == NULL || hash_node == NULL)
		return -1;
	url = xml_node_get_text(ctx->xml, url_node);
	hash = xml_node_get_text(ctx->xml, hash_node);
	if (url == NULL || hash == NULL) {
		xml_node_get_text_free(ctx->xml, url);
		xml_node_get_text_free(ctx->xml, hash);
		return -1;
	}

	wpa_printf(MSG_INFO, "CertURL: %s", url);
	wpa_printf(MSG_INFO, "SHA256 hash: %s", hash);

	if (hexstr2bin(hash, digest1, SHA256_MAC_LEN) < 0) {
		wpa_printf(MSG_INFO, "Invalid SHA256 hash value");
		write_result(ctx, "Invalid SHA256 hash value for downloaded certificate");
		xml_node_get_text_free(ctx->xml, hash);
		return -1;
	}
	xml_node_get_text_free(ctx->xml, hash);

	write_summary(ctx, "Download certificate from %s", url);
	http_ocsp_set(ctx->http, 1);
	res = http_download_file(ctx->http, url, TMP_CERT_DL_FILE, NULL);
	http_ocsp_set(ctx->http,
		      (ctx->workarounds & WORKAROUND_OCSP_OPTIONAL) ? 1 : 2);
	xml_node_get_text_free(ctx->xml, url);
	if (res < 0)
		return -1;

	cert = os_readfile(TMP_CERT_DL_FILE, &len);
	remove(TMP_CERT_DL_FILE);
	if (cert == NULL)
		return -1;

	if (sha256_vector(1, (const u8 **) &cert, &len, digest2) < 0) {
		os_free(cert);
		return -1;
	}

	if (os_memcmp(digest1, digest2, sizeof(digest1)) != 0) {
		wpa_printf(MSG_INFO, "Downloaded certificate fingerprint did not match");
		write_result(ctx, "Downloaded certificate fingerprint did not match");
		os_free(cert);
		return -1;
	}

	b64 = base64_encode(cert, len, NULL);
	os_free(cert);
	if (b64 == NULL)
		return -1;

	f = fopen(fname, "wb");
	if (f == NULL) {
		os_free(b64);
		return -1;
	}

	fprintf(f, "-----BEGIN CERTIFICATE-----\n"
		"%s"
		"-----END CERTIFICATE-----\n",
		b64);

	os_free(b64);
	fclose(f);

	wpa_printf(MSG_INFO, "Downloaded certificate into %s and validated fingerprint",
		   fname);
	write_summary(ctx, "Downloaded certificate into %s and validated fingerprint",
		      fname);

	return 0;
}


static int cmd_dl_aaa_ca(struct hs20_osu_client *ctx, const char *pps_fname,
			 const char *ca_fname)
{
	xml_node_t *pps, *node, *aaa;
	int ret;

	pps = node_from_file(ctx->xml, pps_fname);
	if (pps == NULL) {
		wpa_printf(MSG_INFO, "Could not read or parse '%s'", pps_fname);
		return -1;
	}

	node = get_child_node(ctx->xml, pps,
			      "AAAServerTrustRoot");
	if (node == NULL) {
		wpa_printf(MSG_INFO, "No AAAServerTrustRoot/CertURL found from PPS");
		xml_node_free(ctx->xml, pps);
		return -2;
	}

	aaa = xml_node_first_child(ctx->xml, node);
	if (aaa == NULL) {
		wpa_printf(MSG_INFO, "No AAAServerTrustRoot/CertURL found from PPS");
		xml_node_free(ctx->xml, pps);
		return -1;
	}

	ret = download_cert(ctx, aaa, ca_fname);
	xml_node_free(ctx->xml, pps);

	return ret;
}


/* Remove old credentials based on HomeSP/FQDN */
static void remove_sp_creds(struct hs20_osu_client *ctx, const char *fqdn)
{
	char cmd[300];
	os_snprintf(cmd, sizeof(cmd), "REMOVE_CRED provisioning_sp=%s", fqdn);
	if (wpa_command(ctx->ifname, cmd) < 0)
		wpa_printf(MSG_INFO, "Failed to remove old credential(s)");
}


static void set_pps_cred_policy_spe(struct hs20_osu_client *ctx, int id,
				    xml_node_t *spe)
{
	xml_node_t *ssid;
	char *txt;

	ssid = get_node(ctx->xml, spe, "SSID");
	if (ssid == NULL)
		return;
	txt = xml_node_get_text(ctx->xml, ssid);
	if (txt == NULL)
		return;
	wpa_printf(MSG_DEBUG, "- Policy/SPExclusionList/<X+>/SSID = %s", txt);
	if (set_cred_quoted(ctx->ifname, id, "excluded_ssid", txt) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred excluded_ssid");
	xml_node_get_text_free(ctx->xml, txt);
}


static void set_pps_cred_policy_spel(struct hs20_osu_client *ctx, int id,
				     xml_node_t *spel)
{
	xml_node_t *child;

	xml_node_for_each_child(ctx->xml, child, spel) {
		xml_node_for_each_check(ctx->xml, child);
		set_pps_cred_policy_spe(ctx, id, child);
	}
}


static void set_pps_cred_policy_prp(struct hs20_osu_client *ctx, int id,
				    xml_node_t *prp)
{
	xml_node_t *node;
	char *txt = NULL, *pos;
	char *prio, *country_buf = NULL;
	const char *country;
	char val[200];
	int priority;

	node = get_node(ctx->xml, prp, "Priority");
	if (node == NULL)
		return;
	prio = xml_node_get_text(ctx->xml, node);
	if (prio == NULL)
		return;
	wpa_printf(MSG_INFO, "- Policy/PreferredRoamingPartnerList/<X+>/Priority = %s",
		   prio);
	priority = atoi(prio);
	xml_node_get_text_free(ctx->xml, prio);

	node = get_node(ctx->xml, prp, "Country");
	if (node) {
		country_buf = xml_node_get_text(ctx->xml, node);
		if (country_buf == NULL)
			return;
		country = country_buf;
		wpa_printf(MSG_INFO, "- Policy/PreferredRoamingPartnerList/<X+>/Country = %s",
			   country);
	} else {
		country = "*";
	}

	node = get_node(ctx->xml, prp, "FQDN_Match");
	if (node == NULL)
		goto out;
	txt = xml_node_get_text(ctx->xml, node);
	if (txt == NULL)
		goto out;
	wpa_printf(MSG_INFO, "- Policy/PreferredRoamingPartnerList/<X+>/FQDN_Match = %s",
		   txt);
	pos = strrchr(txt, ',');
	if (pos == NULL)
		goto out;
	*pos++ = '\0';

	snprintf(val, sizeof(val), "%s,%d,%d,%s", txt,
		 strcmp(pos, "includeSubdomains") != 0, priority, country);
	if (set_cred_quoted(ctx->ifname, id, "roaming_partner", val) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred roaming_partner");
out:
	xml_node_get_text_free(ctx->xml, country_buf);
	xml_node_get_text_free(ctx->xml, txt);
}


static void set_pps_cred_policy_prpl(struct hs20_osu_client *ctx, int id,
				     xml_node_t *prpl)
{
	xml_node_t *child;

	xml_node_for_each_child(ctx->xml, child, prpl) {
		xml_node_for_each_check(ctx->xml, child);
		set_pps_cred_policy_prp(ctx, id, child);
	}
}


static void set_pps_cred_policy_min_backhaul(struct hs20_osu_client *ctx, int id,
					     xml_node_t *min_backhaul)
{
	xml_node_t *node;
	char *type, *dl = NULL, *ul = NULL;
	int home;

	node = get_node(ctx->xml, min_backhaul, "NetworkType");
	if (node == NULL) {
		wpa_printf(MSG_INFO, "Ignore MinBackhaulThreshold without mandatory NetworkType node");
		return;
	}

	type = xml_node_get_text(ctx->xml, node);
	if (type == NULL)
		return;
	wpa_printf(MSG_INFO, "- Policy/MinBackhaulThreshold/<X+>/NetworkType = %s",
		   type);
	if (os_strcasecmp(type, "home") == 0)
		home = 1;
	else if (os_strcasecmp(type, "roaming") == 0)
		home = 0;
	else {
		wpa_printf(MSG_INFO, "Ignore MinBackhaulThreshold with invalid NetworkType");
		xml_node_get_text_free(ctx->xml, type);
		return;
	}
	xml_node_get_text_free(ctx->xml, type);

	node = get_node(ctx->xml, min_backhaul, "DLBandwidth");
	if (node)
		dl = xml_node_get_text(ctx->xml, node);

	node = get_node(ctx->xml, min_backhaul, "ULBandwidth");
	if (node)
		ul = xml_node_get_text(ctx->xml, node);

	if (dl == NULL && ul == NULL) {
		wpa_printf(MSG_INFO, "Ignore MinBackhaulThreshold without either DLBandwidth or ULBandwidth nodes");
		return;
	}

	if (dl)
		wpa_printf(MSG_INFO, "- Policy/MinBackhaulThreshold/<X+>/DLBandwidth = %s",
			   dl);
	if (ul)
		wpa_printf(MSG_INFO, "- Policy/MinBackhaulThreshold/<X+>/ULBandwidth = %s",
			   ul);

	if (home) {
		if (dl &&
		    set_cred(ctx->ifname, id, "min_dl_bandwidth_home", dl) < 0)
			wpa_printf(MSG_INFO, "Failed to set cred bandwidth limit");
		if (ul &&
		    set_cred(ctx->ifname, id, "min_ul_bandwidth_home", ul) < 0)
			wpa_printf(MSG_INFO, "Failed to set cred bandwidth limit");
	} else {
		if (dl &&
		    set_cred(ctx->ifname, id, "min_dl_bandwidth_roaming", dl) <
		    0)
			wpa_printf(MSG_INFO, "Failed to set cred bandwidth limit");
		if (ul &&
		    set_cred(ctx->ifname, id, "min_ul_bandwidth_roaming", ul) <
		    0)
			wpa_printf(MSG_INFO, "Failed to set cred bandwidth limit");
	}

	xml_node_get_text_free(ctx->xml, dl);
	xml_node_get_text_free(ctx->xml, ul);
}


static void set_pps_cred_policy_min_backhaul_list(struct hs20_osu_client *ctx,
						  int id, xml_node_t *node)
{
	xml_node_t *child;

	wpa_printf(MSG_INFO, "- Policy/MinBackhaulThreshold");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		set_pps_cred_policy_min_backhaul(ctx, id, child);
	}
}


static void set_pps_cred_policy_update(struct hs20_osu_client *ctx, int id,
				       xml_node_t *node)
{
	wpa_printf(MSG_INFO, "- Policy/PolicyUpdate");
	/* Not used in wpa_supplicant */
}


static void set_pps_cred_policy_required_proto_port(struct hs20_osu_client *ctx,
						    int id, xml_node_t *tuple)
{
	xml_node_t *node;
	char *proto, *port;
	char *buf;
	size_t buflen;

	node = get_node(ctx->xml, tuple, "IPProtocol");
	if (node == NULL) {
		wpa_printf(MSG_INFO, "Ignore RequiredProtoPortTuple without mandatory IPProtocol node");
		return;
	}

	proto = xml_node_get_text(ctx->xml, node);
	if (proto == NULL)
		return;

	wpa_printf(MSG_INFO, "- Policy/RequiredProtoPortTuple/<X+>/IPProtocol = %s",
		   proto);

	node = get_node(ctx->xml, tuple, "PortNumber");
	port = node ? xml_node_get_text(ctx->xml, node) : NULL;
	if (port) {
		wpa_printf(MSG_INFO, "- Policy/RequiredProtoPortTuple/<X+>/PortNumber = %s",
			   port);
		buflen = os_strlen(proto) + os_strlen(port) + 10;
		buf = os_malloc(buflen);
		if (buf)
			os_snprintf(buf, buflen, "%s:%s", proto, port);
		xml_node_get_text_free(ctx->xml, port);
	} else {
		buflen = os_strlen(proto) + 10;
		buf = os_malloc(buflen);
		if (buf)
			os_snprintf(buf, buflen, "%s", proto);
	}

	xml_node_get_text_free(ctx->xml, proto);

	if (buf == NULL)
		return;

	if (set_cred(ctx->ifname, id, "req_conn_capab", buf) < 0)
		wpa_printf(MSG_INFO, "Could not set req_conn_capab");

	os_free(buf);
}


static void set_pps_cred_policy_required_proto_ports(struct hs20_osu_client *ctx,
						     int id, xml_node_t *node)
{
	xml_node_t *child;

	wpa_printf(MSG_INFO, "- Policy/RequiredProtoPortTuple");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		set_pps_cred_policy_required_proto_port(ctx, id, child);
	}
}


static void set_pps_cred_policy_max_bss_load(struct hs20_osu_client *ctx, int id,
					     xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- Policy/MaximumBSSLoadValue - %s", str);
	if (set_cred(ctx->ifname, id, "max_bss_load", str) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred max_bss_load limit");
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_policy(struct hs20_osu_client *ctx, int id,
				xml_node_t *node)
{
	xml_node_t *child;
	const char *name;

	wpa_printf(MSG_INFO, "- Policy");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (os_strcasecmp(name, "PreferredRoamingPartnerList") == 0)
			set_pps_cred_policy_prpl(ctx, id, child);
		else if (os_strcasecmp(name, "MinBackhaulThreshold") == 0)
			set_pps_cred_policy_min_backhaul_list(ctx, id, child);
		else if (os_strcasecmp(name, "PolicyUpdate") == 0)
			set_pps_cred_policy_update(ctx, id, child);
		else if (os_strcasecmp(name, "SPExclusionList") == 0)
			set_pps_cred_policy_spel(ctx, id, child);
		else if (os_strcasecmp(name, "RequiredProtoPortTuple") == 0)
			set_pps_cred_policy_required_proto_ports(ctx, id, child);
		else if (os_strcasecmp(name, "MaximumBSSLoadValue") == 0)
			set_pps_cred_policy_max_bss_load(ctx, id, child);
		else
			wpa_printf(MSG_INFO, "Unknown Policy node '%s'", name);
	}
}


static void set_pps_cred_priority(struct hs20_osu_client *ctx, int id,
				  xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- CredentialPriority = %s", str);
	if (set_cred(ctx->ifname, id, "sp_priority", str) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred sp_priority");
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_aaa_server_trust_root(struct hs20_osu_client *ctx,
					       int id, xml_node_t *node)
{
	wpa_printf(MSG_INFO, "- AAAServerTrustRoot - TODO");
}


static void set_pps_cred_sub_update(struct hs20_osu_client *ctx, int id,
				    xml_node_t *node)
{
	wpa_printf(MSG_INFO, "- SubscriptionUpdate");
	/* not used within wpa_supplicant */
}


static void set_pps_cred_home_sp_network_id(struct hs20_osu_client *ctx,
					    int id, xml_node_t *node)
{
	xml_node_t *ssid_node, *hessid_node;
	char *ssid, *hessid;

	ssid_node = get_node(ctx->xml, node, "SSID");
	if (ssid_node == NULL) {
		wpa_printf(MSG_INFO, "Ignore HomeSP/NetworkID without mandatory SSID node");
		return;
	}

	hessid_node = get_node(ctx->xml, node, "HESSID");

	ssid = xml_node_get_text(ctx->xml, ssid_node);
	if (ssid == NULL)
		return;
	hessid = hessid_node ? xml_node_get_text(ctx->xml, hessid_node) : NULL;

	wpa_printf(MSG_INFO, "- HomeSP/NetworkID/<X+>/SSID = %s", ssid);
	if (hessid)
		wpa_printf(MSG_INFO, "- HomeSP/NetworkID/<X+>/HESSID = %s",
			   hessid);

	/* TODO: Configure to wpa_supplicant */

	xml_node_get_text_free(ctx->xml, ssid);
	xml_node_get_text_free(ctx->xml, hessid);
}


static void set_pps_cred_home_sp_network_ids(struct hs20_osu_client *ctx,
					     int id, xml_node_t *node)
{
	xml_node_t *child;

	wpa_printf(MSG_INFO, "- HomeSP/NetworkID");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		set_pps_cred_home_sp_network_id(ctx, id, child);
	}
}


static void set_pps_cred_home_sp_friendly_name(struct hs20_osu_client *ctx,
					       int id, xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- HomeSP/FriendlyName = %s", str);
	/* not used within wpa_supplicant(?) */
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_home_sp_icon_url(struct hs20_osu_client *ctx,
					  int id, xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- HomeSP/IconURL = %s", str);
	/* not used within wpa_supplicant */
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_home_sp_fqdn(struct hs20_osu_client *ctx, int id,
				      xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- HomeSP/FQDN = %s", str);
	if (set_cred_quoted(ctx->ifname, id, "domain", str) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred domain");
	if (set_cred_quoted(ctx->ifname, id, "domain_suffix_match", str) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred domain_suffix_match");
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_home_sp_oi(struct hs20_osu_client *ctx, int id,
				    xml_node_t *node)
{
	xml_node_t *child;
	const char *name;
	char *homeoi = NULL;
	int required = 0;
	char *str;

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (strcasecmp(name, "HomeOI") == 0 && !homeoi) {
			homeoi = xml_node_get_text(ctx->xml, child);
			wpa_printf(MSG_INFO, "- HomeSP/HomeOIList/<X+>/HomeOI = %s",
				   homeoi);
		} else if (strcasecmp(name, "HomeOIRequired") == 0) {
			str = xml_node_get_text(ctx->xml, child);
			wpa_printf(MSG_INFO, "- HomeSP/HomeOIList/<X+>/HomeOIRequired = '%s'",
				   str);
			if (str == NULL)
				continue;
			required = strcasecmp(str, "true") == 0;
			xml_node_get_text_free(ctx->xml, str);
		} else
			wpa_printf(MSG_INFO, "Unknown HomeOIList node '%s'",
				   name);
	}

	if (homeoi == NULL) {
		wpa_printf(MSG_INFO, "- HomeSP/HomeOIList/<X+> without HomeOI ignored");
		return;
	}

	wpa_printf(MSG_INFO, "- HomeSP/HomeOIList/<X+> '%s' required=%d",
		   homeoi, required);

	if (required) {
		if (set_cred_quoted(ctx->ifname, id, "required_home_ois",
				    homeoi) < 0)
			wpa_printf(MSG_INFO,
				   "Failed to set cred required_home_ois");
	} else {
		if (set_cred_quoted(ctx->ifname, id, "home_ois", homeoi) < 0)
			wpa_printf(MSG_INFO, "Failed to set cred home_ois");
	}

	xml_node_get_text_free(ctx->xml, homeoi);
}


static void set_pps_cred_home_sp_oi_list(struct hs20_osu_client *ctx, int id,
					 xml_node_t *node)
{
	xml_node_t *child;

	wpa_printf(MSG_INFO, "- HomeSP/HomeOIList");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		set_pps_cred_home_sp_oi(ctx, id, child);
	}
}


static void set_pps_cred_home_sp_other_partner(struct hs20_osu_client *ctx,
					       int id, xml_node_t *node)
{
	xml_node_t *child;
	const char *name;
	char *fqdn = NULL;

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (os_strcasecmp(name, "FQDN") == 0 && !fqdn) {
			fqdn = xml_node_get_text(ctx->xml, child);
			wpa_printf(MSG_INFO, "- HomeSP/OtherHomePartners/<X+>/FQDN = %s",
				   fqdn);
		} else
			wpa_printf(MSG_INFO, "Unknown OtherHomePartners node '%s'",
				   name);
	}

	if (fqdn == NULL) {
		wpa_printf(MSG_INFO, "- HomeSP/OtherHomePartners/<X+> without FQDN ignored");
		return;
	}

	if (set_cred_quoted(ctx->ifname, id, "domain", fqdn) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred domain for OtherHomePartners node");

	xml_node_get_text_free(ctx->xml, fqdn);
}


static void set_pps_cred_home_sp_other_partners(struct hs20_osu_client *ctx,
						int id,
						xml_node_t *node)
{
	xml_node_t *child;

	wpa_printf(MSG_INFO, "- HomeSP/OtherHomePartners");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		set_pps_cred_home_sp_other_partner(ctx, id, child);
	}
}


static void set_pps_cred_home_sp_roaming_consortium_oi(
	struct hs20_osu_client *ctx, int id, xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- HomeSP/RoamingConsortiumOI = %s", str);
	if (set_cred_quoted(ctx->ifname, id, "roaming_consortiums",
			    str) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred roaming_consortiums");
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_home_sp(struct hs20_osu_client *ctx, int id,
				 xml_node_t *node)
{
	xml_node_t *child;
	const char *name;

	wpa_printf(MSG_INFO, "- HomeSP");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (os_strcasecmp(name, "NetworkID") == 0)
			set_pps_cred_home_sp_network_ids(ctx, id, child);
		else if (os_strcasecmp(name, "FriendlyName") == 0)
			set_pps_cred_home_sp_friendly_name(ctx, id, child);
		else if (os_strcasecmp(name, "IconURL") == 0)
			set_pps_cred_home_sp_icon_url(ctx, id, child);
		else if (os_strcasecmp(name, "FQDN") == 0)
			set_pps_cred_home_sp_fqdn(ctx, id, child);
		else if (os_strcasecmp(name, "HomeOIList") == 0)
			set_pps_cred_home_sp_oi_list(ctx, id, child);
		else if (os_strcasecmp(name, "OtherHomePartners") == 0)
			set_pps_cred_home_sp_other_partners(ctx, id, child);
		else if (os_strcasecmp(name, "RoamingConsortiumOI") == 0)
			set_pps_cred_home_sp_roaming_consortium_oi(ctx, id,
								   child);
		else
			wpa_printf(MSG_INFO, "Unknown HomeSP node '%s'", name);
	}
}


static void set_pps_cred_sub_params(struct hs20_osu_client *ctx, int id,
				    xml_node_t *node)
{
	wpa_printf(MSG_INFO, "- SubscriptionParameters");
	/* not used within wpa_supplicant */
}


static void set_pps_cred_creation_date(struct hs20_osu_client *ctx, int id,
				       xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- Credential/CreationDate = %s", str);
	/* not used within wpa_supplicant */
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_expiration_date(struct hs20_osu_client *ctx, int id,
					 xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- Credential/ExpirationDate = %s", str);
	/* not used within wpa_supplicant */
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_username(struct hs20_osu_client *ctx, int id,
				  xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- Credential/UsernamePassword/Username = %s",
		   str);
	if (set_cred_quoted(ctx->ifname, id, "username", str) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred username");
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_password(struct hs20_osu_client *ctx, int id,
				  xml_node_t *node)
{
	int len, i;
	char *pw, *hex, *pos, *end;

	pw = xml_node_get_base64_text(ctx->xml, node, &len);
	if (pw == NULL)
		return;

	wpa_printf(MSG_INFO, "- Credential/UsernamePassword/Password = %s", pw);

	hex = malloc(len * 2 + 1);
	if (hex == NULL) {
		free(pw);
		return;
	}
	end = hex + len * 2 + 1;
	pos = hex;
	for (i = 0; i < len; i++) {
		snprintf(pos, end - pos, "%02x", pw[i]);
		pos += 2;
	}
	free(pw);

	if (set_cred(ctx->ifname, id, "password", hex) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred password");
	free(hex);
}


static void set_pps_cred_machine_managed(struct hs20_osu_client *ctx, int id,
					 xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- Credential/UsernamePassword/MachineManaged = %s",
		   str);
	/* not used within wpa_supplicant */
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_soft_token_app(struct hs20_osu_client *ctx, int id,
					xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- Credential/UsernamePassword/SoftTokenApp = %s",
		   str);
	/* not used within wpa_supplicant */
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_able_to_share(struct hs20_osu_client *ctx, int id,
				       xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	if (str == NULL)
		return;
	wpa_printf(MSG_INFO, "- Credential/UsernamePassword/AbleToShare = %s",
		   str);
	/* not used within wpa_supplicant */
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_eap_method_eap_type(struct hs20_osu_client *ctx,
					     int id, xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	int type;
	const char *eap_method = NULL;

	if (!str)
		return;
	wpa_printf(MSG_INFO,
		   "- Credential/UsernamePassword/EAPMethod/EAPType = %s", str);
	type = atoi(str);
	switch (type) {
	case EAP_TYPE_TLS:
		eap_method = "TLS";
		break;
	case EAP_TYPE_TTLS:
		eap_method = "TTLS";
		break;
	case EAP_TYPE_PEAP:
		eap_method = "PEAP";
		break;
	case EAP_TYPE_PWD:
		eap_method = "PWD";
		break;
	}
	xml_node_get_text_free(ctx->xml, str);
	if (!eap_method) {
		wpa_printf(MSG_INFO, "Unknown EAPType value");
		return;
	}

	if (set_cred(ctx->ifname, id, "eap", eap_method) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred eap");
}


static void set_pps_cred_eap_method_inner_method(struct hs20_osu_client *ctx,
						 int id, xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);
	const char *phase2 = NULL;

	if (!str)
		return;
	wpa_printf(MSG_INFO,
		   "- Credential/UsernamePassword/EAPMethod/InnerMethod = %s",
		   str);
	if (os_strcmp(str, "PAP") == 0)
		phase2 = "auth=PAP";
	else if (os_strcmp(str, "CHAP") == 0)
		phase2 = "auth=CHAP";
	else if (os_strcmp(str, "MS-CHAP") == 0)
		phase2 = "auth=MSCHAP";
	else if (os_strcmp(str, "MS-CHAP-V2") == 0)
		phase2 = "auth=MSCHAPV2";
	xml_node_get_text_free(ctx->xml, str);
	if (!phase2) {
		wpa_printf(MSG_INFO, "Unknown InnerMethod value");
		return;
	}

	if (set_cred_quoted(ctx->ifname, id, "phase2", phase2) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred phase2");
}


static void set_pps_cred_eap_method(struct hs20_osu_client *ctx, int id,
				    xml_node_t *node)
{
	xml_node_t *child;
	const char *name;

	wpa_printf(MSG_INFO, "- Credential/UsernamePassword/EAPMethod");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (os_strcasecmp(name, "EAPType") == 0)
			set_pps_cred_eap_method_eap_type(ctx, id, child);
		else if (os_strcasecmp(name, "InnerMethod") == 0)
			set_pps_cred_eap_method_inner_method(ctx, id, child);
		else
			wpa_printf(MSG_INFO, "Unknown Credential/UsernamePassword/EAPMethod node '%s'",
				   name);
	}
}


static void set_pps_cred_username_password(struct hs20_osu_client *ctx, int id,
					   xml_node_t *node)
{
	xml_node_t *child;
	const char *name;

	wpa_printf(MSG_INFO, "- Credential/UsernamePassword");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (os_strcasecmp(name, "Username") == 0)
			set_pps_cred_username(ctx, id, child);
		else if (os_strcasecmp(name, "Password") == 0)
			set_pps_cred_password(ctx, id, child);
		else if (os_strcasecmp(name, "MachineManaged") == 0)
			set_pps_cred_machine_managed(ctx, id, child);
		else if (os_strcasecmp(name, "SoftTokenApp") == 0)
			set_pps_cred_soft_token_app(ctx, id, child);
		else if (os_strcasecmp(name, "AbleToShare") == 0)
			set_pps_cred_able_to_share(ctx, id, child);
		else if (os_strcasecmp(name, "EAPMethod") == 0)
			set_pps_cred_eap_method(ctx, id, child);
		else
			wpa_printf(MSG_INFO, "Unknown Credential/UsernamePassword node '%s'",
				   name);
	}
}


static void set_pps_cred_digital_cert(struct hs20_osu_client *ctx, int id,
				      xml_node_t *node, const char *fqdn)
{
	char buf[200], dir[200];
	int res;

	wpa_printf(MSG_INFO, "- Credential/DigitalCertificate");

	if (getcwd(dir, sizeof(dir)) == NULL)
		return;

	/* TODO: could build username from Subject of Subject AltName */
	if (set_cred_quoted(ctx->ifname, id, "username", "cert") < 0) {
		wpa_printf(MSG_INFO, "Failed to set username");
	}

	res = os_snprintf(buf, sizeof(buf), "%s/SP/%s/client-cert.pem", dir,
			  fqdn);
	if (os_snprintf_error(sizeof(buf), res))
		return;
	if (os_file_exists(buf)) {
		if (set_cred_quoted(ctx->ifname, id, "client_cert", buf) < 0) {
			wpa_printf(MSG_INFO, "Failed to set client_cert");
		}
	}

	res = os_snprintf(buf, sizeof(buf), "%s/SP/%s/client-key.pem", dir,
			  fqdn);
	if (os_snprintf_error(sizeof(buf), res))
		return;
	if (os_file_exists(buf)) {
		if (set_cred_quoted(ctx->ifname, id, "private_key", buf) < 0) {
			wpa_printf(MSG_INFO, "Failed to set private_key");
		}
	}
}


static void set_pps_cred_realm(struct hs20_osu_client *ctx, int id,
			       xml_node_t *node, const char *fqdn, int sim)
{
	char *str = xml_node_get_text(ctx->xml, node);
	char buf[200], dir[200];
	int res;

	if (str == NULL)
		return;

	wpa_printf(MSG_INFO, "- Credential/Realm = %s", str);
	if (set_cred_quoted(ctx->ifname, id, "realm", str) < 0)
		wpa_printf(MSG_INFO, "Failed to set cred realm");
	xml_node_get_text_free(ctx->xml, str);

	if (sim)
		return;

	if (getcwd(dir, sizeof(dir)) == NULL)
		return;
	res = os_snprintf(buf, sizeof(buf), "%s/SP/%s/aaa-ca.pem", dir, fqdn);
	if (os_snprintf_error(sizeof(buf), res))
		return;
	if (os_file_exists(buf)) {
		if (set_cred_quoted(ctx->ifname, id, "ca_cert", buf) < 0) {
			wpa_printf(MSG_INFO, "Failed to set CA cert");
		}
	}
}


static void set_pps_cred_check_aaa_cert_status(struct hs20_osu_client *ctx,
					       int id, xml_node_t *node)
{
	char *str = xml_node_get_text(ctx->xml, node);

	if (str == NULL)
		return;

	wpa_printf(MSG_INFO, "- Credential/CheckAAAServerCertStatus = %s", str);
	if (os_strcasecmp(str, "true") == 0 &&
	    set_cred(ctx->ifname, id, "ocsp", "2") < 0)
		wpa_printf(MSG_INFO, "Failed to set cred ocsp");
	xml_node_get_text_free(ctx->xml, str);
}


static void set_pps_cred_sim(struct hs20_osu_client *ctx, int id,
			     xml_node_t *sim, xml_node_t *realm)
{
	xml_node_t *node;
	char *imsi, *eaptype, *str, buf[20];
	int type;
	int mnc_len = 3;
	size_t imsi_len;

	node = get_node(ctx->xml, sim, "EAPType");
	if (node == NULL) {
		wpa_printf(MSG_INFO, "No SIM/EAPType node in credential");
		return;
	}
	eaptype = xml_node_get_text(ctx->xml, node);
	if (eaptype == NULL) {
		wpa_printf(MSG_INFO, "Could not extract SIM/EAPType");
		return;
	}
	wpa_printf(MSG_INFO, " - Credential/SIM/EAPType = %s", eaptype);
	type = atoi(eaptype);
	xml_node_get_text_free(ctx->xml, eaptype);

	switch (type) {
	case EAP_TYPE_SIM:
		if (set_cred(ctx->ifname, id, "eap", "SIM") < 0)
			wpa_printf(MSG_INFO, "Could not set eap=SIM");
		break;
	case EAP_TYPE_AKA:
		if (set_cred(ctx->ifname, id, "eap", "AKA") < 0)
			wpa_printf(MSG_INFO, "Could not set eap=SIM");
		break;
	case EAP_TYPE_AKA_PRIME:
		if (set_cred(ctx->ifname, id, "eap", "AKA'") < 0)
			wpa_printf(MSG_INFO, "Could not set eap=SIM");
		break;
	default:
		wpa_printf(MSG_INFO, "Unsupported SIM/EAPType %d", type);
		return;
	}

	node = get_node(ctx->xml, sim, "IMSI");
	if (node == NULL) {
		wpa_printf(MSG_INFO, "No SIM/IMSI node in credential");
		return;
	}
	imsi = xml_node_get_text(ctx->xml, node);
	if (imsi == NULL) {
		wpa_printf(MSG_INFO, "Could not extract SIM/IMSI");
		return;
	}
	wpa_printf(MSG_INFO, " - Credential/SIM/IMSI = %s", imsi);
	imsi_len = os_strlen(imsi);
	if (imsi_len < 7 || imsi_len + 2 > sizeof(buf)) {
		wpa_printf(MSG_INFO, "Invalid IMSI length");
		xml_node_get_text_free(ctx->xml, imsi);
		return;
	}

	str = xml_node_get_text(ctx->xml, node);
	if (str) {
		char *pos;
		pos = os_strstr(str, "mnc");
		if (pos && os_strlen(pos) >= 6) {
			if (os_strncmp(imsi + 3, pos + 3, 3) == 0)
				mnc_len = 3;
			else if (os_strncmp(imsi + 3, pos + 4, 2) == 0)
				mnc_len = 2;
		}
		xml_node_get_text_free(ctx->xml, str);
	}

	os_memcpy(buf, imsi, 3 + mnc_len);
	buf[3 + mnc_len] = '-';
	os_strlcpy(buf + 3 + mnc_len + 1, imsi + 3 + mnc_len,
		   sizeof(buf) - 3 - mnc_len - 1);

	xml_node_get_text_free(ctx->xml, imsi);

	if (set_cred_quoted(ctx->ifname, id, "imsi", buf) < 0)
		wpa_printf(MSG_INFO, "Could not set IMSI");

	if (set_cred_quoted(ctx->ifname, id, "milenage",
			    "90dca4eda45b53cf0f12d7c9c3bc6a89:"
			    "cb9cccc4b9258e6dca4760379fb82581:000000000123") <
	    0)
		wpa_printf(MSG_INFO, "Could not set Milenage parameters");
}


static void set_pps_cred_credential(struct hs20_osu_client *ctx, int id,
				    xml_node_t *node, const char *fqdn)
{
	xml_node_t *child, *sim, *realm;
	const char *name;

	wpa_printf(MSG_INFO, "- Credential");

	sim = get_node(ctx->xml, node, "SIM");
	realm = get_node(ctx->xml, node, "Realm");

	xml_node_for_each_child(ctx->xml, child, node) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (os_strcasecmp(name, "CreationDate") == 0)
			set_pps_cred_creation_date(ctx, id, child);
		else if (os_strcasecmp(name, "ExpirationDate") == 0)
			set_pps_cred_expiration_date(ctx, id, child);
		else if (os_strcasecmp(name, "UsernamePassword") == 0)
			set_pps_cred_username_password(ctx, id, child);
		else if (os_strcasecmp(name, "DigitalCertificate") == 0)
			set_pps_cred_digital_cert(ctx, id, child, fqdn);
		else if (os_strcasecmp(name, "Realm") == 0)
			set_pps_cred_realm(ctx, id, child, fqdn, sim != NULL);
		else if (os_strcasecmp(name, "CheckAAAServerCertStatus") == 0)
			set_pps_cred_check_aaa_cert_status(ctx, id, child);
		else if (os_strcasecmp(name, "SIM") == 0)
			set_pps_cred_sim(ctx, id, child, realm);
		else
			wpa_printf(MSG_INFO, "Unknown Credential node '%s'",
				   name);
	}
}


static void set_pps_credential(struct hs20_osu_client *ctx, int id,
			       xml_node_t *cred, const char *fqdn)
{
	xml_node_t *child;
	const char *name;

	xml_node_for_each_child(ctx->xml, child, cred) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (os_strcasecmp(name, "Policy") == 0)
			set_pps_cred_policy(ctx, id, child);
		else if (os_strcasecmp(name, "CredentialPriority") == 0)
			set_pps_cred_priority(ctx, id, child);
		else if (os_strcasecmp(name, "AAAServerTrustRoot") == 0)
			set_pps_cred_aaa_server_trust_root(ctx, id, child);
		else if (os_strcasecmp(name, "SubscriptionUpdate") == 0)
			set_pps_cred_sub_update(ctx, id, child);
		else if (os_strcasecmp(name, "HomeSP") == 0)
			set_pps_cred_home_sp(ctx, id, child);
		else if (os_strcasecmp(name, "SubscriptionParameters") == 0)
			set_pps_cred_sub_params(ctx, id, child);
		else if (os_strcasecmp(name, "Credential") == 0)
			set_pps_cred_credential(ctx, id, child, fqdn);
		else
			wpa_printf(MSG_INFO, "Unknown credential node '%s'",
				   name);
	}
}


static void set_pps(struct hs20_osu_client *ctx, xml_node_t *pps,
		    const char *fqdn)
{
	xml_node_t *child;
	const char *name;
	int id;
	char *update_identifier = NULL;

	/*
	 * TODO: Could consider more complex mechanism that would remove
	 * credentials only if there are changes in the information sent to
	 * wpa_supplicant.
	 */
	remove_sp_creds(ctx, fqdn);

	xml_node_for_each_child(ctx->xml, child, pps) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (os_strcasecmp(name, "UpdateIdentifier") == 0) {
			update_identifier = xml_node_get_text(ctx->xml, child);
			if (update_identifier) {
				wpa_printf(MSG_INFO, "- UpdateIdentifier = %s",
					   update_identifier);
				break;
			}
		}
	}

	xml_node_for_each_child(ctx->xml, child, pps) {
		xml_node_for_each_check(ctx->xml, child);
		name = xml_node_get_localname(ctx->xml, child);
		if (os_strcasecmp(name, "UpdateIdentifier") == 0)
			continue;
		id = add_cred(ctx->ifname);
		if (id < 0) {
			wpa_printf(MSG_INFO, "Failed to add credential to wpa_supplicant");
			write_summary(ctx, "Failed to add credential to wpa_supplicant");
			break;
		}
		write_summary(ctx, "Add a credential to wpa_supplicant");
		if (update_identifier &&
		    set_cred(ctx->ifname, id, "update_identifier",
			     update_identifier) < 0)
			wpa_printf(MSG_INFO, "Failed to set update_identifier");
		if (set_cred_quoted(ctx->ifname, id, "provisioning_sp", fqdn) <
		    0)
			wpa_printf(MSG_INFO, "Failed to set provisioning_sp");
		wpa_printf(MSG_INFO, "credential localname: '%s'", name);
		set_pps_credential(ctx, id, child, fqdn);
	}

	xml_node_get_text_free(ctx->xml, update_identifier);
}


static void cmd_set_pps(struct hs20_osu_client *ctx, const char *pps_fname)
{
	xml_node_t *pps;
	const char *fqdn;
	char *fqdn_buf = NULL, *pos;

	pps = node_from_file(ctx->xml, pps_fname);
	if (pps == NULL) {
		wpa_printf(MSG_INFO, "Could not read or parse '%s'", pps_fname);
		return;
	}

	fqdn = os_strstr(pps_fname, "SP/");
	if (fqdn) {
		fqdn_buf = os_strdup(fqdn + 3);
		if (fqdn_buf == NULL)
			return;
		pos = os_strchr(fqdn_buf, '/');
		if (pos)
			*pos = '\0';
		fqdn = fqdn_buf;
	} else
		fqdn = "wi-fi.org";

	wpa_printf(MSG_INFO, "Set PPS MO info to wpa_supplicant - SP FQDN %s",
		   fqdn);
	set_pps(ctx, pps, fqdn);

	os_free(fqdn_buf);
	xml_node_free(ctx->xml, pps);
}


static int cmd_get_fqdn(struct hs20_osu_client *ctx, const char *pps_fname)
{
	xml_node_t *pps, *node;
	char *fqdn = NULL;

	pps = node_from_file(ctx->xml, pps_fname);
	if (pps == NULL) {
		wpa_printf(MSG_INFO, "Could not read or parse '%s'", pps_fname);
		return -1;
	}

	node = get_child_node(ctx->xml, pps, "HomeSP/FQDN");
	if (node)
		fqdn = xml_node_get_text(ctx->xml, node);

	xml_node_free(ctx->xml, pps);

	if (fqdn) {
		FILE *f = fopen("pps-fqdn", "w");
		if (f) {
			fprintf(f, "%s", fqdn);
			fclose(f);
		}
		xml_node_get_text_free(ctx->xml, fqdn);
		return 0;
	}

	xml_node_get_text_free(ctx->xml, fqdn);
	return -1;
}


static void cmd_to_tnds(struct hs20_osu_client *ctx, const char *in_fname,
			const char *out_fname, const char *urn, int use_path)
{
	xml_node_t *mo, *node;

	mo = node_from_file(ctx->xml, in_fname);
	if (mo == NULL) {
		wpa_printf(MSG_INFO, "Could not read or parse '%s'", in_fname);
		return;
	}

	node = mo_to_tnds(ctx->xml, mo, use_path, urn, NULL);
	if (node) {
		node_to_file(ctx->xml, out_fname, node);
		xml_node_free(ctx->xml, node);
	}

	xml_node_free(ctx->xml, mo);
}


static void cmd_from_tnds(struct hs20_osu_client *ctx, const char *in_fname,
			  const char *out_fname)
{
	xml_node_t *tnds, *mo;

	tnds = node_from_file(ctx->xml, in_fname);
	if (tnds == NULL) {
		wpa_printf(MSG_INFO, "Could not read or parse '%s'", in_fname);
		return;
	}

	mo = tnds_to_mo(ctx->xml, tnds);
	if (mo) {
		node_to_file(ctx->xml, out_fname, mo);
		xml_node_free(ctx->xml, mo);
	}

	xml_node_free(ctx->xml, tnds);
}


static int init_ctx(struct hs20_osu_client *ctx)
{
	os_memset(ctx, 0, sizeof(*ctx));
	ctx->ifname = "wlan0";
	ctx->xml = xml_node_init_ctx(ctx, NULL);
	if (ctx->xml == NULL)
		return -1;

	ctx->http = http_init_ctx(ctx, ctx->xml);
	if (ctx->http == NULL) {
		xml_node_deinit_ctx(ctx->xml);
		return -1;
	}
	http_ocsp_set(ctx->http, 2);

	return 0;
}


static void deinit_ctx(struct hs20_osu_client *ctx)
{
	http_deinit_ctx(ctx->http);
	xml_node_deinit_ctx(ctx->xml);
}


static void check_workarounds(struct hs20_osu_client *ctx)
{
	FILE *f;
	char buf[100];
	unsigned long int val = 0;

	f = fopen("hs20-osu-client.workarounds", "r");
	if (f == NULL)
		return;

	if (fgets(buf, sizeof(buf), f))
		val = strtoul(buf, NULL, 16);

	fclose(f);

	if (val) {
		wpa_printf(MSG_INFO, "Workarounds enabled: 0x%lx", val);
		ctx->workarounds = val;
		if (ctx->workarounds & WORKAROUND_OCSP_OPTIONAL)
			http_ocsp_set(ctx->http, 1);
	}
}


static void usage(void)
{
	printf("usage: hs20-osu-client [-dddqqKtT] [-S<station ifname>] \\\n"
	       "    [-w<wpa_supplicant ctrl_iface dir>] "
	       "[-r<result file>] [-f<debug file>] \\\n"
	       "    [-s<summary file>] \\\n"
	       "    [-x<spp.xsd file name>] \\\n"
	       "    <command> [arguments..]\n"
	       "commands:\n"
	       "- to_tnds <XML MO> <XML MO in TNDS format> [URN]\n"
	       "- to_tnds2 <XML MO> <XML MO in TNDS format (Path) "
	       "[URN]>\n"
	       "- from_tnds <XML MO in TNDS format> <XML MO>\n"
	       "- set_pps <PerProviderSubscription XML file name>\n"
	       "- get_fqdn <PerProviderSubscription XML file name>\n"
	       "- dl_aaa_ca <PPS> <CA file>\n"
	       "- browser <URL>\n");
}


int main(int argc, char *argv[])
{
	struct hs20_osu_client ctx;
	int c;
	int ret = 0;
	const char *wpa_debug_file_path = NULL;
	extern char *wpas_ctrl_path;
	extern int wpa_debug_level;
	extern int wpa_debug_show_keys;
	extern int wpa_debug_timestamp;

	if (init_ctx(&ctx) < 0)
		return -1;

	for (;;) {
		c = getopt(argc, argv, "df:hKqr:s:S:tTw:");
		if (c < 0)
			break;
		switch (c) {
		case 'd':
			if (wpa_debug_level > 0)
				wpa_debug_level--;
			break;
		case 'f':
			wpa_debug_file_path = optarg;
			break;
		case 'K':
			wpa_debug_show_keys++;
			break;
		case 'q':
			wpa_debug_level++;
			break;
		case 'r':
			ctx.result_file = optarg;
			break;
		case 's':
			ctx.summary_file = optarg;
			break;
		case 'S':
			ctx.ifname = optarg;
			break;
		case 't':
			wpa_debug_timestamp++;
			break;
		case 'T':
			ctx.ignore_tls = 1;
			break;
		case 'w':
			wpas_ctrl_path = optarg;
			break;
		case 'h':
		default:
			usage();
			exit(0);
		}
	}

	if (argc - optind < 1) {
		usage();
		exit(0);
	}

	wpa_debug_open_file(wpa_debug_file_path);

#ifdef __linux__
	setlinebuf(stdout);
#endif /* __linux__ */

	if (ctx.result_file)
		unlink(ctx.result_file);
	wpa_printf(MSG_DEBUG, "===[hs20-osu-client START - command: %s ]======"
		   "================", argv[optind]);
	check_workarounds(&ctx);

	if (strcmp(argv[optind], "to_tnds") == 0) {
		if (argc - optind < 2) {
			usage();
			exit(0);
		}
		cmd_to_tnds(&ctx, argv[optind + 1], argv[optind + 2],
			    argc > optind + 3 ? argv[optind + 3] : NULL,
			    0);
	} else if (strcmp(argv[optind], "to_tnds2") == 0) {
		if (argc - optind < 2) {
			usage();
			exit(0);
		}
		cmd_to_tnds(&ctx, argv[optind + 1], argv[optind + 2],
			    argc > optind + 3 ? argv[optind + 3] : NULL,
			    1);
	} else if (strcmp(argv[optind], "from_tnds") == 0) {
		if (argc - optind < 2) {
			usage();
			exit(0);
		}
		cmd_from_tnds(&ctx, argv[optind + 1], argv[optind + 2]);
	} else if (strcmp(argv[optind], "dl_aaa_ca") == 0) {
		if (argc - optind < 2) {
			usage();
			exit(0);
		}
		cmd_dl_aaa_ca(&ctx, argv[optind + 1], argv[optind + 2]);
	} else if (strcmp(argv[optind], "set_pps") == 0) {
		if (argc - optind < 2) {
			usage();
			exit(0);
		}
		cmd_set_pps(&ctx, argv[optind + 1]);
	} else if (strcmp(argv[optind], "get_fqdn") == 0) {
		if (argc - optind < 1) {
			usage();
			exit(0);
		}
		ret = cmd_get_fqdn(&ctx, argv[optind + 1]);
	} else if (strcmp(argv[optind], "browser") == 0) {
		int ret;

		if (argc - optind < 2) {
			usage();
			exit(0);
		}

		wpa_printf(MSG_INFO, "Launch web browser to URL %s",
			   argv[optind + 1]);
		ret = hs20_web_browser(argv[optind + 1], ctx.ignore_tls);
		wpa_printf(MSG_INFO, "Web browser result: %d", ret);
	} else {
		wpa_printf(MSG_INFO, "Unknown command '%s'", argv[optind]);
	}

	deinit_ctx(&ctx);
	wpa_printf(MSG_DEBUG,
		   "===[hs20-osu-client END ]======================");

	wpa_debug_close_file();

	return ret;
}
