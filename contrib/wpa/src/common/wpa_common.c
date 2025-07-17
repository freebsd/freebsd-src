/*
 * WPA/RSN - Shared functions for supplicant and authenticator
 * Copyright (c) 2002-2018, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto/md5.h"
#include "crypto/sha1.h"
#include "crypto/sha256.h"
#include "crypto/sha384.h"
#include "crypto/sha512.h"
#include "crypto/aes_wrap.h"
#include "crypto/crypto.h"
#include "ieee802_11_defs.h"
#include "ieee802_11_common.h"
#include "defs.h"
#include "wpa_common.h"


static unsigned int wpa_kck_len(int akmp, size_t pmk_len)
{
	switch (akmp) {
	case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
	case WPA_KEY_MGMT_IEEE8021X_SHA384:
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
		return 24;
	case WPA_KEY_MGMT_FILS_SHA256:
	case WPA_KEY_MGMT_FT_FILS_SHA256:
	case WPA_KEY_MGMT_FILS_SHA384:
	case WPA_KEY_MGMT_FT_FILS_SHA384:
		return 0;
	case WPA_KEY_MGMT_DPP:
		return pmk_len / 2;
	case WPA_KEY_MGMT_OWE:
		return pmk_len / 2;
	case WPA_KEY_MGMT_SAE_EXT_KEY:
	case WPA_KEY_MGMT_FT_SAE_EXT_KEY:
		return pmk_len / 2;
	default:
		return 16;
	}
}


#ifdef CONFIG_IEEE80211R
static unsigned int wpa_kck2_len(int akmp)
{
	switch (akmp) {
	case WPA_KEY_MGMT_FT_FILS_SHA256:
		return 16;
	case WPA_KEY_MGMT_FT_FILS_SHA384:
		return 24;
	default:
		return 0;
	}
}
#endif /* CONFIG_IEEE80211R */


static unsigned int wpa_kek_len(int akmp, size_t pmk_len)
{
	switch (akmp) {
	case WPA_KEY_MGMT_FILS_SHA384:
	case WPA_KEY_MGMT_FT_FILS_SHA384:
		return 64;
	case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
	case WPA_KEY_MGMT_FILS_SHA256:
	case WPA_KEY_MGMT_FT_FILS_SHA256:
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
	case WPA_KEY_MGMT_IEEE8021X_SHA384:
		return 32;
	case WPA_KEY_MGMT_DPP:
		return pmk_len <= 32 ? 16 : 32;
	case WPA_KEY_MGMT_OWE:
		return pmk_len <= 32 ? 16 : 32;
	case WPA_KEY_MGMT_SAE_EXT_KEY:
	case WPA_KEY_MGMT_FT_SAE_EXT_KEY:
		return pmk_len <= 32 ? 16 : 32;
	default:
		return 16;
	}
}


#ifdef CONFIG_IEEE80211R
static unsigned int wpa_kek2_len(int akmp)
{
	switch (akmp) {
	case WPA_KEY_MGMT_FT_FILS_SHA256:
		return 16;
	case WPA_KEY_MGMT_FT_FILS_SHA384:
		return 32;
	default:
		return 0;
	}
}
#endif /* CONFIG_IEEE80211R */


unsigned int wpa_mic_len(int akmp, size_t pmk_len)
{
	switch (akmp) {
	case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
	case WPA_KEY_MGMT_IEEE8021X_SHA384:
		return 24;
	case WPA_KEY_MGMT_FILS_SHA256:
	case WPA_KEY_MGMT_FILS_SHA384:
	case WPA_KEY_MGMT_FT_FILS_SHA256:
	case WPA_KEY_MGMT_FT_FILS_SHA384:
		return 0;
	case WPA_KEY_MGMT_DPP:
		return pmk_len / 2;
	case WPA_KEY_MGMT_OWE:
		return pmk_len / 2;
	case WPA_KEY_MGMT_SAE_EXT_KEY:
	case WPA_KEY_MGMT_FT_SAE_EXT_KEY:
		return pmk_len / 2;
	default:
		return 16;
	}
}


/**
 * wpa_use_akm_defined - Is AKM-defined Key Descriptor Version used
 * @akmp: WPA_KEY_MGMT_* used in key derivation
 * Returns: 1 if AKM-defined Key Descriptor Version is used; 0 otherwise
 */
int wpa_use_akm_defined(int akmp)
{
	return akmp == WPA_KEY_MGMT_OSEN ||
		akmp == WPA_KEY_MGMT_OWE ||
		akmp == WPA_KEY_MGMT_DPP ||
		akmp == WPA_KEY_MGMT_FT_IEEE8021X_SHA384 ||
		akmp == WPA_KEY_MGMT_IEEE8021X_SHA384 ||
		wpa_key_mgmt_sae(akmp) ||
		wpa_key_mgmt_suite_b(akmp) ||
		wpa_key_mgmt_fils(akmp);
}


/**
 * wpa_use_cmac - Is CMAC integrity algorithm used for EAPOL-Key MIC
 * @akmp: WPA_KEY_MGMT_* used in key derivation
 * Returns: 1 if CMAC is used; 0 otherwise
 */
int wpa_use_cmac(int akmp)
{
	return akmp == WPA_KEY_MGMT_OSEN ||
		akmp == WPA_KEY_MGMT_OWE ||
		akmp == WPA_KEY_MGMT_DPP ||
		wpa_key_mgmt_ft(akmp) ||
		wpa_key_mgmt_sha256(akmp) ||
		(wpa_key_mgmt_sae(akmp) &&
		 !wpa_key_mgmt_sae_ext_key(akmp)) ||
		wpa_key_mgmt_suite_b(akmp);
}


/**
 * wpa_use_aes_key_wrap - Is AES Keywrap algorithm used for EAPOL-Key Key Data
 * @akmp: WPA_KEY_MGMT_* used in key derivation
 * Returns: 1 if AES Keywrap is used; 0 otherwise
 *
 * Note: AKM 00-0F-AC:1 and 00-0F-AC:2 have special rules for selecting whether
 * to use AES Keywrap based on the negotiated pairwise cipher. This function
 * does not cover those special cases.
 */
int wpa_use_aes_key_wrap(int akmp)
{
	return akmp == WPA_KEY_MGMT_OSEN ||
		akmp == WPA_KEY_MGMT_OWE ||
		akmp == WPA_KEY_MGMT_DPP ||
		akmp == WPA_KEY_MGMT_IEEE8021X_SHA384 ||
		wpa_key_mgmt_ft(akmp) ||
		wpa_key_mgmt_sha256(akmp) ||
		wpa_key_mgmt_sae(akmp) ||
		wpa_key_mgmt_suite_b(akmp);
}


/**
 * wpa_eapol_key_mic - Calculate EAPOL-Key MIC
 * @key: EAPOL-Key Key Confirmation Key (KCK)
 * @key_len: KCK length in octets
 * @akmp: WPA_KEY_MGMT_* used in key derivation
 * @ver: Key descriptor version (WPA_KEY_INFO_TYPE_*)
 * @buf: Pointer to the beginning of the EAPOL header (version field)
 * @len: Length of the EAPOL frame (from EAPOL header to the end of the frame)
 * @mic: Pointer to the buffer to which the EAPOL-Key MIC is written
 * Returns: 0 on success, -1 on failure
 *
 * Calculate EAPOL-Key MIC for an EAPOL-Key packet. The EAPOL-Key MIC field has
 * to be cleared (all zeroes) when calling this function.
 *
 * Note: 'IEEE Std 802.11i-2004 - 8.5.2 EAPOL-Key frames' has an error in the
 * description of the Key MIC calculation. It includes packet data from the
 * beginning of the EAPOL-Key header, not EAPOL header. This incorrect change
 * happened during final editing of the standard and the correct behavior is
 * defined in the last draft (IEEE 802.11i/D10).
 */
int wpa_eapol_key_mic(const u8 *key, size_t key_len, int akmp, int ver,
		      const u8 *buf, size_t len, u8 *mic)
{
	u8 hash[SHA512_MAC_LEN];

	if (key_len == 0) {
		wpa_printf(MSG_DEBUG,
			   "WPA: KCK not set - cannot calculate MIC");
		return -1;
	}

	switch (ver) {
#ifndef CONFIG_FIPS
	case WPA_KEY_INFO_TYPE_HMAC_MD5_RC4:
		wpa_printf(MSG_DEBUG, "WPA: EAPOL-Key MIC using HMAC-MD5");
		return hmac_md5(key, key_len, buf, len, mic);
#endif /* CONFIG_FIPS */
	case WPA_KEY_INFO_TYPE_HMAC_SHA1_AES:
		wpa_printf(MSG_DEBUG, "WPA: EAPOL-Key MIC using HMAC-SHA1");
		if (hmac_sha1(key, key_len, buf, len, hash))
			return -1;
		os_memcpy(mic, hash, MD5_MAC_LEN);
		break;
	case WPA_KEY_INFO_TYPE_AES_128_CMAC:
		wpa_printf(MSG_DEBUG, "WPA: EAPOL-Key MIC using AES-CMAC");
		return omac1_aes_128(key, buf, len, mic);
	case WPA_KEY_INFO_TYPE_AKM_DEFINED:
		switch (akmp) {
#ifdef CONFIG_SAE
		case WPA_KEY_MGMT_SAE:
		case WPA_KEY_MGMT_FT_SAE:
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key MIC using AES-CMAC (AKM-defined - SAE)");
			return omac1_aes_128(key, buf, len, mic);
		case WPA_KEY_MGMT_SAE_EXT_KEY:
		case WPA_KEY_MGMT_FT_SAE_EXT_KEY:
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key MIC using HMAC-SHA%u (AKM-defined - SAE-EXT-KEY)",
				   (unsigned int) key_len * 8 * 2);
			if (key_len == 128 / 8) {
				if (hmac_sha256(key, key_len, buf, len, hash))
					return -1;
#ifdef CONFIG_SHA384
			} else if (key_len == 192 / 8) {
				if (hmac_sha384(key, key_len, buf, len, hash))
					return -1;
#endif /* CONFIG_SHA384 */
#ifdef CONFIG_SHA512
			} else if (key_len == 256 / 8) {
				if (hmac_sha512(key, key_len, buf, len, hash))
					return -1;
#endif /* CONFIG_SHA512 */
			} else {
				wpa_printf(MSG_INFO,
					   "SAE: Unsupported KCK length: %u",
					   (unsigned int) key_len);
				return -1;
			}
			os_memcpy(mic, hash, key_len);
			break;
#endif /* CONFIG_SAE */
#ifdef CONFIG_HS20
		case WPA_KEY_MGMT_OSEN:
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key MIC using AES-CMAC (AKM-defined - OSEN)");
			return omac1_aes_128(key, buf, len, mic);
#endif /* CONFIG_HS20 */
#ifdef CONFIG_SUITEB
		case WPA_KEY_MGMT_IEEE8021X_SUITE_B:
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key MIC using HMAC-SHA256 (AKM-defined - Suite B)");
			if (hmac_sha256(key, key_len, buf, len, hash))
				return -1;
			os_memcpy(mic, hash, MD5_MAC_LEN);
			break;
#endif /* CONFIG_SUITEB */
#ifdef CONFIG_SUITEB192
		case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key MIC using HMAC-SHA384 (AKM-defined - Suite B 192-bit)");
			if (hmac_sha384(key, key_len, buf, len, hash))
				return -1;
			os_memcpy(mic, hash, 24);
			break;
#endif /* CONFIG_SUITEB192 */
#ifdef CONFIG_OWE
		case WPA_KEY_MGMT_OWE:
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key MIC using HMAC-SHA%u (AKM-defined - OWE)",
				   (unsigned int) key_len * 8 * 2);
			if (key_len == 128 / 8) {
				if (hmac_sha256(key, key_len, buf, len, hash))
					return -1;
			} else if (key_len == 192 / 8) {
				if (hmac_sha384(key, key_len, buf, len, hash))
					return -1;
			} else if (key_len == 256 / 8) {
				if (hmac_sha512(key, key_len, buf, len, hash))
					return -1;
			} else {
				wpa_printf(MSG_INFO,
					   "OWE: Unsupported KCK length: %u",
					   (unsigned int) key_len);
				return -1;
			}
			os_memcpy(mic, hash, key_len);
			break;
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP
		case WPA_KEY_MGMT_DPP:
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key MIC using HMAC-SHA%u (AKM-defined - DPP)",
				   (unsigned int) key_len * 8 * 2);
			if (key_len == 128 / 8) {
				if (hmac_sha256(key, key_len, buf, len, hash))
					return -1;
			} else if (key_len == 192 / 8) {
				if (hmac_sha384(key, key_len, buf, len, hash))
					return -1;
			} else if (key_len == 256 / 8) {
				if (hmac_sha512(key, key_len, buf, len, hash))
					return -1;
			} else {
				wpa_printf(MSG_INFO,
					   "DPP: Unsupported KCK length: %u",
					   (unsigned int) key_len);
				return -1;
			}
			os_memcpy(mic, hash, key_len);
			break;
#endif /* CONFIG_DPP */
#ifdef CONFIG_SHA384
		case WPA_KEY_MGMT_IEEE8021X_SHA384:
#ifdef CONFIG_IEEE80211R
		case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
#endif /* CONFIG_IEEE80211R */
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key MIC using HMAC-SHA384 (AKM-defined - 802.1X SHA384)");
			if (hmac_sha384(key, key_len, buf, len, hash))
				return -1;
			os_memcpy(mic, hash, 24);
			break;
#endif /* CONFIG_SHA384 */
		default:
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key MIC algorithm not known (AKM-defined - akmp=0x%x)",
				   akmp);
			return -1;
		}
		break;
	default:
		wpa_printf(MSG_DEBUG,
			   "WPA: EAPOL-Key MIC algorithm not known (ver=%d)",
			   ver);
		return -1;
	}

	return 0;
}


/**
 * wpa_pmk_to_ptk - Calculate PTK from PMK, addresses, and nonces
 * @pmk: Pairwise master key
 * @pmk_len: Length of PMK
 * @label: Label to use in derivation
 * @addr1: AA or SA
 * @addr2: SA or AA
 * @nonce1: ANonce or SNonce
 * @nonce2: SNonce or ANonce
 * @ptk: Buffer for pairwise transient key
 * @akmp: Negotiated AKM
 * @cipher: Negotiated pairwise cipher
 * @kdk_len: The length in octets that should be derived for KDK
 * Returns: 0 on success, -1 on failure
 *
 * IEEE Std 802.11i-2004 - 8.5.1.2 Pairwise key hierarchy
 * PTK = PRF-X(PMK, "Pairwise key expansion",
 *             Min(AA, SA) || Max(AA, SA) ||
 *             Min(ANonce, SNonce) || Max(ANonce, SNonce)
 *             [ || Z.x ])
 *
 * The optional Z.x component is used only with DPP and that part is not defined
 * in IEEE 802.11.
 */
int wpa_pmk_to_ptk(const u8 *pmk, size_t pmk_len, const char *label,
		   const u8 *addr1, const u8 *addr2,
		   const u8 *nonce1, const u8 *nonce2,
		   struct wpa_ptk *ptk, int akmp, int cipher,
		   const u8 *z, size_t z_len, size_t kdk_len)
{
#define MAX_Z_LEN 66 /* with NIST P-521 */
	u8 data[2 * ETH_ALEN + 2 * WPA_NONCE_LEN + MAX_Z_LEN];
	size_t data_len = 2 * ETH_ALEN + 2 * WPA_NONCE_LEN;
	u8 tmp[WPA_KCK_MAX_LEN + WPA_KEK_MAX_LEN + WPA_TK_MAX_LEN +
		WPA_KDK_MAX_LEN];
	size_t ptk_len;
#ifdef CONFIG_OWE
	int owe_ptk_workaround = 0;

	if (akmp == (WPA_KEY_MGMT_OWE | WPA_KEY_MGMT_PSK_SHA256)) {
		owe_ptk_workaround = 1;
		akmp = WPA_KEY_MGMT_OWE;
	}
#endif /* CONFIG_OWE */

	if (pmk_len == 0) {
		wpa_printf(MSG_ERROR, "WPA: No PMK set for PTK derivation");
		return -1;
	}

	if (z_len > MAX_Z_LEN)
		return -1;

	if (os_memcmp(addr1, addr2, ETH_ALEN) < 0) {
		os_memcpy(data, addr1, ETH_ALEN);
		os_memcpy(data + ETH_ALEN, addr2, ETH_ALEN);
	} else {
		os_memcpy(data, addr2, ETH_ALEN);
		os_memcpy(data + ETH_ALEN, addr1, ETH_ALEN);
	}

	if (os_memcmp(nonce1, nonce2, WPA_NONCE_LEN) < 0) {
		os_memcpy(data + 2 * ETH_ALEN, nonce1, WPA_NONCE_LEN);
		os_memcpy(data + 2 * ETH_ALEN + WPA_NONCE_LEN, nonce2,
			  WPA_NONCE_LEN);
	} else {
		os_memcpy(data + 2 * ETH_ALEN, nonce2, WPA_NONCE_LEN);
		os_memcpy(data + 2 * ETH_ALEN + WPA_NONCE_LEN, nonce1,
			  WPA_NONCE_LEN);
	}

	if (z && z_len) {
		os_memcpy(data + 2 * ETH_ALEN + 2 * WPA_NONCE_LEN, z, z_len);
		data_len += z_len;
	}

	if (kdk_len > WPA_KDK_MAX_LEN) {
		wpa_printf(MSG_ERROR,
			   "WPA: KDK len=%zu exceeds max supported len",
			   kdk_len);
		return -1;
	}

	ptk->kck_len = wpa_kck_len(akmp, pmk_len);
	ptk->kek_len = wpa_kek_len(akmp, pmk_len);
	ptk->tk_len = wpa_cipher_key_len(cipher);
	ptk->kdk_len = kdk_len;
	if (ptk->tk_len == 0) {
		wpa_printf(MSG_ERROR,
			   "WPA: Unsupported cipher (0x%x) used in PTK derivation",
			   cipher);
		return -1;
	}
	ptk_len = ptk->kck_len + ptk->kek_len + ptk->tk_len + ptk->kdk_len;

	if (wpa_key_mgmt_sha384(akmp)) {
#ifdef CONFIG_SHA384
		wpa_printf(MSG_DEBUG, "WPA: PTK derivation using PRF(SHA384)");
		if (sha384_prf(pmk, pmk_len, label, data, data_len,
			       tmp, ptk_len) < 0)
			return -1;
#else /* CONFIG_SHA384 */
		return -1;
#endif /* CONFIG_SHA384 */
	} else if (wpa_key_mgmt_sha256(akmp)) {
		wpa_printf(MSG_DEBUG, "WPA: PTK derivation using PRF(SHA256)");
		if (sha256_prf(pmk, pmk_len, label, data, data_len,
			       tmp, ptk_len) < 0)
			return -1;
#ifdef CONFIG_OWE
	} else if (akmp == WPA_KEY_MGMT_OWE && (pmk_len == 32 ||
						owe_ptk_workaround)) {
		wpa_printf(MSG_DEBUG, "WPA: PTK derivation using PRF(SHA256)");
		if (sha256_prf(pmk, pmk_len, label, data, data_len,
			       tmp, ptk_len) < 0)
			return -1;
	} else if (akmp == WPA_KEY_MGMT_OWE && pmk_len == 48) {
		wpa_printf(MSG_DEBUG, "WPA: PTK derivation using PRF(SHA384)");
		if (sha384_prf(pmk, pmk_len, label, data, data_len,
			       tmp, ptk_len) < 0)
			return -1;
	} else if (akmp == WPA_KEY_MGMT_OWE && pmk_len == 64) {
		wpa_printf(MSG_DEBUG, "WPA: PTK derivation using PRF(SHA512)");
		if (sha512_prf(pmk, pmk_len, label, data, data_len,
			       tmp, ptk_len) < 0)
			return -1;
	} else if (akmp == WPA_KEY_MGMT_OWE) {
		wpa_printf(MSG_INFO, "OWE: Unknown PMK length %u",
			   (unsigned int) pmk_len);
		return -1;
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP
	} else if (akmp == WPA_KEY_MGMT_DPP && pmk_len == 32) {
		wpa_printf(MSG_DEBUG, "WPA: PTK derivation using PRF(SHA256)");
		if (sha256_prf(pmk, pmk_len, label, data, data_len,
			       tmp, ptk_len) < 0)
			return -1;
	} else if (akmp == WPA_KEY_MGMT_DPP && pmk_len == 48) {
		wpa_printf(MSG_DEBUG, "WPA: PTK derivation using PRF(SHA384)");
		if (sha384_prf(pmk, pmk_len, label, data, data_len,
			       tmp, ptk_len) < 0)
			return -1;
	} else if (akmp == WPA_KEY_MGMT_DPP && pmk_len == 64) {
		wpa_printf(MSG_DEBUG, "WPA: PTK derivation using PRF(SHA512)");
		if (sha512_prf(pmk, pmk_len, label, data, data_len,
			       tmp, ptk_len) < 0)
			return -1;
	} else if (akmp == WPA_KEY_MGMT_DPP) {
		wpa_printf(MSG_INFO, "DPP: Unknown PMK length %u",
			   (unsigned int) pmk_len);
		return -1;
#endif /* CONFIG_DPP */
#ifdef CONFIG_SAE
	} else if (wpa_key_mgmt_sae_ext_key(akmp)) {
		if (pmk_len == 32) {
			wpa_printf(MSG_DEBUG,
				   "SAE: PTK derivation using PRF(SHA256)");
			if (sha256_prf(pmk, pmk_len, label, data, data_len,
				       tmp, ptk_len) < 0)
				return -1;
#ifdef CONFIG_SHA384
		} else if (pmk_len == 48) {
			wpa_printf(MSG_DEBUG,
				   "SAE: PTK derivation using PRF(SHA384)");
			if (sha384_prf(pmk, pmk_len, label, data, data_len,
				       tmp, ptk_len) < 0)
				return -1;
#endif /* CONFIG_SHA384 */
#ifdef CONFIG_SHA512
		} else if (pmk_len == 64) {
			wpa_printf(MSG_DEBUG,
				   "SAE: PTK derivation using PRF(SHA512)");
			if (sha512_prf(pmk, pmk_len, label, data, data_len,
				       tmp, ptk_len) < 0)
				return -1;
#endif /* CONFIG_SHA512 */
		} else {
			wpa_printf(MSG_INFO, "SAE: Unknown PMK length %u",
				   (unsigned int) pmk_len);
			return -1;
		}
#endif /* CONFIG_SAE */
	} else {
		wpa_printf(MSG_DEBUG, "WPA: PTK derivation using PRF(SHA1)");
		if (sha1_prf(pmk, pmk_len, label, data, data_len, tmp,
			     ptk_len) < 0)
			return -1;
	}

	wpa_printf(MSG_DEBUG, "WPA: PTK derivation - A1=" MACSTR " A2=" MACSTR,
		   MAC2STR(addr1), MAC2STR(addr2));
	wpa_hexdump(MSG_DEBUG, "WPA: Nonce1", nonce1, WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "WPA: Nonce2", nonce2, WPA_NONCE_LEN);
	if (z && z_len)
		wpa_hexdump_key(MSG_DEBUG, "WPA: Z.x", z, z_len);
	wpa_hexdump_key(MSG_DEBUG, "WPA: PMK", pmk, pmk_len);
	wpa_hexdump_key(MSG_DEBUG, "WPA: PTK", tmp, ptk_len);

	os_memcpy(ptk->kck, tmp, ptk->kck_len);
	wpa_hexdump_key(MSG_DEBUG, "WPA: KCK", ptk->kck, ptk->kck_len);

	os_memcpy(ptk->kek, tmp + ptk->kck_len, ptk->kek_len);
	wpa_hexdump_key(MSG_DEBUG, "WPA: KEK", ptk->kek, ptk->kek_len);

	os_memcpy(ptk->tk, tmp + ptk->kck_len + ptk->kek_len, ptk->tk_len);
	wpa_hexdump_key(MSG_DEBUG, "WPA: TK", ptk->tk, ptk->tk_len);

	if (kdk_len) {
		os_memcpy(ptk->kdk, tmp + ptk->kck_len + ptk->kek_len +
			  ptk->tk_len, ptk->kdk_len);
		wpa_hexdump_key(MSG_DEBUG, "WPA: KDK", ptk->kdk, ptk->kdk_len);
	}

	ptk->kek2_len = 0;
	ptk->kck2_len = 0;

	os_memset(tmp, 0, sizeof(tmp));
	os_memset(data, 0, data_len);
	return 0;
}

