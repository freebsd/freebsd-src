/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/*
 * IEEE 802.11 generic crypto support.
 */
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>   

#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>		/* XXX ETHER_HDR_LEN */

#include <net80211/ieee80211_var.h>

MALLOC_DEFINE(M_80211_CRYPTO, "80211crypto", "802.11 crypto state");

static	int _ieee80211_crypto_delkey(struct ieee80211vap *,
		struct ieee80211_key *);

/*
 * Table of registered cipher modules.
 */
static	const struct ieee80211_cipher *ciphers[IEEE80211_CIPHER_MAX];

/*
 * Default "null" key management routines.
 */
static int
null_key_alloc(struct ieee80211vap *vap, struct ieee80211_key *k,
	ieee80211_keyix *keyix, ieee80211_keyix *rxkeyix)
{

	if (!ieee80211_is_key_global(vap, k)) {
		/*
		 * Not in the global key table, the driver should handle this
		 * by allocating a slot in the h/w key table/cache.  In
		 * lieu of that return key slot 0 for any unicast key
		 * request.  We disallow the request if this is a group key.
		 * This default policy does the right thing for legacy hardware
		 * with a 4 key table.  It also handles devices that pass
		 * packets through untouched when marked with the WEP bit
		 * and key index 0.
		 */
		if (k->wk_flags & IEEE80211_KEY_GROUP)
			return 0;
		*keyix = 0;	/* NB: use key index 0 for ucast key */
	} else {
		*keyix = ieee80211_crypto_get_key_wepidx(vap, k);
	}
	*rxkeyix = IEEE80211_KEYIX_NONE;	/* XXX maybe *keyix? */
	return 1;
}
static int
null_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k)
{
	return 1;
}
static 	int
null_key_set(struct ieee80211vap *vap, const struct ieee80211_key *k)
{
	return 1;
}
static void null_key_update(struct ieee80211vap *vap) {}

/*
 * Write-arounds for common operations.
 */
static __inline void
cipher_detach(struct ieee80211_key *key)
{
	key->wk_cipher->ic_detach(key);
}

static __inline void *
cipher_attach(struct ieee80211vap *vap, struct ieee80211_key *key)
{
	return key->wk_cipher->ic_attach(vap, key);
}

/* 
 * Wrappers for driver key management methods.
 */
static __inline int
dev_key_alloc(struct ieee80211vap *vap,
	struct ieee80211_key *key,
	ieee80211_keyix *keyix, ieee80211_keyix *rxkeyix)
{
	return vap->iv_key_alloc(vap, key, keyix, rxkeyix);
}

static __inline int
dev_key_delete(struct ieee80211vap *vap,
	const struct ieee80211_key *key)
{
	return vap->iv_key_delete(vap, key);
}

static __inline int
dev_key_set(struct ieee80211vap *vap, const struct ieee80211_key *key)
{
	return vap->iv_key_set(vap, key);
}

/*
 * Setup crypto support for a device/shared instance.
 */
void
ieee80211_crypto_attach(struct ieee80211com *ic)
{
	/* NB: we assume everything is pre-zero'd */
	ciphers[IEEE80211_CIPHER_NONE] = &ieee80211_cipher_none;

	/*
	 * Default set of net80211 supported ciphers.
	 *
	 * These are the default set that all drivers are expected to
	 * support, either/or in hardware and software.
	 *
	 * Drivers can add their own support to this and the
	 * hardware cipher list (ic_cryptocaps.)
	 */
	ic->ic_sw_cryptocaps = IEEE80211_CRYPTO_WEP |
	    IEEE80211_CRYPTO_TKIP | IEEE80211_CRYPTO_AES_CCM;

	/*
	 * Default set of key management types supported by net80211.
	 *
	 * These are supported by software net80211 and announced/
	 * driven by hostapd + wpa_supplicant.
	 *
	 * Drivers doing full supplicant offload must not set
	 * anything here.
	 *
	 * Note that IEEE80211_C_WPA1 and IEEE80211_C_WPA2 are the
	 * "old" style way of drivers announcing key management
	 * capabilities.  There are many, many more key management
	 * suites in 802.11-2016 (see 9.4.2.25.3 - AKM suites.)
	 * For now they still need to be set - these flags are checked
	 * when assembling a beacon to reserve space for the WPA
	 * vendor IE (WPA 1) and RSN IE (WPA 2).
	 */
	ic->ic_sw_keymgmtcaps = 0;
}

