/*
 * Copyright (c) 1998-2002 Luigi Rizzo
 *
 * Work partly supported by: Cisco Systems, Inc. - NSITE lab, RTP, NC
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
 * $FreeBSD$
 */

extern int do_bridge;

/*
 * We need additional per-interface info for the bridge, which is
 * stored in a struct bdg_softc. The ifp2sc[] array provides a pointer
 * to this struct using the if_index as a mapping key.
 * bdg_softc has a backpointer to the struct ifnet, the bridge
 * flags, and a cluster (bridging occurs only between port of the
 * same cluster).
 */

struct cluster_softc;	/* opaque here, defined in bridge.c */

struct bdg_softc {
    struct ifnet *ifp ;
    /* also ((struct arpcom *)ifp)->ac_enaddr is the eth. addr */
    int flags ;
#define IFF_BDG_PROMISC 0x0001  /* set promisc mode on this if.	*/
#define IFF_MUTE        0x0002  /* mute this if for bridging.   */
#define IFF_USED        0x0004  /* use this if for bridging.    */
    struct cluster_softc *cluster;
} ;

extern struct bdg_softc *ifp2sc;

#define BDG_USED(ifp) (ifp2sc[ifp->if_index].flags & IFF_USED)
/*
 * BDG_ACTIVE(ifp) does all checks to see if bridging is enabled, loaded,
 * and used on a given interface.
 */
#define	BDG_ACTIVE(ifp)	(do_bridge && BDG_LOADED && BDG_USED(ifp))

/*
 * The following constants are not legal ifnet pointers, and are used
 * as return values from the classifier, bridge_dst_lookup().
 * The same values are used as index in the statistics arrays,
 * with BDG_FORWARD replacing specifically forwarded packets.
 *
 * These constants are here because they are used in 'netstat'
 * to show bridge statistics.
 */
#define BDG_BCAST	( (struct ifnet *)1 )
#define BDG_MCAST	( (struct ifnet *)2 )
#define BDG_LOCAL	( (struct ifnet *)3 )
#define BDG_DROP	( (struct ifnet *)4 )
#define BDG_UNKNOWN	( (struct ifnet *)5 )
#define BDG_IN		( (struct ifnet *)7 )
#define BDG_OUT		( (struct ifnet *)8 )
#define BDG_FORWARD	( (struct ifnet *)9 )

/*
 * Statistics are passed up with the sysctl interface, "netstat -p bdg"
 * reads them. PF_BDG defines the 'bridge' protocol family.
 */

#define PF_BDG 3 /* XXX superhack */

#define STAT_MAX (int)BDG_FORWARD
struct bdg_port_stat {
    char name[16];
    u_long collisions;
    u_long p_in[STAT_MAX+1];
} ;

/* XXX this should be made dynamic */
#define BDG_MAX_PORTS 128
struct bdg_stats {
    struct bdg_port_stat s[BDG_MAX_PORTS];
} ;


#define BDG_STAT(ifp, type) bdg_stats.s[ifp->if_index].p_in[(uintptr_t)type]++ 
 
#ifdef _KERNEL
typedef	struct ifnet *bridge_in_t(struct ifnet *, struct ether_header *);
/* bdg_forward frees the mbuf if necessary, returning null */
typedef	struct mbuf *bdg_forward_t(struct mbuf *, struct ether_header *const,
		struct ifnet *);
typedef	void bdgtakeifaces_t(void);
extern	bridge_in_t *bridge_in_ptr;
extern	bdg_forward_t *bdg_forward_ptr;
extern	bdgtakeifaces_t *bdgtakeifaces_ptr;

#define	BDG_LOADED	(bdgtakeifaces_ptr != NULL)
#endif /* KERNEL */
