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
#ifdef OPENSSL
#include "openssl/rand.h"
#endif /* OPENSSL */

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
 *
 * One point to note here: a packet in BCAST mode can potentially match
 * a peer in CLIENT mode, but we that is a special case and we check for
 * that early in the decision process.  This avoids having to keep track
 * of what kind of associations are possible etc...  We actually
 * circumvent that problem by requiring that the first b(m)roadcast
 * received after the change back to BCLIENT mode sets the clock.
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
};

#define MATCH_ASSOC(x,y)	AM[(x)][(y)]

/*
 * These routines manage the allocation of memory to peer structures
 * and the maintenance of the peer hash table. The two main entry
 * points are findpeer(), which looks for matching peer sturctures in
 * the peer list, newpeer(), which allocates a new peer structure and
 * adds it to the list, and unpeer(), which demobilizes the association
 * and deallocates the structure.
 */
/*
 * Peer hash tables
 */
struct peer *peer_hash[HASH_SIZE];	/* peer hash table */
int peer_hash_count[HASH_SIZE];		/* peers in each bucket */
struct peer *assoc_hash[HASH_SIZE];	/* association ID hash table */
int assoc_hash_count[HASH_SIZE];	/* peers in each bucket */
static struct peer *peer_free;		/* peer structures free list */
int peer_free_count;			/* count of free structures */

/*
 * Association ID.  We initialize this value randomly, then assign a new
 * value every time the peer structure is incremented.
 */
static associd_t current_association_ID; /* association ID */

/*
 * Memory allocation watermarks.
 */
#define	INIT_PEER_ALLOC		15	/* initialize for 15 peers */
#define	INC_PEER_ALLOC		5	/* when run out, add 5 more */

/*
 * Miscellaneous statistic counters which may be queried.
 */
u_long peer_timereset;			/* time stat counters zeroed */
u_long findpeer_calls;			/* calls to findpeer */
u_long assocpeer_calls;			/* calls to findpeerbyassoc */
u_long peer_allocations;		/* allocations from free list */
u_long peer_demobilizations;		/* structs freed to free list */
int total_peer_structs;			/* peer structs */
int peer_associations;			/* active associations */
static struct peer init_peer_alloc[INIT_PEER_ALLOC]; /* init alloc */

static	void	getmorepeermem	P((void));

/*
 * init_peer - initialize peer data structures and counters
 *
 * N.B. We use the random number routine in here. It had better be
 * initialized prior to getting here.
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
	current_association_ID = (associd_t)ranp2(16);
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

	peer = (struct peer *)emalloc(INC_PEER_ALLOC *
	    sizeof(struct peer));
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
	struct sockaddr_storage *addr,
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
		if (SOCKCMP(addr, &peer->srcadr)
		    && NSRCPORT(addr) == NSRCPORT(&peer->srcadr)) {
			if (mode == -1)
				return (peer);
			else if (peer->hmode == mode)
				break;
		}
		peer = peer->next;
	}
	return (peer);
}


/*
 * findpeer - find and return a peer in the hash table.
 */
struct peer *
findpeer(
	struct sockaddr_storage *srcadr,
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
	for (peer = peer_hash[hash]; peer != NULL; peer = peer->next) {
		if (SOCKCMP(srcadr, &peer->srcadr)
		    && NSRCPORT(srcadr) == NSRCPORT(&peer->srcadr)) {

			/*
			 * if the association matching rules determine
			 * that this is not a valid combination, then
			 * look for the next valid peer association.
			 */
			*action = MATCH_ASSOC(peer->hmode, pkt_mode);

			/*
			 * Sigh!  Check if BCLIENT peer in client
			 * server mode, else return error.
			 */
			if ((*action == AM_POSSBCL) && !(peer->flags &
			    FLAG_MCAST))
				*action = AM_ERR;

			/*
			 * if an error was returned, exit back right
			 * here.
			 */
			if (*action == AM_ERR)
				return ((struct peer *)0);

			/*
			 * if a match is found, we stop our search.
			 */
			if (*action != AM_NOMATCH)
				break;
		}
	}

	/*
	 * If no matching association is found
	 */
	if (peer == 0) {
		*action = MATCH_ASSOC(NO_PEER, pkt_mode);
		return ((struct peer *)0);
	}
	peer->dstadr = dstadr;
	return (peer);
}

