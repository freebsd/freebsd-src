/*
 * ntp_peer.c - management of data maintained for peer associations
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>

#include "ntpd.h"
#include "ntp_stdlib.h"

/*
 *                  Table of valid association combinations
 *                  ---------------------------------------
 *
 *                             packet->mode
 * peer->mode      | UNSPEC  ACTIVE PASSIVE  CLIENT  SERVER  BCAST
 * ----------      | ---------------------------------------------
 * NO_PEER         |   e       1       e       1       1       1
 * ACTIVE          |   e       1       1       0       0       0
 * PASSIVE         |   e       1       e       0       0       0
 * CLIENT          |   e       0       0       0       1       1
 * SERVER          |   e       0       0       0       0       0
 * BCAST	   |   e       0       0       0       0       0
 * CONTROL	   |   e       0       0       0       0       0
 * PRIVATE	   |   e       0       0       0       0       0
 * BCLIENT         |   e       0       0       0       e       1
 * MCLIENT	   |   e       0       0       0       0       0
 *
 * One point to note here:  
 *      a packet in BCAST mode can potentially match a peer in CLIENT
 *      mode, but we that is a special case and we check for that early
 *      in the decision process.  This avoids having to keep track of 
 *      what kind of associations are possible etc...  We actually 
 *      circumvent that problem by requiring that the first b(m)roadcast 
 *      received after the change back to BCLIENT mode sets the clock.
 */

int AM[AM_MODES][AM_MODES] = {
/*	{ UNSPEC,   ACTIVE,     PASSIVE,    CLIENT,     SERVER,     BCAST } */

/*NONE*/{ AM_ERR, AM_NEWPASS, AM_ERR,     AM_FXMIT,   AM_MANYCAST, AM_NEWBCL},

/*A*/	{ AM_ERR, AM_PROCPKT, AM_PROCPKT, AM_NOMATCH, AM_NOMATCH,  AM_NOMATCH},

/*P*/	{ AM_ERR, AM_PROCPKT, AM_ERR,     AM_NOMATCH, AM_NOMATCH,  AM_NOMATCH},

/*C*/	{ AM_ERR, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH, AM_PROCPKT,  AM_POSSBCL},

/*S*/	{ AM_ERR, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH,  AM_NOMATCH},

/*BCST*/{ AM_ERR, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH,  AM_NOMATCH},

/*CNTL*/{ AM_ERR, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH,  AM_NOMATCH},

/*PRIV*/{ AM_ERR, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH,  AM_NOMATCH},

/*BCL*/ { AM_ERR, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH, AM_ERR,      AM_PROCPKT},

/*MCL*/ { AM_ERR, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH, AM_NOMATCH,  AM_NOMATCH}
};

#define MATCH_ASSOC(x,y)	AM[(x)][(y)]

/*
 * These routines manage the allocation of memory to peer structures
 * and the maintenance of the peer hash table.  The two main entry
 * points are findpeer(), which looks for corresponding peer data
 * in the peer list, newpeer(), which allocates a new peer structure
 * and adds it to the list, and unpeer(), which demobilizes the association
 * and deallocates the structure.
 */

/*
 * The peer hash table (imported by the protocol module).
 */
struct peer *peer_hash[HASH_SIZE];
int peer_hash_count[HASH_SIZE];		/* count of peers in each bucket */

/*
 * The association ID hash table.  Used for lookups by association ID
 */
struct peer *assoc_hash[HASH_SIZE];
int assoc_hash_count[HASH_SIZE];

/*
 * The free list.  Clean structures only, please.
 */
static struct peer *peer_free;
int peer_free_count;

/*
 * Association ID.  We initialize this value randomly, the assign a new
 * value every time the peer structure is incremented.
 */
static u_short current_association_ID;

/*
 * Memory allocation watermarks.
 */
#define	INIT_PEER_ALLOC		15	/* initialize space for 15 peers */
#define	INC_PEER_ALLOC		5	/* when we run out, add 5 more */

/*
 * Miscellaneous statistic counters which may be queried.
 */
u_long peer_timereset;		/* time stat counters were zeroed */
u_long findpeer_calls;		/* number of calls to findpeer */
u_long assocpeer_calls;		/* number of calls to findpeerbyassoc */
u_long peer_allocations;	/* number of allocations from the free list */
u_long peer_demobilizations;	/* number of structs freed to free list */
int total_peer_structs;		/* number of peer structs in circulation */
int peer_associations;		/* number of active associations */

/*
 * Our initial allocation of peer space
 */
static struct peer init_peer_alloc[INIT_PEER_ALLOC];

