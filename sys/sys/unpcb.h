/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *
 *	from: @(#)unpcb.h	7.6 (Berkeley) 6/28/90
 *	$Id: unpcb.h,v 1.2 1993/10/16 17:18:19 rgrimes Exp $
 */

/*
 * Protocol control block for an active
 * instance of a UNIX internal protocol.
 *
 * A socket may be associated with an vnode in the
 * file system.  If so, the unp_vnode pointer holds
 * a reference count to this vnode, which should be irele'd
 * when the socket goes away.
 *
 * A socket may be connected to another socket, in which
 * case the control block of the socket to which it is connected
 * is given by unp_conn.
 *
 * A socket may be referenced by a number of sockets (e.g. several
 * sockets may be connected to a datagram socket.)  These sockets
 * are in a linked list starting with unp_refs, linked through
 * unp_nextref and null-terminated.  Note that a socket may be referenced
 * by a number of other sockets and may also reference a socket (not
 * necessarily one which is referencing it).  This generates
 * the need for unp_refs and unp_nextref to be separate fields.
 *
 * Stream sockets keep copies of receive sockbuf sb_cc and sb_mbcnt
 * so that changes in the sockbuf may be computed to modify
 * back pressure on the sender accordingly.
 */
struct	unpcb {
	struct	socket *unp_socket;	/* pointer back to socket */
	struct	vnode *unp_vnode;	/* if associated with file */
	ino_t	unp_ino;		/* fake inode number */
	struct	unpcb *unp_conn;	/* control block of connected socket */
	struct	unpcb *unp_refs;	/* referencing socket linked list */
	struct 	unpcb *unp_nextref;	/* link in unp_refs list */
	struct	mbuf *unp_addr;		/* bound address of socket */
	int	unp_cc;			/* copy of rcv.sb_cc */
	int	unp_mbcnt;		/* copy of rcv.sb_mbcnt */
};

#define	sotounpcb(so)	((struct unpcb *)((so)->so_pcb))