#ifdef CONFIG_FILS

int fils_rmsk_to_pmk(int akmp, const u8 *rmsk, size_t rmsk_len,
		     const u8 *snonce, const u8 *anonce, const u8 *dh_ss,
		     size_t dh_ss_len, u8 *pmk, size_t *pmk_len)
{
	u8 nonces[2 * FILS_NONCE_LEN];
	const u8 *addr[2];
	size_t len[2];
	size_t num_elem;
	int res;

	/* PMK = HMAC-Hash(SNonce || ANonce, rMSK [ || DHss ]) */
	wpa_printf(MSG_DEBUG, "FILS: rMSK to PMK derivation");

	if (wpa_key_mgmt_sha384(akmp))
		*pmk_len = SHA384_MAC_LEN;
	else if (wpa_key_mgmt_sha256(akmp))
		*pmk_len = SHA256_MAC_LEN;
	else
		return -1;

	wpa_hexdump_key(MSG_DEBUG, "FILS: rMSK", rmsk, rmsk_len);
	wpa_hexdump(MSG_DEBUG, "FILS: SNonce", snonce, FILS_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "FILS: ANonce", anonce, FILS_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "FILS: DHss", dh_ss, dh_ss_len);

	os_memcpy(nonces, snonce, FILS_NONCE_LEN);
	os_memcpy(&nonces[FILS_NONCE_LEN], anonce, FILS_NONCE_LEN);
	addr[0] = rmsk;
	len[0] = rmsk_len;
	num_elem = 1;
	if (dh_ss) {
		addr[1] = dh_ss;
		len[1] = dh_ss_len;
		num_elem++;
	}
	if (wpa_key_mgmt_sha384(akmp))
		res = hmac_sha384_vector(nonces, 2 * FILS_NONCE_LEN, num_elem,
					 addr, len, pmk);
	else
		res = hmac_sha256_vector(nonces, 2 * FILS_NONCE_LEN, num_elem,
					 addr, len, pmk);
	if (res == 0)
		wpa_hexdump_key(MSG_DEBUG, "FILS: PMK", pmk, *pmk_len);
	else
		*pmk_len = 0;
	return res;
}


int fils_pmkid_erp(int akmp, const u8 *reauth, size_t reauth_len,
		   u8 *pmkid)
{
	const u8 *addr[1];
	size_t len[1];
	u8 hash[SHA384_MAC_LEN];
	int res;

	/* PMKID = Truncate-128(Hash(EAP-Initiate/Reauth)) */
	addr[0] = reauth;
	len[0] = reauth_len;
	if (wpa_key_mgmt_sha384(akmp))
		res = sha384_vector(1, addr, len, hash);
	else if (wpa_key_mgmt_sha256(akmp))
		res = sha256_vector(1, addr, len, hash);
	else
		return -1;
	if (res)
		return res;
	os_memcpy(pmkid, hash, PMKID_LEN);
	wpa_hexdump(MSG_DEBUG, "FILS: PMKID", pmkid, PMKID_LEN);
	return 0;
}


int fils_pmk_to_ptk(const u8 *pmk, size_t pmk_len, const u8 *spa, const u8 *aa,
		    const u8 *snonce, const u8 *anonce, const u8 *dhss,
		    size_t dhss_len, struct wpa_ptk *ptk,
		    u8 *ick, size_t *ick_len, int akmp, int cipher,
		    u8 *fils_ft, size_t *fils_ft_len, size_t kdk_len)
{
	u8 *data, *pos;
	size_t data_len;
	u8 tmp[FILS_ICK_MAX_LEN + WPA_KEK_MAX_LEN + WPA_TK_MAX_LEN +
	       FILS_FT_MAX_LEN + WPA_KDK_MAX_LEN];
	size_t key_data_len;
	const char *label = "FILS PTK Derivation";
	int ret = -1;
	size_t offset;

	/*
	 * FILS-Key-Data = PRF-X(PMK, "FILS PTK Derivation",
	 *                       SPA || AA || SNonce || ANonce [ || DHss ])
	 * ICK = L(FILS-Key-Data, 0, ICK_bits)
	 * KEK = L(FILS-Key-Data, ICK_bits, KEK_bits)
	 * TK = L(FILS-Key-Data, ICK_bits + KEK_bits, TK_bits)
	 * If doing FT initial mobility domain association:
	 * FILS-FT = L(FILS-Key-Data, ICK_bits + KEK_bits + TK_bits,
	 *             FILS-FT_bits)
	 * When a KDK is derived:
	 * KDK = L(FILS-Key-Data, ICK_bits + KEK_bits + TK_bits + FILS-FT_bits,
	 *	   KDK_bits)
	 */
	data_len = 2 * ETH_ALEN + 2 * FILS_NONCE_LEN + dhss_len;
	data = os_malloc(data_len);
	if (!data)
		goto err;
	pos = data;
	os_memcpy(pos, spa, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, aa, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, snonce, FILS_NONCE_LEN);
	pos += FILS_NONCE_LEN;
	os_memcpy(pos, anonce, FILS_NONCE_LEN);
	pos += FILS_NONCE_LEN;
	if (dhss)
		os_memcpy(pos, dhss, dhss_len);

	ptk->kck_len = 0;
	ptk->kek_len = wpa_kek_len(akmp, pmk_len);
	ptk->tk_len = wpa_cipher_key_len(cipher);
	if (wpa_key_mgmt_sha384(akmp))
		*ick_len = 48;
	else if (wpa_key_mgmt_sha256(akmp))
		*ick_len = 32;
	else
		goto err;
	key_data_len = *ick_len + ptk->kek_len + ptk->tk_len;

	if (kdk_len) {
		if (kdk_len > WPA_KDK_MAX_LEN) {
			wpa_printf(MSG_ERROR, "FILS: KDK len=%zu too big",
				   kdk_len);
			goto err;
		}

		ptk->kdk_len = kdk_len;
		key_data_len += kdk_len;
	} else {
		ptk->kdk_len = 0;
	}

	if (fils_ft && fils_ft_len) {
		if (akmp == WPA_KEY_MGMT_FT_FILS_SHA256) {
			*fils_ft_len = 32;
		} else if (akmp == WPA_KEY_MGMT_FT_FILS_SHA384) {
			*fils_ft_len = 48;
		} else {
			*fils_ft_len = 0;
			fils_ft = NULL;
		}
		key_data_len += *fils_ft_len;
	}

	if (wpa_key_mgmt_sha384(akmp)) {
		wpa_printf(MSG_DEBUG, "FILS: PTK derivation using PRF(SHA384)");
		if (sha384_prf(pmk, pmk_len, label, data, data_len,
			       tmp, key_data_len) < 0)
			goto err;
	} else {
		wpa_printf(MSG_DEBUG, "FILS: PTK derivation using PRF(SHA256)");
		if (sha256_prf(pmk, pmk_len, label, data, data_len,
			       tmp, key_data_len) < 0)
			goto err;
	}

	wpa_printf(MSG_DEBUG, "FILS: PTK derivation - SPA=" MACSTR
		   " AA=" MACSTR, MAC2STR(spa), MAC2STR(aa));
	wpa_hexdump(MSG_DEBUG, "FILS: SNonce", snonce, FILS_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "FILS: ANonce", anonce, FILS_NONCE_LEN);
	if (dhss)
		wpa_hexdump_key(MSG_DEBUG, "FILS: DHss", dhss, dhss_len);
	wpa_hexdump_key(MSG_DEBUG, "FILS: PMK", pmk, pmk_len);
	wpa_hexdump_key(MSG_DEBUG, "FILS: FILS-Key-Data", tmp, key_data_len);

	os_memcpy(ick, tmp, *ick_len);
	offset = *ick_len;
	wpa_hexdump_key(MSG_DEBUG, "FILS: ICK", ick, *ick_len);

	os_memcpy(ptk->kek, tmp + offset, ptk->kek_len);
	wpa_hexdump_key(MSG_DEBUG, "FILS: KEK", ptk->kek, ptk->kek_len);
	offset += ptk->kek_len;

	os_memcpy(ptk->tk, tmp + offset, ptk->tk_len);
	wpa_hexdump_key(MSG_DEBUG, "FILS: TK", ptk->tk, ptk->tk_len);
	offset += ptk->tk_len;

	if (fils_ft && fils_ft_len) {
		os_memcpy(fils_ft, tmp + offset, *fils_ft_len);
		wpa_hexdump_key(MSG_DEBUG, "FILS: FILS-FT",
				fils_ft, *fils_ft_len);
		offset += *fils_ft_len;
	}

	if (ptk->kdk_len) {
		os_memcpy(ptk->kdk, tmp + offset, ptk->kdk_len);
		wpa_hexdump_key(MSG_DEBUG, "FILS: KDK", ptk->kdk, ptk->kdk_len);
	}

	ptk->kek2_len = 0;
	ptk->kck2_len = 0;

	os_memset(tmp, 0, sizeof(tmp));
	ret = 0;
err:
	bin_clear_free(data, data_len);
	return ret;
}


int fils_key_auth_sk(const u8 *ick, size_t ick_len, const u8 *snonce,
		     const u8 *anonce, const u8 *sta_addr, const u8 *bssid,
		     const u8 *g_sta, size_t g_sta_len,
		     const u8 *g_ap, size_t g_ap_len,
		     int akmp, u8 *key_auth_sta, u8 *key_auth_ap,
		     size_t *key_auth_len)
{
	const u8 *addr[6];
	size_t len[6];
	size_t num_elem = 4;
	int res;

	wpa_printf(MSG_DEBUG, "FILS: Key-Auth derivation: STA-MAC=" MACSTR
		   " AP-BSSID=" MACSTR, MAC2STR(sta_addr), MAC2STR(bssid));
	wpa_hexdump_key(MSG_DEBUG, "FILS: ICK", ick, ick_len);
	wpa_hexdump(MSG_DEBUG, "FILS: SNonce", snonce, FILS_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "FILS: ANonce", anonce, FILS_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "FILS: gSTA", g_sta, g_sta_len);
	wpa_hexdump(MSG_DEBUG, "FILS: gAP", g_ap, g_ap_len);

	/*
	 * For (Re)Association Request frame (STA->AP):
	 * Key-Auth = HMAC-Hash(ICK, SNonce || ANonce || STA-MAC || AP-BSSID
	 *                      [ || gSTA || gAP ])
	 */
	addr[0] = snonce;
	len[0] = FILS_NONCE_LEN;
	addr[1] = anonce;
	len[1] = FILS_NONCE_LEN;
	addr[2] = sta_addr;
	len[2] = ETH_ALEN;
	addr[3] = bssid;
	len[3] = ETH_ALEN;
	if (g_sta && g_sta_len && g_ap && g_ap_len) {
		addr[4] = g_sta;
		len[4] = g_sta_len;
		addr[5] = g_ap;
		len[5] = g_ap_len;
		num_elem = 6;
	}

	if (wpa_key_mgmt_sha384(akmp)) {
		*key_auth_len = 48;
		res = hmac_sha384_vector(ick, ick_len, num_elem, addr, len,
					 key_auth_sta);
	} else if (wpa_key_mgmt_sha256(akmp)) {
		*key_auth_len = 32;
		res = hmac_sha256_vector(ick, ick_len, num_elem, addr, len,
					 key_auth_sta);
	} else {
		return -1;
	}
	if (res < 0)
		return res;

	/*
	 * For (Re)Association Response frame (AP->STA):
	 * Key-Auth = HMAC-Hash(ICK, ANonce || SNonce || AP-BSSID || STA-MAC
	 *                      [ || gAP || gSTA ])
	 */
	addr[0] = anonce;
	addr[1] = snonce;
	addr[2] = bssid;
	addr[3] = sta_addr;
	if (g_sta && g_sta_len && g_ap && g_ap_len) {
		addr[4] = g_ap;
		len[4] = g_ap_len;
		addr[5] = g_sta;
		len[5] = g_sta_len;
	}

	if (wpa_key_mgmt_sha384(akmp))
		res = hmac_sha384_vector(ick, ick_len, num_elem, addr, len,
					 key_auth_ap);
	else if (wpa_key_mgmt_sha256(akmp))
		res = hmac_sha256_vector(ick, ick_len, num_elem, addr, len,
					 key_auth_ap);
	if (res < 0)
		return res;

	wpa_hexdump(MSG_DEBUG, "FILS: Key-Auth (STA)",
		    key_auth_sta, *key_auth_len);
	wpa_hexdump(MSG_DEBUG, "FILS: Key-Auth (AP)",
		    key_auth_ap, *key_auth_len);

	return 0;
}

#endif /* CONFIG_FILS */


#ifdef CONFIG_IEEE80211R
int wpa_ft_mic(int key_mgmt, const u8 *kck, size_t kck_len, const u8 *sta_addr,
	       const u8 *ap_addr, u8 transaction_seqnum,
	       const u8 *mdie, size_t mdie_len,
	       const u8 *ftie, size_t ftie_len,
	       const u8 *rsnie, size_t rsnie_len,
	       const u8 *ric, size_t ric_len,
	       const u8 *rsnxe, size_t rsnxe_len,
	       const struct wpabuf *extra,
	       u8 *mic)
{
	const u8 *addr[11];
	size_t len[11];
	size_t i, num_elem = 0;
	u8 zero_mic[32];
	size_t mic_len, fte_fixed_len;
	int res;

	if (kck_len == 16) {
		mic_len = 16;
#ifdef CONFIG_SHA384
	} else if (kck_len == 24) {
		mic_len = 24;
#endif /* CONFIG_SHA384 */
#ifdef CONFIG_SHA512
	} else if (kck_len == 32) {
		mic_len = 32;
#endif /* CONFIG_SHA512 */
	} else {
		wpa_printf(MSG_WARNING, "FT: Unsupported KCK length %u",
			   (unsigned int) kck_len);
		return -1;
	}

	fte_fixed_len = sizeof(struct rsn_ftie) - 16 + mic_len;

	addr[num_elem] = sta_addr;
	len[num_elem] = ETH_ALEN;
	num_elem++;

	addr[num_elem] = ap_addr;
	len[num_elem] = ETH_ALEN;
	num_elem++;

	addr[num_elem] = &transaction_seqnum;
	len[num_elem] = 1;
	num_elem++;

	if (rsnie) {
		addr[num_elem] = rsnie;
		len[num_elem] = rsnie_len;
		num_elem++;
	}
	if (mdie) {
		addr[num_elem] = mdie;
		len[num_elem] = mdie_len;
		num_elem++;
	}
	if (ftie) {
		if (ftie_len < 2 + fte_fixed_len)
			return -1;

		/* IE hdr and mic_control */
		addr[num_elem] = ftie;
		len[num_elem] = 2 + 2;
		num_elem++;

		/* MIC field with all zeros */
		os_memset(zero_mic, 0, mic_len);
		addr[num_elem] = zero_mic;
		len[num_elem] = mic_len;
		num_elem++;

		/* Rest of FTIE */
		addr[num_elem] = ftie + 2 + 2 + mic_len;
		len[num_elem] = ftie_len - (2 + 2 + mic_len);
		num_elem++;
	}
	if (ric) {
		addr[num_elem] = ric;
		len[num_elem] = ric_len;
		num_elem++;
	}

	if (rsnxe) {
		addr[num_elem] = rsnxe;
		len[num_elem] = rsnxe_len;
		num_elem++;
	}

	if (extra) {
		addr[num_elem] = wpabuf_head(extra);
		len[num_elem] = wpabuf_len(extra);
		num_elem++;
	}

	for (i = 0; i < num_elem; i++)
		wpa_hexdump(MSG_MSGDUMP, "FT: MIC data", addr[i], len[i]);
	res = -1;
#ifdef CONFIG_SHA512
	if (kck_len == 32) {
		u8 hash[SHA512_MAC_LEN];

		if (hmac_sha512_vector(kck, kck_len, num_elem, addr, len, hash))
			return -1;
		os_memcpy(mic, hash, 32);
		res = 0;
	}
#endif /* CONFIG_SHA384 */
#ifdef CONFIG_SHA384
	if (kck_len == 24) {
		u8 hash[SHA384_MAC_LEN];

		if (hmac_sha384_vector(kck, kck_len, num_elem, addr, len, hash))
			return -1;
		os_memcpy(mic, hash, 24);
		res = 0;
	}
#endif /* CONFIG_SHA384 */
	if (kck_len == 16 && key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY) {
		u8 hash[SHA256_MAC_LEN];

		if (hmac_sha256_vector(kck, kck_len, num_elem, addr, len, hash))
			return -1;
		os_memcpy(mic, hash, 16);
		res = 0;
	}
	if (kck_len == 16 && key_mgmt != WPA_KEY_MGMT_FT_SAE_EXT_KEY &&
	    omac1_aes_128_vector(kck, num_elem, addr, len, mic) == 0)
		res = 0;

	return res;
}