/*
 * Initialization data.  When configuring peers at initialization time,
 * we try to get their poll update timers initialized to different values
 * to prevent us from sending big clumps of data all at once.
 */
/* static u_long init_peer_starttime; */

static	void	getmorepeermem	P((void));
static	void	key_expire	P((struct peer *));

/*
 * init_peer - initialize peer data structures and counters
 *
 * N.B. We use the random number routine in here.  It had better be
 *      initialized prior to getting here.
 */
void
init_peer(void)
{
	register int i;

	/*
	 * Clear hash table and counters.
	 */
	for (i = 0; i < HASH_SIZE; i++) {
		peer_hash[i] = 0;
		peer_hash_count[i] = 0;
		assoc_hash[i] = 0;
		assoc_hash_count[i] = 0;
	}

	/*
	 * Clear stat counters
	 */
	findpeer_calls = peer_allocations = 0;
	assocpeer_calls = peer_demobilizations = 0;

	/*
	 * Initialization counter.
	 */
	/* init_peer_starttime = 0; */

	/*
	 * Initialize peer memory.
	 */
	peer_free = 0;
	for (i = 0; i < INIT_PEER_ALLOC; i++) {
		init_peer_alloc[i].next = peer_free;
		peer_free = &init_peer_alloc[i];
	}
	total_peer_structs = INIT_PEER_ALLOC;
	peer_free_count = INIT_PEER_ALLOC;

	/*
	 * Initialize our first association ID
	 */
	current_association_ID = (u_short)ranp2(16);
	if (current_association_ID == 0)
	    current_association_ID = 1;
}


/*
 * getmorepeermem - add more peer structures to the free list
 */
static void
getmorepeermem(void)
{
	register int i;
	register struct peer *peer;

	peer = (struct peer *)emalloc(INC_PEER_ALLOC*sizeof(struct peer));
	for (i = 0; i < INC_PEER_ALLOC; i++) {
		peer->next = peer_free;
		peer_free = peer;
		peer++;
	}

	total_peer_structs += INC_PEER_ALLOC;
	peer_free_count += INC_PEER_ALLOC;
}



/*
 * findexistingpeer - return a pointer to a peer in the hash table
 */
struct peer *
findexistingpeer(
	struct sockaddr_in *addr,
	struct peer *start_peer,
	int mode
	)
{
	register struct peer *peer;

	/*
	 * start_peer is included so we can locate instances of the
	 * same peer through different interfaces in the hash table.
	 */
	if (start_peer == 0)
	    peer = peer_hash[HASH_ADDR(addr)];
	else
	    peer = start_peer->next;
	
	while (peer != 0) {
		if (NSRCADR(addr) == NSRCADR(&peer->srcadr)
		    && NSRCPORT(addr) == NSRCPORT(&peer->srcadr)) {
			if (mode == -1)
				return peer;
			else if (peer->hmode == mode)
				break;
		}
		peer = peer->next;
	}

	return peer;
}


/*
 * findpeer - find and return a peer in the hash table.
 */
struct peer *
findpeer(
	struct sockaddr_in *srcadr,
	struct interface *dstadr,
	int fd,
	int pkt_mode,
	int *action
	)
{
	register struct peer *peer;
	int hash;

	findpeer_calls++;
	hash = HASH_ADDR(srcadr);
	for (peer = peer_hash[hash]; peer != 0; peer = peer->next) {
		if (NSRCADR(srcadr) == NSRCADR(&peer->srcadr)
		    && NSRCPORT(srcadr) == NSRCPORT(&peer->srcadr)) {
			/* 
			 * if the association matching rules determine that
			 * this is not a valid combination, then look for
			 * the next valid peer association.
			 */
			*action = MATCH_ASSOC(peer->hmode, pkt_mode);

			/*
			 * Sigh!  Check if BCLIENT peer in client
			 * server mode, else return error
			 */
			if ((*action == AM_POSSBCL) &&
			    !(peer->cast_flags & FLAG_MCAST1)) {
				*action = AM_ERR;
			}

			/* if an error was returned, exit back right here */
			if (*action == AM_ERR)
				return (struct peer *)0;

			/* if a match is found, we stop our search */
			if (*action != AM_NOMATCH)
				break;
		}
	}

#ifdef DEBUG
	if (debug > 1)
		printf("pkt_mode %d action %d\n", pkt_mode, *action);
#endif
	/* if no matching association is found */
	if (peer == 0) {
		*action = MATCH_ASSOC(NO_PEER, pkt_mode);
#ifdef DEBUG
		if (debug > 1)
			printf("pkt_mode %d action %d\n", pkt_mode, *action);
#endif
		return (struct peer *)0;
	}

	/* reset the default interface to something more meaningful */
	if ((peer->dstadr == any_interface))
		peer->dstadr = dstadr;
	return peer;
}

