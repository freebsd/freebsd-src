/*-
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
/* from: static char sccsid[] = "@(#)unix.c	5.11 (Berkeley) 7/1/91"; */
static const char unix_c_rcsid[] =
	"$Id: unix.c,v 1.5 1994/02/04 03:17:15 wollman Exp $";
#endif /* not lint */

/*
 * Display protocol blocks in the unix domain.
 */
#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/time.h>
#include <sys/proc.h>
#define KERNEL
struct uio;
#include <sys/file.h>
struct file *file, *fileNFILE;
int nfiles;

int	Aflag;
extern	char *calloc();

unixpr(fileheadaddr, nfilesaddr, unixsw)
	off_t fileheadaddr, nfilesaddr;
	struct protosw *unixsw;
{
	register struct file *fp, *lfp;
	struct file *filep;
	struct socket sock, *so = &sock;
	int i;

	if (fileheadaddr == 0 || nfilesaddr == 0) {
		printf("filehead or nfiles not in namelist.\n");
		return;
	}
	if (kvm_read(nfilesaddr, (char *)&nfiles, sizeof (nfiles)) !=
						sizeof (nfiles)) {
		printf("nfiles: bad read.\n");
		return;
	}
	if (kvm_read(fileheadaddr, (char *)&filep, sizeof (filep))
						!= sizeof (filep)) {
		printf("File table address, bad read.\n");
		return;
	}
	file = (struct file *)calloc(nfiles, sizeof (struct file));
	if (file == (struct file *)0) {
		printf("Out of memory (file table).\n");
		return;
	}
	for (lfp = file, i = nfiles; 
		filep != NULL && i-- > 0;
		filep = lfp->f_filef, lfp++) 
	{
		if(kvm_read((off_t)filep, (char *)lfp, sizeof (struct file))
						!= sizeof(struct file)) {
			printf("File table read error.\n");
			return;
		}
	}
	fileNFILE = file + nfiles;
	for (fp = file; fp < fileNFILE; fp++) {
		if (fp->f_count == 0 || fp->f_type != DTYPE_SOCKET)
			continue;
		if (kvm_read((off_t)fp->f_data, (char *)so, sizeof (*so))
					!= sizeof (*so))
			continue;
		/* kludge */
		if (so->so_proto >= unixsw && so->so_proto <= unixsw + 2)
			if (so->so_pcb)
				unixdomainpr(so, fp->f_data);
	}
	free((char *)file);
}

static	char *socktype[] =
    { "#0", "stream", "dgram", "raw", "rdm", "seqpacket" };

unixdomainpr(so, soaddr)
	register struct socket *so;
	caddr_t soaddr;
{
	struct unpcb unpcb, *unp = &unpcb;
	struct mbuf mbuf, *m;
	struct sockaddr_un *sa;
	static int first = 1;

	if (kvm_read((off_t)so->so_pcb, (char *)unp, sizeof (*unp))
				!= sizeof (*unp))
		return;
	if (unp->unp_addr) {
		m = &mbuf;
		if (kvm_read((off_t)unp->unp_addr, (char *)m, sizeof (*m))
				!= sizeof (*m))
			m = (struct mbuf *)0;
		sa = (struct sockaddr_un *)(m->m_dat);
	} else
		m = (struct mbuf *)0;
	if (first) {
		printf("Active local domain sockets\n");
		printf(
"%-8.8s %-6.6s %-6.6s %-6.6s %8.8s %8.8s %8.8s %8.8s Addr\n",
		    "Address", "Type", "Recv-Q", "Send-Q",
		    "Inode", "Conn", "Refs", "Nextref");
		first = 0;
	}
	printf("%8x %-6.6s %6d %6d %8x %8x %8x %8x",
	    soaddr, socktype[so->so_type], so->so_rcv.sb_cc, so->so_snd.sb_cc,
	    unp->unp_vnode, unp->unp_conn,
	    unp->unp_refs, unp->unp_nextref);
	if (m)
		printf(" %.*s", m->m_len - sizeof(sa->sun_family) - 1,
		    sa->sun_path);
	putchar('\n');
}