/*
 * Teardown crypto support.
 */
void
ieee80211_crypto_detach(struct ieee80211com *ic)
{
}

/*
 * Set the supported ciphers for software encryption.
 */
void
ieee80211_crypto_set_supported_software_ciphers(struct ieee80211com *ic,
    uint32_t cipher_set)
{
	ic->ic_sw_cryptocaps = cipher_set;
}

/*
 * Set the supported ciphers for hardware encryption.
 */
void
ieee80211_crypto_set_supported_hardware_ciphers(struct ieee80211com *ic,
    uint32_t cipher_set)
{
	ic->ic_cryptocaps = cipher_set;
}

/*
 * Set the supported software key management by the driver.
 *
 * These are the key management suites that are supported via
 * the driver via hostapd/wpa_supplicant.
 *
 * Key management which is completely offloaded (ie, the supplicant
 * runs in hardware/firmware) must not be set here.
 */
void
ieee80211_crypto_set_supported_driver_keymgmt(struct ieee80211com *ic,
    uint32_t keymgmt_set)
{

	ic->ic_sw_keymgmtcaps = keymgmt_set;
}

/*
 * Setup crypto support for a vap.
 */
void
ieee80211_crypto_vattach(struct ieee80211vap *vap)
{
	int i;

	/* NB: we assume everything is pre-zero'd */
	vap->iv_max_keyix = IEEE80211_WEP_NKID;
	vap->iv_def_txkey = IEEE80211_KEYIX_NONE;
	for (i = 0; i < IEEE80211_WEP_NKID; i++)
		ieee80211_crypto_resetkey(vap, &vap->iv_nw_keys[i],
			IEEE80211_KEYIX_NONE);
	/*
	 * Initialize the driver key support routines to noop entries.
	 * This is useful especially for the cipher test modules.
	 */
	vap->iv_key_alloc = null_key_alloc;
	vap->iv_key_set = null_key_set;
	vap->iv_key_delete = null_key_delete;
	vap->iv_key_update_begin = null_key_update;
	vap->iv_key_update_end = null_key_update;
}

/*
 * Teardown crypto support for a vap.
 */
void
ieee80211_crypto_vdetach(struct ieee80211vap *vap)
{
	ieee80211_crypto_delglobalkeys(vap);
}

/*
 * Register a crypto cipher module.
 */
void
ieee80211_crypto_register(const struct ieee80211_cipher *cip)
{
	if (cip->ic_cipher >= IEEE80211_CIPHER_MAX) {
		net80211_printf("%s: cipher %s has an invalid cipher index %u\n",
			__func__, cip->ic_name, cip->ic_cipher);
		return;
	}
	if (ciphers[cip->ic_cipher] != NULL && ciphers[cip->ic_cipher] != cip) {
		net80211_printf("%s: cipher %s registered with a different template\n",
			__func__, cip->ic_name);
		return;
	}
	ciphers[cip->ic_cipher] = cip;
}

/*
 * Unregister a crypto cipher module.
 */
void
ieee80211_crypto_unregister(const struct ieee80211_cipher *cip)
{
	if (cip->ic_cipher >= IEEE80211_CIPHER_MAX) {
		net80211_printf("%s: cipher %s has an invalid cipher index %u\n",
			__func__, cip->ic_name, cip->ic_cipher);
		return;
	}
	if (ciphers[cip->ic_cipher] != NULL && ciphers[cip->ic_cipher] != cip) {
		net80211_printf("%s: cipher %s registered with a different template\n",
			__func__, cip->ic_name);
		return;
	}
	/* NB: don't complain about not being registered */
	/* XXX disallow if references */
	ciphers[cip->ic_cipher] = NULL;
}

int
ieee80211_crypto_available(u_int cipher)
{
	return cipher < IEEE80211_CIPHER_MAX && ciphers[cipher] != NULL;
}

