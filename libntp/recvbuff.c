#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include "ntp_machine.h"
#include "ntp_assert.h"
#include "ntp_fp.h"
#include "ntp_syslog.h"
#include "ntp_stdlib.h"
#include "ntp_io.h"
#include "ntp_lists.h"
#include "recvbuff.h"
#include "iosignal.h"



#ifdef DEBUG
static void uninit_recvbuff(void);
#endif

/*
 * Memory allocation
 */
static u_long volatile full_recvbufs;	/* number of recvbufs on fulllist */
static u_long volatile free_recvbufs;	/* number of recvbufs on freelist */
static u_long volatile total_recvbufs;	/* total recvbufs currently in use */
static u_long volatile lowater_adds;	/* number of times we have added memory */
static u_long volatile buffer_shortfall;/* number of missed free receive buffers
					   between replenishments */

static ISC_LIST(recvbuf_t)	full_recv_list;	/* Currently used recv buffers */
static recvbuf_t *		free_recv_list;	/* Currently unused buffers */
	
#if defined(SYS_WINNT)

/*
 * For Windows we need to set up a lock to manipulate the
 * recv buffers to prevent corruption. We keep it lock for as
 * short a time as possible
 */
static CRITICAL_SECTION RecvLock;
# define LOCK()		EnterCriticalSection(&RecvLock)
# define UNLOCK()	LeaveCriticalSection(&RecvLock)
#else
# define LOCK()	
# define UNLOCK()	
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
	memset(buff, 0, sizeof(*buff));
}

static void
create_buffers(int nbufs)
{
	register recvbuf_t *bufp;
	int i, abuf;

	abuf = nbufs + buffer_shortfall;
	buffer_shortfall = 0;

#ifndef DEBUG
	bufp = emalloc(abuf * sizeof(*bufp));
#endif

	for (i = 0; i < abuf; i++) {
#ifdef DEBUG
		/*
		 * Allocate each buffer individually so they can be
		 * free()d during ntpd shutdown on DEBUG builds to
		 * keep them out of heap leak reports.
		 */
		bufp = emalloc(sizeof(*bufp));
#endif
		memset(bufp, 0, sizeof(*bufp));
		LINK_SLIST(free_recv_list, bufp, link.next);
		bufp++;
		free_recvbufs++;
		total_recvbufs++;
	}
	lowater_adds++;
}

void
init_recvbuff(int nbufs)
{

	/*
	 * Init buffer free list and stat counters
	 */
	ISC_LIST_INIT(full_recv_list);
	free_recvbufs = total_recvbufs = 0;
	full_recvbufs = lowater_adds = 0;

	create_buffers(nbufs);

#if defined(SYS_WINNT)
	InitializeCriticalSection(&RecvLock);
#endif

#ifdef DEBUG
	atexit(&uninit_recvbuff);
#endif
}


#ifdef DEBUG
static void
uninit_recvbuff(void)
{
	recvbuf_t *rbunlinked;

	while ((rbunlinked = ISC_LIST_HEAD(full_recv_list)) != NULL) {
		ISC_LIST_DEQUEUE_TYPE(full_recv_list, rbunlinked, link, recvbuf_t);
		free(rbunlinked);
	}

	do {
		UNLINK_HEAD_SLIST(rbunlinked, free_recv_list, link.next);
		if (rbunlinked != NULL)
			free(rbunlinked);
	} while (rbunlinked != NULL);
}
#endif	/* DEBUG */


/*
 * freerecvbuf - make a single recvbuf available for reuse
 */
void
freerecvbuf(recvbuf_t *rb)
{
	if (rb == NULL) {
		msyslog(LOG_ERR, "freerecvbuff received NULL buffer");
		return;
	}

	LOCK();
	(rb->used)--;
	if (rb->used != 0)
		msyslog(LOG_ERR, "******** freerecvbuff non-zero usage: %d *******", rb->used);
	LINK_SLIST(free_recv_list, rb, link.next);
	free_recvbufs++;
	UNLOCK();
}

	
void
add_full_recv_buffer(recvbuf_t *rb)
{
	if (rb == NULL) {
		msyslog(LOG_ERR, "add_full_recv_buffer received NULL buffer");
		return;
	}
	LOCK();
	ISC_LINK_INIT(rb, link);
	ISC_LIST_APPEND(full_recv_list, rb, link);
	full_recvbufs++;
	UNLOCK();
}

recvbuf_t *
get_free_recv_buffer(void)
{
	recvbuf_t *buffer;

	LOCK();
	UNLINK_HEAD_SLIST(buffer, free_recv_list, link.next);
	if (buffer != NULL) {
		free_recvbufs--;
		initialise_buffer(buffer);
		(buffer->used)++;
	} else
		buffer_shortfall++;
	UNLOCK();
	return (buffer);
}

#ifdef HAVE_IO_COMPLETION_PORT
recvbuf_t *
get_free_recv_buffer_alloc(void)
{
	recvbuf_t *buffer;
	
	buffer = get_free_recv_buffer();
	if (NULL == buffer) {
		create_buffers(RECV_INC);
		buffer = get_free_recv_buffer();
	}
	NTP_ENSURE(buffer != NULL);
	return (buffer);
}
#endif

recvbuf_t *
get_full_recv_buffer(void)
{
	recvbuf_t *rbuf;
	LOCK();
	
#ifdef HAVE_SIGNALED_IO
	/*
	 * make sure there are free buffers when we
	 * wander off to do lengthy packet processing with
	 * any buffer we grab from the full list.
	 * 
	 * fixes malloc() interrupted by SIGIO risk
	 * (Bug 889)
	 */
	if (NULL == free_recv_list || buffer_shortfall > 0) {
		/*
		 * try to get us some more buffers
		 */
		create_buffers(RECV_INC);
	}
#endif

	/*
	 * try to grab a full buffer
	 */
	rbuf = ISC_LIST_HEAD(full_recv_list);
	if (rbuf != NULL) {
		ISC_LIST_DEQUEUE_TYPE(full_recv_list, rbuf, link, recvbuf_t);
		--full_recvbufs;
	} else
		/*
		 * Make sure we reset the full count to 0
		 */
		full_recvbufs = 0;
	UNLOCK();
	return (rbuf);
}

/*
 * Checks to see if there are buffers to process
 */
isc_boolean_t has_full_recv_buffer(void)
{
	if (ISC_LIST_HEAD(full_recv_list) != NULL)
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}