static int wpa_ft_parse_ftie(const u8 *ie, size_t ie_len,
			     struct wpa_ft_ies *parse, const u8 *opt)
{
	const u8 *end, *pos;
	u8 link_id;

	pos = opt;
	end = ie + ie_len;
	wpa_hexdump(MSG_DEBUG, "FT: Parse FTE subelements", pos, end - pos);

	while (end - pos >= 2) {
		u8 id, len;

		id = *pos++;
		len = *pos++;
		if (len > end - pos) {
			wpa_printf(MSG_DEBUG, "FT: Truncated subelement");
			return -1;
		}

		switch (id) {
		case FTIE_SUBELEM_R1KH_ID:
			if (len != FT_R1KH_ID_LEN) {
				wpa_printf(MSG_DEBUG,
					   "FT: Invalid R1KH-ID length in FTIE: %d",
					   len);
				return -1;
			}
			parse->r1kh_id = pos;
			wpa_hexdump(MSG_DEBUG, "FT: R1KH-ID",
				    parse->r1kh_id, FT_R1KH_ID_LEN);
			break;
		case FTIE_SUBELEM_GTK:
			wpa_printf(MSG_DEBUG, "FT: GTK");
			parse->gtk = pos;
			parse->gtk_len = len;
			break;
		case FTIE_SUBELEM_R0KH_ID:
			if (len < 1 || len > FT_R0KH_ID_MAX_LEN) {
				wpa_printf(MSG_DEBUG,
					   "FT: Invalid R0KH-ID length in FTIE: %d",
					   len);
				return -1;
			}
			parse->r0kh_id = pos;
			parse->r0kh_id_len = len;
			wpa_hexdump(MSG_DEBUG, "FT: R0KH-ID",
				    parse->r0kh_id, parse->r0kh_id_len);
			break;
		case FTIE_SUBELEM_IGTK:
			wpa_printf(MSG_DEBUG, "FT: IGTK");
			parse->igtk = pos;
			parse->igtk_len = len;
			break;
#ifdef CONFIG_OCV
		case FTIE_SUBELEM_OCI:
			parse->oci = pos;
			parse->oci_len = len;
			wpa_hexdump(MSG_DEBUG, "FT: OCI",
				    parse->oci, parse->oci_len);
			break;
#endif /* CONFIG_OCV */
		case FTIE_SUBELEM_BIGTK:
			wpa_printf(MSG_DEBUG, "FT: BIGTK");
			parse->bigtk = pos;
			parse->bigtk_len = len;
			break;
		case FTIE_SUBELEM_MLO_GTK:
			if (len < 2 + 1 + 1 + 8) {
				wpa_printf(MSG_DEBUG,
					   "FT: Too short MLO GTK in FTE");
				return -1;
			}
			link_id = pos[2] & 0x0f;
			wpa_printf(MSG_DEBUG, "FT: MLO GTK (Link ID %u)",
				   link_id);
			if (link_id >= MAX_NUM_MLD_LINKS)
				break;
			parse->valid_mlo_gtks |= BIT(link_id);
			parse->mlo_gtk[link_id] = pos;
			parse->mlo_gtk_len[link_id] = len;
			break;
		case FTIE_SUBELEM_MLO_IGTK:
			if (len < 2 + 6 + 1 + 1) {
				wpa_printf(MSG_DEBUG,
					   "FT: Too short MLO IGTK in FTE");
				return -1;
			}
			link_id = pos[2 + 6] & 0x0f;
			wpa_printf(MSG_DEBUG, "FT: MLO IGTK (Link ID %u)",
				   link_id);
			if (link_id >= MAX_NUM_MLD_LINKS)
				break;
			parse->valid_mlo_igtks |= BIT(link_id);
			parse->mlo_igtk[link_id] = pos;
			parse->mlo_igtk_len[link_id] = len;
			break;
		case FTIE_SUBELEM_MLO_BIGTK:
			if (len < 2 + 6 + 1 + 1) {
				wpa_printf(MSG_DEBUG,
					   "FT: Too short MLO BIGTK in FTE");
				return -1;
			}
			link_id = pos[2 + 6] & 0x0f;
			wpa_printf(MSG_DEBUG, "FT: MLO BIGTK (Link ID %u)",
				   link_id);
			if (link_id >= MAX_NUM_MLD_LINKS)
				break;
			parse->valid_mlo_bigtks |= BIT(link_id);
			parse->mlo_bigtk[link_id] = pos;
			parse->mlo_bigtk_len[link_id] = len;
			break;
		default:
			wpa_printf(MSG_DEBUG, "FT: Unknown subelem id %u", id);
			break;
		}

		pos += len;
	}

	return 0;
}


static int wpa_ft_parse_fte(int key_mgmt, const u8 *ie, size_t len,
			    struct wpa_ft_ies *parse)
{
	size_t mic_len;
	u8 mic_len_info;
	const u8 *pos = ie;
	const u8 *end = pos + len;

	wpa_hexdump(MSG_DEBUG, "FT: FTE-MIC Control", pos, 2);
	parse->fte_rsnxe_used = pos[0] & FTE_MIC_CTRL_RSNXE_USED;
	mic_len_info = (pos[0] & FTE_MIC_CTRL_MIC_LEN_MASK) >>
		FTE_MIC_CTRL_MIC_LEN_SHIFT;
	parse->fte_elem_count = pos[1];
	pos += 2;

	if (key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY) {
		switch (mic_len_info) {
		case FTE_MIC_LEN_16:
			mic_len = 16;
			break;
		case FTE_MIC_LEN_24:
			mic_len = 24;
			break;
		case FTE_MIC_LEN_32:
			mic_len = 32;
			break;
		default:
			wpa_printf(MSG_DEBUG,
				   "FT: Unknown MIC Length subfield value %u",
				   mic_len_info);
			return -1;
		}
	} else {
		mic_len = wpa_key_mgmt_sha384(key_mgmt) ? 24 : 16;
	}
	if (mic_len > (size_t) (end - pos)) {
		wpa_printf(MSG_DEBUG, "FT: No room for %zu octet MIC in FTE",
			   mic_len);
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "FT: FTE-MIC", pos, mic_len);
	parse->fte_mic = pos;
	parse->fte_mic_len = mic_len;
	pos += mic_len;

	if (2 * WPA_NONCE_LEN > end - pos)
		return -1;
	parse->fte_anonce = pos;
	wpa_hexdump(MSG_DEBUG, "FT: FTE-ANonce",
		    parse->fte_anonce, WPA_NONCE_LEN);
	pos += WPA_NONCE_LEN;
	parse->fte_snonce = pos;
	wpa_hexdump(MSG_DEBUG, "FT: FTE-SNonce",
		    parse->fte_snonce, WPA_NONCE_LEN);
	pos += WPA_NONCE_LEN;

	return wpa_ft_parse_ftie(ie, len, parse, pos);
}


int wpa_ft_parse_ies(const u8 *ies, size_t ies_len, struct wpa_ft_ies *parse,
		     int key_mgmt, bool reassoc_resp)
{
	const u8 *end, *pos;
	struct wpa_ie_data data;
	int ret;
	int prot_ie_count = 0;
	const u8 *fte = NULL;
	size_t fte_len = 0;
	bool is_fte = false;
	struct ieee802_11_elems elems;

	os_memset(parse, 0, sizeof(*parse));
	if (ies == NULL)
		return 0;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "FT: Failed to parse elements");
		goto fail;
	}

	pos = ies;
	end = ies + ies_len;
	while (end - pos >= 2) {
		u8 id, len;

		id = *pos++;
		len = *pos++;
		if (len > end - pos)
			break;

		if (id != WLAN_EID_FAST_BSS_TRANSITION &&
		    id != WLAN_EID_FRAGMENT)
			is_fte = false;

		switch (id) {
		case WLAN_EID_RSN:
			wpa_hexdump(MSG_DEBUG, "FT: RSNE", pos, len);
			parse->rsn = pos;
			parse->rsn_len = len;
			ret = wpa_parse_wpa_ie_rsn(parse->rsn - 2,
						   parse->rsn_len + 2,
						   &data);
			if (ret < 0) {
				wpa_printf(MSG_DEBUG, "FT: Failed to parse "
					   "RSN IE: %d", ret);
				goto fail;
			}
			parse->rsn_capab = data.capabilities;
			if (data.num_pmkid == 1 && data.pmkid)
				parse->rsn_pmkid = data.pmkid;
			parse->key_mgmt = data.key_mgmt;
			parse->pairwise_cipher = data.pairwise_cipher;
			if (!key_mgmt)
				key_mgmt = parse->key_mgmt;
			break;
		case WLAN_EID_RSNX:
			wpa_hexdump(MSG_DEBUG, "FT: RSNXE", pos, len);
			if (len < 1)
				break;
			parse->rsnxe = pos;
			parse->rsnxe_len = len;
			break;
		case WLAN_EID_MOBILITY_DOMAIN:
			wpa_hexdump(MSG_DEBUG, "FT: MDE", pos, len);
			if (len < sizeof(struct rsn_mdie))
				goto fail;
			parse->mdie = pos;
			parse->mdie_len = len;
			break;
		case WLAN_EID_FAST_BSS_TRANSITION:
			wpa_hexdump(MSG_DEBUG, "FT: FTE", pos, len);
			/* The first two octets (MIC Control field) is in the
			 * same offset for all cases, but the second field (MIC)
			 * has variable length with three different values.
			 * In particular the FT-SAE-EXT-KEY is inconvinient to
			 * parse, so try to handle this in pieces instead of
			 * using the struct rsn_ftie* definitions. */

			if (len < 2)
				goto fail;
			prot_ie_count = pos[1]; /* Element Count field in
						 * MIC Control */
			is_fte = true;
			fte = pos;
			fte_len = len;
			break;
		case WLAN_EID_FRAGMENT:
			if (is_fte) {
				wpa_hexdump(MSG_DEBUG, "FT: FTE fragment",
					    pos, len);
				fte_len += 2 + len;
			}
			break;
		case WLAN_EID_TIMEOUT_INTERVAL:
			wpa_hexdump(MSG_DEBUG, "FT: Timeout Interval",
				    pos, len);
			if (len != 5)
				break;
			parse->tie = pos;
			parse->tie_len = len;
			break;
		case WLAN_EID_RIC_DATA:
			if (parse->ric == NULL)
				parse->ric = pos - 2;
			break;
		}

		pos += len;
	}

	if (fte) {
		int res;

		if (fte_len < 255) {
			res = wpa_ft_parse_fte(key_mgmt, fte, fte_len, parse);
		} else {
			parse->fte_buf = ieee802_11_defrag(fte, fte_len, false);
			if (!parse->fte_buf)
				goto fail;
			res = wpa_ft_parse_fte(key_mgmt,
					       wpabuf_head(parse->fte_buf),
					       wpabuf_len(parse->fte_buf),
					       parse);
		}
		if (res < 0)
			goto fail;

		/* FTE might be fragmented. If it is, the separate Fragment
		 * elements are included in MIC calculation as full elements. */
		parse->ftie = fte;
		parse->ftie_len = fte_len;
	}

	if (prot_ie_count == 0)
		return 0; /* no MIC */

	/*
	 * Check that the protected IE count matches with IEs included in the
	 * frame.
	 */
	if (reassoc_resp && elems.basic_mle) {
		unsigned int link_id;

		/* TODO: This count should be done based on all _requested_,
		 * not _accepted_ links. */
		for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
			if (parse->mlo_gtk[link_id]) {
				if (parse->rsn)
					prot_ie_count--;
				if (parse->rsnxe)
					prot_ie_count--;
			}
		}
	} else {
		if (parse->rsn)
			prot_ie_count--;
		if (parse->rsnxe)
			prot_ie_count--;
	}
	if (parse->mdie)
		prot_ie_count--;
	if (parse->ftie)
		prot_ie_count--;
	if (prot_ie_count < 0) {
		wpa_printf(MSG_DEBUG, "FT: Some required IEs not included in "
			   "the protected IE count");
		goto fail;
	}

	if (prot_ie_count == 0 && parse->ric) {
		wpa_printf(MSG_DEBUG, "FT: RIC IE(s) in the frame, but not "
			   "included in protected IE count");
		goto fail;
	}

	/* Determine the end of the RIC IE(s) */
	if (parse->ric) {
		pos = parse->ric;
		while (end - pos >= 2 && 2 + pos[1] <= end - pos &&
		       prot_ie_count) {
			prot_ie_count--;
			pos += 2 + pos[1];
		}
		parse->ric_len = pos - parse->ric;
	}
	if (prot_ie_count) {
		wpa_printf(MSG_DEBUG, "FT: %d protected IEs missing from "
			   "frame", (int) prot_ie_count);
		goto fail;
	}

	return 0;

fail:
	wpa_ft_parse_ies_free(parse);
	return -1;
}


void wpa_ft_parse_ies_free(struct wpa_ft_ies *parse)
{
	if (!parse)
		return;
	wpabuf_free(parse->fte_buf);
	parse->fte_buf = NULL;
}

#endif /* CONFIG_IEEE80211R */


#ifdef CONFIG_PASN

/*
 * pasn_use_sha384 - Should SHA384 be used or SHA256
 *
 * @akmp: Authentication and key management protocol
 * @cipher: The cipher suite
 *
 * According to IEEE P802.11az/D2.7, 12.12.7, the hash algorithm to use is the
 * hash algorithm defined for the Base AKM (see Table 9-151 (AKM suite
 * selectors)). When there is no Base AKM, the hash algorithm is selected based
 * on the pairwise cipher suite provided in the RSNE by the AP in the second
 * PASN frame. SHA-256 is used as the hash algorithm, except for the ciphers
 * 00-0F-AC:9 and 00-0F-AC:10 for which SHA-384 is used.
 */
bool pasn_use_sha384(int akmp, int cipher)
{
	return (akmp == WPA_KEY_MGMT_PASN && (cipher == WPA_CIPHER_CCMP_256 ||
					      cipher == WPA_CIPHER_GCMP_256)) ||
		wpa_key_mgmt_sha384(akmp);
}


/**
 * pasn_pmk_to_ptk - Calculate PASN PTK from PMK, addresses, etc.
 * @pmk: Pairwise master key
 * @pmk_len: Length of PMK
 * @spa: Suppplicant address
 * @bssid: AP BSSID
 * @dhss: Is the shared secret (DHss) derived from the PASN ephemeral key
 *	exchange encoded as an octet string
 * @dhss_len: The length of dhss in octets
 * @ptk: Buffer for pairwise transient key
 * @akmp: Negotiated AKM
 * @cipher: Negotiated pairwise cipher
 * @kdk_len: the length in octets that should be derived for HTLK. Can be zero.
 * Returns: 0 on success, -1 on failure
 */
int pasn_pmk_to_ptk(const u8 *pmk, size_t pmk_len,
		    const u8 *spa, const u8 *bssid,
		    const u8 *dhss, size_t dhss_len,
		    struct wpa_ptk *ptk, int akmp, int cipher,
		    size_t kdk_len)
{
	u8 tmp[WPA_KCK_MAX_LEN + WPA_TK_MAX_LEN + WPA_KDK_MAX_LEN];
	u8 *data;
	size_t data_len, ptk_len;
	int ret = -1;
	const char *label = "PASN PTK Derivation";

	if (!pmk || !pmk_len) {
		wpa_printf(MSG_ERROR, "PASN: No PMK set for PTK derivation");
		return -1;
	}

	if (!dhss || !dhss_len) {
		wpa_printf(MSG_ERROR, "PASN: No DHss set for PTK derivation");
		return -1;
	}

	/*
	 * PASN-PTK = KDF(PMK, “PASN PTK Derivation”, SPA || BSSID || DHss)
	 *
	 * KCK = L(PASN-PTK, 0, 256)
	 * TK = L(PASN-PTK, 256, TK_bits)
	 * KDK = L(PASN-PTK, 256 + TK_bits, kdk_len * 8)
	 */
	data_len = 2 * ETH_ALEN + dhss_len;
	data = os_zalloc(data_len);
	if (!data)
		return -1;

	os_memcpy(data, spa, ETH_ALEN);
	os_memcpy(data + ETH_ALEN, bssid, ETH_ALEN);
	os_memcpy(data + 2 * ETH_ALEN, dhss, dhss_len);

	ptk->kck_len = WPA_PASN_KCK_LEN;
	ptk->tk_len = wpa_cipher_key_len(cipher);
	ptk->kdk_len = kdk_len;
	ptk->kek_len = 0;
	ptk->kek2_len = 0;
	ptk->kck2_len = 0;

	if (ptk->tk_len == 0) {
		wpa_printf(MSG_ERROR,
			   "PASN: Unsupported cipher (0x%x) used in PTK derivation",
			   cipher);
		goto err;
	}

	ptk_len = ptk->kck_len + ptk->tk_len + ptk->kdk_len;
	if (ptk_len > sizeof(tmp))
		goto err;

	if (pasn_use_sha384(akmp, cipher)) {
		wpa_printf(MSG_DEBUG, "PASN: PTK derivation using SHA384");

		if (sha384_prf(pmk, pmk_len, label, data, data_len, tmp,
			       ptk_len) < 0)
			goto err;
	} else {
		wpa_printf(MSG_DEBUG, "PASN: PTK derivation using SHA256");

		if (sha256_prf(pmk, pmk_len, label, data, data_len, tmp,
			       ptk_len) < 0)
			goto err;
	}

	wpa_printf(MSG_DEBUG,
		   "PASN: PTK derivation: SPA=" MACSTR " BSSID=" MACSTR,
		   MAC2STR(spa), MAC2STR(bssid));

	wpa_hexdump_key(MSG_DEBUG, "PASN: DHss", dhss, dhss_len);
	wpa_hexdump_key(MSG_DEBUG, "PASN: PMK", pmk, pmk_len);
	wpa_hexdump_key(MSG_DEBUG, "PASN: PASN-PTK", tmp, ptk_len);

	os_memcpy(ptk->kck, tmp, WPA_PASN_KCK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "PASN: KCK:", ptk->kck, WPA_PASN_KCK_LEN);

	os_memcpy(ptk->tk, tmp + WPA_PASN_KCK_LEN, ptk->tk_len);
	wpa_hexdump_key(MSG_DEBUG, "PASN: TK:", ptk->tk, ptk->tk_len);

	if (kdk_len) {
		os_memcpy(ptk->kdk, tmp + WPA_PASN_KCK_LEN + ptk->tk_len,
			  ptk->kdk_len);
		wpa_hexdump_key(MSG_DEBUG, "PASN: KDK:",
				ptk->kdk, ptk->kdk_len);
	}

	forced_memzero(tmp, sizeof(tmp));
	ret = 0;
err:
	bin_clear_free(data, data_len);
	return ret;
}


/*
 * pasn_mic_len - Returns the MIC length for PASN authentication
 */
u8 pasn_mic_len(int akmp, int cipher)
{
	if (pasn_use_sha384(akmp, cipher))
		return 24;

	return 16;
}


/**
 * wpa_ltf_keyseed - Compute LTF keyseed from KDK
 * @ptk: Buffer that holds pairwise transient key
 * @akmp: Negotiated AKM
 * @cipher: Negotiated pairwise cipher
 * Returns: 0 on success, -1 on failure
 */
