/*
 * Copyright (c) 1983, 1988 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#ifndef lint
static char sccsid[] = "@(#)host.c	5.12 (Berkeley) 2/2/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netimp/if_imp.h>
#include <netimp/if_imphost.h>

extern	int kmem;
extern 	int nflag;
extern	char *inetname();

/*
 * Print the host tables associated with the ARPANET IMP.
 * Symbolic addresses are shown unless the nflag is given.
 */
hostpr(impsoftcaddr, nimpaddr)
	off_t impsoftcaddr, nimpaddr;
{
	struct mbuf *hosts, mb;
	struct imp_softc imp_softc;
	register struct mbuf *m;
	register struct hmbuf *mh;
	register struct host *hp;
	char flagbuf[10], *flags;
	int i, nimp;

	if (impsoftcaddr == 0) {
		printf("imp_softc: symbol not in namelist\n");
		return;
	}
	if (nimpaddr == 0) {
		printf("nimp: symbol not in namelist\n");
		return;
	}
	kvm_read(nimpaddr, (char *)&nimp, sizeof (nimp));
	for (i = 0; i < nimp; i++) {
	    kvm_read(impsoftcaddr, (char *)&imp_softc, sizeof (imp_softc));
	    impsoftcaddr += sizeof (imp_softc);
	    m = imp_softc.imp_hosts;
	    printf("IMP%d Host Table\n", i);
	    printf("%-5.5s %-6.6s %-8.8s %-4.4s %-9.9s %-4.4s %s\n", "Flags",
	        "Host", "Imp", "Qcnt", "Q Address", "RFNM", "Timer");
	    while (m) {
		kvm_read((off_t)m, (char *)&mb, sizeof (mb));
		m = &mb;
		mh = (struct hmbuf *)(m->m_dat);
		if (mh->hm_count == 0) {
			m = m->m_next;
			continue;
		}
		for (hp = mh->hm_hosts; hp < mh->hm_hosts + HPMBUF; hp++) {
			if ((hp->h_flags&HF_INUSE) == 0 && hp->h_timer == 0)
				continue;
			flags = flagbuf;
			*flags++ = hp->h_flags&HF_INUSE ? 'A' : 'F';
			if (hp->h_flags&HF_DEAD)
				*flags++ = 'D';
			if (hp->h_flags&HF_UNREACH)
				*flags++ = 'U';
			*flags = '\0';
			printf("%-5.5s %-6d %-8d %-4d %-9x %-4d %d\n",
				flagbuf,
				hp->h_host,
				ntohs(hp->h_imp),
				hp->h_qcnt,
				hp->h_q,
				hp->h_rfnm,
				hp->h_timer);
		}
		m = m->m_next;
	    }
	}
}

impstats(impsoftcaddr, nimpaddr)
	off_t impsoftcaddr, nimpaddr;
{
	struct imp_softc imp_softc;
	int i, nimp;
	extern char *plural();

	if (impsoftcaddr == 0 || nimpaddr == 0)
		return;
	kvm_read(nimpaddr, (char *)&nimp, sizeof (nimp));
	for (i = 0; i < nimp; i++) {
		kvm_read(impsoftcaddr, (char *)&imp_softc, sizeof (imp_softc));
		impsoftcaddr += sizeof(imp_softc);
		printf("imp%d statistics:\n", i);
#define	p(f, m)		printf(m, imp_softc.f, plural(imp_softc.f))
		p(imp_if.if_ipackets, "\t%u input message%s\n");
		p(imp_if.if_opackets, "\t%u output message%s\n");
		printf("\t%u times output blocked at least %d sec.\n",
		    imp_softc.imp_block, IMP_OTIMER);
		p(imp_incomplete, "\t%u \"incomplete\" message%s\n");
		p(imp_lostrfnm, "\t%u lost RFNM message%s\n");
		p(imp_badrfnm, "\t%u late/bogus RFNM/incomplete message%s\n");
		p(imp_garbage, "\t%u unknown message type%s\n");
	}
}
