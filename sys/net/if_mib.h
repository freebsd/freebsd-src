/*
 * Copyright 1996 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

#ifndef _NET_IF_MIB_H
#define	_NET_IF_MIB_H	1

struct ifmibdata {
	char	ifmd_name[IFNAMSIZ]; /* name of interface */
	int	ifmd_pcount;	/* number of promiscuous listeners */
	int	ifmd_flags;	/* interface flags */
	struct	if_data ifmd_data; /* generic information and statistics */
	int	ifmd_snd_len;	/* instantaneous length of send queue */
	int	ifmd_snd_maxlen; /* maximum length of send queue */
	int	ifmd_snd_drops;	/* number of drops in send queue */
};

/*
 * sysctl MIB tags at the net.link.generic level
 */
#define	IFMIB_SYSTEM	1	/* non-interface-specific */
#define	IFMIB_IFDATA	2	/* per-interface data table */

/*
 * MIB tags for the various net.link.generic.ifdata tables
 */
#define	IFDATA_GENERAL	1	/* generic stats for all kinds of ifaces */
#define	IFDATA_LINKSPECIFIC	2 /* specific to the type of interface */

/*
 * MIB tags at the net.link.generic.system level
 */
#define	IFMIB_IFCOUNT	1	/* number of interfaces configured */

/*
 * MIB tags as the net.link level
 * All of the other values are IFT_* names defined in if_types.h.
 */
#define	NETLINK_GENERIC	0	/* functions not specific to a type of iface */

/*
 * The reason why the IFDATA_LINKSPECIFIC stuff is not under the
 * net.link.<iftype> branches is twofold:
 *   1) It's easier to code this way, and doesn't require duplication.
 *   2) The fourth level under net.link.<iftype> is <pf>; that is to say,
 *	the net.link.<iftype> tree instruments the adaptation layers between
 *	<iftype> and a particular protocol family (e.g., net.link.ether.inet
 *	instruments ARP).  This does not really leave room for anything else
 *	that needs to have a well-known number.
 */
#endif /* _NET_IF_MIB_H */
