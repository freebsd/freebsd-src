/* $OpenBSD: hostfile.c,v 1.50 2010/12/04 13:31:37 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for manipulating the known hosts files.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * Copyright (c) 1999, 2000 Markus Friedl.  All rights reserved.
 * Copyright (c) 1999 Niels Provos.  All rights reserved.
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

#include "includes.h"

#include <sys/types.h>

#include <netinet/in.h>

#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <resolv.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmalloc.h"
#include "match.h"
#include "key.h"
#include "hostfile.h"
#include "log.h"
#include "misc.h"

struct hostkeys {
	struct hostkey_entry *entries;
	u_int num_entries;
};

static int
extract_salt(const char *s, u_int l, char *salt, size_t salt_len)
{
	char *p, *b64salt;
	u_int b64len;
	int ret;

	if (l < sizeof(HASH_MAGIC) - 1) {
		debug2("extract_salt: string too short");
		return (-1);
	}
	if (strncmp(s, HASH_MAGIC, sizeof(HASH_MAGIC) - 1) != 0) {
		debug2("extract_salt: invalid magic identifier");
		return (-1);
	}
	s += sizeof(HASH_MAGIC) - 1;
	l -= sizeof(HASH_MAGIC) - 1;
	if ((p = memchr(s, HASH_DELIM, l)) == NULL) {
		debug2("extract_salt: missing salt termination character");
		return (-1);
	}

	b64len = p - s;
	/* Sanity check */
	if (b64len == 0 || b64len > 1024) {
		debug2("extract_salt: bad encoded salt length %u", b64len);
		return (-1);
	}
	b64salt = xmalloc(1 + b64len);
	memcpy(b64salt, s, b64len);
	b64salt[b64len] = '\0';

	ret = __b64_pton(b64salt, salt, salt_len);
	xfree(b64salt);
	if (ret == -1) {
		debug2("extract_salt: salt decode error");
		return (-1);
	}
	if (ret != SHA_DIGEST_LENGTH) {
		debug2("extract_salt: expected salt len %d, got %d",
		    SHA_DIGEST_LENGTH, ret);
		return (-1);
	}

	return (0);
}

char *
host_hash(const char *host, const char *name_from_hostfile, u_int src_len)
{
	const EVP_MD *md = EVP_sha1();
	HMAC_CTX mac_ctx;
	char salt[256], result[256], uu_salt[512], uu_result[512];
	static char encoded[1024];
	u_int i, len;

	len = EVP_MD_size(md);

	if (name_from_hostfile == NULL) {
		/* Create new salt */
		for (i = 0; i < len; i++)
			salt[i] = arc4random();
	} else {
		/* Extract salt from known host entry */
		if (extract_salt(name_from_hostfile, src_len, salt,
		    sizeof(salt)) == -1)
			return (NULL);
	}

	HMAC_Init(&mac_ctx, salt, len, md);
	HMAC_Update(&mac_ctx, host, strlen(host));
	HMAC_Final(&mac_ctx, result, NULL);
	HMAC_cleanup(&mac_ctx);

	if (__b64_ntop(salt, len, uu_salt, sizeof(uu_salt)) == -1 ||
	    __b64_ntop(result, len, uu_result, sizeof(uu_result)) == -1)
		fatal("host_hash: __b64_ntop failed");

	snprintf(encoded, sizeof(encoded), "%s%s%c%s", HASH_MAGIC, uu_salt,
	    HASH_DELIM, uu_result);

	return (encoded);
}

/*
 * Parses an RSA (number of bits, e, n) or DSA key from a string.  Moves the
 * pointer over the key.  Skips any whitespace at the beginning and at end.
 */

int
hostfile_read_key(char **cpp, u_int *bitsp, Key *ret)
{
	char *cp;

	/* Skip leading whitespace. */
	for (cp = *cpp; *cp == ' ' || *cp == '\t'; cp++)
		;

	if (key_read(ret, &cp) != 1)
		return 0;

	/* Skip trailing whitespace. */
	for (; *cp == ' ' || *cp == '\t'; cp++)
		;

	/* Return results. */
	*cpp = cp;
	if (bitsp != NULL)
		*bitsp = key_size(ret);
	return 1;
}

static int
hostfile_check_key(int bits, const Key *key, const char *host,
    const char *filename, u_long linenum)
{
	if (key == NULL || key->type != KEY_RSA1 || key->rsa == NULL)
		return 1;
	if (bits != BN_num_bits(key->rsa->n)) {
		logit("Warning: %s, line %lu: keysize mismatch for host %s: "
		    "actual %d vs. announced %d.",
		    filename, linenum, host, BN_num_bits(key->rsa->n), bits);
		logit("Warning: replace %d with %d in %s, line %lu.",
		    bits, BN_num_bits(key->rsa->n), filename, linenum);
	}
	return 1;
}