/* XXX well-known names! */
static const char *cipher_modnames[IEEE80211_CIPHER_MAX] = {
	[IEEE80211_CIPHER_WEP]	   = "wlan_wep",
	[IEEE80211_CIPHER_TKIP]	   = "wlan_tkip",
	[IEEE80211_CIPHER_AES_OCB] = "wlan_aes_ocb",
	[IEEE80211_CIPHER_AES_CCM] = "wlan_ccmp",
	[IEEE80211_CIPHER_TKIPMIC] = "#4",	/* NB: reserved */
	[IEEE80211_CIPHER_CKIP]	   = "wlan_ckip",
	[IEEE80211_CIPHER_NONE]	   = "wlan_none",
	[IEEE80211_CIPHER_AES_CCM_256] = "wlan_ccmp",
	[IEEE80211_CIPHER_BIP_CMAC_128] = "wlan_bip_cmac",
	[IEEE80211_CIPHER_BIP_CMAC_256] = "wlan_bip_cmac",
	[IEEE80211_CIPHER_BIP_GMAC_128] = "wlan_bip_gmac",
	[IEEE80211_CIPHER_BIP_GMAC_256] = "wlan_bip_gmac",
	[IEEE80211_CIPHER_AES_GCM_128]  = "wlan_gcmp",
	[IEEE80211_CIPHER_AES_GCM_256]  = "wlan_gcmp",
};

/* NB: there must be no overlap between user-supplied and device-owned flags */
CTASSERT((IEEE80211_KEY_COMMON & IEEE80211_KEY_DEVICE) == 0);

/*
 * Establish a relationship between the specified key and cipher
 * and, if necessary, allocate a hardware index from the driver.
 * Note that when a fixed key index is required it must be specified.
 *
 * This must be the first call applied to a key; all the other key
 * routines assume wk_cipher is setup.
 *
 * Locking must be handled by the caller using:
 *	ieee80211_key_update_begin(vap);
 *	ieee80211_key_update_end(vap);
 */
