/*-
 * Copyright (c) 2000 Alfred Perlstein <alfred@FreeBSD.org>
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
 *	$FreeBSD$
 */

#define ACCEPT_FILTER_MOD

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h> 
#include <sys/unistd.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/mbuf.h>
#include <sys/resource.h>
#include <sys/sysent.h>
#include <sys/resourcevar.h>

/*
 * XXX: doesn't work with 0.9 requests, make a seperate filter
 * based on this one if you want to decode those.
 */

/* check for GET */
static void sohashttpget(struct socket *so, void *arg, int waitflag);
/* check for end of HTTP request */
static void soishttpconnected(struct socket *so, void *arg, int waitflag);
static char sbindex(struct mbuf **mp, int *begin, int end);

static struct accept_filter accf_http_filter = {
	"httpready",
	sohashttpget,
	NULL,
	NULL
};

static moduledata_t accf_http_mod = {
	"accf_http",
	accept_filt_generic_mod_event,
	&accf_http_filter
};

DECLARE_MODULE(accf_http, accf_http_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);


static char
sbindex(struct mbuf **mp, int *begin, int end)
{
	struct mbuf *m = *mp;
	int diff = end - *begin + 1;

	while (m->m_len < diff) {
		*begin += m->m_len;
		diff -= m->m_len;
		if (m->m_next) {
			m = m->m_next;
		} else if (m->m_nextpkt) {
			m = m->m_nextpkt;
		} else {
			/* only happens if end > data in socket buffer */
			panic("sbindex: not enough data");
		}
	}
	*mp = m;
	return *(mtod(m, char *) + diff - 1);
}

static void
sohashttpget(struct socket *so, void *arg, int waitflag)
{

	if ((so->so_state & SS_CANTRCVMORE) == 0) {
		struct mbuf *m;

		if (so->so_rcv.sb_cc < 6)
			return;
		m = so->so_rcv.sb_mb;
		if (bcmp(mtod(m, char *), "GET ", 4) == 0) {
			soishttpconnected(so, arg, waitflag);
			return;
		}
	}

	so->so_upcall = NULL;
	so->so_rcv.sb_flags &= ~SB_UPCALL;
	soisconnected(so);
	return;
}

static void
soishttpconnected(struct socket *so, void *arg, int waitflag)
{
	char a, b, c;
	struct mbuf *y, *z;

	if ((so->so_state & SS_CANTRCVMORE) == 0) {
		/* seek to end and keep track of next to last mbuf */
		y = so->so_rcv.sb_mb;
		while (y->m_nextpkt)
			y = y->m_nextpkt;
		z = y;	
		while (y->m_next) {
			z = y;
			y = y->m_next;
		}
			
		if (z->m_len + y->m_len > 2) {
			int index = y->m_len - 1;

			c = *(mtod(y, char *) + index--);
			switch (index) {
			case -1:
				y = z;
				index = y->m_len - 1;
				b = *(mtod(y, char *) + index--);
				break;
			case 0:
				b = *(mtod(y, char *) + index--);
				y = z;
				index = y->m_len - 1;
				break;
			default:
				b = *(mtod(y, char *) + index--);
				break;
			}
			a = *(mtod(y, char *) + index--);
		} else {
			int begin = 0;
			int end = so->so_rcv.sb_cc - 3;

			y = so->so_rcv.sb_mb;
			a = sbindex(&y, &begin, end++);
			b = sbindex(&y, &begin, end++);
			c = sbindex(&y, &begin, end++);
		}

		if (c == '\n' && (b == '\n' || (b == '\r' && a == '\n'))) {
			/* we have all request headers */
			goto done;
		} else {
			/* still need more data */
			so->so_upcall = soishttpconnected;
			so->so_rcv.sb_flags |= SB_UPCALL;
			return;
		}
	}

done:
	so->so_upcall = NULL;
	so->so_rcv.sb_flags &= ~SB_UPCALL;
	soisconnected(so);
	return;
}
