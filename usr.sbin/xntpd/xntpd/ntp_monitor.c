/* ntp_monitor.c,v 3.1 1993/07/06 01:11:21 jbj Exp
 * ntp_monitor.c - monitor who is using the xntpd server
 */
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_if.h"
#include "ntp_stdlib.h"

/*
 * I'm still not sure I like what I've done here.  It certainly consumes
 * memory like it is going out of style, and also may not be as low
 * overhead as I'd imagined.
 *
 * Anyway, we record statistics based on source address, mode and version
 * (for now, anyway.  Check the code).  The receive procedure calls us with
 * the incoming rbufp before it does anything else.
 *
 * Each entry is doubly linked into two lists, a hash table and a
 * most-recently-used list.  When a packet arrives it is looked up
 * in the hash table.  If found, the statistics are updated and the
 * entry relinked at the head of the MRU list.  If not found, a new
 * entry is allocated, initialized and linked into both the hash
 * table and at the head of the MRU list.
 *
 * Memory is usually allocated by grabbing a big chunk of new memory
 * and cutting it up into littler pieces.  The exception to this when we
 * hit the memory limit.  Then we free memory by grabbing entries off
 * the tail for the MRU list, unlinking from the hash table, and
 * reinitializing.
 */

/*
 * Limits on the number of structures allocated.  This limit is picked
 * with the illicit knowlege that we can only return somewhat less
 * than 8K bytes in a mode 7 response packet, and that each structure
 * will require about 20 bytes of space in the response.
 */
#define	MAXMONMEM	400	/* we allocate up to 400 structures */
#define	MONMEMINC	40	/* allocate them 40 at a time */

/*
 * Hashing stuff
 */
#define	MON_HASH_SIZE	128
#define	MON_HASH_MASK	(MON_HASH_SIZE-1)
#define	MON_HASH(addr)	((int)(ntohl((addr)) & MON_HASH_MASK))

/*
 * Pointers to the hash table, the MRU list and the count table.  Memory
 * for the hash and count tables is only allocated if monitoring is turned on.
 */
static	struct mon_data *mon_hash;	/* Pointer to array of hash buckets */
static	int *mon_hash_count;		/* Point to hash count stats keeper */
	struct mon_data mon_mru_list;

/*
 * List of free structures structures, and counters of free and total
 * structures.  The free structures are linked with the hash_next field.
 */
static	struct mon_data *mon_free;

static	int mon_free_mem;		/* number of structures on free list */
static	int mon_total_mem;		/* total number of structures allocated */
static	int mon_mem_increments;		/* number of times we've called malloc() */

/*
 * Initialization state.  We may be monitoring, we may not.  If
 * we aren't, we may not even have allocated any memory yet.
 */
	int mon_enabled;
static	int mon_have_memory;

/*
 * Imported from the timer module
 */
extern U_LONG current_time;

static	void	mon_getmoremem	P((void));

/*
 * init_mon - initialize monitoring global data
 */
void
init_mon()
{
	/*
	 * Don't do much of anything here.  We don't allocate memory
	 * until someone explicitly starts us.
	 */
	mon_enabled = 0;
	mon_have_memory = 0;

	mon_free_mem = 0;
	mon_total_mem = 0;
	mon_mem_increments = 0;
	mon_free = 0;
	mon_hash = 0;
	mon_hash_count = 0;
	bzero((char *)&mon_mru_list, sizeof mon_mru_list);
}


/*
 * mon_start - start up the monitoring software
 */
void
mon_start()
{
	register struct mon_data *md;
	register int i;

	if (mon_enabled)
		return;
	
	if (!mon_have_memory) {
		mon_hash = (struct mon_data *)
		    emalloc(MON_HASH_SIZE * sizeof(struct mon_data));
		bzero((char *)mon_hash, MON_HASH_SIZE*sizeof(struct mon_data));
		mon_hash_count = (int *)emalloc(MON_HASH_SIZE * sizeof(int));
		mon_free_mem = 0;
		mon_total_mem = 0;
		mon_mem_increments = 0;
		mon_free = 0;
		mon_getmoremem();
		mon_have_memory = 1;
	}

	md = mon_hash;
	for (i = 0; i < MON_HASH_SIZE; i++, md++) {
		md->hash_next = md;
		md->hash_prev = md;
		*(mon_hash_count + i) = 0;
	}

	mon_mru_list.mru_next = &mon_mru_list;
	mon_mru_list.mru_prev = &mon_mru_list;

	mon_enabled = 1;
}


/*
 * mon_stop - stop the monitoring software
 */