int
ieee80211_crypto_newkey(struct ieee80211vap *vap,
	int cipher, int flags, struct ieee80211_key *key)
{
	struct ieee80211com *ic = vap->iv_ic;
	const struct ieee80211_cipher *cip;
	ieee80211_keyix keyix, rxkeyix;
	void *keyctx;
	int oflags;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
	    "%s: cipher %u flags 0x%x keyix %u\n",
	    __func__, cipher, flags, key->wk_keyix);

	/*
	 * Validate cipher and set reference to cipher routines.
	 */
	if (cipher >= IEEE80211_CIPHER_MAX) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
		    "%s: invalid cipher %u\n", __func__, cipher);
		vap->iv_stats.is_crypto_badcipher++;
		return 0;
	}
	cip = ciphers[cipher];
	if (cip == NULL) {
		/*
		 * Auto-load cipher module if we have a well-known name
		 * for it.  It might be better to use string names rather
		 * than numbers and craft a module name based on the cipher
		 * name; e.g. wlan_cipher_<cipher-name>.
		 */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
		    "%s: unregistered cipher %u, load module %s\n",
		    __func__, cipher, cipher_modnames[cipher]);
		ieee80211_load_module(cipher_modnames[cipher]);
		/*
		 * If cipher module loaded it should immediately
		 * call ieee80211_crypto_register which will fill
		 * in the entry in the ciphers array.
		 */
		cip = ciphers[cipher];
		if (cip == NULL) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
			    "%s: unable to load cipher %u, module %s\n",
			    __func__, cipher, cipher_modnames[cipher]);
			vap->iv_stats.is_crypto_nocipher++;
			return 0;
		}
	}

	oflags = key->wk_flags;
	flags &= IEEE80211_KEY_COMMON;
	/* NB: preserve device attributes */
	flags |= (oflags & IEEE80211_KEY_DEVICE);
	/*
	 * If the hardware does not support the cipher then
	 * fallback to a host-based implementation.
	 */
	if ((ic->ic_cryptocaps & (1<<cipher)) == 0) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
		    "%s: no h/w support for cipher %s, falling back to s/w\n",
		    __func__, cip->ic_name);
		flags |= IEEE80211_KEY_SWCRYPT;
	}
	/*
	 * Check if the software cipher is available; if not then
	 * fail it early.
	 *
	 * Some devices do not support all ciphers in software
	 * (for example they don't support a "raw" data path.)
	 */
	if ((flags & IEEE80211_KEY_SWCRYPT) &&
	    (ic->ic_sw_cryptocaps & (1<<cipher)) == 0) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
		    "%s: no s/w support for cipher %s, rejecting\n",
		    __func__, cip->ic_name);
		vap->iv_stats.is_crypto_swcipherfail++;
		return (0);
	}
	/*
	 * Hardware TKIP with software MIC is an important
	 * combination; we handle it by flagging each key,
	 * the cipher modules honor it.
	 */
	if (cipher == IEEE80211_CIPHER_TKIP &&
	    (ic->ic_cryptocaps & IEEE80211_CRYPTO_TKIPMIC) == 0) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
		    "%s: no h/w support for TKIP MIC, falling back to s/w\n",
		    __func__);
		flags |= IEEE80211_KEY_SWMIC;
	}

	/*
	 * Bind cipher to key instance.  Note we do this
	 * after checking the device capabilities so the
	 * cipher module can optimize space usage based on
	 * whether or not it needs to do the cipher work.
	 */
	if (key->wk_cipher != cip || key->wk_flags != flags) {
		/*
		 * Fillin the flags so cipher modules can see s/w
		 * crypto requirements and potentially allocate
		 * different state and/or attach different method
		 * pointers.
		 */
		key->wk_flags = flags;
		keyctx = cip->ic_attach(vap, key);
		if (keyctx == NULL) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
				"%s: unable to attach cipher %s\n",
				__func__, cip->ic_name);
			key->wk_flags = oflags;	/* restore old flags */
			vap->iv_stats.is_crypto_attachfail++;
			return 0;
		}
		cipher_detach(key);
		key->wk_cipher = cip;		/* XXX refcnt? */
		key->wk_private = keyctx;
	}

	/*
	 * Ask the driver for a key index if we don't have one.
	 * Note that entries in the global key table always have
	 * an index; this means it's safe to call this routine
	 * for these entries just to setup the reference to the
	 * cipher template.  Note also that when using software
	 * crypto we also call the driver to give us a key index.
	 */
	if ((key->wk_flags & IEEE80211_KEY_DEVKEY) == 0) {
		if (!dev_key_alloc(vap, key, &keyix, &rxkeyix)) {
			/*
			 * Unable to setup driver state.
			 */
			vap->iv_stats.is_crypto_keyfail++;
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
			    "%s: unable to setup cipher %s\n",
			    __func__, cip->ic_name);
			return 0;
		}
		if (key->wk_flags != flags) {
			/*
			 * Driver overrode flags we setup; typically because
			 * resources were unavailable to handle _this_ key.
			 * Re-attach the cipher context to allow cipher
			 * modules to handle differing requirements.
			 */
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
			    "%s: driver override for cipher %s, flags "
			    "%b -> %b\n", __func__, cip->ic_name,
			    oflags, IEEE80211_KEY_BITS,
			    key->wk_flags, IEEE80211_KEY_BITS);
			keyctx = cip->ic_attach(vap, key);
			if (keyctx == NULL) {
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
				    "%s: unable to attach cipher %s with "
				    "flags %b\n", __func__, cip->ic_name,
				    key->wk_flags, IEEE80211_KEY_BITS);
				key->wk_flags = oflags;	/* restore old flags */
				vap->iv_stats.is_crypto_attachfail++;
				return 0;
			}
			cipher_detach(key);
			key->wk_cipher = cip;		/* XXX refcnt? */
			key->wk_private = keyctx;
		}
		key->wk_keyix = keyix;
		key->wk_rxkeyix = rxkeyix;
		key->wk_flags |= IEEE80211_KEY_DEVKEY;
	}
	return 1;
}

/*
 * Remove the key (no locking, for internal use).
 */
static int
_ieee80211_crypto_delkey(struct ieee80211vap *vap, struct ieee80211_key *key)
{
	KASSERT(key->wk_cipher != NULL, ("No cipher!"));

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
	    "%s: %s keyix %u flags %b rsc %ju tsc %ju len %u\n",
	    __func__, key->wk_cipher->ic_name,
	    key->wk_keyix, key->wk_flags, IEEE80211_KEY_BITS,
	    key->wk_keyrsc[IEEE80211_NONQOS_TID], key->wk_keytsc,
	    key->wk_keylen);

	if (key->wk_flags & IEEE80211_KEY_DEVKEY) {
		/*
		 * Remove hardware entry.
		 */
		/* XXX key cache */
		if (!dev_key_delete(vap, key)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
			    "%s: driver did not delete key index %u\n",
			    __func__, key->wk_keyix);
			vap->iv_stats.is_crypto_delkey++;
			/* XXX recovery? */
		}
	}
	cipher_detach(key);
	memset(key, 0, sizeof(*key));
	ieee80211_crypto_resetkey(vap, key, IEEE80211_KEYIX_NONE);
	return 1;
}