/*
 * findpeerbyassocid - find and return a peer using his association ID
 */
struct peer *
findpeerbyassoc(
	int assoc
	)
{
	register struct peer *peer;
	int hash;

	assocpeer_calls++;

	hash = assoc & HASH_MASK;
	for (peer = assoc_hash[hash]; peer != 0; peer = peer->ass_next) {
		if ((u_short)assoc == peer->associd)
		    return peer;	/* got it! */
	}

	/*
	 * Out of luck.  Return 0.
	 */
	return (struct peer *)0;
}

/*
 * findmanycastpeer - find and return an manycast peer if it exists
 *
 *
 *   the current implementation loops across all hash-buckets
 *
 *        *** THERE IS AN URGENT NEED TO CHANGE THIS ***
 */
struct peer *
findmanycastpeer(
	l_fp *p_org
	)
{
	register struct peer *peer;
	register struct peer *manycast_peer = 0;
	int i = 0;

	for (i = 0; i < HASH_SIZE; i++) {
		if (peer_hash_count[i] == 0)
			continue;

		for (peer = peer_hash[i]; peer != 0; peer = peer->next) {
			if (peer->cast_flags & MDF_ACAST &&
			    peer->flags & FLAG_CONFIG) {
				if (L_ISEQU(&peer->xmt, p_org))
					return peer; /* got it */
				else
					manycast_peer = peer;
			}
		}
	}

	/*
	 * Out of luck.  Return the manycastpeer for what it is worth.
	 */
	return manycast_peer;
}

/*
 * key_expire - garbage collect keys
 */
static void
key_expire(
	struct peer *peer
	)
{
	int i;

	if (peer->keylist != 0) {
		for (i = 0; i <= peer->keynumber; i++)
			authtrust(peer->keylist[i], 0);
		free(peer->keylist);
		peer->keylist = 0;
	}
	if (peer->keyid > NTP_MAXKEY) {
		authtrust(peer->keyid, 0);
		peer->keyid = 0;
	}
}

/*
 * key_rekey - expire all keys and roll a new private value. Note the
 * 32-bit mask is necessary for 64-bit u_longs.
 */
void
key_expire_all(
	)
{
	struct peer *peer, *next_peer;
	int n;

	for (n = 0; n < HASH_SIZE; n++) {
		for (peer = peer_hash[n]; peer != 0; peer = next_peer) {
			next_peer = peer->next;
			key_expire(peer);
		}
	}
	sys_private = (u_long)RANDOM & 0xffffffff;
#ifdef DEBUG
	if (debug)
		printf("key_expire_all: at %lu private %08lx\n",
		    current_time, sys_private);
#endif
}
/*
 * unpeer - remove peer structure from hash table and free structure
 */
void
unpeer(
	struct peer *peer_to_remove
	)
{
	int hash;

#ifdef DEBUG
	if (debug > 1)
		printf("demobilize %u\n", peer_to_remove->associd);
#endif
	key_expire(peer_to_remove);
	hash = HASH_ADDR(&peer_to_remove->srcadr);
	peer_hash_count[hash]--;
	peer_demobilizations++;
	peer_associations--;

#ifdef REFCLOCK
	/*
	 * If this peer is actually a clock, shut it down first
	 */
	if (peer_to_remove->flags & FLAG_REFCLOCK)
		refclock_unpeer(peer_to_remove);
#endif
	peer_to_remove->action = 0;	/* disable timeout actions */
	if (peer_hash[hash] == peer_to_remove)
		peer_hash[hash] = peer_to_remove->next;
	else {
		register struct peer *peer;

		peer = peer_hash[hash];
		while (peer != 0 && peer->next != peer_to_remove)
		    peer = peer->next;
		
		if (peer == 0) {
			peer_hash_count[hash]++;
			msyslog(LOG_ERR, "peer struct for %s not in table!",
				ntoa(&peer->srcadr));
		} else {
			peer->next = peer_to_remove->next;
		}
	}

	/*
	 * Remove him from the association hash as well.
	 */
	hash = peer_to_remove->associd & HASH_MASK;
	assoc_hash_count[hash]--;
	if (assoc_hash[hash] == peer_to_remove)
		assoc_hash[hash] = peer_to_remove->ass_next;
	else {
		register struct peer *peer;

		peer = assoc_hash[hash];
		while (peer != 0 && peer->ass_next != peer_to_remove)
		    peer = peer->ass_next;
		
		if (peer == 0) {
			assoc_hash_count[hash]++;
			msyslog(LOG_ERR,
				"peer struct for %s not in association table!",
				ntoa(&peer->srcadr));
		} else {
			peer->ass_next = peer_to_remove->ass_next;
		}
	}
	peer_to_remove->next = peer_free;
	peer_free = peer_to_remove;
	peer_free_count++;
}


