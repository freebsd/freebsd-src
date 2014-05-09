#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include "ntp_machine.h"
#include "ntp_fp.h"
#include "ntp_syslog.h"
#include "ntp_stdlib.h"
#include "ntp_io.h"
#include "recvbuff.h"
#include "iosignal.h"

#include <isc/list.h>
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
static ISC_LIST(recvbuf_t)	free_recv_list;	/* Currently unused buffers */
	
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

static void 
initialise_buffer(recvbuf_t *buff)
{
	memset((char *) buff, 0, sizeof(recvbuf_t));

#if defined SYS_WINNT
	buff->wsabuff.len = RX_BUFF_SIZE;
	buff->wsabuff.buf = (char *) buff->recv_buffer;
#endif
}

static void
create_buffers(int nbufs)
{
	register recvbuf_t *bufp;
	int i, abuf;

	abuf = nbufs + buffer_shortfall;
	buffer_shortfall = 0;

	bufp = (recvbuf_t *) emalloc(abuf*sizeof(recvbuf_t));

	for (i = 0; i < abuf; i++)
	{
		memset((char *) bufp, 0, sizeof(recvbuf_t));
		ISC_LIST_APPEND(free_recv_list, bufp, link);
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
	ISC_LIST_INIT(free_recv_list);
	free_recvbufs = total_recvbufs = 0;
	full_recvbufs = lowater_adds = 0;

	create_buffers(nbufs);

#if defined(SYS_WINNT)
	InitializeCriticalSection(&RecvLock);
#endif

}

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
	ISC_LIST_APPEND(free_recv_list, rb, link);
#if defined SYS_WINNT
	rb->wsabuff.len = RX_BUFF_SIZE;
	rb->wsabuff.buf = (char *) rb->recv_buffer;
#endif
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
	ISC_LIST_APPEND(full_recv_list, rb, link);
	full_recvbufs++;
	UNLOCK();
}

recvbuf_t *
get_free_recv_buffer(void)
{
	recvbuf_t * buffer = NULL;
	LOCK();
	buffer = ISC_LIST_HEAD(free_recv_list);
	if (buffer != NULL)
	{
		ISC_LIST_DEQUEUE(free_recv_list, buffer, link);
		free_recvbufs--;
		initialise_buffer(buffer);
		(buffer->used)++;
	}
	else
	{
		buffer_shortfall++;
	}
	UNLOCK();
	return (buffer);
}

#ifdef HAVE_IO_COMPLETION_PORT
recvbuf_t *
get_free_recv_buffer_alloc(void)
{
	recvbuf_t * buffer = get_free_recv_buffer();
	if (buffer == NULL)
	{
		create_buffers(RECV_INC);
		buffer = get_free_recv_buffer();
	}
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
	 * wander off to do lengthy paket processing with
	 * any buffer we grab from the full list.
	 * 
	 * fixes malloc() interrupted by SIGIO risk
	 * (Bug 889)
	 */
	rbuf = ISC_LIST_HEAD(free_recv_list);
	if (rbuf == NULL || buffer_shortfall > 0) {
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
	if (rbuf != NULL)
	{
		ISC_LIST_DEQUEUE(full_recv_list, rbuf, link);
		--full_recvbufs;
	}
	else
	{
		/*
		 * Make sure we reset the full count to 0
		 */
		full_recvbufs = 0;
	}
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