/*
 * Remove the specified key.
 */
int
ieee80211_crypto_delkey(struct ieee80211vap *vap, struct ieee80211_key *key)
{
	int status;

	ieee80211_key_update_begin(vap);
	status = _ieee80211_crypto_delkey(vap, key);
	ieee80211_key_update_end(vap);
	return status;
}

/*
 * Clear the global key table.
 */
void
ieee80211_crypto_delglobalkeys(struct ieee80211vap *vap)
{
	int i;

	ieee80211_key_update_begin(vap);
	for (i = 0; i < IEEE80211_WEP_NKID; i++)
		(void) _ieee80211_crypto_delkey(vap, &vap->iv_nw_keys[i]);
	ieee80211_key_update_end(vap);
}

/*
 * Set the contents of the specified key.
 *
 * Locking must be handled by the caller using:
 *	ieee80211_key_update_begin(vap);
 *	ieee80211_key_update_end(vap);
 */
int
ieee80211_crypto_setkey(struct ieee80211vap *vap, struct ieee80211_key *key)
{
	const struct ieee80211_cipher *cip = key->wk_cipher;

	KASSERT(cip != NULL, ("No cipher!"));

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
	    "%s: %s keyix %u flags %b mac %s rsc %ju tsc %ju len %u\n",
	    __func__, cip->ic_name, key->wk_keyix,
	    key->wk_flags, IEEE80211_KEY_BITS, ether_sprintf(key->wk_macaddr),
	    key->wk_keyrsc[IEEE80211_NONQOS_TID], key->wk_keytsc,
	    key->wk_keylen);

	if ((key->wk_flags & IEEE80211_KEY_DEVKEY)  == 0) {
		/* XXX nothing allocated, should not happen */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
		    "%s: no device key setup done; should not happen!\n",
		    __func__);
		vap->iv_stats.is_crypto_setkey_nokey++;
		return 0;
	}
	/*
	 * Give cipher a chance to validate key contents.
	 * XXX should happen before modifying state.
	 */
	if (!cip->ic_setkey(key)) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
		    "%s: cipher %s rejected key index %u len %u flags %b\n",
		    __func__, cip->ic_name, key->wk_keyix,
		    key->wk_keylen, key->wk_flags, IEEE80211_KEY_BITS);
		vap->iv_stats.is_crypto_setkey_cipher++;
		return 0;
	}
	return dev_key_set(vap, key);
}

/*
 * Return index if the key is a WEP key (0..3); -1 otherwise.
 *
 * This is different to "get_keyid" which defaults to returning
 * 0 for unicast keys; it assumes that it won't be used for WEP.
 */
int
ieee80211_crypto_get_key_wepidx(const struct ieee80211vap *vap,
    const struct ieee80211_key *k)
{

	if (ieee80211_is_key_global(vap, k)) {
		return (k - vap->iv_nw_keys);
	}
	return (-1);
}

/*
 * Note: only supports a single unicast key (0).
 */
uint8_t
ieee80211_crypto_get_keyid(struct ieee80211vap *vap, struct ieee80211_key *k)
{
	if (ieee80211_is_key_global(vap, k)) {
		return (k - vap->iv_nw_keys);
	}

	return (0);
}