int wpa_ltf_keyseed(struct wpa_ptk *ptk, int akmp, int cipher)
{
	u8 *buf;
	size_t buf_len;
	u8 hash[SHA384_MAC_LEN];
	const u8 *kdk = ptk->kdk;
	size_t kdk_len = ptk->kdk_len;
	const char *label = "Secure LTF key seed";

	if (!kdk || !kdk_len) {
		wpa_printf(MSG_ERROR, "WPA: No KDK for LTF keyseed generation");
		return -1;
	}

	buf = (u8 *)label;
	buf_len = os_strlen(label);

	if (pasn_use_sha384(akmp, cipher)) {
		wpa_printf(MSG_DEBUG,
			   "WPA: Secure LTF keyseed using HMAC-SHA384");

		if (hmac_sha384(kdk, kdk_len, buf, buf_len, hash)) {
			wpa_printf(MSG_ERROR,
				   "WPA: HMAC-SHA384 compute failed");
			return -1;
		}
		os_memcpy(ptk->ltf_keyseed, hash, SHA384_MAC_LEN);
		ptk->ltf_keyseed_len = SHA384_MAC_LEN;
		wpa_hexdump_key(MSG_DEBUG, "WPA: Secure LTF keyseed: ",
				ptk->ltf_keyseed, ptk->ltf_keyseed_len);

	} else {
		wpa_printf(MSG_DEBUG, "WPA: LTF keyseed using HMAC-SHA256");

		if (hmac_sha256(kdk, kdk_len, buf, buf_len, hash)) {
			wpa_printf(MSG_ERROR,
				   "WPA: HMAC-SHA256 compute failed");
			return -1;
		}
		os_memcpy(ptk->ltf_keyseed, hash, SHA256_MAC_LEN);
		ptk->ltf_keyseed_len = SHA256_MAC_LEN;
		wpa_hexdump_key(MSG_DEBUG, "WPA: Secure LTF keyseed: ",
				ptk->ltf_keyseed, ptk->ltf_keyseed_len);
	}

	return 0;
}


/**
 * pasn_mic - Calculate PASN MIC
 * @kck: The key confirmation key for the PASN PTKSA
 * @akmp: Negotiated AKM
 * @cipher: Negotiated pairwise cipher
 * @addr1: For the 2nd PASN frame supplicant address; for the 3rd frame the
 *	BSSID
 * @addr2: For the 2nd PASN frame the BSSID; for the 3rd frame the supplicant
 *	address
 * @data: For calculating the MIC for the 2nd PASN frame, this should hold the
 *	Beacon frame RSNE + RSNXE. For calculating the MIC for the 3rd PASN
 *	frame, this should hold the hash of the body of the PASN 1st frame.
 * @data_len: The length of data
 * @frame: The body of the PASN frame including the MIC element with the octets
 *	in the MIC field of the MIC element set to 0.
 * @frame_len: The length of frame
 * @mic: Buffer to hold the MIC on success. Should be big enough to handle the
 *	maximal MIC length
 * Returns: 0 on success, -1 on failure
 */
int pasn_mic(const u8 *kck, int akmp, int cipher,
	     const u8 *addr1, const u8 *addr2,
	     const u8 *data, size_t data_len,
	     const u8 *frame, size_t frame_len, u8 *mic)
{
	u8 *buf;
	u8 hash[SHA384_MAC_LEN];
	size_t buf_len = 2 * ETH_ALEN + data_len + frame_len;
	int ret = -1;

	if (!kck) {
		wpa_printf(MSG_ERROR, "PASN: No KCK for MIC calculation");
		return -1;
	}

	if (!data || !data_len) {
		wpa_printf(MSG_ERROR, "PASN: invalid data for MIC calculation");
		return -1;
	}

	if (!frame || !frame_len) {
		wpa_printf(MSG_ERROR, "PASN: invalid data for MIC calculation");
		return -1;
	}

	buf = os_zalloc(buf_len);
	if (!buf)
		return -1;

	os_memcpy(buf, addr1, ETH_ALEN);
	os_memcpy(buf + ETH_ALEN, addr2, ETH_ALEN);

	wpa_hexdump_key(MSG_DEBUG, "PASN: MIC: data", data, data_len);
	os_memcpy(buf + 2 * ETH_ALEN, data, data_len);

	wpa_hexdump_key(MSG_DEBUG, "PASN: MIC: frame", frame, frame_len);
	os_memcpy(buf + 2 * ETH_ALEN + data_len, frame, frame_len);

	wpa_hexdump_key(MSG_DEBUG, "PASN: MIC: KCK", kck, WPA_PASN_KCK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "PASN: MIC: buf", buf, buf_len);

	if (pasn_use_sha384(akmp, cipher)) {
		wpa_printf(MSG_DEBUG, "PASN: MIC using HMAC-SHA384");

		if (hmac_sha384(kck, WPA_PASN_KCK_LEN, buf, buf_len, hash))
			goto err;

		os_memcpy(mic, hash, 24);
		wpa_hexdump_key(MSG_DEBUG, "PASN: MIC: mic: ", mic, 24);
	} else {
		wpa_printf(MSG_DEBUG, "PASN: MIC using HMAC-SHA256");

		if (hmac_sha256(kck, WPA_PASN_KCK_LEN, buf, buf_len, hash))
			goto err;

		os_memcpy(mic, hash, 16);
		wpa_hexdump_key(MSG_DEBUG, "PASN: MIC: mic: ", mic, 16);
	}

	ret = 0;
err:
	bin_clear_free(buf, buf_len);
	return ret;
}


/**
 * pasn_auth_frame_hash - Computes a hash of an Authentication frame body
 * @akmp: Negotiated AKM
 * @cipher: Negotiated pairwise cipher
 * @data: Pointer to the Authentication frame body
 * @len: Length of the Authentication frame body
 * @hash: On return would hold the computed hash. Should be big enough to handle
 *	SHA384.
 * Returns: 0 on success, -1 on failure
 */
int pasn_auth_frame_hash(int akmp, int cipher, const u8 *data, size_t len,
			 u8 *hash)
{
	if (pasn_use_sha384(akmp, cipher)) {
		wpa_printf(MSG_DEBUG, "PASN: Frame hash using SHA-384");
		return sha384_vector(1, &data, &len, hash);
	} else {
		wpa_printf(MSG_DEBUG, "PASN: Frame hash using SHA-256");
		return sha256_vector(1, &data, &len, hash);
	}
}

#endif /* CONFIG_PASN */


static int rsn_selector_to_bitfield(const u8 *s)
{
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_NONE)
		return WPA_CIPHER_NONE;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_TKIP)
		return WPA_CIPHER_TKIP;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_CCMP)
		return WPA_CIPHER_CCMP;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_AES_128_CMAC)
		return WPA_CIPHER_AES_128_CMAC;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_GCMP)
		return WPA_CIPHER_GCMP;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_CCMP_256)
		return WPA_CIPHER_CCMP_256;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_GCMP_256)
		return WPA_CIPHER_GCMP_256;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_BIP_GMAC_128)
		return WPA_CIPHER_BIP_GMAC_128;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_BIP_GMAC_256)
		return WPA_CIPHER_BIP_GMAC_256;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_BIP_CMAC_256)
		return WPA_CIPHER_BIP_CMAC_256;
	if (RSN_SELECTOR_GET(s) == RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED)
		return WPA_CIPHER_GTK_NOT_USED;
	return 0;
}


static int rsn_key_mgmt_to_bitfield(const u8 *s)
{
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_UNSPEC_802_1X)
		return WPA_KEY_MGMT_IEEE8021X;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X)
		return WPA_KEY_MGMT_PSK;
#ifdef CONFIG_IEEE80211R
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_FT_802_1X)
		return WPA_KEY_MGMT_FT_IEEE8021X;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_FT_PSK)
		return WPA_KEY_MGMT_FT_PSK;
#ifdef CONFIG_SHA384
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384)
		return WPA_KEY_MGMT_FT_IEEE8021X_SHA384;
#endif /* CONFIG_SHA384 */
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_SHA384
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_802_1X_SHA384)
		return WPA_KEY_MGMT_IEEE8021X_SHA384;
#endif /* CONFIG_SHA384 */
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_802_1X_SHA256)
		return WPA_KEY_MGMT_IEEE8021X_SHA256;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_PSK_SHA256)
		return WPA_KEY_MGMT_PSK_SHA256;
#ifdef CONFIG_SAE
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_SAE)
		return WPA_KEY_MGMT_SAE;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_SAE_EXT_KEY)
		return WPA_KEY_MGMT_SAE_EXT_KEY;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_FT_SAE)
		return WPA_KEY_MGMT_FT_SAE;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_FT_SAE_EXT_KEY)
		return WPA_KEY_MGMT_FT_SAE_EXT_KEY;
#endif /* CONFIG_SAE */
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_802_1X_SUITE_B)
		return WPA_KEY_MGMT_IEEE8021X_SUITE_B;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192)
		return WPA_KEY_MGMT_IEEE8021X_SUITE_B_192;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_FILS_SHA256)
		return WPA_KEY_MGMT_FILS_SHA256;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_FILS_SHA384)
		return WPA_KEY_MGMT_FILS_SHA384;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_FT_FILS_SHA256)
		return WPA_KEY_MGMT_FT_FILS_SHA256;
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_FT_FILS_SHA384)
		return WPA_KEY_MGMT_FT_FILS_SHA384;
#ifdef CONFIG_OWE
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_OWE)
		return WPA_KEY_MGMT_OWE;
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_DPP)
		return WPA_KEY_MGMT_DPP;
#endif /* CONFIG_DPP */
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_OSEN)
		return WPA_KEY_MGMT_OSEN;
#ifdef CONFIG_PASN
	if (RSN_SELECTOR_GET(s) == RSN_AUTH_KEY_MGMT_PASN)
		return WPA_KEY_MGMT_PASN;
#endif /* CONFIG_PASN */
	return 0;
}


int wpa_cipher_valid_group(int cipher)
{
	return wpa_cipher_valid_pairwise(cipher) ||
		cipher == WPA_CIPHER_GTK_NOT_USED;
}


int wpa_cipher_valid_mgmt_group(int cipher)
{
	return cipher == WPA_CIPHER_GTK_NOT_USED ||
		cipher == WPA_CIPHER_AES_128_CMAC ||
		cipher == WPA_CIPHER_BIP_GMAC_128 ||
		cipher == WPA_CIPHER_BIP_GMAC_256 ||
		cipher == WPA_CIPHER_BIP_CMAC_256;
}


/**
 * wpa_parse_wpa_ie_rsn - Parse RSN IE
 * @rsn_ie: Buffer containing RSN IE
 * @rsn_ie_len: RSN IE buffer length (including IE number and length octets)
 * @data: Pointer to structure that will be filled in with parsed data
 * Returns: 0 on success, <0 on failure
 */
int wpa_parse_wpa_ie_rsn(const u8 *rsn_ie, size_t rsn_ie_len,
			 struct wpa_ie_data *data)
{
	const u8 *pos;
	int left;
	int i, count;

	os_memset(data, 0, sizeof(*data));
	data->proto = WPA_PROTO_RSN;
	data->pairwise_cipher = WPA_CIPHER_CCMP;
	data->group_cipher = WPA_CIPHER_CCMP;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;
	data->mgmt_group_cipher = WPA_CIPHER_AES_128_CMAC;

	if (rsn_ie_len == 0) {
		/* No RSN IE - fail silently */
		return -1;
	}

	if (rsn_ie_len < sizeof(struct rsn_ie_hdr)) {
		wpa_printf(MSG_DEBUG, "%s: ie len too short %lu",
			   __func__, (unsigned long) rsn_ie_len);
		return -1;
	}

	if (rsn_ie_len >= 6 && rsn_ie[1] >= 4 &&
	    rsn_ie[1] == rsn_ie_len - 2 &&
	    WPA_GET_BE32(&rsn_ie[2]) == OSEN_IE_VENDOR_TYPE) {
		pos = rsn_ie + 6;
		left = rsn_ie_len - 6;

		data->group_cipher = WPA_CIPHER_GTK_NOT_USED;
		data->has_group = 1;
		data->key_mgmt = WPA_KEY_MGMT_OSEN;
		data->proto = WPA_PROTO_OSEN;
	} else {
		const struct rsn_ie_hdr *hdr;

		hdr = (const struct rsn_ie_hdr *) rsn_ie;

		if (hdr->elem_id != WLAN_EID_RSN ||
		    hdr->len != rsn_ie_len - 2 ||
		    WPA_GET_LE16(hdr->version) != RSN_VERSION) {
			wpa_printf(MSG_DEBUG, "%s: malformed ie or unknown version",
				   __func__);
			return -2;
		}

		pos = (const u8 *) (hdr + 1);
		left = rsn_ie_len - sizeof(*hdr);
	}

	if (left >= RSN_SELECTOR_LEN) {
		data->group_cipher = rsn_selector_to_bitfield(pos);
		data->has_group = 1;
		if (!wpa_cipher_valid_group(data->group_cipher)) {
			wpa_printf(MSG_DEBUG,
				   "%s: invalid group cipher 0x%x (%08x)",
				   __func__, data->group_cipher,
				   WPA_GET_BE32(pos));
#ifdef CONFIG_NO_TKIP
			if (RSN_SELECTOR_GET(pos) == RSN_CIPHER_SUITE_TKIP) {
				wpa_printf(MSG_DEBUG,
					   "%s: TKIP as group cipher not supported in CONFIG_NO_TKIP=y build",
					   __func__);
			}
#endif /* CONFIG_NO_TKIP */
			return -1;
		}
		pos += RSN_SELECTOR_LEN;
		left -= RSN_SELECTOR_LEN;
	} else if (left > 0) {
		wpa_printf(MSG_DEBUG, "%s: ie length mismatch, %u too much",
			   __func__, left);
		return -3;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || count > left / RSN_SELECTOR_LEN) {
			wpa_printf(MSG_DEBUG, "%s: ie count botch (pairwise), "
				   "count %u left %u", __func__, count, left);
			return -4;
		}
		if (count)
			data->has_pairwise = 1;
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= rsn_selector_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
		if (data->pairwise_cipher & WPA_CIPHER_AES_128_CMAC) {
			wpa_printf(MSG_DEBUG, "%s: AES-128-CMAC used as "
				   "pairwise cipher", __func__);
			return -1;
		}
	} else if (left == 1) {
		wpa_printf(MSG_DEBUG, "%s: ie too short (for key mgmt)",
			   __func__);
		return -5;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || count > left / RSN_SELECTOR_LEN) {
			wpa_printf(MSG_DEBUG, "%s: ie count botch (key mgmt), "
				   "count %u left %u", __func__, count, left);
			return -6;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= rsn_key_mgmt_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
		wpa_printf(MSG_DEBUG, "%s: ie too short (for capabilities)",
			   __func__);
		return -7;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left >= 2) {
		u16 num_pmkid = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (num_pmkid > (unsigned int) left / PMKID_LEN) {
			wpa_printf(MSG_DEBUG, "%s: PMKID underflow "
				   "(num_pmkid=%u left=%d)",
				   __func__, num_pmkid, left);
			data->num_pmkid = 0;
			return -9;
		} else {
			data->num_pmkid = num_pmkid;
			data->pmkid = pos;
			pos += data->num_pmkid * PMKID_LEN;
			left -= data->num_pmkid * PMKID_LEN;
		}
	}

	if (left >= 4) {
		data->mgmt_group_cipher = rsn_selector_to_bitfield(pos);
		if (!wpa_cipher_valid_mgmt_group(data->mgmt_group_cipher)) {
			wpa_printf(MSG_DEBUG,
				   "%s: Unsupported management group cipher 0x%x (%08x)",
				   __func__, data->mgmt_group_cipher,
				   WPA_GET_BE32(pos));
			return -10;
		}
		pos += RSN_SELECTOR_LEN;
		left -= RSN_SELECTOR_LEN;
	}

	if (left > 0) {
		wpa_hexdump(MSG_DEBUG,
			    "wpa_parse_wpa_ie_rsn: ignore trailing bytes",
			    pos, left);
	}

	return 0;
}


static int wpa_selector_to_bitfield(const u8 *s)
{
	if (RSN_SELECTOR_GET(s) == WPA_CIPHER_SUITE_NONE)
		return WPA_CIPHER_NONE;
	if (RSN_SELECTOR_GET(s) == WPA_CIPHER_SUITE_TKIP)
		return WPA_CIPHER_TKIP;
	if (RSN_SELECTOR_GET(s) == WPA_CIPHER_SUITE_CCMP)
		return WPA_CIPHER_CCMP;
	return 0;
}


static int wpa_key_mgmt_to_bitfield(const u8 *s)
{
	if (RSN_SELECTOR_GET(s) == WPA_AUTH_KEY_MGMT_UNSPEC_802_1X)
		return WPA_KEY_MGMT_IEEE8021X;
	if (RSN_SELECTOR_GET(s) == WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X)
		return WPA_KEY_MGMT_PSK;
	if (RSN_SELECTOR_GET(s) == WPA_AUTH_KEY_MGMT_NONE)
		return WPA_KEY_MGMT_WPA_NONE;
	return 0;
}


int wpa_parse_wpa_ie_wpa(const u8 *wpa_ie, size_t wpa_ie_len,
			 struct wpa_ie_data *data)
{
	const struct wpa_ie_hdr *hdr;
	const u8 *pos;
	int left;
	int i, count;

	os_memset(data, 0, sizeof(*data));
	data->proto = WPA_PROTO_WPA;
	data->pairwise_cipher = WPA_CIPHER_TKIP;
	data->group_cipher = WPA_CIPHER_TKIP;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;
	data->mgmt_group_cipher = 0;

	if (wpa_ie_len < sizeof(struct wpa_ie_hdr)) {
		wpa_printf(MSG_DEBUG, "%s: ie len too short %lu",
			   __func__, (unsigned long) wpa_ie_len);
		return -1;
	}

	hdr = (const struct wpa_ie_hdr *) wpa_ie;

	if (hdr->elem_id != WLAN_EID_VENDOR_SPECIFIC ||
	    hdr->len != wpa_ie_len - 2 ||
	    RSN_SELECTOR_GET(hdr->oui) != WPA_OUI_TYPE ||
	    WPA_GET_LE16(hdr->version) != WPA_VERSION) {
		wpa_printf(MSG_DEBUG, "%s: malformed ie or unknown version",
			   __func__);
		return -2;
	}

	pos = (const u8 *) (hdr + 1);
	left = wpa_ie_len - sizeof(*hdr);

	if (left >= WPA_SELECTOR_LEN) {
		data->group_cipher = wpa_selector_to_bitfield(pos);
		pos += WPA_SELECTOR_LEN;
		left -= WPA_SELECTOR_LEN;
	} else if (left > 0) {
		wpa_printf(MSG_DEBUG, "%s: ie length mismatch, %u too much",
			   __func__, left);
		return -3;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || count > left / WPA_SELECTOR_LEN) {
			wpa_printf(MSG_DEBUG, "%s: ie count botch (pairwise), "
				   "count %u left %u", __func__, count, left);
			return -4;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= wpa_selector_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
		wpa_printf(MSG_DEBUG, "%s: ie too short (for key mgmt)",
			   __func__);
		return -5;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || count > left / WPA_SELECTOR_LEN) {
			wpa_printf(MSG_DEBUG, "%s: ie count botch (key mgmt), "
				   "count %u left %u", __func__, count, left);
			return -6;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= wpa_key_mgmt_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
		wpa_printf(MSG_DEBUG, "%s: ie too short (for capabilities)",
			   __func__);
		return -7;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left > 0) {
		wpa_hexdump(MSG_DEBUG,
			    "wpa_parse_wpa_ie_wpa: ignore trailing bytes",
			    pos, left);
	}

	return 0;
}


int wpa_default_rsn_cipher(int freq)
{
	if (freq > 56160)
		return WPA_CIPHER_GCMP; /* DMG */

	return WPA_CIPHER_CCMP;
}


#ifdef CONFIG_IEEE80211R

/**
 * wpa_derive_pmk_r0 - Derive PMK-R0 and PMKR0Name
 *
 * IEEE Std 802.11r-2008 - 8.5.1.5.3
 */