void
mon_stop()
{
	register struct mon_data *md;
	register int i;

	if (!mon_enabled)
		return;
	
	/*
	 * Put everything back on the free list
	 */
	md = mon_hash;
	for (i = 0; i < MON_HASH_SIZE; i++, md++) {
		if (md->hash_next != md) {
			md->hash_prev->hash_next = mon_free;
			mon_free = md->hash_next;
			mon_free_mem += *(mon_hash_count + i);
			md->hash_next = md;
			md->hash_prev = md;
			*(mon_hash_count + i) = 0;
		}
	}

	mon_mru_list.mru_next = &mon_mru_list;
	mon_mru_list.mru_prev = &mon_mru_list;

	mon_enabled = 0;
}


/*
 * monitor - record stats about this packet
 */
void
monitor(rbufp)
	struct recvbuf *rbufp;
{
	register struct pkt *pkt;
	register struct mon_data *md;
	register U_LONG netnum;
	register int hash;
	register int mode;
	register struct mon_data *mdhash;

	if (!mon_enabled)
		return;

	pkt = &rbufp->recv_pkt;
	netnum = NSRCADR(&rbufp->recv_srcadr);
	hash = MON_HASH(netnum);
	mode = PKT_MODE(pkt->li_vn_mode);

	md = (mon_hash + hash)->hash_next;
	while (md != (mon_hash + hash)) {
		if (md->rmtadr == netnum && md->mode == (u_char)mode) {
			md->lasttime = current_time;
			md->count++;
			md->version = PKT_VERSION(pkt->li_vn_mode);
			md->rmtport = NSRCPORT(&rbufp->recv_srcadr);

			/*
			 * Shuffle him to the head of the
			 * mru list.  What a crock.
			 */
			md->mru_next->mru_prev = md->mru_prev;
			md->mru_prev->mru_next = md->mru_next;
			md->mru_next = mon_mru_list.mru_next;
			md->mru_prev = &mon_mru_list;
			mon_mru_list.mru_next->mru_prev = md;
			mon_mru_list.mru_next = md;
			return;
		}
		md = md->hash_next;
	}

	/*
	 * If we got here, this is the first we've heard of this
	 * guy.  Get him some memory, either from the free list
	 * or from the tail of the MRU list.
	 */
	if (mon_free_mem == 0 && mon_total_mem >= MAXMONMEM) {
		/*
		 * Get it from MRU list
		 */
		md = mon_mru_list.mru_prev;
		md->mru_prev->mru_next = &mon_mru_list;
		mon_mru_list.mru_prev = md->mru_prev;
		md->hash_next->hash_prev = md->hash_prev;
		md->hash_prev->hash_next = md->hash_next;
		*(mon_hash_count + MON_HASH(md->rmtadr)) -= 1;
	} else {
		if (mon_free_mem == 0)
			mon_getmoremem();
		md = mon_free;
		mon_free = md->hash_next;
		mon_free_mem--;
	}

	/*
	 * Got one, initialize it
	 */
	md->lasttime = md->firsttime = current_time;
	md->count = 1;
	md->rmtadr = netnum;
	md->rmtport = NSRCPORT(&rbufp->recv_srcadr);
	md->mode = (u_char) mode;
	md->version = PKT_VERSION(pkt->li_vn_mode);

	/*
	 * Shuffle him into the hash table, inserting him at the
	 * end.  Also put him on top of the MRU list.
	 */
	mdhash = mon_hash + MON_HASH(netnum);
	md->hash_next = mdhash;
	md->hash_prev = mdhash->hash_prev;
	mdhash->hash_prev->hash_next = md;
	mdhash->hash_prev = md;
	*(mon_hash_count + MON_HASH(netnum)) += 1;

	md->mru_next = mon_mru_list.mru_next;
	md->mru_prev = &mon_mru_list;
	mon_mru_list.mru_next->mru_prev = md;
	mon_mru_list.mru_next = md;
}


/*
 * mon_getmoremem - get more memory and put it on the free list
 */
static void
mon_getmoremem()
{
	register struct mon_data *md;
	register int i;
	struct mon_data *freedata;

	md = (struct mon_data *)emalloc(MONMEMINC * sizeof(struct mon_data));
	freedata = mon_free;
	mon_free = md;

	for (i = 0; i < (MONMEMINC-1); i++) {
		md->hash_next = (md + 1);
		md++;
	}

	/*
	 * md now points at the last.  Link in the rest of the chain.
	 */
	md->hash_next = freedata;

	mon_free_mem += MONMEMINC;
	mon_total_mem += MONMEMINC;
	mon_mem_increments++;
}
