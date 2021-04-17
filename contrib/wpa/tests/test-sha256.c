/*
 * Test program for SHA256
 * Copyright (c) 2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto/crypto.h"


static int cavp_shavs(const char *fname)
{
	FILE *f;
	int ret = 0;
	char buf[15000], *pos, *pos2;
	u8 msg[6400];
	int msg_len = 0, tmp_len;
	u8 md[32], hash[32];
	int ok = 0;

	printf("CAVP SHAVS test vectors from %s\n", fname);

	f = fopen(fname, "r");
	if (f == NULL) {
		printf("%s does not exist - cannot validate CAVP SHAVS test vectors\n",
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

		if (os_strcmp(buf, "Len") == 0) {
			msg_len = atoi(pos);
		} else if (os_strcmp(buf, "Msg") == 0) {
			tmp_len = os_strlen(pos);
			if (msg_len == 0 && tmp_len == 2)
				tmp_len = 0;
			if (msg_len != tmp_len * 4) {
				printf("Unexpected Msg length (msg_len=%u tmp_len=%u, Msg='%s'\n",
				       msg_len, tmp_len, pos);
				ret++;
				break;
			}

			if (hexstr2bin(pos, msg, msg_len / 8) < 0) {
				printf("Invalid hex string '%s'\n", pos);
				ret++;
				break;
			}
		} else if (os_strcmp(buf, "MD") == 0) {
			const u8 *addr[1];
			size_t len[1];

			tmp_len = os_strlen(pos);
			if (tmp_len != 2 * 32) {
				printf("Unexpected MD length (MD='%s'\n",
				       pos);
				ret++;
				break;
			}

			if (hexstr2bin(pos, md, 32) < 0) {
				printf("Invalid hex string '%s'\n", pos);
				ret++;
				break;
			}

			addr[0] = msg;
			len[0] = msg_len / 8;
			if (sha256_vector(1, addr, len, hash) < 0 ||
			    os_memcmp(hash, md, 32) != 0)
				ret++;
			else
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
	int errors = 0;

	if (cavp_shavs("CAVP/SHA256ShortMsg.rsp"))
		errors++;
	if (cavp_shavs("CAVP/SHA256LongMsg.rsp"))
		errors++;

	return errors;
}