int wpa_derive_pmk_r0(const u8 *xxkey, size_t xxkey_len,
		      const u8 *ssid, size_t ssid_len,
		      const u8 *mdid, const u8 *r0kh_id, size_t r0kh_id_len,
		      const u8 *s0kh_id, u8 *pmk_r0, u8 *pmk_r0_name,
		      int key_mgmt)
{
	u8 buf[1 + SSID_MAX_LEN + MOBILITY_DOMAIN_ID_LEN + 1 +
	       FT_R0KH_ID_MAX_LEN + ETH_ALEN];
	u8 *pos, r0_key_data[64 + 16], hash[64];
	const u8 *addr[2];
	size_t len[2];
	size_t q, r0_key_data_len;
	int res;

	if (key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY &&
	    (xxkey_len == SHA256_MAC_LEN || xxkey_len == SHA384_MAC_LEN ||
	     xxkey_len == SHA512_MAC_LEN))
		q = xxkey_len;
	else if (wpa_key_mgmt_sha384(key_mgmt))
		q = SHA384_MAC_LEN;
	else
		q = SHA256_MAC_LEN;
	r0_key_data_len = q + 16;

	/*
	 * R0-Key-Data = KDF-Hash-Length(XXKey, "FT-R0",
	 *                       SSIDlength || SSID || MDID || R0KHlength ||
	 *                       R0KH-ID || S0KH-ID)
	 * XXKey is either the second 256 bits of MSK or PSK; or the first
	 * 384 bits of MSK for FT-EAP-SHA384; or PMK from SAE.
	 * PMK-R0 = L(R0-Key-Data, 0, Q)
	 * PMK-R0Name-Salt = L(R0-Key-Data, Q, 128)
	 * Q = 384 for FT-EAP-SHA384; the length of the digest generated by H()
	 * for FT-SAE-EXT-KEY; or otherwise, 256
	 */
	if (ssid_len > SSID_MAX_LEN || r0kh_id_len > FT_R0KH_ID_MAX_LEN)
		return -1;
	wpa_printf(MSG_DEBUG, "FT: Derive PMK-R0 using KDF-SHA%zu", q * 8);
	wpa_hexdump_key(MSG_DEBUG, "FT: XXKey", xxkey, xxkey_len);
	wpa_hexdump_ascii(MSG_DEBUG, "FT: SSID", ssid, ssid_len);
	wpa_hexdump(MSG_DEBUG, "FT: MDID", mdid, MOBILITY_DOMAIN_ID_LEN);
	wpa_hexdump_ascii(MSG_DEBUG, "FT: R0KH-ID", r0kh_id, r0kh_id_len);
	wpa_printf(MSG_DEBUG, "FT: S0KH-ID: " MACSTR, MAC2STR(s0kh_id));
	pos = buf;
	*pos++ = ssid_len;
	os_memcpy(pos, ssid, ssid_len);
	pos += ssid_len;
	os_memcpy(pos, mdid, MOBILITY_DOMAIN_ID_LEN);
	pos += MOBILITY_DOMAIN_ID_LEN;
	*pos++ = r0kh_id_len;
	os_memcpy(pos, r0kh_id, r0kh_id_len);
	pos += r0kh_id_len;
	os_memcpy(pos, s0kh_id, ETH_ALEN);
	pos += ETH_ALEN;

	res = -1;
#ifdef CONFIG_SHA512
	if (q == SHA512_MAC_LEN) {
		if (xxkey_len != SHA512_MAC_LEN) {
			wpa_printf(MSG_ERROR,
				   "FT: Unexpected XXKey length %d (expected %d)",
				   (int) xxkey_len, SHA512_MAC_LEN);
			return -1;
		}
		res = sha512_prf(xxkey, xxkey_len, "FT-R0", buf, pos - buf,
				 r0_key_data, r0_key_data_len);
	}
#endif /* CONFIG_SHA512 */
#ifdef CONFIG_SHA384
	if (q == SHA384_MAC_LEN) {
		if (xxkey_len != SHA384_MAC_LEN) {
			wpa_printf(MSG_ERROR,
				   "FT: Unexpected XXKey length %d (expected %d)",
				   (int) xxkey_len, SHA384_MAC_LEN);
			return -1;
		}
		res = sha384_prf(xxkey, xxkey_len, "FT-R0", buf, pos - buf,
				 r0_key_data, r0_key_data_len);
	}
#endif /* CONFIG_SHA384 */
	if (q == SHA256_MAC_LEN) {
		if (xxkey_len != PMK_LEN) {
			wpa_printf(MSG_ERROR,
				   "FT: Unexpected XXKey length %d (expected %d)",
				   (int) xxkey_len, PMK_LEN);
			return -1;
		}
		res = sha256_prf(xxkey, xxkey_len, "FT-R0", buf, pos - buf,
				 r0_key_data, r0_key_data_len);
	}
	if (res < 0)
		return res;
	os_memcpy(pmk_r0, r0_key_data, q);
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R0", pmk_r0, q);
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R0Name-Salt", &r0_key_data[q], 16);

	/*
	 * PMKR0Name = Truncate-128(Hash("FT-R0N" || PMK-R0Name-Salt)
	 */
	addr[0] = (const u8 *) "FT-R0N";
	len[0] = 6;
	addr[1] = &r0_key_data[q];
	len[1] = 16;

	res = -1;
#ifdef CONFIG_SHA512
	if (q == SHA512_MAC_LEN)
		res = sha512_vector(2, addr, len, hash);
#endif /* CONFIG_SHA512 */
#ifdef CONFIG_SHA384
	if (q == SHA384_MAC_LEN)
		res = sha384_vector(2, addr, len, hash);
#endif /* CONFIG_SHA384 */
	if (q == SHA256_MAC_LEN)
		res = sha256_vector(2, addr, len, hash);
	if (res < 0) {
		wpa_printf(MSG_DEBUG,
			   "FT: Failed to derive PMKR0Name (PMK-R0 len %zu)",
			   q);
		return res;
	}
	os_memcpy(pmk_r0_name, hash, WPA_PMK_NAME_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: PMKR0Name", pmk_r0_name, WPA_PMK_NAME_LEN);
	forced_memzero(r0_key_data, sizeof(r0_key_data));
	return 0;
}


/**
 * wpa_derive_pmk_r1_name - Derive PMKR1Name
 *
 * IEEE Std 802.11r-2008 - 8.5.1.5.4
 */
int wpa_derive_pmk_r1_name(const u8 *pmk_r0_name, const u8 *r1kh_id,
			   const u8 *s1kh_id, u8 *pmk_r1_name,
			   size_t pmk_r1_len)
{
	u8 hash[64];
	const u8 *addr[4];
	size_t len[4];
	int res;
	const char *title;

	/*
	 * PMKR1Name = Truncate-128(Hash("FT-R1N" || PMKR0Name ||
	 *                               R1KH-ID || S1KH-ID))
	 */
	addr[0] = (const u8 *) "FT-R1N";
	len[0] = 6;
	addr[1] = pmk_r0_name;
	len[1] = WPA_PMK_NAME_LEN;
	addr[2] = r1kh_id;
	len[2] = FT_R1KH_ID_LEN;
	addr[3] = s1kh_id;
	len[3] = ETH_ALEN;

	res = -1;
#ifdef CONFIG_SHA512
	if (pmk_r1_len == SHA512_MAC_LEN) {
		title = "FT: PMKR1Name (using SHA512)";
		res = sha512_vector(4, addr, len, hash);
	}
#endif /* CONFIG_SHA512 */
#ifdef CONFIG_SHA384
	if (pmk_r1_len == SHA384_MAC_LEN) {
		title = "FT: PMKR1Name (using SHA384)";
		res = sha384_vector(4, addr, len, hash);
	}
#endif /* CONFIG_SHA384 */
	if (pmk_r1_len == SHA256_MAC_LEN) {
		title = "FT: PMKR1Name (using SHA256)";
		res = sha256_vector(4, addr, len, hash);
	}
	if (res < 0) {
		wpa_printf(MSG_DEBUG,
			   "FT: Failed to derive PMKR1Name (PMK-R1 len %zu)",
			   pmk_r1_len);
		return res;
	}
	os_memcpy(pmk_r1_name, hash, WPA_PMK_NAME_LEN);
	wpa_hexdump(MSG_DEBUG, title, pmk_r1_name, WPA_PMK_NAME_LEN);
	return 0;
}


/**
 * wpa_derive_pmk_r1 - Derive PMK-R1 and PMKR1Name from PMK-R0
 *
 * IEEE Std 802.11r-2008 - 8.5.1.5.4
 */
int wpa_derive_pmk_r1(const u8 *pmk_r0, size_t pmk_r0_len,
		      const u8 *pmk_r0_name,
		      const u8 *r1kh_id, const u8 *s1kh_id,
		      u8 *pmk_r1, u8 *pmk_r1_name)
{
	u8 buf[FT_R1KH_ID_LEN + ETH_ALEN];
	u8 *pos;
	int res;

	/* PMK-R1 = KDF-Hash(PMK-R0, "FT-R1", R1KH-ID || S1KH-ID) */
	wpa_printf(MSG_DEBUG, "FT: Derive PMK-R1 using KDF-SHA%zu",
		   pmk_r0_len * 8);
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R0", pmk_r0, pmk_r0_len);
	wpa_hexdump(MSG_DEBUG, "FT: R1KH-ID", r1kh_id, FT_R1KH_ID_LEN);
	wpa_printf(MSG_DEBUG, "FT: S1KH-ID: " MACSTR, MAC2STR(s1kh_id));
	pos = buf;
	os_memcpy(pos, r1kh_id, FT_R1KH_ID_LEN);
	pos += FT_R1KH_ID_LEN;
	os_memcpy(pos, s1kh_id, ETH_ALEN);
	pos += ETH_ALEN;

	res = -1;
#ifdef CONFIG_SHA512
	if (pmk_r0_len == SHA512_MAC_LEN)
		res = sha512_prf(pmk_r0, pmk_r0_len, "FT-R1",
				 buf, pos - buf, pmk_r1, pmk_r0_len);
#endif /* CONFIG_SHA512 */
#ifdef CONFIG_SHA384
	if (pmk_r0_len == SHA384_MAC_LEN)
		res = sha384_prf(pmk_r0, pmk_r0_len, "FT-R1",
				 buf, pos - buf, pmk_r1, pmk_r0_len);
#endif /* CONFIG_SHA384 */
	if (pmk_r0_len == SHA256_MAC_LEN)
		res = sha256_prf(pmk_r0, pmk_r0_len, "FT-R1",
				 buf, pos - buf, pmk_r1, pmk_r0_len);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "FT: Failed to derive PMK-R1");
		return res;
	}
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R1", pmk_r1, pmk_r0_len);

	return wpa_derive_pmk_r1_name(pmk_r0_name, r1kh_id, s1kh_id,
				      pmk_r1_name, pmk_r0_len);
}


/**
 * wpa_pmk_r1_to_ptk - Derive PTK and PTKName from PMK-R1
 *
 * IEEE Std 802.11r-2008 - 8.5.1.5.5
 */
int wpa_pmk_r1_to_ptk(const u8 *pmk_r1, size_t pmk_r1_len,
		      const u8 *snonce, const u8 *anonce,
		      const u8 *sta_addr, const u8 *bssid,
		      const u8 *pmk_r1_name,
		      struct wpa_ptk *ptk, u8 *ptk_name, int akmp, int cipher,
		      size_t kdk_len)
{
	u8 buf[2 * WPA_NONCE_LEN + 2 * ETH_ALEN];
	u8 *pos, hash[32];
	const u8 *addr[6];
	size_t len[6];
	u8 tmp[2 * WPA_KCK_MAX_LEN + 2 * WPA_KEK_MAX_LEN + WPA_TK_MAX_LEN +
	       WPA_KDK_MAX_LEN];
	size_t ptk_len, offset;
	size_t key_len;
	int res;

	if (kdk_len > WPA_KDK_MAX_LEN) {
		wpa_printf(MSG_ERROR,
			   "FT: KDK len=%zu exceeds max supported len",
			   kdk_len);
		return -1;
	}

	if (akmp == WPA_KEY_MGMT_FT_SAE_EXT_KEY &&
	    (pmk_r1_len == SHA256_MAC_LEN || pmk_r1_len == SHA384_MAC_LEN ||
	     pmk_r1_len == SHA512_MAC_LEN))
		key_len = pmk_r1_len;
	else if (wpa_key_mgmt_sha384(akmp))
		key_len = SHA384_MAC_LEN;
	else
		key_len = SHA256_MAC_LEN;

	/*
	 * PTK = KDF-PTKLen(PMK-R1, "FT-PTK", SNonce || ANonce ||
	 *                  BSSID || STA-ADDR)
	 */
	wpa_printf(MSG_DEBUG, "FT: Derive PTK using KDF-SHA%zu", key_len * 8);
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R1", pmk_r1, pmk_r1_len);
	wpa_hexdump(MSG_DEBUG, "FT: SNonce", snonce, WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: ANonce", anonce, WPA_NONCE_LEN);
	wpa_printf(MSG_DEBUG, "FT: BSSID=" MACSTR " STA-ADDR=" MACSTR,
		   MAC2STR(bssid), MAC2STR(sta_addr));
	pos = buf;
	os_memcpy(pos, snonce, WPA_NONCE_LEN);
	pos += WPA_NONCE_LEN;
	os_memcpy(pos, anonce, WPA_NONCE_LEN);
	pos += WPA_NONCE_LEN;
	os_memcpy(pos, bssid, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, sta_addr, ETH_ALEN);
	pos += ETH_ALEN;

	ptk->kck_len = wpa_kck_len(akmp, key_len);
	ptk->kck2_len = wpa_kck2_len(akmp);
	ptk->kek_len = wpa_kek_len(akmp, key_len);
	ptk->kek2_len = wpa_kek2_len(akmp);
	ptk->tk_len = wpa_cipher_key_len(cipher);
	ptk->kdk_len = kdk_len;
	ptk_len = ptk->kck_len + ptk->kek_len + ptk->tk_len +
		ptk->kck2_len + ptk->kek2_len + ptk->kdk_len;

	res = -1;
#ifdef CONFIG_SHA512
	if (key_len == SHA512_MAC_LEN) {
		if (pmk_r1_len != SHA512_MAC_LEN) {
			wpa_printf(MSG_ERROR,
				   "FT: Unexpected PMK-R1 length %d (expected %d)",
				   (int) pmk_r1_len, SHA512_MAC_LEN);
			return -1;
		}
		res = sha512_prf(pmk_r1, pmk_r1_len, "FT-PTK",
				 buf, pos - buf, tmp, ptk_len);
	}
#endif /* CONFIG_SHA512 */
#ifdef CONFIG_SHA384
	if (key_len == SHA384_MAC_LEN) {
		if (pmk_r1_len != SHA384_MAC_LEN) {
			wpa_printf(MSG_ERROR,
				   "FT: Unexpected PMK-R1 length %d (expected %d)",
				   (int) pmk_r1_len, SHA384_MAC_LEN);
			return -1;
		}
		res = sha384_prf(pmk_r1, pmk_r1_len, "FT-PTK",
				 buf, pos - buf, tmp, ptk_len);
	}
#endif /* CONFIG_SHA384 */
	if (key_len == SHA256_MAC_LEN) {
		if (pmk_r1_len != PMK_LEN) {
			wpa_printf(MSG_ERROR,
				   "FT: Unexpected PMK-R1 length %d (expected %d)",
				   (int) pmk_r1_len, PMK_LEN);
			return -1;
		}
		res = sha256_prf(pmk_r1, pmk_r1_len, "FT-PTK",
				 buf, pos - buf, tmp, ptk_len);
	}
	if (res < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "FT: PTK", tmp, ptk_len);

	/*
	 * PTKName = Truncate-128(SHA-256(PMKR1Name || "FT-PTKN" || SNonce ||
	 *                                ANonce || BSSID || STA-ADDR))
	 */
	wpa_hexdump(MSG_DEBUG, "FT: PMKR1Name", pmk_r1_name, WPA_PMK_NAME_LEN);
	addr[0] = pmk_r1_name;
	len[0] = WPA_PMK_NAME_LEN;
	addr[1] = (const u8 *) "FT-PTKN";
	len[1] = 7;
	addr[2] = snonce;
	len[2] = WPA_NONCE_LEN;
	addr[3] = anonce;
	len[3] = WPA_NONCE_LEN;
	addr[4] = bssid;
	len[4] = ETH_ALEN;
	addr[5] = sta_addr;
	len[5] = ETH_ALEN;

	if (sha256_vector(6, addr, len, hash) < 0)
		return -1;
	os_memcpy(ptk_name, hash, WPA_PMK_NAME_LEN);

	os_memcpy(ptk->kck, tmp, ptk->kck_len);
	offset = ptk->kck_len;
	os_memcpy(ptk->kek, tmp + offset, ptk->kek_len);
	offset += ptk->kek_len;
	os_memcpy(ptk->tk, tmp + offset, ptk->tk_len);
	offset += ptk->tk_len;
	os_memcpy(ptk->kck2, tmp + offset, ptk->kck2_len);
	offset += ptk->kck2_len;
	os_memcpy(ptk->kek2, tmp + offset, ptk->kek2_len);
	offset += ptk->kek2_len;
	os_memcpy(ptk->kdk, tmp + offset, ptk->kdk_len);

	wpa_hexdump_key(MSG_DEBUG, "FT: KCK", ptk->kck, ptk->kck_len);
	wpa_hexdump_key(MSG_DEBUG, "FT: KEK", ptk->kek, ptk->kek_len);
	if (ptk->kck2_len)
		wpa_hexdump_key(MSG_DEBUG, "FT: KCK2",
				ptk->kck2, ptk->kck2_len);
	if (ptk->kek2_len)
		wpa_hexdump_key(MSG_DEBUG, "FT: KEK2",
				ptk->kek2, ptk->kek2_len);
	if (ptk->kdk_len)
		wpa_hexdump_key(MSG_DEBUG, "FT: KDK", ptk->kdk, ptk->kdk_len);

	wpa_hexdump_key(MSG_DEBUG, "FT: TK", ptk->tk, ptk->tk_len);
	wpa_hexdump(MSG_DEBUG, "FT: PTKName", ptk_name, WPA_PMK_NAME_LEN);

	forced_memzero(tmp, sizeof(tmp));

	return 0;
}

#endif /* CONFIG_IEEE80211R */


/**
 * rsn_pmkid - Calculate PMK identifier
 * @pmk: Pairwise master key
 * @pmk_len: Length of pmk in bytes
 * @aa: Authenticator address
 * @spa: Supplicant address
 * @pmkid: Buffer for PMKID
 * @akmp: Negotiated key management protocol
 *
 * IEEE Std 802.11-2016 - 12.7.1.3 Pairwise key hierarchy
 * AKM: 00-0F-AC:3, 00-0F-AC:5, 00-0F-AC:6, 00-0F-AC:14, 00-0F-AC:16
 * PMKID = Truncate-128(HMAC-SHA-256(PMK, "PMK Name" || AA || SPA))
 * AKM: 00-0F-AC:11
 * See rsn_pmkid_suite_b()
 * AKM: 00-0F-AC:12
 * See rsn_pmkid_suite_b_192()
 * AKM: 00-0F-AC:13, 00-0F-AC:15, 00-0F-AC:17
 * PMKID = Truncate-128(HMAC-SHA-384(PMK, "PMK Name" || AA || SPA))
 * Otherwise:
 * PMKID = Truncate-128(HMAC-SHA-1(PMK, "PMK Name" || AA || SPA))
 */
void rsn_pmkid(const u8 *pmk, size_t pmk_len, const u8 *aa, const u8 *spa,
	       u8 *pmkid, int akmp)
{
	char *title = "PMK Name";
	const u8 *addr[3];
	const size_t len[3] = { 8, ETH_ALEN, ETH_ALEN };
	unsigned char hash[SHA384_MAC_LEN];

	addr[0] = (u8 *) title;
	addr[1] = aa;
	addr[2] = spa;

	if (0) {
#if defined(CONFIG_FILS) || defined(CONFIG_SHA384)
	} else if (wpa_key_mgmt_sha384(akmp)) {
		wpa_printf(MSG_DEBUG, "RSN: Derive PMKID using HMAC-SHA-384");
		hmac_sha384_vector(pmk, pmk_len, 3, addr, len, hash);
#endif /* CONFIG_FILS || CONFIG_SHA384 */
	} else if (wpa_key_mgmt_sha256(akmp)) {
		wpa_printf(MSG_DEBUG, "RSN: Derive PMKID using HMAC-SHA-256");
		hmac_sha256_vector(pmk, pmk_len, 3, addr, len, hash);
	} else {
		wpa_printf(MSG_DEBUG, "RSN: Derive PMKID using HMAC-SHA-1");
		hmac_sha1_vector(pmk, pmk_len, 3, addr, len, hash);
	}
	wpa_hexdump(MSG_DEBUG, "RSN: Derived PMKID", hash, PMKID_LEN);
	os_memcpy(pmkid, hash, PMKID_LEN);
}


#ifdef CONFIG_SUITEB
/**
 * rsn_pmkid_suite_b - Calculate PMK identifier for Suite B AKM
 * @kck: Key confirmation key
 * @kck_len: Length of kck in bytes
 * @aa: Authenticator address
 * @spa: Supplicant address
 * @pmkid: Buffer for PMKID
 * Returns: 0 on success, -1 on failure
 *
 * IEEE Std 802.11ac-2013 - 11.6.1.3 Pairwise key hierarchy
 * PMKID = Truncate(HMAC-SHA-256(KCK, "PMK Name" || AA || SPA))
 */
int rsn_pmkid_suite_b(const u8 *kck, size_t kck_len, const u8 *aa,
		      const u8 *spa, u8 *pmkid)
{
	char *title = "PMK Name";
	const u8 *addr[3];
	const size_t len[3] = { 8, ETH_ALEN, ETH_ALEN };
	unsigned char hash[SHA256_MAC_LEN];

	addr[0] = (u8 *) title;
	addr[1] = aa;
	addr[2] = spa;

	if (hmac_sha256_vector(kck, kck_len, 3, addr, len, hash) < 0)
		return -1;
	os_memcpy(pmkid, hash, PMKID_LEN);
	return 0;
}
#endif /* CONFIG_SUITEB */


#ifdef CONFIG_SUITEB192
/**
 * rsn_pmkid_suite_b_192 - Calculate PMK identifier for Suite B AKM
 * @kck: Key confirmation key
 * @kck_len: Length of kck in bytes
 * @aa: Authenticator address
 * @spa: Supplicant address
 * @pmkid: Buffer for PMKID
 * Returns: 0 on success, -1 on failure
 *
 * IEEE Std 802.11ac-2013 - 11.6.1.3 Pairwise key hierarchy
 * PMKID = Truncate(HMAC-SHA-384(KCK, "PMK Name" || AA || SPA))
 */
int rsn_pmkid_suite_b_192(const u8 *kck, size_t kck_len, const u8 *aa,
			  const u8 *spa, u8 *pmkid)
{
	char *title = "PMK Name";
	const u8 *addr[3];
	const size_t len[3] = { 8, ETH_ALEN, ETH_ALEN };
	unsigned char hash[SHA384_MAC_LEN];

	addr[0] = (u8 *) title;
	addr[1] = aa;
	addr[2] = spa;

	if (hmac_sha384_vector(kck, kck_len, 3, addr, len, hash) < 0)
		return -1;
	os_memcpy(pmkid, hash, PMKID_LEN);
	return 0;
}
#endif /* CONFIG_SUITEB192 */


/**
 * wpa_cipher_txt - Convert cipher suite to a text string
 * @cipher: Cipher suite (WPA_CIPHER_* enum)
 * Returns: Pointer to a text string of the cipher suite name
 */
const char * wpa_cipher_txt(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_NONE:
		return "NONE";
#ifdef CONFIG_WEP
	case WPA_CIPHER_WEP40:
		return "WEP-40";
	case WPA_CIPHER_WEP104:
		return "WEP-104";
#endif /* CONFIG_WEP */
	case WPA_CIPHER_TKIP:
		return "TKIP";
	case WPA_CIPHER_CCMP:
		return "CCMP";
	case WPA_CIPHER_CCMP | WPA_CIPHER_TKIP:
		return "CCMP+TKIP";
	case WPA_CIPHER_GCMP:
		return "GCMP";
	case WPA_CIPHER_GCMP_256:
		return "GCMP-256";
	case WPA_CIPHER_CCMP_256:
		return "CCMP-256";
	case WPA_CIPHER_AES_128_CMAC:
		return "BIP";
	case WPA_CIPHER_BIP_GMAC_128:
		return "BIP-GMAC-128";
	case WPA_CIPHER_BIP_GMAC_256:
		return "BIP-GMAC-256";
	case WPA_CIPHER_BIP_CMAC_256:
		return "BIP-CMAC-256";
	case WPA_CIPHER_GTK_NOT_USED:
		return "GTK_NOT_USED";
	default:
		return "UNKNOWN";
	}
}