/*
 * peer_config - configure a new peer
 */
struct peer *
peer_config(
	struct sockaddr_in *srcadr,
	struct interface *dstadr,
	int hmode,
	int version,
	int minpoll,
	int maxpoll,
	int flags,
	int ttl,
	u_long key
	)
{
	register struct peer *peer;

	/*
	 * See if we have this guy in the tables already.  If
	 * so just mark him configured.
	 */
	peer = findexistingpeer(srcadr, (struct peer *)0, hmode);
	if (dstadr != 0) {
		while (peer != 0) {
			if (peer->dstadr == dstadr)
			    break;
			peer = findexistingpeer(srcadr, peer, hmode);
		}
	}

	/*
	 * If we found one, just change his mode and mark him configured.
	 */
	if (peer != 0) {
		peer->hmode = (u_char)hmode;
		peer->version = (u_char)version;
		peer->minpoll = (u_char)minpoll;
		peer->maxpoll = (u_char)maxpoll;
		peer->hpoll = peer->minpoll;
		peer->ppoll = peer->minpoll;
		peer->flags = flags | FLAG_CONFIG |
			(peer->flags & FLAG_REFCLOCK);
		peer->cast_flags = (hmode == MODE_BROADCAST) ?
			IN_CLASSD(ntohl(srcadr->sin_addr.s_addr)) ? MDF_MCAST : MDF_BCAST : MDF_UCAST;
		peer->ttl = (u_char)ttl;
		peer->keyid = key;
		peer->keynumber = 0;
		return peer;
	}

	/*
	 * If we're here this guy is unknown to us.  Make a new peer
	 * structure for him.
	 */
	peer = newpeer(srcadr, dstadr, hmode, version, minpoll, maxpoll,
		       ttl, key);
	if (peer != 0) {
		peer->flags |= flags | FLAG_CONFIG;
#ifdef DEBUG
		if (debug)
			printf("peer_config: %s mode %d vers %d min %d max %d flags 0x%04x ttl %d key %lu\n",
			    ntoa(&peer->srcadr), peer->hmode, peer->version,
			    peer->minpoll, peer->maxpoll, peer->flags,
			    peer->ttl, peer->keyid);
#endif
	}
	return peer;
}


/*
 * newpeer - initialize a new peer association
 */
struct peer *
newpeer(
	struct sockaddr_in *srcadr,
	struct interface *dstadr,
	int hmode,
	int version,
	int minpoll,
	int maxpoll,
	int ttl,
	u_long key
	)
{
	register struct peer *peer;
	register int i;

	/*
	 * Some dirt here.  Some of the initialization requires
	 * knowlege of our system state.
	 */
	if (peer_free_count == 0)
	    getmorepeermem();

	peer = peer_free;
	peer_free = peer->next;
	peer_free_count--;
	peer_associations++;

	/*
	 * Initialize the structure.  This stuff is sort of part of
	 * the receive procedure and part of the clear procedure rolled
		 * into one.
		 *
	 * Zero the whole thing for now.  We might be pickier later.
	 */
	memset((char *)peer, 0, sizeof(struct peer));

	peer->srcadr = *srcadr;
	if (dstadr != 0)
		peer->dstadr = dstadr;
	else if (hmode == MODE_BROADCAST)
		peer->dstadr = findbcastinter(srcadr);
	else
		peer->dstadr = any_interface;
	peer->cast_flags = (hmode == MODE_BROADCAST) ?
	    (IN_CLASSD(ntohl(srcadr->sin_addr.s_addr))) ? MDF_MCAST :
	    MDF_BCAST : (hmode == MODE_BCLIENT || hmode == MODE_MCLIENT) ?
	    (peer->dstadr->flags & INT_MULTICAST) ? MDF_MCAST : MDF_BCAST :
	    MDF_UCAST;
	/* Set manycast flags if appropriate */
	if (IN_CLASSD(ntohl(srcadr->sin_addr.s_addr)) && hmode == MODE_CLIENT)
		peer->cast_flags = MDF_ACAST;
	peer->hmode = (u_char)hmode;
	peer->keyid = key;
	peer->version = (u_char)version;
	peer->minpoll = (u_char)minpoll;
	peer->maxpoll = (u_char)maxpoll;
	peer->hpoll = peer->minpoll;
	peer->ppoll = peer->minpoll;
	peer->ttl = ttl;
	peer->leap = LEAP_NOTINSYNC;
	peer->precision = sys_precision;
	peer->variance = MAXDISPERSE;
	peer->epoch = current_time;
	peer->stratum = STRATUM_UNSPEC;
	peer_clear(peer);
	peer->update = peer->outdate = current_time;
	peer->nextdate = peer->outdate + RANDPOLL(NTP_MINPOLL);
	if (peer->flags & FLAG_BURST)
		peer->burst = NTP_SHIFT;

	/*
	 * Assign him an association ID and increment the system variable
	 */
	peer->associd = current_association_ID;
	if (++current_association_ID == 0)
	    ++current_association_ID;

	/*
	 * Note time on statistics timers.
	 */
	peer->timereset = current_time;
	peer->timereachable = current_time;
	peer->timereceived = current_time;

#ifdef REFCLOCK
	if (ISREFCLOCKADR(&peer->srcadr)) {
		/*
		 * We let the reference clock support do clock
		 * dependent initialization.  This includes setting
		 * the peer timer, since the clock may have requirements
		 * for this.
		 */
		if (!refclock_newpeer(peer)) {
			/*
			 * Dump it, something screwed up
			 */
			peer->next = peer_free;
			peer_free = peer;
			peer_free_count++;
			return 0;
		}
	}
#endif

	/*
	 * Put him in the hash tables.
	 */
	i = HASH_ADDR(&peer->srcadr);
	peer->next = peer_hash[i];
	peer_hash[i] = peer;
	peer_hash_count[i]++;

	i = peer->associd & HASH_MASK;
	peer->ass_next = assoc_hash[i];
	assoc_hash[i] = peer;
	assoc_hash_count[i]++;
#ifdef DEBUG
	if (debug > 1)
		printf("mobilize %u next %lu\n", peer->associd,
		    peer->nextdate - peer->outdate);
#endif
	return peer;
}


