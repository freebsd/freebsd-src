/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/kdb.h>
#include <sys/libkern.h>
#include <sys/ttydefaults.h>

#include <machine/gdb_machdep.h>
#include <machine/kdb.h>

#include <gdb/gdb.h>
#include <gdb/gdb_int.h>

static char gdb_rxbuf[GDB_BUFSZ];
char *gdb_rxp = NULL;
size_t gdb_rxsz = 0;

/*
 * The goal here is to allow in-place framing without making the math around
 * 'gdb_txbuf' more complicated.  A generous reading of union special rule for
 * "common initial sequence" suggests this may be valid in standard C99 and
 * later.
 */
static union {
	struct _midbuf {
		char mb_pad1;
		char mb_buf[GDB_BUFSZ];
		char mb_pad2[4];
	} __packed txu_midbuf;
	/* sizeof includes trailing nul byte and this is intentional. */
	char txu_fullbuf[GDB_BUFSZ + sizeof("$#..")];
} gdb_tx_u;
#define	gdb_txbuf	gdb_tx_u.txu_midbuf.mb_buf
#define	gdb_tx_fullbuf	gdb_tx_u.txu_fullbuf
_Static_assert(sizeof(gdb_tx_u.txu_midbuf) == sizeof(gdb_tx_u.txu_fullbuf) &&
    offsetof(struct _midbuf, mb_buf) == 1,
    "assertions necessary for correctness");
char *gdb_txp = NULL;			/* Used in inline functions. */

#define	C2N(c)	(((c) < 'A') ? (c) - '0' : \
	    10 + (((c) < 'a') ? (c) - 'A' : (c) - 'a'))
#define	N2C(n)	(((n) < 10) ? (n) + '0' : (n) + 'a' - 10)

/*
 * Get a single character
 */

static int
gdb_getc(void)
{
	int c;

	do
		c = gdb_cur->gdb_getc();
	while (c == -1);

	if (c == CTRL('C')) {
		printf("Received ^C; trying to switch back to ddb.\n");

		if (gdb_cur->gdb_dbfeatures & GDB_DBGP_FEAT_WANTTERM)
			gdb_cur->gdb_term();

		if (kdb_dbbe_select("ddb") != 0)
			printf("The ddb backend could not be selected.\n");
		else {
			printf("using longjmp, hope it works!\n");
			kdb_reenter();
		}
	}
	return (c);
}

/*
 * Functions to receive and extract from a packet.
 */

int
gdb_rx_begin(void)
{
	int c, cksum;

	gdb_rxp = NULL;
	do {
		/*
		 * Wait for the start character, ignore all others.
		 * XXX needs a timeout.
		 */
		while ((c = gdb_getc()) != '$')
			;

		/* Read until a # or end of buffer is found. */
		cksum = 0;
		gdb_rxsz = 0;
		while (gdb_rxsz < sizeof(gdb_rxbuf) - 1) {
			c = gdb_getc();
			if (c == '#')
				break;
			gdb_rxbuf[gdb_rxsz++] = c;
			cksum += c;
		}
		gdb_rxbuf[gdb_rxsz] = 0;
		cksum &= 0xff;

		/* Bail out on a buffer overflow. */
		if (c != '#') {
			gdb_nack();
			return (ENOSPC);
		}

		/*
		 * In Not-AckMode, we can assume reliable transport and neither
		 * need to verify checksums nor send Ack/Nack.
		 */
		if (!gdb_ackmode)
			break;

		c = gdb_getc();
		cksum -= (C2N(c) << 4) & 0xf0;
		c = gdb_getc();
		cksum -= C2N(c) & 0x0f;
		if (cksum == 0) {
			gdb_ack();
		} else {
			gdb_nack();
			printf("GDB: packet `%s' has invalid checksum\n",
			    gdb_rxbuf);
		}
	} while (cksum != 0);

	gdb_rxp = gdb_rxbuf;
	return (0);
}

int
gdb_rx_equal(const char *str)
{
	int len;

	len = strlen(str);
	if (len > gdb_rxsz || strncmp(str, gdb_rxp, len) != 0)
		return (0);
	gdb_rxp += len;
	gdb_rxsz -= len;
	return (1);
}

int
gdb_rx_mem(unsigned char *addr, size_t size)
{
	unsigned char *p;
	void *prev;
	void *wctx;
	jmp_buf jb;
	size_t cnt;
	int ret;
	unsigned char c;

	if (size * 2 != gdb_rxsz)
		return (-1);

	wctx = gdb_begin_write();
	prev = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		p = addr;
		cnt = size;
		while (cnt-- > 0) {
			c = (C2N(gdb_rxp[0]) << 4) & 0xf0;
			c |= C2N(gdb_rxp[1]) & 0x0f;
			*p++ = c;
			gdb_rxsz -= 2;
			gdb_rxp += 2;
		}
		kdb_cpu_sync_icache(addr, size);
	}
	(void)kdb_jmpbuf(prev);
	gdb_end_write(wctx);
	return ((ret == 0) ? 1 : 0);
}

int
gdb_rx_varhex(uintmax_t *vp)
{
	uintmax_t v;
	int c, neg;

	c = gdb_rx_char();
	neg = (c == '-') ? 1 : 0;
	if (neg == 1)
		c = gdb_rx_char();
	if (!isxdigit(c)) {
		gdb_rxp -= ((c == -1) ? 0 : 1) + neg;
		gdb_rxsz += ((c == -1) ? 0 : 1) + neg;
		return (-1);
	}
	v = 0;
	do {
		v <<= 4;
		v += C2N(c);
		c = gdb_rx_char();
	} while (isxdigit(c));
	if (c != EOF) {
		gdb_rxp--;
		gdb_rxsz++;
	}
	*vp = (neg) ? -v : v;
	return (0);
}