/**
 * wpa_key_mgmt_txt - Convert key management suite to a text string
 * @key_mgmt: Key management suite (WPA_KEY_MGMT_* enum)
 * @proto: WPA/WPA2 version (WPA_PROTO_*)
 * Returns: Pointer to a text string of the key management suite name
 */
const char * wpa_key_mgmt_txt(int key_mgmt, int proto)
{
	switch (key_mgmt) {
	case WPA_KEY_MGMT_IEEE8021X:
		if (proto == (WPA_PROTO_RSN | WPA_PROTO_WPA))
			return "WPA2+WPA/IEEE 802.1X/EAP";
		return proto == WPA_PROTO_RSN ?
			"WPA2/IEEE 802.1X/EAP" : "WPA/IEEE 802.1X/EAP";
	case WPA_KEY_MGMT_PSK:
		if (proto == (WPA_PROTO_RSN | WPA_PROTO_WPA))
			return "WPA2-PSK+WPA-PSK";
		return proto == WPA_PROTO_RSN ?
			"WPA2-PSK" : "WPA-PSK";
	case WPA_KEY_MGMT_NONE:
		return "NONE";
	case WPA_KEY_MGMT_WPA_NONE:
		return "WPA-NONE";
	case WPA_KEY_MGMT_IEEE8021X_NO_WPA:
		return "IEEE 802.1X (no WPA)";
#ifdef CONFIG_IEEE80211R
	case WPA_KEY_MGMT_FT_IEEE8021X:
		return "FT-EAP";
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
		return "FT-EAP-SHA384";
	case WPA_KEY_MGMT_FT_PSK:
		return "FT-PSK";
#endif /* CONFIG_IEEE80211R */
	case WPA_KEY_MGMT_IEEE8021X_SHA256:
		return "WPA2-EAP-SHA256";
	case WPA_KEY_MGMT_PSK_SHA256:
		return "WPA2-PSK-SHA256";
	case WPA_KEY_MGMT_WPS:
		return "WPS";
	case WPA_KEY_MGMT_SAE:
		return "SAE";
	case WPA_KEY_MGMT_SAE_EXT_KEY:
		return "SAE-EXT-KEY";
	case WPA_KEY_MGMT_FT_SAE:
		return "FT-SAE";
	case WPA_KEY_MGMT_FT_SAE_EXT_KEY:
		return "FT-SAE-EXT-KEY";
	case WPA_KEY_MGMT_OSEN:
		return "OSEN";
	case WPA_KEY_MGMT_IEEE8021X_SUITE_B:
		return "WPA2-EAP-SUITE-B";
	case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
		return "WPA2-EAP-SUITE-B-192";
	case WPA_KEY_MGMT_FILS_SHA256:
		return "FILS-SHA256";
	case WPA_KEY_MGMT_FILS_SHA384:
		return "FILS-SHA384";
	case WPA_KEY_MGMT_FT_FILS_SHA256:
		return "FT-FILS-SHA256";
	case WPA_KEY_MGMT_FT_FILS_SHA384:
		return "FT-FILS-SHA384";
	case WPA_KEY_MGMT_OWE:
		return "OWE";
	case WPA_KEY_MGMT_DPP:
		return "DPP";
	case WPA_KEY_MGMT_PASN:
		return "PASN";
	case WPA_KEY_MGMT_IEEE8021X_SHA384:
		return "WPA2-EAP-SHA384";
	default:
		return "UNKNOWN";
	}
}


u32 wpa_akm_to_suite(int akm)
{
	if (akm & WPA_KEY_MGMT_FT_IEEE8021X_SHA384)
		return RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384;
	if (akm & WPA_KEY_MGMT_FT_IEEE8021X)
		return RSN_AUTH_KEY_MGMT_FT_802_1X;
	if (akm & WPA_KEY_MGMT_FT_PSK)
		return RSN_AUTH_KEY_MGMT_FT_PSK;
	if (akm & WPA_KEY_MGMT_IEEE8021X_SHA384)
		return RSN_AUTH_KEY_MGMT_802_1X_SHA384;
	if (akm & WPA_KEY_MGMT_IEEE8021X_SHA256)
		return RSN_AUTH_KEY_MGMT_802_1X_SHA256;
	if (akm & WPA_KEY_MGMT_IEEE8021X)
		return RSN_AUTH_KEY_MGMT_UNSPEC_802_1X;
	if (akm & WPA_KEY_MGMT_PSK_SHA256)
		return RSN_AUTH_KEY_MGMT_PSK_SHA256;
	if (akm & WPA_KEY_MGMT_PSK)
		return RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X;
	if (akm & WPA_KEY_MGMT_CCKM)
		return RSN_AUTH_KEY_MGMT_CCKM;
	if (akm & WPA_KEY_MGMT_OSEN)
		return RSN_AUTH_KEY_MGMT_OSEN;
	if (akm & WPA_KEY_MGMT_IEEE8021X_SUITE_B)
		return RSN_AUTH_KEY_MGMT_802_1X_SUITE_B;
	if (akm & WPA_KEY_MGMT_IEEE8021X_SUITE_B_192)
		return RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192;
	if (akm & WPA_KEY_MGMT_FILS_SHA256)
		return RSN_AUTH_KEY_MGMT_FILS_SHA256;
	if (akm & WPA_KEY_MGMT_FILS_SHA384)
		return RSN_AUTH_KEY_MGMT_FILS_SHA384;
	if (akm & WPA_KEY_MGMT_FT_FILS_SHA256)
		return RSN_AUTH_KEY_MGMT_FT_FILS_SHA256;
	if (akm & WPA_KEY_MGMT_FT_FILS_SHA384)
		return RSN_AUTH_KEY_MGMT_FT_FILS_SHA384;
	if (akm & WPA_KEY_MGMT_SAE)
		return RSN_AUTH_KEY_MGMT_SAE;
	if (akm & WPA_KEY_MGMT_SAE_EXT_KEY)
		return RSN_AUTH_KEY_MGMT_SAE_EXT_KEY;
	if (akm & WPA_KEY_MGMT_FT_SAE)
		return RSN_AUTH_KEY_MGMT_FT_SAE;
	if (akm & WPA_KEY_MGMT_FT_SAE_EXT_KEY)
		return RSN_AUTH_KEY_MGMT_FT_SAE_EXT_KEY;
	if (akm & WPA_KEY_MGMT_OWE)
		return RSN_AUTH_KEY_MGMT_OWE;
	if (akm & WPA_KEY_MGMT_DPP)
		return RSN_AUTH_KEY_MGMT_DPP;
	return 0;
}


int wpa_compare_rsn_ie(int ft_initial_assoc,
		       const u8 *ie1, size_t ie1len,
		       const u8 *ie2, size_t ie2len)
{
	if (ie1 == NULL || ie2 == NULL)
		return -1;

	if (ie1len == ie2len && os_memcmp(ie1, ie2, ie1len) == 0)
		return 0; /* identical IEs */

#ifdef CONFIG_IEEE80211R
	if (ft_initial_assoc) {
		struct wpa_ie_data ie1d, ie2d;
		/*
		 * The PMKID-List in RSN IE is different between Beacon/Probe
		 * Response/(Re)Association Request frames and EAPOL-Key
		 * messages in FT initial mobility domain association. Allow
		 * for this, but verify that other parts of the RSN IEs are
		 * identical.
		 */
		if (wpa_parse_wpa_ie_rsn(ie1, ie1len, &ie1d) < 0 ||
		    wpa_parse_wpa_ie_rsn(ie2, ie2len, &ie2d) < 0)
			return -1;
		if (ie1d.proto == ie2d.proto &&
		    ie1d.pairwise_cipher == ie2d.pairwise_cipher &&
		    ie1d.group_cipher == ie2d.group_cipher &&
		    ie1d.key_mgmt == ie2d.key_mgmt &&
		    ie1d.capabilities == ie2d.capabilities &&
		    ie1d.mgmt_group_cipher == ie2d.mgmt_group_cipher)
			return 0;
	}
#endif /* CONFIG_IEEE80211R */

	return -1;
}


int wpa_insert_pmkid(u8 *ies, size_t *ies_len, const u8 *pmkid, bool replace)
{
	u8 *start, *end, *rpos, *rend;
	int added = 0;

	start = ies;
	end = ies + *ies_len;

	while (start < end) {
		if (*start == WLAN_EID_RSN)
			break;
		start += 2 + start[1];
	}
	if (start >= end) {
		wpa_printf(MSG_ERROR, "RSN: Could not find RSNE in IEs data");
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "RSN: RSNE before modification",
		    start, 2 + start[1]);

	/* Find start of PMKID-Count */
	rpos = start + 2;
	rend = rpos + start[1];

	/* Skip Version and Group Data Cipher Suite */
	rpos += 2 + 4;
	/* Skip Pairwise Cipher Suite Count and List */
	rpos += 2 + WPA_GET_LE16(rpos) * RSN_SELECTOR_LEN;
	/* Skip AKM Suite Count and List */
	rpos += 2 + WPA_GET_LE16(rpos) * RSN_SELECTOR_LEN;

	if (rpos == rend) {
		/* Add RSN Capabilities */
		os_memmove(rpos + 2, rpos, end - rpos);
		*rpos++ = 0;
		*rpos++ = 0;
		added += 2;
		start[1] += 2;
		rend = rpos;
	} else {
		/* Skip RSN Capabilities */
		rpos += 2;
		if (rpos > rend) {
			wpa_printf(MSG_ERROR,
				   "RSN: Could not parse RSNE in IEs data");
			return -1;
		}
	}

	if (rpos == rend) {
		/* No PMKID-Count field included; add it */
		os_memmove(rpos + 2 + PMKID_LEN, rpos, end + added - rpos);
		WPA_PUT_LE16(rpos, 1);
		rpos += 2;
		os_memcpy(rpos, pmkid, PMKID_LEN);
		added += 2 + PMKID_LEN;
		start[1] += 2 + PMKID_LEN;
	} else {
		u16 num_pmkid;

		if (rend - rpos < 2)
			return -1;
		num_pmkid = WPA_GET_LE16(rpos);
		if (num_pmkid * PMKID_LEN > rend - rpos - 2)
			return -1;
		/* PMKID-Count was included; use it */
		if (replace && num_pmkid != 0) {
			u8 *after;

			/*
			 * PMKID may have been included in RSN IE in
			 * (Re)Association Request frame, so remove the old
			 * PMKID(s) first before adding the new one.
			 */
			wpa_printf(MSG_DEBUG,
				   "RSN: Remove %u old PMKID(s) from RSNE",
				   num_pmkid);
			after = rpos + 2 + num_pmkid * PMKID_LEN;
			os_memmove(rpos + 2, after, end - after);
			start[1] -= num_pmkid * PMKID_LEN;
			added -= num_pmkid * PMKID_LEN;
			num_pmkid = 0;
		}
		WPA_PUT_LE16(rpos, num_pmkid + 1);
		rpos += 2;
		os_memmove(rpos + PMKID_LEN, rpos, end + added - rpos);
		os_memcpy(rpos, pmkid, PMKID_LEN);
		added += PMKID_LEN;
		start[1] += PMKID_LEN;
	}

	wpa_hexdump(MSG_DEBUG, "RSN: RSNE after modification (PMKID inserted)",
		    start, 2 + start[1]);

	*ies_len += added;

	return 0;
}


int wpa_cipher_key_len(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_CCMP_256:
	case WPA_CIPHER_GCMP_256:
	case WPA_CIPHER_BIP_GMAC_256:
	case WPA_CIPHER_BIP_CMAC_256:
		return 32;
	case WPA_CIPHER_CCMP:
	case WPA_CIPHER_GCMP:
	case WPA_CIPHER_AES_128_CMAC:
	case WPA_CIPHER_BIP_GMAC_128:
		return 16;
	case WPA_CIPHER_TKIP:
		return 32;
	default:
		return 0;
	}
}


int wpa_cipher_rsc_len(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_CCMP_256:
	case WPA_CIPHER_GCMP_256:
	case WPA_CIPHER_CCMP:
	case WPA_CIPHER_GCMP:
	case WPA_CIPHER_TKIP:
		return 6;
	default:
		return 0;
	}
}


enum wpa_alg wpa_cipher_to_alg(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_CCMP_256:
		return WPA_ALG_CCMP_256;
	case WPA_CIPHER_GCMP_256:
		return WPA_ALG_GCMP_256;
	case WPA_CIPHER_CCMP:
		return WPA_ALG_CCMP;
	case WPA_CIPHER_GCMP:
		return WPA_ALG_GCMP;
	case WPA_CIPHER_TKIP:
		return WPA_ALG_TKIP;
	case WPA_CIPHER_AES_128_CMAC:
		return WPA_ALG_BIP_CMAC_128;
	case WPA_CIPHER_BIP_GMAC_128:
		return WPA_ALG_BIP_GMAC_128;
	case WPA_CIPHER_BIP_GMAC_256:
		return WPA_ALG_BIP_GMAC_256;
	case WPA_CIPHER_BIP_CMAC_256:
		return WPA_ALG_BIP_CMAC_256;
	default:
		return WPA_ALG_NONE;
	}
}


int wpa_cipher_valid_pairwise(int cipher)
{
#ifdef CONFIG_NO_TKIP
	return cipher == WPA_CIPHER_CCMP_256 ||
		cipher == WPA_CIPHER_GCMP_256 ||
		cipher == WPA_CIPHER_CCMP ||
		cipher == WPA_CIPHER_GCMP;
#else /* CONFIG_NO_TKIP */
	return cipher == WPA_CIPHER_CCMP_256 ||
		cipher == WPA_CIPHER_GCMP_256 ||
		cipher == WPA_CIPHER_CCMP ||
		cipher == WPA_CIPHER_GCMP ||
		cipher == WPA_CIPHER_TKIP;
#endif /* CONFIG_NO_TKIP */
}


u32 wpa_cipher_to_suite(int proto, int cipher)
{
	if (cipher & WPA_CIPHER_CCMP_256)
		return RSN_CIPHER_SUITE_CCMP_256;
	if (cipher & WPA_CIPHER_GCMP_256)
		return RSN_CIPHER_SUITE_GCMP_256;
	if (cipher & WPA_CIPHER_CCMP)
		return (proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_CCMP : WPA_CIPHER_SUITE_CCMP);
	if (cipher & WPA_CIPHER_GCMP)
		return RSN_CIPHER_SUITE_GCMP;
	if (cipher & WPA_CIPHER_TKIP)
		return (proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_TKIP : WPA_CIPHER_SUITE_TKIP);
	if (cipher & WPA_CIPHER_NONE)
		return (proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_NONE : WPA_CIPHER_SUITE_NONE);
	if (cipher & WPA_CIPHER_GTK_NOT_USED)
		return RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED;
	if (cipher & WPA_CIPHER_AES_128_CMAC)
		return RSN_CIPHER_SUITE_AES_128_CMAC;
	if (cipher & WPA_CIPHER_BIP_GMAC_128)
		return RSN_CIPHER_SUITE_BIP_GMAC_128;
	if (cipher & WPA_CIPHER_BIP_GMAC_256)
		return RSN_CIPHER_SUITE_BIP_GMAC_256;
	if (cipher & WPA_CIPHER_BIP_CMAC_256)
		return RSN_CIPHER_SUITE_BIP_CMAC_256;
	return 0;
}