static HostkeyMarker
check_markers(char **cpp)
{
	char marker[32], *sp, *cp = *cpp;
	int ret = MRK_NONE;

	while (*cp == '@') {
		/* Only one marker is allowed */
		if (ret != MRK_NONE)
			return MRK_ERROR;
		/* Markers are terminated by whitespace */
		if ((sp = strchr(cp, ' ')) == NULL &&
		    (sp = strchr(cp, '\t')) == NULL)
			return MRK_ERROR;
		/* Extract marker for comparison */
		if (sp <= cp + 1 || sp >= cp + sizeof(marker))
			return MRK_ERROR;
		memcpy(marker, cp, sp - cp);
		marker[sp - cp] = '\0';
		if (strcmp(marker, CA_MARKER) == 0)
			ret = MRK_CA;
		else if (strcmp(marker, REVOKE_MARKER) == 0)
			ret = MRK_REVOKE;
		else
			return MRK_ERROR;

		/* Skip past marker and any whitespace that follows it */
		cp = sp;
		for (; *cp == ' ' || *cp == '\t'; cp++)
			;
	}
	*cpp = cp;
	return ret;
}

struct hostkeys *
init_hostkeys(void)
{
	struct hostkeys *ret = xcalloc(1, sizeof(*ret));

	ret->entries = NULL;
	return ret;
}

void
load_hostkeys(struct hostkeys *hostkeys, const char *host, const char *path)
{
	FILE *f;
	char line[8192];
	u_long linenum = 0, num_loaded = 0;
	char *cp, *cp2, *hashed_host;
	HostkeyMarker marker;
	Key *key;
	int kbits;

	if ((f = fopen(path, "r")) == NULL)
		return;
	debug3("%s: loading entries for host \"%.100s\" from file \"%s\"",
	    __func__, host, path);
	while (read_keyfile_line(f, path, line, sizeof(line), &linenum) == 0) {
		cp = line;

		/* Skip any leading whitespace, comments and empty lines. */
		for (; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '#' || *cp == '\n')
			continue;

		if ((marker = check_markers(&cp)) == MRK_ERROR) {
			verbose("%s: invalid marker at %s:%lu",
			    __func__, path, linenum);
			continue;
		}

		/* Find the end of the host name portion. */
		for (cp2 = cp; *cp2 && *cp2 != ' ' && *cp2 != '\t'; cp2++)
			;

		/* Check if the host name matches. */
		if (match_hostname(host, cp, (u_int) (cp2 - cp)) != 1) {
			if (*cp != HASH_DELIM)
				continue;
			hashed_host = host_hash(host, cp, (u_int) (cp2 - cp));
			if (hashed_host == NULL) {
				debug("Invalid hashed host line %lu of %s",
				    linenum, path);
				continue;
			}
			if (strncmp(hashed_host, cp, (u_int) (cp2 - cp)) != 0)
				continue;
		}

		/* Got a match.  Skip host name. */
		cp = cp2;

		/*
		 * Extract the key from the line.  This will skip any leading
		 * whitespace.  Ignore badly formatted lines.
		 */
		key = key_new(KEY_UNSPEC);
		if (!hostfile_read_key(&cp, &kbits, key)) {
			key_free(key);
			key = key_new(KEY_RSA1);
			if (!hostfile_read_key(&cp, &kbits, key)) {
				key_free(key);
				continue;
			}
		}
		if (!hostfile_check_key(kbits, key, host, path, linenum))
			continue;

		debug3("%s: found %skey type %s in file %s:%lu", __func__,
		    marker == MRK_NONE ? "" :
		    (marker == MRK_CA ? "ca " : "revoked "),
		    key_type(key), path, linenum);
		hostkeys->entries = xrealloc(hostkeys->entries,
		    hostkeys->num_entries + 1, sizeof(*hostkeys->entries));
		hostkeys->entries[hostkeys->num_entries].host = xstrdup(host);
		hostkeys->entries[hostkeys->num_entries].file = xstrdup(path);
		hostkeys->entries[hostkeys->num_entries].line = linenum;
		hostkeys->entries[hostkeys->num_entries].key = key;
		hostkeys->entries[hostkeys->num_entries].marker = marker;
		hostkeys->num_entries++;
		num_loaded++;
	}
	debug3("%s: loaded %lu keys", __func__, num_loaded);
	fclose(f);
	return;
}	

void
free_hostkeys(struct hostkeys *hostkeys)
{
	u_int i;

	for (i = 0; i < hostkeys->num_entries; i++) {
		xfree(hostkeys->entries[i].host);
		xfree(hostkeys->entries[i].file);
		key_free(hostkeys->entries[i].key);
		bzero(hostkeys->entries + i, sizeof(*hostkeys->entries));
	}
	if (hostkeys->entries != NULL)
		xfree(hostkeys->entries);
	hostkeys->entries = NULL;
	hostkeys->num_entries = 0;
	xfree(hostkeys);
}