/*
 * Function to build and send a package.
 */

void
gdb_tx_begin(char tp)
{

	gdb_txp = gdb_txbuf;
	if (tp != '\0')
		gdb_tx_char(tp);
}

/*
 * Take raw packet buffer and perform typical GDB packet framing, but not run-
 * length encoding, before forwarding to driver ::gdb_sendpacket() routine.
 */
static void
gdb_tx_sendpacket(void)
{
	size_t msglen, i;
	unsigned char csum;

	msglen = gdb_txp - gdb_txbuf;

	/* Add GDB packet framing */
	gdb_tx_fullbuf[0] = '$';

	csum = 0;
	for (i = 0; i < msglen; i++)
		csum += (unsigned char)gdb_txbuf[i];
	snprintf(&gdb_tx_fullbuf[1 + msglen], 4, "#%02x", (unsigned)csum);

	gdb_cur->gdb_sendpacket(gdb_tx_fullbuf, msglen + 4);
}

int
gdb_tx_end(void)
{
	const char *p;
	int runlen;
	unsigned char c, cksum;

	do {
		if (gdb_cur->gdb_sendpacket != NULL) {
			gdb_tx_sendpacket();
			goto getack;
		}

		gdb_cur->gdb_putc('$');

		cksum = 0;
		p = gdb_txbuf;
		while (p < gdb_txp) {
			/* Send a character and start run-length encoding. */
			c = *p++;
			gdb_cur->gdb_putc(c);
			cksum += c;
			runlen = 0;
			/* Determine run-length and update checksum. */
			while (p < gdb_txp && *p == c) {
				runlen++;
				p++;
			}
			/* Emit the run-length encoded string. */
			while (runlen >= 97) {
				gdb_cur->gdb_putc('*');
				cksum += '*';
				gdb_cur->gdb_putc(97+29);
				cksum += 97+29;
				runlen -= 97;
				if (runlen > 0) {
					gdb_cur->gdb_putc(c);
					cksum += c;
					runlen--;
				}
			}
			/* Don't emit '$', '#', '+', '-' or a run length below 3. */
			while (runlen == 1 || runlen == 2 ||
			    runlen + 29 == '$' || runlen + 29 == '#' ||
			    runlen + 29 == '+' || runlen + 29 == '-') {
				gdb_cur->gdb_putc(c);
				cksum += c;
				runlen--;
			}
			if (runlen == 0)
				continue;
			gdb_cur->gdb_putc('*');
			cksum += '*';
			gdb_cur->gdb_putc(runlen+29);
			cksum += runlen+29;
		}

		gdb_cur->gdb_putc('#');
		c = cksum >> 4;
		gdb_cur->gdb_putc(N2C(c));
		c = cksum & 0x0f;
		gdb_cur->gdb_putc(N2C(c));

getack:
		/*
		 * In NoAckMode, it is assumed that the underlying transport is
		 * reliable and thus neither conservant sends acknowledgements;
		 * there is nothing to wait for here.
		 */
		if (!gdb_ackmode)
			break;

		c = gdb_getc();
	} while (c != '+');

	return (0);
}

int
gdb_tx_mem(const unsigned char *addr, size_t size)
{
	void *prev;
	jmp_buf jb;
	int ret;

	prev = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		while (size-- > 0) {
			*gdb_txp++ = N2C(*addr >> 4);
			*gdb_txp++ = N2C(*addr & 0x0f);
			addr++;
		}
	}
	(void)kdb_jmpbuf(prev);
	return ((ret == 0) ? 1 : 0);
}

void
gdb_tx_reg(int regnum)
{
	unsigned char *regp;
	size_t regsz;

	regp = gdb_cpu_getreg(regnum, &regsz);
	if (regp == NULL) {
		/* Register unavailable. */
		while (regsz--) {
			gdb_tx_char('x');
			gdb_tx_char('x');
		}
	} else
		gdb_tx_mem(regp, regsz);
}

bool
gdb_txbuf_has_capacity(size_t req)
{
	return (((char *)gdb_txbuf + sizeof(gdb_txbuf) - gdb_txp) >= req);
}

/* Read binary data up until the end of the packet or until we have datalen decoded bytes */
int
gdb_rx_bindata(unsigned char *data, size_t datalen, size_t *amt)
{
	int c;

	*amt = 0;

	while (*amt < datalen) {
		c = gdb_rx_char();
		if (c == EOF)
			break;
		/* Escaped character up next */
		if (c == '}') {
			/* Malformed packet. */
			if ((c = gdb_rx_char()) == EOF)
				return (1);
			c ^= 0x20;
		}
		*(data++) = c & 0xff;
		(*amt)++;
	}

	return (0);
}

int
gdb_search_mem(const unsigned char *addr, size_t size, const unsigned char *pat, size_t patlen, const unsigned char **found)
{
	void *prev;
	jmp_buf jb;
	int ret;

	prev = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0)
		*found = memmem(addr, size, pat, patlen);

	(void)kdb_jmpbuf(prev);
	return ((ret == 0) ? 1 : 0);
}