int rsn_cipher_put_suites(u8 *start, int ciphers)
{
	u8 *pos = start;

	if (ciphers & WPA_CIPHER_CCMP_256) {
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_CCMP_256);
		pos += RSN_SELECTOR_LEN;
	}
	if (ciphers & WPA_CIPHER_GCMP_256) {
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_GCMP_256);
		pos += RSN_SELECTOR_LEN;
	}
	if (ciphers & WPA_CIPHER_CCMP) {
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_CCMP);
		pos += RSN_SELECTOR_LEN;
	}
	if (ciphers & WPA_CIPHER_GCMP) {
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_GCMP);
		pos += RSN_SELECTOR_LEN;
	}
	if (ciphers & WPA_CIPHER_TKIP) {
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_TKIP);
		pos += RSN_SELECTOR_LEN;
	}
	if (ciphers & WPA_CIPHER_NONE) {
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_NONE);
		pos += RSN_SELECTOR_LEN;
	}

	return (pos - start) / RSN_SELECTOR_LEN;
}


int wpa_cipher_put_suites(u8 *start, int ciphers)
{
	u8 *pos = start;

	if (ciphers & WPA_CIPHER_CCMP) {
		RSN_SELECTOR_PUT(pos, WPA_CIPHER_SUITE_CCMP);
		pos += WPA_SELECTOR_LEN;
	}
	if (ciphers & WPA_CIPHER_TKIP) {
		RSN_SELECTOR_PUT(pos, WPA_CIPHER_SUITE_TKIP);
		pos += WPA_SELECTOR_LEN;
	}
	if (ciphers & WPA_CIPHER_NONE) {
		RSN_SELECTOR_PUT(pos, WPA_CIPHER_SUITE_NONE);
		pos += WPA_SELECTOR_LEN;
	}

	return (pos - start) / RSN_SELECTOR_LEN;
}


int wpa_pick_pairwise_cipher(int ciphers, int none_allowed)
{
	if (ciphers & WPA_CIPHER_CCMP_256)
		return WPA_CIPHER_CCMP_256;
	if (ciphers & WPA_CIPHER_GCMP_256)
		return WPA_CIPHER_GCMP_256;
	if (ciphers & WPA_CIPHER_CCMP)
		return WPA_CIPHER_CCMP;
	if (ciphers & WPA_CIPHER_GCMP)
		return WPA_CIPHER_GCMP;
	if (ciphers & WPA_CIPHER_TKIP)
		return WPA_CIPHER_TKIP;
	if (none_allowed && (ciphers & WPA_CIPHER_NONE))
		return WPA_CIPHER_NONE;
	return -1;
}


int wpa_pick_group_cipher(int ciphers)
{
	if (ciphers & WPA_CIPHER_CCMP_256)
		return WPA_CIPHER_CCMP_256;
	if (ciphers & WPA_CIPHER_GCMP_256)
		return WPA_CIPHER_GCMP_256;
	if (ciphers & WPA_CIPHER_CCMP)
		return WPA_CIPHER_CCMP;
	if (ciphers & WPA_CIPHER_GCMP)
		return WPA_CIPHER_GCMP;
	if (ciphers & WPA_CIPHER_GTK_NOT_USED)
		return WPA_CIPHER_GTK_NOT_USED;
	if (ciphers & WPA_CIPHER_TKIP)
		return WPA_CIPHER_TKIP;
	return -1;
}


int wpa_parse_cipher(const char *value)
{
	int val = 0, last;
	char *start, *end, *buf;

	buf = os_strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (os_strcmp(start, "CCMP-256") == 0)
			val |= WPA_CIPHER_CCMP_256;
		else if (os_strcmp(start, "GCMP-256") == 0)
			val |= WPA_CIPHER_GCMP_256;
		else if (os_strcmp(start, "CCMP") == 0)
			val |= WPA_CIPHER_CCMP;
		else if (os_strcmp(start, "GCMP") == 0)
			val |= WPA_CIPHER_GCMP;
#ifndef CONFIG_NO_TKIP
		else if (os_strcmp(start, "TKIP") == 0)
			val |= WPA_CIPHER_TKIP;
#endif /* CONFIG_NO_TKIP */
#ifdef CONFIG_WEP
		else if (os_strcmp(start, "WEP104") == 0)
			val |= WPA_CIPHER_WEP104;
		else if (os_strcmp(start, "WEP40") == 0)
			val |= WPA_CIPHER_WEP40;
#endif /* CONFIG_WEP */
		else if (os_strcmp(start, "NONE") == 0)
			val |= WPA_CIPHER_NONE;
		else if (os_strcmp(start, "GTK_NOT_USED") == 0)
			val |= WPA_CIPHER_GTK_NOT_USED;
		else if (os_strcmp(start, "AES-128-CMAC") == 0)
			val |= WPA_CIPHER_AES_128_CMAC;
		else if (os_strcmp(start, "BIP-GMAC-128") == 0)
			val |= WPA_CIPHER_BIP_GMAC_128;
		else if (os_strcmp(start, "BIP-GMAC-256") == 0)
			val |= WPA_CIPHER_BIP_GMAC_256;
		else if (os_strcmp(start, "BIP-CMAC-256") == 0)
			val |= WPA_CIPHER_BIP_CMAC_256;
		else {
			os_free(buf);
			return -1;
		}

		if (last)
			break;
		start = end + 1;
	}
	os_free(buf);

	return val;
}


int wpa_write_ciphers(char *start, char *end, int ciphers, const char *delim)
{
	char *pos = start;
	int ret;

	if (ciphers & WPA_CIPHER_CCMP_256) {
		ret = os_snprintf(pos, end - pos, "%sCCMP-256",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}
	if (ciphers & WPA_CIPHER_GCMP_256) {
		ret = os_snprintf(pos, end - pos, "%sGCMP-256",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}
	if (ciphers & WPA_CIPHER_CCMP) {
		ret = os_snprintf(pos, end - pos, "%sCCMP",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}
	if (ciphers & WPA_CIPHER_GCMP) {
		ret = os_snprintf(pos, end - pos, "%sGCMP",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}
	if (ciphers & WPA_CIPHER_TKIP) {
		ret = os_snprintf(pos, end - pos, "%sTKIP",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}
	if (ciphers & WPA_CIPHER_AES_128_CMAC) {
		ret = os_snprintf(pos, end - pos, "%sAES-128-CMAC",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}
	if (ciphers & WPA_CIPHER_BIP_GMAC_128) {
		ret = os_snprintf(pos, end - pos, "%sBIP-GMAC-128",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}
	if (ciphers & WPA_CIPHER_BIP_GMAC_256) {
		ret = os_snprintf(pos, end - pos, "%sBIP-GMAC-256",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}
	if (ciphers & WPA_CIPHER_BIP_CMAC_256) {
		ret = os_snprintf(pos, end - pos, "%sBIP-CMAC-256",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}
	if (ciphers & WPA_CIPHER_NONE) {
		ret = os_snprintf(pos, end - pos, "%sNONE",
				  pos == start ? "" : delim);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}

	return pos - start;
}


int wpa_select_ap_group_cipher(int wpa, int wpa_pairwise, int rsn_pairwise)
{
	int pairwise = 0;

	/* Select group cipher based on the enabled pairwise cipher suites */
	if (wpa & 1)
		pairwise |= wpa_pairwise;
	if (wpa & 2)
		pairwise |= rsn_pairwise;

	if (pairwise & WPA_CIPHER_TKIP)
		return WPA_CIPHER_TKIP;
	if ((pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP)) == WPA_CIPHER_GCMP)
		return WPA_CIPHER_GCMP;
	if ((pairwise & (WPA_CIPHER_GCMP_256 | WPA_CIPHER_CCMP |
			 WPA_CIPHER_GCMP)) == WPA_CIPHER_GCMP_256)
		return WPA_CIPHER_GCMP_256;
	if ((pairwise & (WPA_CIPHER_CCMP_256 | WPA_CIPHER_CCMP |
			 WPA_CIPHER_GCMP)) == WPA_CIPHER_CCMP_256)
		return WPA_CIPHER_CCMP_256;
	return WPA_CIPHER_CCMP;
}


#ifdef CONFIG_FILS
int fils_domain_name_hash(const char *domain, u8 *hash)
{
	char buf[255], *wpos = buf;
	const char *pos = domain;
	size_t len;
	const u8 *addr[1];
	u8 mac[SHA256_MAC_LEN];

	for (len = 0; len < sizeof(buf) && *pos; len++) {
		if (isalpha(*pos) && isupper(*pos))
			*wpos++ = tolower(*pos);
		else
			*wpos++ = *pos;
		pos++;
	}

	addr[0] = (const u8 *) buf;
	if (sha256_vector(1, addr, &len, mac) < 0)
		return -1;
	os_memcpy(hash, mac, 2);
	return 0;
}
#endif /* CONFIG_FILS */


/**
 * wpa_parse_vendor_specific - Parse Vendor Specific IEs
 * @pos: Pointer to the IE header
 * @end: Pointer to the end of the Key Data buffer
 * @ie: Pointer to parsed IE data
 */
static void wpa_parse_vendor_specific(const u8 *pos, const u8 *end,
				      struct wpa_eapol_ie_parse *ie)
{
	unsigned int oui;

	if (pos[1] < 4) {
		wpa_printf(MSG_MSGDUMP,
			   "Too short vendor specific IE ignored (len=%u)",
			   pos[1]);
		return;
	}

	oui = WPA_GET_BE24(&pos[2]);
	if (oui == OUI_MICROSOFT && pos[5] == WMM_OUI_TYPE && pos[1] > 4) {
		if (pos[6] == WMM_OUI_SUBTYPE_INFORMATION_ELEMENT) {
			ie->wmm = &pos[2];
			ie->wmm_len = pos[1];
			wpa_hexdump(MSG_DEBUG, "WPA: WMM IE",
				    ie->wmm, ie->wmm_len);
		} else if (pos[6] == WMM_OUI_SUBTYPE_PARAMETER_ELEMENT) {
			ie->wmm = &pos[2];
			ie->wmm_len = pos[1];
			wpa_hexdump(MSG_DEBUG, "WPA: WMM Parameter Element",
				    ie->wmm, ie->wmm_len);
		}
	}
}


/**
 * wpa_parse_generic - Parse EAPOL-Key Key Data Generic IEs
 * @pos: Pointer to the IE header
 * @ie: Pointer to parsed IE data
 * Returns: 0 on success, 1 if end mark is found, 2 if KDE is not recognized
 */
static int wpa_parse_generic(const u8 *pos, struct wpa_eapol_ie_parse *ie)
{
	u8 len = pos[1];
	size_t dlen = 2 + len;
	u32 selector;
	const u8 *p;
	size_t left;
	u8 link_id;
	char title[50];
	int ret;

	if (len == 0)
		return 1;

	if (len < RSN_SELECTOR_LEN)
		return 2;

	p = pos + 2;
	selector = RSN_SELECTOR_GET(p);
	p += RSN_SELECTOR_LEN;
	left = len - RSN_SELECTOR_LEN;

	if (left >= 2 && selector == WPA_OUI_TYPE && p[0] == 1 && p[1] == 0) {
		ie->wpa_ie = pos;
		ie->wpa_ie_len = dlen;
		wpa_hexdump(MSG_DEBUG, "WPA: WPA IE in EAPOL-Key",
			    ie->wpa_ie, ie->wpa_ie_len);
		return 0;
	}

	if (selector == OSEN_IE_VENDOR_TYPE) {
		ie->osen = pos;
		ie->osen_len = dlen;
		return 0;
	}

	if (left >= PMKID_LEN && selector == RSN_KEY_DATA_PMKID) {
		ie->pmkid = p;
		wpa_hexdump(MSG_DEBUG, "WPA: PMKID in EAPOL-Key", pos, dlen);
		return 0;
	}

	if (left >= 2 && selector == RSN_KEY_DATA_KEYID) {
		ie->key_id = p;
		wpa_hexdump(MSG_DEBUG, "WPA: KeyID in EAPOL-Key", pos, dlen);
		return 0;
	}

	if (left > 2 && selector == RSN_KEY_DATA_GROUPKEY) {
		ie->gtk = p;
		ie->gtk_len = left;
		wpa_hexdump_key(MSG_DEBUG, "WPA: GTK in EAPOL-Key", pos, dlen);
		return 0;
	}

	if (left >= ETH_ALEN && selector == RSN_KEY_DATA_MAC_ADDR) {
		ie->mac_addr = p;
		wpa_printf(MSG_DEBUG, "WPA: MAC Address in EAPOL-Key: " MACSTR,
			   MAC2STR(ie->mac_addr));
		return 0;
	}

	if (left > 2 && selector == RSN_KEY_DATA_IGTK) {
		ie->igtk = p;
		ie->igtk_len = left;
		wpa_hexdump_key(MSG_DEBUG, "WPA: IGTK in EAPOL-Key",
				pos, dlen);
		return 0;
	}

	if (left > 2 && selector == RSN_KEY_DATA_BIGTK) {
		ie->bigtk = p;
		ie->bigtk_len = left;
		wpa_hexdump_key(MSG_DEBUG, "WPA: BIGTK in EAPOL-Key",
				pos, dlen);
		return 0;
	}

	if (left >= 1 && selector == WFA_KEY_DATA_IP_ADDR_REQ) {
		ie->ip_addr_req = p;
		wpa_hexdump(MSG_DEBUG, "WPA: IP Address Request in EAPOL-Key",
			    ie->ip_addr_req, left);
		return 0;
	}

	if (left >= 3 * 4 && selector == WFA_KEY_DATA_IP_ADDR_ALLOC) {
		ie->ip_addr_alloc = p;
		wpa_hexdump(MSG_DEBUG,
			    "WPA: IP Address Allocation in EAPOL-Key",
			    ie->ip_addr_alloc, left);
		return 0;
	}

	if (left > 2 && selector == RSN_KEY_DATA_OCI) {
		ie->oci = p;
		ie->oci_len = left;
		wpa_hexdump(MSG_DEBUG, "WPA: OCI KDE in EAPOL-Key",
			    pos, dlen);
		return 0;
	}

	if (left >= 1 && selector == WFA_KEY_DATA_TRANSITION_DISABLE) {
		ie->transition_disable = p;
		ie->transition_disable_len = left;
		wpa_hexdump(MSG_DEBUG,
			    "WPA: Transition Disable KDE in EAPOL-Key",
			    pos, dlen);
		return 0;
	}

	if (left >= 2 && selector == WFA_KEY_DATA_DPP) {
		ie->dpp_kde = p;
		ie->dpp_kde_len = left;
		wpa_hexdump(MSG_DEBUG, "WPA: DPP KDE in EAPOL-Key", pos, dlen);
		return 0;
	}

	if (left >= RSN_MLO_GTK_KDE_PREFIX_LENGTH &&
	    selector == RSN_KEY_DATA_MLO_GTK) {
		link_id = (p[0] & RSN_MLO_GTK_KDE_PREFIX0_LINK_ID_MASK) >>
			RSN_MLO_GTK_KDE_PREFIX0_LINK_ID_SHIFT;
		if (link_id >= MAX_NUM_MLD_LINKS)
			return 2;

		ie->valid_mlo_gtks |= BIT(link_id);
		ie->mlo_gtk[link_id] = p;
		ie->mlo_gtk_len[link_id] = left;
		ret = os_snprintf(title, sizeof(title),
				  "RSN: Link ID %u - MLO GTK KDE in EAPOL-Key",
				  link_id);
		if (!os_snprintf_error(sizeof(title), ret))
			wpa_hexdump_key(MSG_DEBUG, title, pos, dlen);
		return 0;
	}

	if (left >= RSN_MLO_IGTK_KDE_PREFIX_LENGTH &&
	    selector == RSN_KEY_DATA_MLO_IGTK) {
		link_id = (p[8] & RSN_MLO_IGTK_KDE_PREFIX8_LINK_ID_MASK) >>
			  RSN_MLO_IGTK_KDE_PREFIX8_LINK_ID_SHIFT;
		if (link_id >= MAX_NUM_MLD_LINKS)
			return 2;

		ie->valid_mlo_igtks |= BIT(link_id);
		ie->mlo_igtk[link_id] = p;
		ie->mlo_igtk_len[link_id] = left;
		ret = os_snprintf(title, sizeof(title),
				  "RSN: Link ID %u - MLO IGTK KDE in EAPOL-Key",
				  link_id);
		if (!os_snprintf_error(sizeof(title), ret))
			wpa_hexdump_key(MSG_DEBUG, title, pos, dlen);
		return 0;
	}

	if (left >= RSN_MLO_BIGTK_KDE_PREFIX_LENGTH &&
	    selector == RSN_KEY_DATA_MLO_BIGTK) {
		link_id = (p[8] & RSN_MLO_BIGTK_KDE_PREFIX8_LINK_ID_MASK) >>
			  RSN_MLO_BIGTK_KDE_PREFIX8_LINK_ID_SHIFT;
		if (link_id >= MAX_NUM_MLD_LINKS)
			return 2;

		ie->valid_mlo_bigtks |= BIT(link_id);
		ie->mlo_bigtk[link_id] = p;
		ie->mlo_bigtk_len[link_id] = left;
		ret = os_snprintf(title, sizeof(title),
				  "RSN: Link ID %u - MLO BIGTK KDE in EAPOL-Key",
				  link_id);
		if (!os_snprintf_error(sizeof(title), ret))
			wpa_hexdump_key(MSG_DEBUG, title, pos, dlen);
		return 0;
	}

	if (left >= RSN_MLO_LINK_KDE_FIXED_LENGTH &&
	    selector == RSN_KEY_DATA_MLO_LINK) {
		link_id = (p[0] & RSN_MLO_LINK_KDE_LI_LINK_ID_MASK) >>
			  RSN_MLO_LINK_KDE_LI_LINK_ID_SHIFT;
		if (link_id >= MAX_NUM_MLD_LINKS)
			return 2;

		ie->valid_mlo_links |= BIT(link_id);
		ie->mlo_link[link_id] = p;
		ie->mlo_link_len[link_id] = left;
		ret = os_snprintf(title, sizeof(title),
				  "RSN: Link ID %u - MLO Link KDE in EAPOL-Key",
				  link_id);
		if (!os_snprintf_error(sizeof(title), ret))
			wpa_hexdump(MSG_DEBUG, title, pos, dlen);
		return 0;
	}

	return 2;
}


/**
 * wpa_parse_kde_ies - Parse EAPOL-Key Key Data IEs
 * @buf: Pointer to the Key Data buffer
 * @len: Key Data Length
 * @ie: Pointer to parsed IE data
 * Returns: 0 on success, -1 on failure
 */
int wpa_parse_kde_ies(const u8 *buf, size_t len, struct wpa_eapol_ie_parse *ie)
{
	const u8 *pos, *end;
	int ret = 0;
	size_t dlen = 0;

	os_memset(ie, 0, sizeof(*ie));
	for (pos = buf, end = pos + len; end - pos > 1; pos += dlen) {
		if (pos[0] == 0xdd &&
		    ((pos == buf + len - 1) || pos[1] == 0)) {
			/* Ignore padding */
			break;
		}
		dlen = 2 + pos[1];
		if ((int) dlen > end - pos) {
			wpa_printf(MSG_DEBUG,
				   "WPA: EAPOL-Key Key Data underflow (ie=%d len=%d pos=%d)",
				   pos[0], pos[1], (int) (pos - buf));
			wpa_hexdump_key(MSG_DEBUG, "WPA: Key Data", buf, len);
			ret = -1;
			break;
		}
		if (*pos == WLAN_EID_RSN) {
			ie->rsn_ie = pos;
			ie->rsn_ie_len = dlen;
			wpa_hexdump(MSG_DEBUG, "WPA: RSN IE in EAPOL-Key",
				    ie->rsn_ie, ie->rsn_ie_len);
		} else if (*pos == WLAN_EID_RSNX) {
			ie->rsnxe = pos;
			ie->rsnxe_len = dlen;
			wpa_hexdump(MSG_DEBUG, "WPA: RSNXE in EAPOL-Key",
				    ie->rsnxe, ie->rsnxe_len);
		} else if (*pos == WLAN_EID_MOBILITY_DOMAIN) {
			ie->mdie = pos;
			ie->mdie_len = dlen;
			wpa_hexdump(MSG_DEBUG, "WPA: MDIE in EAPOL-Key",
				    ie->mdie, ie->mdie_len);
		} else if (*pos == WLAN_EID_FAST_BSS_TRANSITION) {
			ie->ftie = pos;
			ie->ftie_len = dlen;
			wpa_hexdump(MSG_DEBUG, "WPA: FTIE in EAPOL-Key",
				    ie->ftie, ie->ftie_len);
		} else if (*pos == WLAN_EID_TIMEOUT_INTERVAL && pos[1] >= 5) {
			if (pos[2] == WLAN_TIMEOUT_REASSOC_DEADLINE) {
				ie->reassoc_deadline = pos;
				wpa_hexdump(MSG_DEBUG, "WPA: Reassoc Deadline "
					    "in EAPOL-Key",
					    ie->reassoc_deadline, dlen);
			} else if (pos[2] == WLAN_TIMEOUT_KEY_LIFETIME) {
				ie->key_lifetime = pos;
				wpa_hexdump(MSG_DEBUG, "WPA: KeyLifetime "
					    "in EAPOL-Key",
					    ie->key_lifetime, dlen);
			} else {
				wpa_hexdump(MSG_DEBUG, "WPA: Unrecognized "
					    "EAPOL-Key Key Data IE",
					    pos, dlen);
			}
		} else if (*pos == WLAN_EID_LINK_ID) {
			if (pos[1] >= 18) {
				ie->lnkid = pos;
				ie->lnkid_len = dlen;
			}
		} else if (*pos == WLAN_EID_EXT_CAPAB) {
			ie->ext_capab = pos;
			ie->ext_capab_len = dlen;
		} else if (*pos == WLAN_EID_SUPP_RATES) {
			ie->supp_rates = pos;
			ie->supp_rates_len = dlen;
		} else if (*pos == WLAN_EID_EXT_SUPP_RATES) {
			ie->ext_supp_rates = pos;
			ie->ext_supp_rates_len = dlen;
		} else if (*pos == WLAN_EID_HT_CAP &&
			   pos[1] >= sizeof(struct ieee80211_ht_capabilities)) {
			ie->ht_capabilities = pos + 2;
		} else if (*pos == WLAN_EID_AID) {
			if (pos[1] >= 2)
				ie->aid = WPA_GET_LE16(pos + 2) & 0x3fff;
		} else if (*pos == WLAN_EID_VHT_CAP &&
			   pos[1] >= sizeof(struct ieee80211_vht_capabilities))
		{
			ie->vht_capabilities = pos + 2;
		} else if (*pos == WLAN_EID_EXTENSION &&
			   pos[1] >= 1 + IEEE80211_HE_CAPAB_MIN_LEN &&
			   pos[2] == WLAN_EID_EXT_HE_CAPABILITIES) {
			ie->he_capabilities = pos + 3;
			ie->he_capab_len = pos[1] - 1;
		} else if (*pos == WLAN_EID_EXTENSION &&
			   pos[1] >= 1 +
			   sizeof(struct ieee80211_he_6ghz_band_cap) &&
			   pos[2] == WLAN_EID_EXT_HE_6GHZ_BAND_CAP) {
			ie->he_6ghz_capabilities = pos + 3;
		} else if (*pos == WLAN_EID_EXTENSION &&
			   pos[1] >= 1 + IEEE80211_EHT_CAPAB_MIN_LEN &&
			   pos[2] == WLAN_EID_EXT_EHT_CAPABILITIES) {
			ie->eht_capabilities = pos + 3;
			ie->eht_capab_len = pos[1] - 1;
		} else if (*pos == WLAN_EID_QOS && pos[1] >= 1) {
			ie->qosinfo = pos[2];
		} else if (*pos == WLAN_EID_SUPPORTED_CHANNELS) {
			ie->supp_channels = pos + 2;
			ie->supp_channels_len = pos[1];
		} else if (*pos == WLAN_EID_SUPPORTED_OPERATING_CLASSES) {
			/*
			 * The value of the Length field of the Supported
			 * Operating Classes element is between 2 and 253.
			 * Silently skip invalid elements to avoid interop
			 * issues when trying to use the value.
			 */
			if (pos[1] >= 2 && pos[1] <= 253) {
				ie->supp_oper_classes = pos + 2;
				ie->supp_oper_classes_len = pos[1];
			}
		} else if (*pos == WLAN_EID_SSID) {
			ie->ssid = pos + 2;
			ie->ssid_len = pos[1];
			wpa_hexdump_ascii(MSG_DEBUG, "RSN: SSID in EAPOL-Key",
					  ie->ssid, ie->ssid_len);
		} else if (*pos == WLAN_EID_VENDOR_SPECIFIC) {
			ret = wpa_parse_generic(pos, ie);
			if (ret == 1) {
				/* end mark found */
				ret = 0;
				break;
			}

			if (ret == 2) {
				/* not a known KDE */
				wpa_parse_vendor_specific(pos, end, ie);
			}

			ret = 0;
		} else {
			wpa_hexdump(MSG_DEBUG,
				    "WPA: Unrecognized EAPOL-Key Key Data IE",
				    pos, dlen);
		}
	}

	return ret;
}


#ifdef CONFIG_PASN

/*
 * wpa_pasn_build_auth_header - Add the MAC header and initialize Authentication
 * frame for PASN
 *
 * @buf: Buffer in which the header will be added
 * @bssid: The BSSID of the AP
 * @src: Source address
 * @dst: Destination address
 * @trans_seq: Authentication transaction sequence number
 * @status: Authentication status
 */
void wpa_pasn_build_auth_header(struct wpabuf *buf, const u8 *bssid,
				const u8 *src, const u8 *dst,
				u8 trans_seq, u16 status)
{
	struct ieee80211_mgmt *auth;

	wpa_printf(MSG_DEBUG, "PASN: Add authentication header. trans_seq=%u",
		   trans_seq);

	auth = wpabuf_put(buf, offsetof(struct ieee80211_mgmt,
					u.auth.variable));

	auth->frame_control = host_to_le16((WLAN_FC_TYPE_MGMT << 2) |
					   (WLAN_FC_STYPE_AUTH << 4));

	os_memcpy(auth->da, dst, ETH_ALEN);
	os_memcpy(auth->sa, src, ETH_ALEN);
	os_memcpy(auth->bssid, bssid, ETH_ALEN);
	auth->seq_ctrl = 0;

	auth->u.auth.auth_alg = host_to_le16(WLAN_AUTH_PASN);
	auth->u.auth.auth_transaction = host_to_le16(trans_seq);
	auth->u.auth.status_code = host_to_le16(status);
}


/*
 * wpa_pasn_add_rsne - Add an RSNE for PASN authentication
 * @buf: Buffer in which the IE will be added
 * @pmkid: Optional PMKID. Can be NULL.
 * @akmp: Authentication and key management protocol
 * @cipher: The cipher suite
 */
int wpa_pasn_add_rsne(struct wpabuf *buf, const u8 *pmkid, int akmp, int cipher)
{
	struct rsn_ie_hdr *hdr;
	u32 suite;
	u16 capab;
	u8 *pos;
	u8 rsne_len;

	wpa_printf(MSG_DEBUG, "PASN: Add RSNE");

	rsne_len = sizeof(*hdr) + RSN_SELECTOR_LEN +
		2 + RSN_SELECTOR_LEN + 2 + RSN_SELECTOR_LEN +
		2 + RSN_SELECTOR_LEN + 2 + (pmkid ? PMKID_LEN : 0);

	if (wpabuf_tailroom(buf) < rsne_len)
		return -1;
	hdr = wpabuf_put(buf, rsne_len);
	hdr->elem_id = WLAN_EID_RSN;
	hdr->len = rsne_len - 2;
	WPA_PUT_LE16(hdr->version, RSN_VERSION);
	pos = (u8 *) (hdr + 1);

	/* Group addressed data is not allowed */
	RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED);
	pos += RSN_SELECTOR_LEN;

	/* Add the pairwise cipher */
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, cipher);
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	/* Add the AKM suite */
	WPA_PUT_LE16(pos, 1);
	pos += 2;

	switch (akmp) {
	case WPA_KEY_MGMT_PASN:
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_PASN);
		break;
#ifdef CONFIG_SAE
	case WPA_KEY_MGMT_SAE:
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_SAE);
		break;
	case WPA_KEY_MGMT_SAE_EXT_KEY:
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_SAE_EXT_KEY);
		break;
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	case WPA_KEY_MGMT_FILS_SHA256:
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FILS_SHA256);
		break;
	case WPA_KEY_MGMT_FILS_SHA384:
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FILS_SHA384);
		break;
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211R
	case WPA_KEY_MGMT_FT_PSK:
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_PSK);
		break;
	case WPA_KEY_MGMT_FT_IEEE8021X:
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X);
		break;
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384);
		break;
