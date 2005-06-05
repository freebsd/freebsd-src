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
RCSID("$OpenBSD: hostfile.c,v 1.34 2005/03/10 22:01:05 deraadt Exp $");

#include <resolv.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include "packet.h"
#include "match.h"
#include "key.h"
#include "hostfile.h"
#include "log.h"
#include "xmalloc.h"

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
		debug2("extract_salt: expected salt len %u, got %u",
		    salt_len, ret);
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
	*bitsp = key_size(ret);
	return 1;
}

static int
hostfile_check_key(int bits, const Key *key, const char *host, const char *filename, int linenum)
{
	if (key == NULL || key->type != KEY_RSA1 || key->rsa == NULL)
		return 1;
	if (bits != BN_num_bits(key->rsa->n)) {
		logit("Warning: %s, line %d: keysize mismatch for host %s: "
		    "actual %d vs. announced %d.",
		    filename, linenum, host, BN_num_bits(key->rsa->n), bits);
		logit("Warning: replace %d with %d in %s, line %d.",
		    bits, BN_num_bits(key->rsa->n), filename, linenum);
	}
	return 1;
}

/*
 * Checks whether the given host (which must be in all lowercase) is already
 * in the list of our known hosts. Returns HOST_OK if the host is known and
 * has the specified key, HOST_NEW if the host is not known, and HOST_CHANGED
 * if the host is known but used to have a different host key.
 *
 * If no 'key' has been specified and a key of type 'keytype' is known
 * for the specified host, then HOST_FOUND is returned.
 */

static HostStatus
check_host_in_hostfile_by_key_or_type(const char *filename,
    const char *host, const Key *key, int keytype, Key *found, int *numret)
{
	FILE *f;
	char line[8192];
	int linenum = 0;
	u_int kbits;
	char *cp, *cp2, *hashed_host;
	HostStatus end_return;

	debug3("check_host_in_hostfile: filename %s", filename);

	/* Open the file containing the list of known hosts. */
	f = fopen(filename, "r");
	if (!f)
		return HOST_NEW;

	/*
	 * Return value when the loop terminates.  This is set to
	 * HOST_CHANGED if we have seen a different key for the host and have
	 * not found the proper one.
	 */
	end_return = HOST_NEW;

	/* Go through the file. */
	while (fgets(line, sizeof(line), f)) {
		cp = line;
		linenum++;

		/* Skip any leading whitespace, comments and empty lines. */
		for (; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '#' || *cp == '\n')
			continue;

		/* Find the end of the host name portion. */
		for (cp2 = cp; *cp2 && *cp2 != ' ' && *cp2 != '\t'; cp2++)
			;

		/* Check if the host name matches. */
		if (match_hostname(host, cp, (u_int) (cp2 - cp)) != 1) {
			if (*cp != HASH_DELIM)
				continue;
			hashed_host = host_hash(host, cp, (u_int) (cp2 - cp));
			if (hashed_host == NULL) {
				debug("Invalid hashed host line %d of %s",
				    linenum, filename);
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
		if (!hostfile_read_key(&cp, &kbits, found))
			continue;

		if (numret != NULL)
			*numret = linenum;

		if (key == NULL) {
			/* we found a key of the requested type */
			if (found->type == keytype)
				return HOST_FOUND;
			continue;
		}

		if (!hostfile_check_key(kbits, found, host, filename, linenum))
			continue;

		/* Check if the current key is the same as the given key. */
		if (key_equal(key, found)) {
			/* Ok, they match. */
			debug3("check_host_in_hostfile: match line %d", linenum);
			fclose(f);
			return HOST_OK;
		}
		/*
		 * They do not match.  We will continue to go through the
		 * file; however, we note that we will not return that it is
		 * new.
		 */
		end_return = HOST_CHANGED;
	}
	/* Clear variables and close the file. */
	fclose(f);

	/*
	 * Return either HOST_NEW or HOST_CHANGED, depending on whether we
	 * saw a different key for the host.
	 */
	return end_return;
}

HostStatus
check_host_in_hostfile(const char *filename, const char *host, const Key *key,
    Key *found, int *numret)
{
	if (key == NULL)
		fatal("no key to look up");
	return (check_host_in_hostfile_by_key_or_type(filename, host, key, 0,
	    found, numret));
}

int
lookup_key_in_hostfile_by_type(const char *filename, const char *host,
    int keytype, Key *found, int *numret)
{
	return (check_host_in_hostfile_by_key_or_type(filename, host, NULL,
	    keytype, found, numret) == HOST_FOUND);
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
	char *hashed_host;

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