/*
 * findpeerbyassocid - find and return a peer using his association ID
 */
struct peer *
findpeerbyassoc(
	u_int assoc
	)
{
	register struct peer *peer;
	int hash;

	assocpeer_calls++;

	hash = assoc & HASH_MASK;
	for (peer = assoc_hash[hash]; peer != 0; peer =
	    peer->ass_next) {
		if (assoc == peer->associd)
		    return (peer);
	}
	return (NULL);
}


/*
 * clear_all - flush all time values for all associations
 */
void
clear_all(void)
{
	struct peer *peer, *next_peer;
	int n;

	/*
	 * This routine is called when the clock is stepped, and so all
	 * previously saved time values are untrusted.
	 */
	for (n = 0; n < HASH_SIZE; n++) {
		for (peer = peer_hash[n]; peer != 0; peer = next_peer) {
			next_peer = peer->next;
			if (peer->flags & FLAG_CONFIG) {
				if (!(peer->cast_flags & (MDF_ACAST |
				     MDF_MCAST | MDF_BCAST)))
					peer_clear(peer, "STEP");
			} else {
				unpeer(peer);
			}
		}
	}
#ifdef DEBUG
	if (debug)
		printf("clear_all: at %lu\n", current_time);
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
#ifdef OPENSSL
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */

	if (peer_to_remove->flags & FLAG_SKEY) {
		sprintf(statstr, "unpeer %d flash %x reach %03o flags %04x",
		    peer_to_remove->associd, peer_to_remove->flash,
		    peer_to_remove->reach, peer_to_remove->flags);
		record_crypto_stats(&peer_to_remove->srcadr, statstr);
#ifdef DEBUG
		if (debug)
			printf("peer: %s\n", statstr);
#endif
	}
#endif /* OPENSSL */
#ifdef DEBUG
	if (debug)
		printf("demobilize %u %d\n", peer_to_remove->associd,
		    peer_associations);
#endif
	peer_clear(peer_to_remove, "NULL");
	hash = HASH_ADDR(&peer_to_remove->srcadr);
	peer_hash_count[hash]--;
	peer_demobilizations++;
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
				stoa(&peer->srcadr));
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
				stoa(&peer->srcadr));
		} else {
			peer->ass_next = peer_to_remove->ass_next;
		}
	}
	peer_to_remove->next = peer_free;
	peer_free = peer_to_remove;
	peer_free_count++;
	peer_associations--;
}


/*
 * peer_config - configure a new association
 */
struct peer *
peer_config(
	struct sockaddr_storage *srcadr,
	struct interface *dstadr,
	int hmode,
	int version,
	int minpoll,
	int maxpoll,
	u_int flags,
	int ttl,
	keyid_t key,
	u_char *keystr
	)
{
	register struct peer *peer;
	u_char cast_flags;

	/*
	 * First search from the beginning for an association with given
	 * remote address and mode. If an interface is given, search
	 * from there to find the association which matches that
	 * destination.
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
	 * We do a dirty little jig to figure the cast flags. This is
	 * probably not the best place to do this, at least until the
	 * configure code is rebuilt. Note only one flag can be set.
	 */
	switch (hmode) {

	case MODE_BROADCAST:
		if(srcadr->ss_family == AF_INET) {
			if (IN_CLASSD(ntohl(((struct sockaddr_in*)srcadr)->sin_addr.s_addr)))
				cast_flags = MDF_MCAST;
			else
				cast_flags = MDF_BCAST;
			break;
		}
		else {
                        if (IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6*)srcadr)->sin6_addr))
        	                cast_flags = MDF_MCAST;
	        	else
                        	cast_flags = MDF_BCAST;
                	break;
                }

	case MODE_CLIENT:
		if(srcadr->ss_family == AF_INET) {
			if (IN_CLASSD(ntohl(((struct sockaddr_in*)srcadr)->sin_addr.s_addr)))
				cast_flags = MDF_ACAST;
			else
				cast_flags = MDF_UCAST;
			break;
		}
		else {
			if (IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6*)srcadr)->sin6_addr))
				cast_flags = MDF_ACAST;
			else
				cast_flags = MDF_UCAST;
			break;
		}

	default:
		cast_flags = MDF_UCAST;
	}

	/*
	 * If the peer is already configured, some dope has a duplicate
	 * configureation entry or another dope is wiggling from afar.
	 */
	if (peer != 0) {
		peer->hmode = (u_char)hmode;
		peer->version = (u_char) version;
		peer->minpoll = (u_char) minpoll;
		peer->maxpoll = (u_char) maxpoll;
		peer->flags = flags | FLAG_CONFIG |
			(peer->flags & FLAG_REFCLOCK);
		peer->cast_flags = cast_flags;
		peer->ttl = (u_char) ttl;
		peer->keyid = key;
		peer->precision = sys_precision;
		peer_clear(peer, "RMOT");
		return (peer);
	}

	/*
	 * Here no match has been found, so presumably this is a new
	 * persistent association. Mobilize the thing and initialize its
	 * variables. If emulating ntpdate, force iburst.
	 */
	if (mode_ntpdate)
		flags |= FLAG_IBURST;
	peer = newpeer(srcadr, dstadr, hmode, version, minpoll, maxpoll,
	    flags | FLAG_CONFIG, cast_flags, ttl, key);
	return (peer);
}


