/*
 * Copyright (c) 2000, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: subr.c,v 1.11 2001/04/16 04:33:01 bp Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <err.h>

#include <netsmb/netbios.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>
#include <cflib.h>

extern char *__progname;

static int smblib_initialized;

struct rcfile *smb_rc;

int
smb_lib_init(void)
{
	int error;
	int kv;
	size_t kvlen = sizeof(kv);

	if (smblib_initialized)
		return 0;
#if __FreeBSD_version > 400000
	error = sysctlbyname("net.smb.version", &kv, &kvlen, NULL, 0);
	if (error) {
		warnx("%s: can't find kernel module\n", __FUNCTION__);
		return error;
	}
	if (NSMB_VERSION != kv) {
		warnx("%s: kernel module version(%d) don't match library(%d).\n", __FUNCTION__, kv, NSMB_VERSION);
		return EINVAL;
	}
#endif
	if ((error = nls_setlocale("")) != 0) {
		warnx("%s: can't initialise locale\n", __FUNCTION__);
		return error;
	}
	smblib_initialized++;
	return 0;
}

/*
 * Print a (descriptive) error message
 * error values:
 *  	   0 - no specific error code available;
 *  1..32767 - system error
 */
void
smb_error(const char *fmt, int error,...) {
	va_list ap;
	const char *cp;
	int errtype = error & SMB_ERRTYPE_MASK;

	fprintf(stderr, "%s: ", __progname);
	va_start(ap, error);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (error == -1)
		error = errno;
	else
		error &= ~SMB_ERRTYPE_MASK;
	switch (errtype) {
	    case SMB_SYS_ERROR:
		if (error)
			fprintf(stderr, ": syserr = %s\n", strerror(error));
		else
			fprintf(stderr, "\n");
		break;
	    case SMB_RAP_ERROR:
		fprintf(stderr, ": raperr = %d (0x%04x)\n", error, error);
		break;
	    case SMB_NB_ERROR:
		cp = nb_strerror(error);
		if (cp == NULL)
			fprintf(stderr, ": nberr = unknown (0x%04x)\n", error);
		else
			fprintf(stderr, ": nberr = %s\n", cp);
		break;
	    default:
		fprintf(stderr, "\n");
	}
}

char *
smb_printb(char *dest, int flags, const struct smb_bitname *bnp) {
	int first = 1;

	strcpy(dest, "<");
	for(; bnp->bn_bit; bnp++) {
		if (flags & bnp->bn_bit) {
			strcat(dest, bnp->bn_name);
			first = 0;
		}
		if (!first && (flags & bnp[1].bn_bit))
			strcat(dest, "|");
	}
	strcat(dest, ">");
	return dest;
}

/*
 * first read ~/.smbrc, next try to merge SMB_CFG_FILE
 */
int
smb_open_rcfile(void)
{
	char *home, *fn;
	int error;

	home = getenv("HOME");
	if (home) {
		fn = malloc(strlen(home) + 20);
		sprintf(fn, "%s/.nsmbrc", home);
		error = rc_open(fn, "r", &smb_rc);
		free(fn);
	}
	error = rc_merge(SMB_CFG_FILE, &smb_rc);
	if (smb_rc == NULL) {
		printf("Warning: no cfg file(s) found.\n");
		return ENOENT;
	}
	return 0;
}

void *
smb_dumptree(void)
{
	size_t len;
	void *p;
	int error;
	
	error = sysctlbyname("net.smb.treedump", NULL, &len, NULL, 0);
	if (error)
		return NULL;
	p = malloc(len);
	if (p == NULL)
		return NULL;
	error = sysctlbyname("net.smb.treedump", p, &len, NULL, 0);
	if (error) {
		free(p);
		return NULL;
	}
	return p;
}

void
smb_simplecrypt(char *dst, const char *src)
{
	int ch, pos;

	*dst++ = '$';
	*dst++ = '$';
	*dst++ = '1';
	pos = 27;
	while (*src) {
		ch = *src++;
		if (isascii(ch))
		    ch = (isupper(ch) ? ('A' + (ch - 'A' + 13) % 26) :
			  islower(ch) ? ('a' + (ch - 'a' + 13) % 26) : ch);
		ch ^= pos;
		pos += 13;
		sprintf(dst, "%02x", ch);
		dst += 2;
	}
	*dst = 0;
}

int
smb_simpledecrypt(char *dst, const char *src)
{
	char *ep, hexval[3];
	int len, ch, pos;

	if (strncmp(src, "$$1", 3) != 0)
		return EINVAL;
	src += 3;
	len = strlen(src);
	if (len & 1)
		return EINVAL;
	len /= 2;
	hexval[2] = 0;
	pos = 27;
	while (len--) {
		hexval[0] = *src++;
		hexval[1] = *src++;
		ch = strtoul(hexval, &ep, 16);
		if (*ep != 0)
			return EINVAL;
		ch ^= pos;
		pos += 13;
		if (isascii(ch))
		    ch = (isupper(ch) ? ('A' + (ch - 'A' + 13) % 26) :
			  islower(ch) ? ('a' + (ch - 'a' + 13) % 26) : ch);
		*dst++ = ch;
	}
	*dst = 0;
	return 0;
}