struct ieee80211_key *
ieee80211_crypto_get_txkey(struct ieee80211_node *ni, struct mbuf *m)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_frame *wh;

	/*
	 * Multicast traffic always uses the multicast key.
	 *
	 * Historically we would fall back to the default
	 * transmit key if there was no unicast key.  This
	 * behaviour was documented up to IEEE Std 802.11-2016,
	 * 12.9.2.2 Per-MSDU/Per-A-MSDU Tx pseudocode, in the
	 * 'else' case but is no longer in later versions of
	 * the standard.  Additionally falling back to the
	 * group key for unicast was a security risk.
	 */
	wh = mtod(m, struct ieee80211_frame *);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		if (vap->iv_def_txkey == IEEE80211_KEYIX_NONE) {
			IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO,
			    wh->i_addr1,
			    "no default transmit key (%s) deftxkey %u",
			    __func__, vap->iv_def_txkey);
			vap->iv_stats.is_tx_nodefkey++;
			return NULL;
		}
		return &vap->iv_nw_keys[vap->iv_def_txkey];
	}

	if (IEEE80211_KEY_UNDEFINED(&ni->ni_ucastkey))
		return NULL;
	return &ni->ni_ucastkey;
}

/*
 * Add privacy headers appropriate for the specified key.
 */
struct ieee80211_key *
ieee80211_crypto_encap(struct ieee80211_node *ni, struct mbuf *m)
{
	struct ieee80211_key *k;
	const struct ieee80211_cipher *cip;

	if ((k = ieee80211_crypto_get_txkey(ni, m)) != NULL) {
		cip = k->wk_cipher;
		return (cip->ic_encap(k, m) ? k : NULL);
	}

	return NULL;
}

/*
 * Validate and strip privacy headers (and trailer) for a
 * received frame that has the WEP/Privacy bit set.
 */
int
ieee80211_crypto_decap(struct ieee80211_node *ni, struct mbuf *m, int hdrlen,
    struct ieee80211_key **key)
{
#define	IEEE80211_WEP_HDRLEN	(IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN)
#define	IEEE80211_WEP_MINLEN \
	(sizeof(struct ieee80211_frame) + \
	IEEE80211_WEP_HDRLEN + IEEE80211_WEP_CRCLEN)
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_key *k;
	struct ieee80211_frame *wh;
	const struct ieee80211_rx_stats *rxs;
	const struct ieee80211_cipher *cip;
	uint8_t keyid;

	/*
	 * Check for hardware decryption and IV stripping.
	 * If the IV is stripped then we definitely can't find a key.
	 * Set the key to NULL but return true; upper layers
	 * will need to handle a NULL key for a successful
	 * decrypt.
	 */
	rxs = ieee80211_get_rx_params_ptr(m);
	if ((rxs != NULL) && (rxs->c_pktflags & IEEE80211_RX_F_DECRYPTED)) {
		if (rxs->c_pktflags & IEEE80211_RX_F_IV_STRIP) {
			/*
			 * Hardware decrypted, IV stripped.
			 * We can't find a key with a stripped IV.
			 * Return successful.
			 */
			*key = NULL;
			return (1);
		}
	}

	/* NB: this minimum size data frame could be bigger */
	if (m->m_pkthdr.len < IEEE80211_WEP_MINLEN) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
			"%s: WEP data frame too short, len %u\n",
			__func__, m->m_pkthdr.len);
		vap->iv_stats.is_rx_tooshort++;	/* XXX need unique stat? */
		*key = NULL;
		return (0);
	}

	/*
	 * Locate the key. If unicast and there is no unicast
	 * key then we fall back to the key id in the header.
	 * This assumes unicast keys are only configured when
	 * the key id in the header is meaningless (typically 0).
	 */
	wh = mtod(m, struct ieee80211_frame *);
	m_copydata(m, hdrlen + IEEE80211_WEP_IVLEN, sizeof(keyid), &keyid);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    IEEE80211_KEY_UNDEFINED(&ni->ni_ucastkey))
		k = &vap->iv_nw_keys[keyid >> 6];
	else
		k = &ni->ni_ucastkey;

	/*
	 * Ensure crypto header is contiguous and long enough for all
	 * decap work.
	 */
	cip = k->wk_cipher;
	if (m->m_len < hdrlen + cip->ic_header) {
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
		    "frame is too short (%d < %u) for crypto decap",
		    cip->ic_name, m->m_len, hdrlen + cip->ic_header);
		vap->iv_stats.is_rx_tooshort++;
		*key = NULL;
		return (0);
	}

	/*
	 * Attempt decryption.
	 *
	 * If we fail then don't return the key - return NULL
	 * and an error.
	 */
	if (cip->ic_decap(k, m, hdrlen)) {
		/* success */
		*key = k;
		return (1);
	}

	/* Failure */
	*key = NULL;
	return (0);