/*
 * newpeer - initialize a new peer association
 */
struct peer *
newpeer(
	struct sockaddr_storage *srcadr,
	struct interface *dstadr,
	int hmode,
	int version,
	int minpoll,
	int maxpoll,
	u_int flags,
	u_char cast_flags,
	int ttl,
	keyid_t key
	)
{
	register struct peer *peer;
	register int i;
#ifdef OPENSSL
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */
#endif /* OPENSSL */

	/*
	 * Allocate a new peer structure. Some dirt here, since some of
	 * the initialization requires knowlege of our system state.
	 */
	if (peer_free_count == 0)
		getmorepeermem();
	peer = peer_free;
	peer_free = peer->next;
	peer_free_count--;
	peer_associations++;
	memset((char *)peer, 0, sizeof(struct peer));

	/*
	 * Assign an association ID and increment the system variable.
	 */
	peer->associd = current_association_ID;
	if (++current_association_ID == 0)
		++current_association_ID;

	/*
	 * Initialize the peer structure and dance the interface jig.
	 * Reference clocks step the loopback waltz, the others
	 * squaredance around the interface list looking for a buddy. If
	 * the dance peters out, there is always the wildcard interface.
	 * This might happen in some systems and would preclude proper
	 * operation with public key cryptography.
	 */
	if (ISREFCLOCKADR(srcadr))
		peer->dstadr = loopback_interface;
	else if (cast_flags & (MDF_BCLNT | MDF_ACAST | MDF_MCAST | MDF_BCAST)) {
		peer->dstadr = findbcastinter(srcadr);
		/*
		 * If it was a multicast packet, findbcastinter() may not
		 * find it, so try a little harder.
		 */
		if (peer->dstadr == ANY_INTERFACE_CHOOSE(srcadr))
			peer->dstadr = findinterface(srcadr);
	} else if (dstadr != NULL && dstadr != ANY_INTERFACE_CHOOSE(srcadr))
		peer->dstadr = dstadr;
	else
		peer->dstadr = findinterface(srcadr);
	peer->srcadr = *srcadr;
	peer->hmode = (u_char)hmode;
	peer->version = (u_char)version;
	peer->minpoll = (u_char)max(NTP_MINPOLL, minpoll);
	peer->maxpoll = (u_char)min(NTP_MAXPOLL, maxpoll);
	peer->flags = flags;
	if (key != 0)
		peer->flags |= FLAG_AUTHENABLE;
	if (key > NTP_MAXKEY)
		peer->flags |= FLAG_SKEY;
	peer->cast_flags = cast_flags;
	peer->ttl = (u_char)ttl;
	peer->keyid = key;
	peer->precision = sys_precision;
	if (cast_flags & MDF_ACAST)
		peer_clear(peer, "ACST");
	else if (cast_flags & MDF_MCAST)
		peer_clear(peer, "MCST");
	else if (cast_flags & MDF_BCAST)
		peer_clear(peer, "BCST");
	else
		peer_clear(peer, "INIT");
	if (mode_ntpdate)
		peer_ntpdate++;

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
			return (NULL);
		}
	}
#endif

	/*
	 * Put the new peer in the hash tables.
	 */
	i = HASH_ADDR(&peer->srcadr);
	peer->next = peer_hash[i];
	peer_hash[i] = peer;
	peer_hash_count[i]++;
	i = peer->associd & HASH_MASK;
	peer->ass_next = assoc_hash[i];
	assoc_hash[i] = peer;
	assoc_hash_count[i]++;