#endif /* CONFIG_IEEE80211R */
	default:
		wpa_printf(MSG_ERROR, "PASN: Invalid AKMP=0x%x", akmp);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	/* RSN Capabilities: PASN mandates both MFP capable and required */
	capab = WPA_CAPABILITY_MFPC | WPA_CAPABILITY_MFPR;
	WPA_PUT_LE16(pos, capab);
	pos += 2;

	if (pmkid) {
		wpa_printf(MSG_DEBUG, "PASN: Adding PMKID");

		WPA_PUT_LE16(pos, 1);
		pos += 2;
		os_memcpy(pos, pmkid, PMKID_LEN);
		pos += PMKID_LEN;
	} else {
		WPA_PUT_LE16(pos, 0);
		pos += 2;
	}

	/* Group addressed management is not allowed */
	RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED);

	return 0;
}


/*
 * wpa_pasn_add_parameter_ie - Add PASN Parameters IE for PASN authentication
 * @buf: Buffer in which the IE will be added
 * @pasn_group: Finite Cyclic Group ID for PASN authentication
 * @wrapped_data_format: Format of the data in the Wrapped Data IE
 * @pubkey: A buffer holding the local public key. Can be NULL
 * @compressed: In case pubkey is included, indicates if the public key is
 *     compressed (only x coordinate is included) or not (both x and y
 *     coordinates are included)
 * @comeback: A buffer holding the comeback token. Can be NULL
 * @after: If comeback is set, defined the comeback time in seconds. -1 to not
 *	include the Comeback After field (frames from non-AP STA).
 */
void wpa_pasn_add_parameter_ie(struct wpabuf *buf, u16 pasn_group,
			       u8 wrapped_data_format,
			       const struct wpabuf *pubkey, bool compressed,
			       const struct wpabuf *comeback, int after)
{
	struct pasn_parameter_ie *params;

	wpa_printf(MSG_DEBUG, "PASN: Add PASN Parameters element");

	params = wpabuf_put(buf, sizeof(*params));

	params->id = WLAN_EID_EXTENSION;
	params->len = sizeof(*params) - 2;
	params->id_ext = WLAN_EID_EXT_PASN_PARAMS;
	params->control = 0;
	params->wrapped_data_format = wrapped_data_format;

	if (comeback) {
		wpa_printf(MSG_DEBUG, "PASN: Adding comeback data");

		/*
		 * 2 octets for the 'after' field + 1 octet for the length +
		 * actual cookie data
		 */
		if (after >= 0)
			params->len += 2;
		params->len += 1 + wpabuf_len(comeback);
		params->control |= WPA_PASN_CTRL_COMEBACK_INFO_PRESENT;

		if (after >= 0)
			wpabuf_put_le16(buf, after);
		wpabuf_put_u8(buf, wpabuf_len(comeback));
		wpabuf_put_buf(buf, comeback);
	}

	if (pubkey) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Adding public key and group ID %u",
			   pasn_group);

		/*
		 * 2 octets for the finite cyclic group + 2 octets public key
		 * length + 1 octet for the compressed/uncompressed indication +
		 * the actual key.
		 */
		params->len += 2 + 1 + 1 + wpabuf_len(pubkey);
		params->control |= WPA_PASN_CTRL_GROUP_AND_KEY_PRESENT;

		wpabuf_put_le16(buf, pasn_group);

		/*
		 * The first octet indicates whether the public key is
		 * compressed, as defined in RFC 5480 section 2.2.
		 */
		wpabuf_put_u8(buf, wpabuf_len(pubkey) + 1);
		wpabuf_put_u8(buf, compressed ? WPA_PASN_PUBKEY_COMPRESSED_0 :
			      WPA_PASN_PUBKEY_UNCOMPRESSED);

		wpabuf_put_buf(buf, pubkey);
	}
}

/*
 * wpa_pasn_add_wrapped_data - Add a Wrapped Data IE to PASN Authentication
 * frame. If needed, the Wrapped Data IE would be fragmented.
 *
 * @buf: Buffer in which the IE will be added
 * @wrapped_data_buf: Buffer holding the wrapped data
 */
int wpa_pasn_add_wrapped_data(struct wpabuf *buf,
			      struct wpabuf *wrapped_data_buf)
{
	const u8 *data;
	size_t data_len;
	u8 len;

	if (!wrapped_data_buf)
		return 0;

	wpa_printf(MSG_DEBUG, "PASN: Add wrapped data");

	data = wpabuf_head_u8(wrapped_data_buf);
	data_len = wpabuf_len(wrapped_data_buf);

	/* nothing to add */
	if (!data_len)
		return 0;

	if (data_len <= 254)
		len = 1 + data_len;
	else
		len = 255;

	if (wpabuf_tailroom(buf) < 3 + data_len)
		return -1;

	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	wpabuf_put_u8(buf, len);
	wpabuf_put_u8(buf, WLAN_EID_EXT_WRAPPED_DATA);
	wpabuf_put_data(buf, data, len - 1);

	data += len - 1;
	data_len -= len - 1;

	while (data_len) {
		if (wpabuf_tailroom(buf) < 1 + data_len)
			return -1;
		wpabuf_put_u8(buf, WLAN_EID_FRAGMENT);
		len = data_len > 255 ? 255 : data_len;
		wpabuf_put_u8(buf, len);
		wpabuf_put_data(buf, data, len);
		data += len;
		data_len -= len;
	}

	return 0;
}


/*
 * wpa_pasn_validate_rsne - Validate PSAN specific data of RSNE
 * @data: Parsed representation of an RSNE
 * Returns -1 for invalid data; otherwise 0
 */
int wpa_pasn_validate_rsne(const struct wpa_ie_data *data)
{
	u16 capab = WPA_CAPABILITY_MFPC | WPA_CAPABILITY_MFPR;

	if (data->proto != WPA_PROTO_RSN)
		return -1;

	if ((data->capabilities & capab) != capab) {
		wpa_printf(MSG_DEBUG, "PASN: Invalid RSNE capabilities");
		return -1;
	}

	if (!data->has_group || data->group_cipher != WPA_CIPHER_GTK_NOT_USED) {
		wpa_printf(MSG_DEBUG, "PASN: Invalid group data cipher");
		return -1;
	}

	if (!data->has_pairwise || !data->pairwise_cipher ||
	    (data->pairwise_cipher & (data->pairwise_cipher - 1))) {
		wpa_printf(MSG_DEBUG, "PASN: No valid pairwise suite");
		return -1;
	}

	switch (data->key_mgmt) {
#ifdef CONFIG_SAE
	case WPA_KEY_MGMT_SAE:
	case WPA_KEY_MGMT_SAE_EXT_KEY:
	/* fall through */
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	case WPA_KEY_MGMT_FILS_SHA256:
	case WPA_KEY_MGMT_FILS_SHA384:
	/* fall through */
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211R
	case WPA_KEY_MGMT_FT_PSK:
	case WPA_KEY_MGMT_FT_IEEE8021X:
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
	/* fall through */
#endif /* CONFIG_IEEE80211R */
	case WPA_KEY_MGMT_PASN:
		break;
	default:
		wpa_printf(MSG_ERROR, "PASN: invalid key_mgmt: 0x%0x",
			   data->key_mgmt);
		return -1;
	}

	if (data->mgmt_group_cipher != WPA_CIPHER_GTK_NOT_USED) {
		wpa_printf(MSG_DEBUG, "PASN: Invalid group mgmt cipher");
		return -1;
	}

	if (data->num_pmkid > 1) {
		wpa_printf(MSG_DEBUG, "PASN: Invalid number of PMKIDs");
		return -1;
	}

	return 0;
}


/*
 * wpa_pasn_parse_parameter_ie - Validates PASN Parameters IE
 * @data: Pointer to the PASN Parameters IE (starting with the EID).
 * @len: Length of the data in the PASN Parameters IE
 * @from_ap: Whether this was received from an AP
 * @pasn_params: On successful return would hold the parsed PASN parameters.
 * Returns: -1 for invalid data; otherwise 0
 *
 * Note: On successful return, the pointers in &pasn_params point to the data in
 * the IE and are not locally allocated (so they should not be freed etc.).
 */
int wpa_pasn_parse_parameter_ie(const u8 *data, u8 len, bool from_ap,
				struct wpa_pasn_params_data *pasn_params)
{
	struct pasn_parameter_ie *params = (struct pasn_parameter_ie *) data;
	const u8 *pos = (const u8 *) (params + 1);

	if (!pasn_params) {
		wpa_printf(MSG_DEBUG, "PASN: Invalid params");
		return -1;
	}

	if (!params || ((size_t) (params->len + 2) < sizeof(*params)) ||
	    len < sizeof(*params) || params->len + 2 != len) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Invalid parameters IE. len=(%u, %u)",
			   params ? params->len : 0, len);
		return -1;
	}

	os_memset(pasn_params, 0, sizeof(*pasn_params));

	switch (params->wrapped_data_format) {
	case WPA_PASN_WRAPPED_DATA_NO:
	case WPA_PASN_WRAPPED_DATA_SAE:
	case WPA_PASN_WRAPPED_DATA_FILS_SK:
	case WPA_PASN_WRAPPED_DATA_FT:
		break;
	default:
		wpa_printf(MSG_DEBUG, "PASN: Invalid wrapped data format");
		return -1;
	}

	pasn_params->wrapped_data_format = params->wrapped_data_format;

	len -= sizeof(*params);

	if (params->control & WPA_PASN_CTRL_COMEBACK_INFO_PRESENT) {
		if (from_ap) {
			if (len < 2) {
				wpa_printf(MSG_DEBUG,
					   "PASN: Invalid Parameters IE: Truncated Comeback After");
				return -1;
			}
			pasn_params->after = WPA_GET_LE16(pos);
			pos += 2;
			len -= 2;
		}

		if (len < 1 || len < 1 + *pos) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Invalid Parameters IE: comeback len");
			return -1;
		}

		pasn_params->comeback_len = *pos++;
		len--;
		pasn_params->comeback = pos;
		len -=  pasn_params->comeback_len;
		pos += pasn_params->comeback_len;
	}

	if (params->control & WPA_PASN_CTRL_GROUP_AND_KEY_PRESENT) {
		if (len < 3 || len < 3 + pos[2]) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Invalid Parameters IE: group and key");
			return -1;
		}

		pasn_params->group = WPA_GET_LE16(pos);
		pos += 2;
		len -= 2;
		pasn_params->pubkey_len = *pos++;
		len--;
		pasn_params->pubkey = pos;
		len -= pasn_params->pubkey_len;
		pos += pasn_params->pubkey_len;
	}

	if (len) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Invalid Parameters IE. Bytes left=%u", len);
		return -1;
	}

	return 0;
}


void wpa_pasn_add_rsnxe(struct wpabuf *buf, u16 capab)
{
	size_t flen;

	flen = (capab & 0xff00) ? 2 : 1;
	if (!capab)
		return; /* no supported extended RSN capabilities */
	if (wpabuf_tailroom(buf) < 2 + flen)
		return;
	capab |= flen - 1; /* bit 0-3 = Field length (n - 1) */

	wpabuf_put_u8(buf, WLAN_EID_RSNX);
	wpabuf_put_u8(buf, flen);
	wpabuf_put_u8(buf, capab & 0x00ff);
	capab >>= 8;
	if (capab)
		wpabuf_put_u8(buf, capab);
}


/*
 * wpa_pasn_add_extra_ies - Add protocol specific IEs in Authentication
 * frame for PASN.
 *
 * @buf: Buffer in which the elements will be added
 * @extra_ies: Protocol specific elements to add
 * @len: Length of the elements
 * Returns: 0 on success, -1 on failure
 */

int wpa_pasn_add_extra_ies(struct wpabuf *buf, const u8 *extra_ies, size_t len)
{
	if (!len || !extra_ies || !buf)
		return 0;

	if (wpabuf_tailroom(buf) < sizeof(len))
		return -1;

	wpabuf_put_data(buf, extra_ies, len);
	return 0;
}

#endif /* CONFIG_PASN */
