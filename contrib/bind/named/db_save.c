#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)db_save.c	4.16 (Berkeley) 3/21/91";
static char rcsid[] = "$Id: db_save.c,v 8.4 1996/08/05 08:31:30 vixie Exp $";
#endif /* not lint */

/*
 * ++Copyright++ 1986
 * -
 * Copyright (c) 1986
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

/*
 * Buffer allocation and deallocation routines.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "named.h"

/*
 * Allocate a name buffer & save name.
 */
struct namebuf *
savename(name, len)
	const char *name;
	int len;
{
	register struct namebuf *np;

	assert(len >= 0 && len <= (MAXLABEL * 2));
	np = (struct namebuf *) malloc(NAMESIZE(len));
	if (np == NULL)
		panic(errno, "savename: malloc");
	bzero((char*)np, NAMESIZE(len));
	NAMELEN(*np) = len;
	bcopy(name, NAME(*np), len);
	NAME(*np)[len] = '\0';
	return (np);
}

/*
 * Allocate a data buffer & save data.
 */
struct databuf *
#ifdef DMALLOC
savedata_tagged(file, line, class, type, ttl, data, size)
	char *file;
	int line;
#else
savedata(class, type, ttl, data, size)
#endif
	int class, type;
	u_int32_t ttl;
	u_char *data;
	int size;
{
	register struct databuf *dp;
	int bytes = (type == T_NS) ? DATASIZE(size)+INT32SZ : DATASIZE(size);

	dp = (struct databuf *) 
#ifdef DMALLOC
		dmalloc(file, line, bytes)
#else
		malloc(bytes)
#endif
	;
	if (dp == NULL)
		panic(errno, "savedata: malloc");
	bzero((char*)dp, bytes);
	dp->d_next = NULL;
	dp->d_type = type;
	dp->d_class = class;
	dp->d_ttl = ttl;
	dp->d_size = size;
	dp->d_mark = 0;
	dp->d_flags = 0;
	dp->d_cred = 0;
	dp->d_clev = 0;
#ifdef NCACHE
	dp->d_rcode = NOERROR;
#endif
#ifdef STATS
	dp->d_ns = NULL;
#endif
	dp->d_nstime = 0;
	bcopy(data, dp->d_data, dp->d_size);
	return (dp);
}

int hashsizes[] = {	/* hashtable sizes */
	2,
	11,
	113,
	337,
	977,
	2053,
	4073,
	8011,
	16001,
	0
};

/*
 * Allocate a data buffer & save data.
 */
struct hashbuf *
savehash(oldhtp)
	register struct hashbuf *oldhtp;
{
	register struct hashbuf *htp;
	register struct namebuf *np, *nnp, **hp;
	register int n;
	int newsize;

	if (oldhtp == NULL)
		newsize = hashsizes[0];
	else {
		for (n = 0; newsize = hashsizes[n++]; )
			if (oldhtp->h_size == newsize) {
				newsize = hashsizes[n];
				break;
			}
		if (newsize == 0)
			newsize = oldhtp->h_size * 2 + 1;
	}
	dprintf(4, (ddt, "savehash GROWING to %d\n", newsize));
	htp = (struct hashbuf *) malloc((unsigned)HASHSIZE(newsize));
	if (htp == NULL) {
		syslog(LOG_ERR, "savehash: %m");
		exit(1);
	}
	htp->h_size = newsize;
	bzero((char *) htp->h_tab, newsize * sizeof(struct namebuf *));
	if (oldhtp == NULL) {
		htp->h_cnt = 0;
		return (htp);
	}
	dprintf(4, (ddt, "savehash(%#lx) cnt=%d, sz=%d, newsz=%d\n",
		    (u_long)oldhtp, oldhtp->h_cnt, oldhtp->h_size, newsize));
	htp->h_cnt = oldhtp->h_cnt;
	for (n = 0; n < oldhtp->h_size; n++) {
		for (np = oldhtp->h_tab[n]; np != NULL; np = nnp) {
			nnp = np->n_next;
			hp = &htp->h_tab[np->n_hashval % htp->h_size];
			np->n_next = *hp;
			*hp = np;
		}
	}
	free((char *) oldhtp);
	return (htp);
}