#undef IEEE80211_WEP_MINLEN
#undef IEEE80211_WEP_HDRLEN
}

/**
 * @brief Check and remove any post-defragmentation MIC from an MSDU.
 *
 * This is called after defragmentation.  Crypto types that implement
 * a MIC/ICV check per MSDU will not implement this function.
 *
 * As an example, TKIP decapsulation covers both MIC/ICV checks per
 * MPDU (the "WEP" ICV) and then a Michael MIC verification on the
 * defragmented MSDU.  Please see 802.11-2020 12.5.2.1.3 (TKIP decapsulation)
 * for more information.
 *
 * @param vap	the current VAP
 * @param k	the current key
 * @param m	the mbuf representing the MSDU
 * @param f	set to 1 to force a MSDU MIC check, even if HW decrypted
 * @returns	0 if error / MIC check failed, 1 if OK
 */
int
ieee80211_crypto_demic(struct ieee80211vap *vap, struct ieee80211_key *k,
    struct mbuf *m, int force)
{
	const struct ieee80211_cipher *cip;
	const struct ieee80211_rx_stats *rxs;
	struct ieee80211_frame *wh;

	rxs = ieee80211_get_rx_params_ptr(m);
	wh = mtod(m, struct ieee80211_frame *);

	/*
	 * Handle demic / mic errors from hardware-decrypted offload devices.
	 */
	if ((rxs != NULL) && (rxs->c_pktflags & IEEE80211_RX_F_DECRYPTED)) {
		if ((rxs->c_pktflags & IEEE80211_RX_F_FAIL_MMIC) != 0) {
			/*
			 * Hardware has said MMIC failed.  We don't care about
			 * whether it was stripped or not.
			 *
			 * Eventually - teach the demic methods in crypto
			 * modules to handle a NULL key and not to dereference
			 * it.
			 */
			ieee80211_notify_michael_failure(vap, wh,
			    IEEE80211_KEYIX_NONE);
			return (0);
		}

		if ((rxs->c_pktflags &
		    (IEEE80211_RX_F_MIC_STRIP|IEEE80211_RX_F_MMIC_STRIP)) != 0) {
			/*
			 * Hardware has decrypted and not indicated a
			 * MIC failure and has stripped the MIC.
			 * We may not have a key, so for now just
			 * return OK.
			 */
			return (1);
		}
	}

	/*
	 * If we don't have a key at this point then we don't
	 * have to demic anything.
	 */
	if (k == NULL)
		return (1);

	cip = k->wk_cipher;
	return (cip->ic_miclen > 0 ? cip->ic_demic(k, m, force) : 1);
}

static void
load_ucastkey(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_key *k;

	if (vap->iv_state != IEEE80211_S_RUN)
		return;
	k = &ni->ni_ucastkey;
	if (k->wk_flags & IEEE80211_KEY_DEVKEY)
		dev_key_set(vap, k);
}

/*
 * Re-load all keys known to the 802.11 layer that may
 * have hardware state backing them.  This is used by
 * drivers on resume to push keys down into the device.
 */
void
ieee80211_crypto_reload_keys(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;
	int i;

	/*
	 * Keys in the global key table of each vap.
	 */
	/* NB: used only during resume so don't lock for now */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_state != IEEE80211_S_RUN)
			continue;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			const struct ieee80211_key *k = &vap->iv_nw_keys[i];
			if (k->wk_flags & IEEE80211_KEY_DEVKEY)
				dev_key_set(vap, k);
		}
	}
	/*
	 * Unicast keys.
	 */
	ieee80211_iterate_nodes(&ic->ic_sta, load_ucastkey, NULL);
}

/*
 * Set the default key index for WEP, or KEYIX_NONE for no default TX key.
 *
 * This should be done as part of a key update block (iv_key_update_begin /
 * iv_key_update_end.)
 */
void
ieee80211_crypto_set_deftxkey(struct ieee80211vap *vap, ieee80211_keyix kid)
{

	/* XXX TODO: assert we're in a key update block */

	vap->iv_update_deftxkey(vap, kid);
}