/*
 * peer_unconfig - remove the configuration bit from a peer
 */
int
peer_unconfig(
	struct sockaddr_in *srcadr,
	struct interface *dstadr,
	int mode
	)
{
	register struct peer *peer;
	int num_found;

	num_found = 0;
	peer = findexistingpeer(srcadr, (struct peer *)0, mode);
	while (peer != 0) {
		if (peer->flags & FLAG_CONFIG
		    && (dstadr == 0 || peer->dstadr == dstadr)) {
			num_found++;
			/*
			 * Tricky stuff here.  If the peer is polling us
			 * in active mode, turn off the configuration bit
			 * and make the mode passive.  This allows us to
			 * avoid dumping a lot of history for peers we
			 * might choose to keep track of in passive mode.
			 * The protocol will eventually terminate undesirables
			 * on its own.
			 */
			if (peer->hmode == MODE_ACTIVE
			    && peer->pmode == MODE_ACTIVE) {
				peer->hmode = MODE_PASSIVE;
				peer->flags &= ~FLAG_CONFIG;
			} else {
				unpeer(peer);
				peer = 0;
			}
		}
		peer = findexistingpeer(srcadr, peer, mode);
	}
	return num_found;
}

/*
 * peer_copy_manycast - copy manycast peer variables to new association
 *   (right now it simply copies the transmit timestamp)
 */
void
peer_config_manycast(
	struct peer *peer1,
	struct peer *peer2
	)
{
	peer2->cast_flags = MDF_ACAST;
	peer2->xmt = peer1->xmt;
}

/*
 * peer_clr_stats - clear peer module stat counters
 */
void
peer_clr_stats(void)
{
	findpeer_calls = 0;
	assocpeer_calls = 0;
	peer_allocations = 0;
	peer_demobilizations = 0;
	peer_timereset = current_time;
}

/*
 * peer_reset - reset stat counters in a peer structure
 */
void
peer_reset(
	struct peer *peer
	)
{
	if (peer == 0)
	    return;
	peer->sent = 0;
	peer->received = 0;
	peer->processed = 0;
	peer->badauth = 0;
	peer->bogusorg = 0;
	peer->oldpkt = 0;
	peer->seldisptoolarge = 0;
	peer->selbroken = 0;
	peer->seltooold = 0;
	peer->timereset = current_time;
}


/*
 * peer_all_reset - reset all peer stat counters
 */
void
peer_all_reset(void)
{
	struct peer *peer;
	int hash;

	for (hash = 0; hash < HASH_SIZE; hash++)
	    for (peer = peer_hash[hash]; peer != 0; peer = peer->next)
		peer_reset(peer);
}
