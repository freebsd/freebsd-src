/*
 * 
 * hostfile.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Thu Jun 29 07:10:56 1995 ylo
 * 
 * Functions for manipulating the known hosts files.
 * 
 */

#include "includes.h"
RCSID("$OpenBSD: hostfile.c,v 1.13 2000/02/18 10:20:20 markus Exp $");

#include "packet.h"
#include "ssh.h"

/*
 * Reads a multiple-precision integer in decimal from the buffer, and advances
 * the pointer.  The integer must already be initialized.  This function is
 * permitted to modify the buffer.  This leaves *cpp to point just beyond the
 * last processed (and maybe modified) character.  Note that this may modify
 * the buffer containing the number.
 */

int
auth_rsa_read_bignum(char **cpp, BIGNUM * value)
{
	char *cp = *cpp;
	int old;

	/* Skip any leading whitespace. */
	for (; *cp == ' ' || *cp == '\t'; cp++)
		;

	/* Check that it begins with a decimal digit. */
	if (*cp < '0' || *cp > '9')
		return 0;

	/* Save starting position. */
	*cpp = cp;

	/* Move forward until all decimal digits skipped. */
	for (; *cp >= '0' && *cp <= '9'; cp++)
		;

	/* Save the old terminating character, and replace it by \0. */
	old = *cp;
	*cp = 0;

	/* Parse the number. */
	if (BN_dec2bn(&value, *cpp) == 0)
		return 0;

	/* Restore old terminating character. */
	*cp = old;

	/* Move beyond the number and return success. */
	*cpp = cp;
	return 1;
}

/*
 * Parses an RSA key (number of bits, e, n) from a string.  Moves the pointer
 * over the key.  Skips any whitespace at the beginning and at end.
 */

int
auth_rsa_read_key(char **cpp, unsigned int *bitsp, BIGNUM * e, BIGNUM * n)
{
	unsigned int bits;
	char *cp;

	/* Skip leading whitespace. */
	for (cp = *cpp; *cp == ' ' || *cp == '\t'; cp++)
		;

	/* Get number of bits. */
	if (*cp < '0' || *cp > '9')
		return 0;	/* Bad bit count... */
	for (bits = 0; *cp >= '0' && *cp <= '9'; cp++)
		bits = 10 * bits + *cp - '0';

	/* Get public exponent. */
	if (!auth_rsa_read_bignum(&cp, e))
		return 0;

	/* Get public modulus. */
	if (!auth_rsa_read_bignum(&cp, n))
		return 0;

	/* Skip trailing whitespace. */
	for (; *cp == ' ' || *cp == '\t'; cp++)
		;

	/* Return results. */
	*cpp = cp;
	*bitsp = bits;
	return 1;
}

/*
 * Tries to match the host name (which must be in all lowercase) against the
 * comma-separated sequence of subpatterns (each possibly preceded by ! to
 * indicate negation).  Returns true if there is a positive match; zero
 * otherwise.
 */

int
match_hostname(const char *host, const char *pattern, unsigned int len)
{
	char sub[1024];
	int negated;
	int got_positive;
	unsigned int i, subi;

	got_positive = 0;
	for (i = 0; i < len;) {
		/* Check if the subpattern is negated. */
		if (pattern[i] == '!') {
			negated = 1;
			i++;
		} else
			negated = 0;

		/*
		 * Extract the subpattern up to a comma or end.  Convert the
		 * subpattern to lowercase.
		 */
		for (subi = 0;
		     i < len && subi < sizeof(sub) - 1 && pattern[i] != ',';
		     subi++, i++)
			sub[subi] = isupper(pattern[i]) ? tolower(pattern[i]) : pattern[i];
		/* If subpattern too long, return failure (no match). */
		if (subi >= sizeof(sub) - 1)
			return 0;

		/* If the subpattern was terminated by a comma, skip the comma. */
		if (i < len && pattern[i] == ',')
			i++;

		/* Null-terminate the subpattern. */
		sub[subi] = '\0';

		/* Try to match the subpattern against the host name. */
		if (match_pattern(host, sub)) {
			if (negated)
				return 0;	/* Fail */
			else
				got_positive = 1;
		}
	}

	/*
	 * Return success if got a positive match.  If there was a negative
	 * match, we have already returned zero and never get here.
	 */
	return got_positive;
}

/*
 * Checks whether the given host (which must be in all lowercase) is already
 * in the list of our known hosts. Returns HOST_OK if the host is known and
 * has the specified key, HOST_NEW if the host is not known, and HOST_CHANGED
 * if the host is known but used to have a different host key.
 */

HostStatus
check_host_in_hostfile(const char *filename, const char *host,
		       BIGNUM * e, BIGNUM * n, BIGNUM * ke, BIGNUM * kn)
{
	FILE *f;
	char line[8192];
	int linenum = 0;
	unsigned int kbits, hostlen;
	char *cp, *cp2;
	HostStatus end_return;

	/* Open the file containing the list of known hosts. */
	f = fopen(filename, "r");
	if (!f)
		return HOST_NEW;

	/* Cache the length of the host name. */
	hostlen = strlen(host);

	/*
	 * Return value when the loop terminates.  This is set to
	 * HOST_CHANGED if we have seen a different key for the host and have
	 * not found the proper one.
	 */
	end_return = HOST_NEW;

	/* Go trough the file. */
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
		if (!match_hostname(host, cp, (unsigned int) (cp2 - cp)))
			continue;

		/* Got a match.  Skip host name. */
		cp = cp2;

		/*
		 * Extract the key from the line.  This will skip any leading
		 * whitespace.  Ignore badly formatted lines.
		 */
		if (!auth_rsa_read_key(&cp, &kbits, ke, kn))
			continue;

		if (kbits != BN_num_bits(kn)) {
			error("Warning: %s, line %d: keysize mismatch for host %s: "
			      "actual %d vs. announced %d.",
			      filename, linenum, host, BN_num_bits(kn), kbits);
			error("Warning: replace %d with %d in %s, line %d.",
			      kbits, BN_num_bits(kn), filename, linenum);
		}
		/* Check if the current key is the same as the given key. */
		if (BN_cmp(ke, e) == 0 && BN_cmp(kn, n) == 0) {
			/* Ok, they match. */
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

/*
 * Appends an entry to the host file.  Returns false if the entry could not
 * be appended.
 */

int
add_host_to_hostfile(const char *filename, const char *host,
		     BIGNUM * e, BIGNUM * n)
{
	FILE *f;
	char *buf;
	unsigned int bits;

	/* Open the file for appending. */
	f = fopen(filename, "a");
	if (!f)
		return 0;

	/* size of modulus 'n' */
	bits = BN_num_bits(n);

	/* Print the host name and key to the file. */
	fprintf(f, "%s %u ", host, bits);
	buf = BN_bn2dec(e);
	if (buf == NULL) {
		error("add_host_to_hostfile: BN_bn2dec(e) failed");
		fclose(f);
		return 0;
	}
	fprintf(f, "%s ", buf);
	free(buf);
	buf = BN_bn2dec(n);
	if (buf == NULL) {
		error("add_host_to_hostfile: BN_bn2dec(n) failed");
		fclose(f);
		return 0;
	}
	fprintf(f, "%s\n", buf);
	free(buf);

	/* Close the file. */
	fclose(f);
	return 1;
}