/**
 * @brief Calculate the AAD required for this frame for AES-GCM/AES-CCM.
 *
 * The contents are described in 802.11-2020 12.5.3.3.3 (Construct AAD)
 * under AES-CCM and are shared with AES-GCM as covered in 12.5.5.3.3
 * (Construct AAD) (AES-GCM).
 *
 * NOTE: the first two bytes are a 16 bit big-endian length, which are used
 * by AES-CCM as part of the Adata field (RFC 3610, section 2.2
 * (Authentication)) to indicate the length of the Adata field itself.
 * Since this is small and fits in 0xfeff bytes, the length field
 * uses the two byte big endian option.
 *
 * AES-GCM doesn't require the length at the beginning and will need to
 * skip it.
 *
 * TODO: net80211 currently doesn't support negotiating SPP (Signaling
 * and Payload Protected A-MSDUs) and thus bit 7 of the QoS control field
 * is always masked.
 *
 * TODO: net80211 currently doesn't support DMG (802.11ad) so bit 7
 * (A-MSDU present) and bit 8 (A-MSDU type) are always masked.
 *
 * @param wh	802.11 frame to calculate the AAD over
 * @param aad	AAD (additional authentication data) buffer
 * @param len	The AAD buffer length in bytes.
 * @returns	The number of AAD payload bytes (ignoring the first two
 * 		bytes, which are the AAD payload length in big-endian).
 */
uint16_t
ieee80211_crypto_init_aad(const struct ieee80211_frame *wh, uint8_t *aad,
    int len)
{
	uint16_t aad_len;

	memset(aad, 0, len);

	/*
	 * AAD for PV0 MPDUs:
	 *
	 * FC with bits 4..6 and 11..13 masked to zero; 14 is always one
	 * A1 | A2 | A3
	 * SC with bits 4..15 (seq#) masked to zero
	 * A4 (if present)
	 * QC (if present)
	 */
	aad[0] = 0;	/* AAD length >> 8 */
	/* NB: aad[1] set below */
	aad[2] = wh->i_fc[0] & 0x8f;	/* see above for bitfields */
	aad[3] = wh->i_fc[1] & 0xc7;	/* see above for bitfields */
	/* mask aad[3] b7 if frame is data frame w/ QoS control field */
	if (IEEE80211_IS_QOS_ANY(wh))
		aad[3] &= 0x7f;

	/* NB: we know 3 addresses are contiguous */
	memcpy(aad + 4, wh->i_addr1, 3 * IEEE80211_ADDR_LEN);
	aad[22] = wh->i_seq[0] & IEEE80211_SEQ_FRAG_MASK;
	aad[23] = 0; /* all bits masked */
	/*
	 * Construct variable-length portion of AAD based
	 * on whether this is a 4-address frame/QOS frame.
	 * We always zero-pad to 32 bytes before running it
	 * through the cipher.
	 */
	if (IEEE80211_IS_DSTODS(wh)) {
		IEEE80211_ADDR_COPY(aad + 24,
			((const struct ieee80211_frame_addr4 *)wh)->i_addr4);
		if (IEEE80211_IS_QOS_ANY(wh)) {
			const struct ieee80211_qosframe_addr4 *qwh4 =
				(const struct ieee80211_qosframe_addr4 *) wh;
			/* TODO: SPP A-MSDU / A-MSDU present bit */
			aad[30] = qwh4->i_qos[0] & 0x0f;/* just priority bits */
			aad[31] = 0;
			aad_len = aad[1] = 22 + IEEE80211_ADDR_LEN + 2;
		} else {
			*(uint16_t *)&aad[30] = 0;
			aad_len = aad[1] = 22 + IEEE80211_ADDR_LEN;
		}
	} else {
		if (IEEE80211_IS_QOS_ANY(wh)) {
			const struct ieee80211_qosframe *qwh =
				(const struct ieee80211_qosframe*) wh;
			/* TODO: SPP A-MSDU / A-MSDU present bit */
			aad[24] = qwh->i_qos[0] & 0x0f;	/* just priority bits */
			aad[25] = 0;
			aad_len = aad[1] = 22 + 2;
		} else {
			*(uint16_t *)&aad[24] = 0;
			aad_len = aad[1] = 22;
		}
		*(uint16_t *)&aad[26] = 0;
		*(uint32_t *)&aad[28] = 0;
	}

	return (aad_len);
}