static int
check_key_not_revoked(struct hostkeys *hostkeys, Key *k)
{
	int is_cert = key_is_cert(k);
	u_int i;

	for (i = 0; i < hostkeys->num_entries; i++) {
		if (hostkeys->entries[i].marker != MRK_REVOKE)
			continue;
		if (key_equal_public(k, hostkeys->entries[i].key))
			return -1;
		if (is_cert &&
		    key_equal_public(k->cert->signature_key,
		    hostkeys->entries[i].key))
			return -1;
	}
	return 0;
}

/*
 * Match keys against a specified key, or look one up by key type.
 *
 * If looking for a keytype (key == NULL) and one is found then return
 * HOST_FOUND, otherwise HOST_NEW.
 *
 * If looking for a key (key != NULL):
 *  1. If the key is a cert and a matching CA is found, return HOST_OK
 *  2. If the key is not a cert and a matching key is found, return HOST_OK
 *  3. If no key matches but a key with a different type is found, then
 *     return HOST_CHANGED
 *  4. If no matching keys are found, then return HOST_NEW.
 *
 * Finally, check any found key is not revoked.
 */
static HostStatus
check_hostkeys_by_key_or_type(struct hostkeys *hostkeys,
    Key *k, int keytype, const struct hostkey_entry **found)
{
	u_int i;
	HostStatus end_return = HOST_NEW;
	int want_cert = key_is_cert(k);
	HostkeyMarker want_marker = want_cert ? MRK_CA : MRK_NONE;
	int proto = (k ? k->type : keytype) == KEY_RSA1 ? 1 : 2;

	if (found != NULL)
		*found = NULL;

	for (i = 0; i < hostkeys->num_entries; i++) {
		if (proto == 1 && hostkeys->entries[i].key->type != KEY_RSA1)
			continue;
		if (proto == 2 && hostkeys->entries[i].key->type == KEY_RSA1)
			continue;
		if (hostkeys->entries[i].marker != want_marker)
			continue;
		if (k == NULL) {
			if (hostkeys->entries[i].key->type != keytype)
				continue;
			end_return = HOST_FOUND;
			if (found != NULL)
				*found = hostkeys->entries + i;
			k = hostkeys->entries[i].key;
			break;
		}
		if (want_cert) {
			if (key_equal_public(k->cert->signature_key,
			    hostkeys->entries[i].key)) {
				/* A matching CA exists */
				end_return = HOST_OK;
				if (found != NULL)
					*found = hostkeys->entries + i;
				break;
			}
		} else {
			if (key_equal(k, hostkeys->entries[i].key)) {
				end_return = HOST_OK;
				if (found != NULL)
					*found = hostkeys->entries + i;
				break;
			}
			/* A non-maching key exists */
			end_return = HOST_CHANGED;
			if (found != NULL)
				*found = hostkeys->entries + i;
		}
	}
	if (check_key_not_revoked(hostkeys, k) != 0) {
		end_return = HOST_REVOKED;
		if (found != NULL)
			*found = NULL;
	}
	return end_return;
}
	
HostStatus
check_key_in_hostkeys(struct hostkeys *hostkeys, Key *key,
    const struct hostkey_entry **found)
{
	if (key == NULL)
		fatal("no key to look up");
	return check_hostkeys_by_key_or_type(hostkeys, key, 0, found);
}

int
lookup_key_in_hostkeys_by_type(struct hostkeys *hostkeys, int keytype,
    const struct hostkey_entry **found)
{
	return (check_hostkeys_by_key_or_type(hostkeys, NULL, keytype,
	    found) == HOST_FOUND);
}

/*
 * Appends an entry to the host file.  Returns false if the entry could not
 * be appended.
 */

int
add_host_to_hostfile(const char *filename, const char *host, const Key *key,
    int store_hash)
{
	FILE *f;
	int success = 0;
	char *hashed_host = NULL;

	if (key == NULL)
		return 1;	/* XXX ? */
	f = fopen(filename, "a");
	if (!f)
		return 0;

	if (store_hash) {
		if ((hashed_host = host_hash(host, NULL, 0)) == NULL) {
			error("add_host_to_hostfile: host_hash failed");
			fclose(f);
			return 0;
		}
	}
	fprintf(f, "%s ", store_hash ? hashed_host : host);

	if (key_write(key, f)) {
		success = 1;
	} else {
		error("add_host_to_hostfile: saving key in %s failed", filename);
	}
	fprintf(f, "\n");
	fclose(f);
	return success;
}