#ifdef OPENSSL
	if (peer->flags & FLAG_SKEY) {
		sprintf(statstr, "newpeer %d", peer->associd);
		record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
		if (debug)
			printf("peer: %s\n", statstr);
#endif
	}
#endif /* OPENSSL */
#ifdef DEBUG
	if (debug)
		printf(
		    "newpeer: %s->%s mode %d vers %d poll %d %d flags 0x%x 0x%x ttl %d key %08x\n",
		    peer->dstadr == NULL ? "null" : stoa(&peer->dstadr->sin),
		    stoa(&peer->srcadr),
		    peer->hmode, peer->version, peer->minpoll,
		    peer->maxpoll, peer->flags, peer->cast_flags,
		    peer->ttl, peer->keyid);
#endif
	return (peer);
}


/*
 * peer_unconfig - remove the configuration bit from a peer
 */
int
peer_unconfig(
	struct sockaddr_storage *srcadr,
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
			 * Tricky stuff here. If the peer is polling us
			 * in active mode, turn off the configuration
			 * bit and make the mode passive. This allows us
			 * to avoid dumping a lot of history for peers
			 * we might choose to keep track of in passive
			 * mode. The protocol will eventually terminate
			 * undesirables on its own.
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
	return (num_found);
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
	peer->rank = 0;
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


#ifdef OPENSSL
/*
 * expire_all - flush all crypto data and update timestamps.
 */
void
expire_all(void)
{
	struct peer *peer, *next_peer;
	int n;

	/*
	 * This routine is called about once per day from the timer
	 * routine and when the client is first synchronized. Search the
	 * peer list for all associations and flush only the key list
	 * and cookie. If a manycast client association, flush
	 * everything. Then, recompute and sign the agreement public
	 * value, if present.
	 */
	if (!crypto_flags)
		return;
	for (n = 0; n < HASH_SIZE; n++) {
		for (peer = peer_hash[n]; peer != 0; peer = next_peer) {
			next_peer = peer->next;
			if (!(peer->flags & FLAG_SKEY)) {
				continue;
			} else if (peer->cast_flags & MDF_ACAST) {
				peer_clear(peer, "ACST");
			} else if (peer->hmode == MODE_ACTIVE ||
			    peer->hmode == MODE_PASSIVE) {
				key_expire(peer);
				peer->crypto &= ~(CRYPTO_FLAG_AUTO |
				    CRYPTO_FLAG_AGREE);
			}
				
		}
	}
	RAND_bytes((u_char *)&sys_private, 4);
	crypto_update();
	resetmanycast();
}
#endif /* OPENSSL */


/*
 * findmanycastpeer - find and return a manycast peer
 */
struct peer *
findmanycastpeer(
	struct recvbuf *rbufp
	)
{
	register struct peer *peer;
	struct pkt *pkt;
	l_fp p_org;
	int i;

 	/*
 	 * This routine is called upon arrival of a client-mode message
	 * from a manycast server. Search the peer list for a manycast
	 * client association where the last transmit timestamp matches
	 * the originate timestamp. This assumes the transmit timestamps
	 * for possibly more than one manycast association are unique.
	 */
	pkt = &rbufp->recv_pkt;
	for (i = 0; i < HASH_SIZE; i++) {
		if (peer_hash_count[i] == 0)
			continue;

		for (peer = peer_hash[i]; peer != 0; peer =
		    peer->next) {
			if (peer->cast_flags & MDF_ACAST) {
				NTOHL_FP(&pkt->org, &p_org);
				if (L_ISEQU(&peer->xmt, &p_org))
					return (peer);
			}
		}
	}
	return (NULL);
}


/*
 * resetmanycast - reset all manycast clients
 */
void
resetmanycast(void)
{
	register struct peer *peer;
	int i;

	/*
	 * This routine is called when the number of client associations
	 * falls below the minimum. Search the peer list for manycast
	 * client associations and reset the ttl and poll interval.
	 */
	for (i = 0; i < HASH_SIZE; i++) {
		if (peer_hash_count[i] == 0)
			continue;

		for (peer = peer_hash[i]; peer != 0; peer =
		    peer->next) {
			if (peer->cast_flags & MDF_ACAST) {
				peer->ttl = 0;
				poll_update(peer, 0);
			}
		}
	}
}
