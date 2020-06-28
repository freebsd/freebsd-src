#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>

#include "ntp_assert.h"
#include "ntp_syslog.h"
#include "ntp_stdlib.h"
#include "ntp_lists.h"
#include "recvbuff.h"
#include "iosignal.h"

#if (RECV_INC & (RECV_INC-1))
# error RECV_INC not a power of 2!
#endif
#if (RECV_BATCH & (RECV_BATCH - 1))
#error RECV_BATCH not a power of 2!
#endif
#if (RECV_BATCH < RECV_INC)
#error RECV_BATCH must be >= RECV_INC!
#endif

/*
 * Memory allocation
 */
static u_long volatile full_recvbufs;	/* recvbufs on full_recv_fifo */
static u_long volatile free_recvbufs;	/* recvbufs on free_recv_list */
static u_long volatile total_recvbufs;	/* total recvbufs currently in use */
static u_long volatile lowater_adds;	/* number of times we have added memory */
static u_long volatile buffer_shortfall;/* number of missed free receive buffers
					   between replenishments */
static u_long limit_recvbufs;		/* maximum total of receive buffers */
static u_long emerg_recvbufs;		/* emergency/urgent buffers to keep */

static DECL_FIFO_ANCHOR(recvbuf_t) full_recv_fifo;
static recvbuf_t *		   free_recv_list;
	
#if defined(SYS_WINNT)

/*
 * For Windows we need to set up a lock to manipulate the
 * recv buffers to prevent corruption. We keep it lock for as
 * short a time as possible
 */
static CRITICAL_SECTION RecvLock;
static CRITICAL_SECTION FreeLock;
# define LOCK_R()	EnterCriticalSection(&RecvLock)
# define UNLOCK_R()	LeaveCriticalSection(&RecvLock)
# define LOCK_F()	EnterCriticalSection(&FreeLock)
# define UNLOCK_F()	LeaveCriticalSection(&FreeLock)
#else
# define LOCK_R()	do {} while (FALSE)
# define UNLOCK_R()	do {} while (FALSE)
# define LOCK_F()	do {} while (FALSE)
# define UNLOCK_F()	do {} while (FALSE)
#endif

#ifdef DEBUG
static void uninit_recvbuff(void);
#endif


u_long
free_recvbuffs (void)
{
	return free_recvbufs;
}

u_long
full_recvbuffs (void)
{
	return full_recvbufs;
}

u_long
total_recvbuffs (void)
{
	return total_recvbufs;
}

u_long
lowater_additions(void)
{
	return lowater_adds;
}

static inline void 
initialise_buffer(recvbuf_t *buff)
{
	ZERO(*buff);
}

static void
create_buffers(
	size_t		nbufs)
{
#   ifndef DEBUG
	static const u_int chunk = RECV_INC;
#   else
	/* Allocate each buffer individually so they can be free()d
	 * during ntpd shutdown on DEBUG builds to keep them out of heap
	 * leak reports.
	 */
	static const u_int chunk = 1;
#   endif

	register recvbuf_t *bufp;
	u_int i;
	size_t abuf;

	if (limit_recvbufs <= total_recvbufs)
		return;
	
	abuf = nbufs + buffer_shortfall;
	buffer_shortfall = 0;

	if (abuf < nbufs || abuf > RECV_BATCH)
		abuf = RECV_BATCH;	/* clamp on overflow */
	else
		abuf += (~abuf + 1) & (RECV_INC - 1);	/* round up */
	
	if (abuf > (limit_recvbufs - total_recvbufs))
		abuf = limit_recvbufs - total_recvbufs;
	abuf += (~abuf + 1) & (chunk - 1);		/* round up */
	
	while (abuf) {
		bufp = calloc(chunk, sizeof(*bufp));
		if (!bufp) {
			limit_recvbufs = total_recvbufs;
			break;
		}
		for (i = chunk; i; --i,++bufp) {
			LINK_SLIST(free_recv_list, bufp, link);
		}
		free_recvbufs += chunk;
		total_recvbufs += chunk;
		abuf -= chunk;
	}
	++lowater_adds;
}

void
init_recvbuff(int nbufs)
{

	/*
	 * Init buffer free list and stat counters
	 */
	free_recvbufs = total_recvbufs = 0;
	full_recvbufs = lowater_adds = 0;

	limit_recvbufs = RECV_TOOMANY;
	emerg_recvbufs = RECV_CLOCK;

	create_buffers(nbufs);

#   if defined(SYS_WINNT)
	InitializeCriticalSection(&RecvLock);
	InitializeCriticalSection(&FreeLock);
#   endif

#   ifdef DEBUG
	atexit(&uninit_recvbuff);
#   endif
}


#ifdef DEBUG
static void
uninit_recvbuff(void)
{
	recvbuf_t *rbunlinked;

	for (;;) {
		UNLINK_FIFO(rbunlinked, full_recv_fifo, link);
		if (rbunlinked == NULL)
			break;
		free(rbunlinked);
	}

	for (;;) {
		UNLINK_HEAD_SLIST(rbunlinked, free_recv_list, link);
		if (rbunlinked == NULL)
			break;
		free(rbunlinked);
	}
#   if defined(SYS_WINNT)
	DeleteCriticalSection(&FreeLock);
	DeleteCriticalSection(&RecvLock);
#   endif
}
#endif	/* DEBUG */


