/*
 * Copyright (c) 1998-2000 Luigi Rizzo
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
 * the hash table for bridge
 */
typedef struct hash_table {
    struct ifnet *name ;
    unsigned char etheraddr[6] ;
    unsigned short used ;
} bdg_hash_table ;

extern bdg_hash_table *bdg_table ;

/*
 * We need additional info for the bridge. The bdg_ifp2sc[] array
 * provides a pointer to this struct using the if_index.   
 * bdg_softc has a backpointer to the struct ifnet, the bridge
 * flags, and a cluster (bridging occurs only between port of the
 * same cluster).
 */
struct bdg_softc {
    struct ifnet *ifp ;
    /* also ((struct arpcom *)ifp)->ac_enaddr is the eth. addr */
    int flags ;
#define IFF_BDG_PROMISC 0x0001  /* set promisc mode on this if.  */
#define IFF_MUTE        0x0002  /* mute this if for bridging.   */
#define IFF_USED        0x0004  /* use this if for bridging.    */
    short cluster_id ; /* in network format */
    u_long magic;
} ;

extern struct bdg_softc *ifp2sc;

#define BDG_USED(ifp) (ifp2sc[ifp->if_index].flags & IFF_USED)
#define BDG_MUTED(ifp) (ifp2sc[ifp->if_index].flags & IFF_MUTE)
#define BDG_MUTE(ifp) ifp2sc[ifp->if_index].flags |= IFF_MUTE
#define BDG_UNMUTE(ifp) ifp2sc[ifp->if_index].flags &= ~IFF_MUTE
#define BDG_CLUSTER(ifp) (ifp2sc[ifp->if_index].cluster_id)
#define BDG_EH(ifp)	((struct arpcom *)ifp)->ac_enaddr

#define BDG_SAMECLUSTER(ifp,src) \
	(src == NULL || BDG_CLUSTER(ifp) == BDG_CLUSTER(src) )

/*
 * BDG_ACTIVE(ifp) does all checks to see if bridging is loaded,
 * activated and used on a given interface.
 */
#define	BDG_ACTIVE(ifp)	(do_bridge && BDG_LOADED && BDG_USED(ifp))

#define BDG_MAX_PORTS 128
typedef struct _bdg_addr {
    unsigned char etheraddr[6] ;
    short cluster_id ;
} bdg_addr ;
extern bdg_addr bdg_addresses[BDG_MAX_PORTS];
extern int bdg_ports ;

/*
 * out of the 6 bytes, the last ones are more "variable". Since
 * we are on a little endian machine, we have to do some gimmick...
 */
#define HASH_SIZE 8192	/* must be a power of 2 */
#define HASH_FN(addr)   (	\
	ntohs( ((short *)addr)[1] ^ ((short *)addr)[2] ) & (HASH_SIZE -1))

#ifdef __i386__
#define BDG_MATCH(a,b) ( \
    ((unsigned short *)(a))[2] == ((unsigned short *)(b))[2] && \
    *((unsigned int *)(a)) == *((unsigned int *)(b)) )
#define IS_ETHER_BROADCAST(a) ( \
	*((unsigned int *)(a)) == 0xffffffff && \
	((unsigned short *)(a))[2] == 0xffff )
#else
/* for machines that do not support unaligned access */
#define BDG_MATCH(a,b)		(!bcmp(a, b, ETHER_ADDR_LEN) )
#define	IS_ETHER_BROADCAST(a)	(!bcmp(a, "\377\377\377\377\377\377", 6))
#endif
/*
 * The following constants are not legal ifnet pointers, and are used
 * as return values from the classifier, bridge_dst_lookup()
 * The same values are used as index in the statistics arrays,
 * with BDG_FORWARD replacing specifically forwarded packets.
 */
#define BDG_BCAST	( (struct ifnet *)1 )
#define BDG_MCAST	( (struct ifnet *)2 )
#define BDG_LOCAL	( (struct ifnet *)3 )
#define BDG_DROP	( (struct ifnet *)4 )
#define BDG_UNKNOWN	( (struct ifnet *)5 )
#define BDG_IN		( (struct ifnet *)7 )
#define BDG_OUT		( (struct ifnet *)8 )
#define BDG_FORWARD	( (struct ifnet *)9 )

#define PF_BDG 3 /* XXX superhack */
/*
 * statistics, passed up with sysctl interface and ns -p bdg
 */

#define STAT_MAX (int)BDG_FORWARD
struct bdg_port_stat {
    char name[16];
    u_long collisions;
    u_long p_in[STAT_MAX+1];
} ;

struct bdg_stats {
    struct bdg_port_stat s[16];
} ;


#define BDG_STAT(ifp, type) bdg_stats.s[ifp->if_index].p_in[(long)type]++ 
 
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
