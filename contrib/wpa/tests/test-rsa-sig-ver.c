/*
 * Testing tool for RSA PKCS #1 v1.5 signature verification
 * Copyright (c) 2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "crypto/crypto.h"
#include "tls/rsa.h"
#include "tls/asn1.h"
#include "tls/pkcs1.h"


static int cavp_rsa_sig_ver(const char *fname)
{
	FILE *f;
	int ret = 0;
	char buf[15000], *pos, *pos2;
	u8 msg[200], n[512], s[512], em[512], e[512];
	size_t msg_len = 0, n_len = 0, s_len = 0, em_len, e_len = 0;
	size_t tmp_len;
	char sha_alg[20];
	int ok = 0;

	printf("CAVP RSA SigVer test vectors from %s\n", fname);

	f = fopen(fname, "r");
	if (f == NULL) {
		printf("%s does not exist - cannot validate CAVP RSA SigVer test vectors\n",
			fname);
		return 0;
	}

	while (fgets(buf, sizeof(buf), f)) {
		pos = os_strchr(buf, '=');
		if (pos == NULL)
			continue;
		pos2 = pos - 1;
		while (pos2 >= buf && *pos2 == ' ')
			*pos2-- = '\0';
		*pos++ = '\0';
		while (*pos == ' ')
			*pos++ = '\0';
		pos2 = os_strchr(pos, '\r');
		if (!pos2)
			pos2 = os_strchr(pos, '\n');
		if (pos2)
			*pos2 = '\0';
		else
			pos2 = pos + os_strlen(pos);

		if (os_strcmp(buf, "SHAAlg") == 0) {
			os_strlcpy(sha_alg, pos, sizeof(sha_alg));
		} else if (os_strcmp(buf, "Msg") == 0) {
			tmp_len = os_strlen(pos);
			if (tmp_len > sizeof(msg) * 2) {
				printf("Too long Msg\n");
				fclose(f);
				return -1;
			}
			msg_len = tmp_len / 2;
			if (hexstr2bin(pos, msg, msg_len) < 0) {
				printf("Invalid hex string '%s'\n", pos);
				ret++;
				break;
			}
		} else if (os_strcmp(buf, "n") == 0) {
			tmp_len = os_strlen(pos);
			if (tmp_len > sizeof(n) * 2) {
				printf("Too long n\n");
				fclose(f);
				return -1;
			}
			n_len = tmp_len / 2;
			if (hexstr2bin(pos, n, n_len) < 0) {
				printf("Invalid hex string '%s'\n", pos);
				ret++;
				break;
			}
		} else if (os_strcmp(buf, "e") == 0) {
			tmp_len = os_strlen(pos);
			if (tmp_len > sizeof(e) * 2) {
				printf("Too long e\n");
				fclose(f);
				return -1;
			}
			e_len = tmp_len / 2;
			if (hexstr2bin(pos, e, e_len) < 0) {
				printf("Invalid hex string '%s'\n", pos);
				ret++;
				break;
			}
		} else if (os_strcmp(buf, "S") == 0) {
			tmp_len = os_strlen(pos);
			if (tmp_len > sizeof(s) * 2) {
				printf("Too long S\n");
				fclose(f);
				return -1;
			}
			s_len = tmp_len / 2;
			if (hexstr2bin(pos, s, s_len) < 0) {
				printf("Invalid hex string '%s'\n", pos);
				ret++;
				break;
			}
		} else if (os_strncmp(buf, "EM", 2) == 0) {
			tmp_len = os_strlen(pos);
			if (tmp_len > sizeof(em) * 2) {
				fclose(f);
				return -1;
			}
			em_len = tmp_len / 2;
			if (hexstr2bin(pos, em, em_len) < 0) {
				printf("Invalid hex string '%s'\n", pos);
				ret++;
				break;
			}
		} else if (os_strcmp(buf, "Result") == 0) {
			const u8 *addr[1];
			size_t len[1];
			struct crypto_public_key *pk;
			int res;
			u8 hash[32];
			size_t hash_len;
			const struct asn1_oid *alg;

			addr[0] = msg;
			len[0] = msg_len;
			if (os_strcmp(sha_alg, "SHA1") == 0) {
				if (sha1_vector(1, addr, len, hash) < 0) {
					fclose(f);
					return -1;
				}
				hash_len = 20;
				alg = &asn1_sha1_oid;
			} else if (os_strcmp(sha_alg, "SHA256") == 0) {
				if (sha256_vector(1, addr, len, hash) < 0) {
					fclose(f);
					return -1;
				}
				hash_len = 32;
				alg = &asn1_sha256_oid;
			} else {
				continue;
			}

			printf("\nExpected result: %s\n", pos);
			wpa_hexdump(MSG_INFO, "Hash(Msg)", hash, hash_len);

			pk = crypto_public_key_import_parts(n, n_len,
							    e, e_len);
			if (pk == NULL) {
				printf("Failed to import public key\n");
				ret++;
				continue;
			}

			res = pkcs1_v15_sig_ver(pk, s, s_len, alg,
						hash, hash_len);
			crypto_public_key_free(pk);
			if ((*pos == 'F' && !res) || (*pos != 'F' && res)) {
				printf("FAIL\n");
				ret++;
				continue;
			}

			printf("PASS\n");
			ok++;
		}
	}

	fclose(f);

	if (ret)
		printf("Test case failed\n");
	else
		printf("%d test vectors OK\n", ok);

	return ret;
}


int main(int argc, char *argv[])
{
	int ret = 0;
	int i;

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	for (i = 1; i < argc; i++) {
		if (cavp_rsa_sig_ver(argv[i]))
			ret++;
	}

	if (argc < 2 && cavp_rsa_sig_ver("CAVP/SigVer15_186-3.rsp"))
		ret++;
	if (argc < 2 && cavp_rsa_sig_ver("CAVP/SigVer15EMTest.txt"))
		ret++;

	return ret;
}