/*
 * freerecvbuf - make a single recvbuf available for reuse
 */
void
freerecvbuf(recvbuf_t *rb)
{
	if (rb) {
		if (--rb->used != 0) {
			msyslog(LOG_ERR, "******** freerecvbuff non-zero usage: %d *******", rb->used);
			rb->used = 0;
		}
		LOCK_F();
		LINK_SLIST(free_recv_list, rb, link);
		++free_recvbufs;
		UNLOCK_F();
	}
}

	
void
add_full_recv_buffer(recvbuf_t *rb)
{
	if (rb == NULL) {
		msyslog(LOG_ERR, "add_full_recv_buffer received NULL buffer");
		return;
	}
	LOCK_R();
	LINK_FIFO(full_recv_fifo, rb, link);
	++full_recvbufs;
	UNLOCK_R();
}


recvbuf_t *
get_free_recv_buffer(
    int /*BOOL*/ urgent
    )
{
	recvbuf_t *buffer = NULL;

	LOCK_F();
	if (free_recvbufs > (urgent ? emerg_recvbufs : 0)) {
		UNLINK_HEAD_SLIST(buffer, free_recv_list, link);
	}
	
	if (buffer != NULL) {
		if (free_recvbufs)
			--free_recvbufs;
		initialise_buffer(buffer);
		++buffer->used;
	} else {
		++buffer_shortfall;
	}
	UNLOCK_F();

	return buffer;
}


#ifdef HAVE_IO_COMPLETION_PORT
recvbuf_t *
get_free_recv_buffer_alloc(
    int /*BOOL*/ urgent
    )
{
	LOCK_F();	
	if (free_recvbufs <= emerg_recvbufs || buffer_shortfall > 0)
		create_buffers(RECV_INC);
	UNLOCK_F();
	return get_free_recv_buffer(urgent);
}
#endif


recvbuf_t *
get_full_recv_buffer(void)
{
	recvbuf_t *	rbuf;

	/*
	 * make sure there are free buffers when we wander off to do
	 * lengthy packet processing with any buffer we grab from the
	 * full list.
	 * 
	 * fixes malloc() interrupted by SIGIO risk (Bug 889)
	 */
	LOCK_F();	
	if (free_recvbufs <= emerg_recvbufs || buffer_shortfall > 0)
		create_buffers(RECV_INC);
	UNLOCK_F();

	/*
	 * try to grab a full buffer
	 */
	LOCK_R();
	UNLINK_FIFO(rbuf, full_recv_fifo, link);
	if (rbuf != NULL && full_recvbufs)
		--full_recvbufs;
	UNLOCK_R();

	return rbuf;
}


/*
 * purge_recv_buffers_for_fd() - purges any previously-received input
 *				 from a given file descriptor.
 */
void
purge_recv_buffers_for_fd(
	int	fd
	)
{
	recvbuf_t *rbufp;
	recvbuf_t *next;
	recvbuf_t *punlinked;
	recvbuf_t *freelist = NULL;

	/* We want to hold only one lock at a time. So we do a scan on
	 * the full buffer queue, collecting items as we go, and when
	 * done we spool the the collected items to 'freerecvbuf()'.
	 */
	LOCK_R();

	for (rbufp = HEAD_FIFO(full_recv_fifo);
	     rbufp != NULL;
	     rbufp = next)
	{
		next = rbufp->link;
#	    ifdef HAVE_IO_COMPLETION_PORT
		if (rbufp->dstadr == NULL && rbufp->fd == fd)
#	    else
		if (rbufp->fd == fd)
#	    endif
		{
			UNLINK_MID_FIFO(punlinked, full_recv_fifo,
					rbufp, link, recvbuf_t);
			INSIST(punlinked == rbufp);
			if (full_recvbufs)
				--full_recvbufs;
			rbufp->link = freelist;
			freelist = rbufp;
		}
	}

	UNLOCK_R();
	
	while (freelist) {
		next = freelist->link;
		freerecvbuf(freelist);
		freelist = next;
	}
}


/*
 * Checks to see if there are buffers to process
 */
isc_boolean_t has_full_recv_buffer(void)
{
	if (HEAD_FIFO(full_recv_fifo) != NULL)
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}


#ifdef NTP_DEBUG_LISTS_H
void
check_gen_fifo_consistency(void *fifo)
{
	gen_fifo *pf;
	gen_node *pthis;
	gen_node **pptail;

	pf = fifo;
	REQUIRE((NULL == pf->phead && NULL == pf->pptail) ||
		(NULL != pf->phead && NULL != pf->pptail));

	pptail = &pf->phead;
	for (pthis = pf->phead;
	     pthis != NULL;
	     pthis = pthis->link)
		if (NULL != pthis->link)
			pptail = &pthis->link;

	REQUIRE(NULL == pf->pptail || pptail == pf->pptail);
}
#endif	/* NTP_DEBUG_LISTS_H */
