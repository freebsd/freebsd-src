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

/*
 * Memory allocation
 */
static u_long volatile full_recvbufs;		/* number of recvbufs on fulllist */
static u_long volatile free_recvbufs;		/* number of recvbufs on freelist */
static u_long volatile total_recvbufs;		/* total recvbufs currently in use */
static u_long volatile lowater_adds;	/* number of times we have added memory */

static	struct recvbuf *volatile freelist;  /* free buffers */
static	struct recvbuf *volatile fulllist;  /* lifo buffers with data */
static	struct recvbuf *volatile beginlist; /* fifo buffers with data */
	
#if defined(HAVE_IO_COMPLETION_PORT)
static CRITICAL_SECTION RecvCritSection;
# define RECV_BLOCK_IO()	EnterCriticalSection(&RecvCritSection)
# define RECV_UNBLOCK_IO()	LeaveCriticalSection(&RecvCritSection)
#else
# define RECV_BLOCK_IO()	
# define RECV_UNBLOCK_IO()	
#endif

u_long
free_recvbuffs (void)
{
	return free_recvbufs;
}

u_long
full_recvbuffs (void)
{
	return free_recvbufs;
}

u_long
total_recvbuffs (void)
{
	return free_recvbufs;
}

u_long
lowater_additions(void)
{
	return lowater_adds;
}

static void 
initialise_buffer(struct recvbuf *buff)
{
	memset((char *) buff, 0, sizeof(struct recvbuf));

#if defined HAVE_IO_COMPLETION_PORT
	buff->iocompletioninfo.overlapped.hEvent = CreateEvent(NULL, FALSE,FALSE, NULL);
	buff->wsabuff.len = RX_BUFF_SIZE;
	buff->wsabuff.buf = (char *) buff->recv_buffer;
#endif
}

static void
create_buffers(void)
{
	register struct recvbuf *buf;
	int i;
	buf = (struct recvbuf *)
	    emalloc(RECV_INC*sizeof(struct recvbuf));
	for (i = 0; i < RECV_INC; i++)
	{
		initialise_buffer(buf);
		buf->next = (struct recvbuf *) freelist;
		freelist = buf;
		buf++;
	}

	free_recvbufs += RECV_INC;
	total_recvbufs += RECV_INC;
	lowater_adds++;
}

void
init_recvbuff(int nbufs)
{
	register struct recvbuf *buf;
	int i;

	/*
	 * Init buffer free list and stat counters
	 */
	freelist = 0;

	buf = (struct recvbuf *)
	    emalloc(nbufs*sizeof(struct recvbuf));
	for (i = 0; i < nbufs; i++)
	{
		initialise_buffer(buf);
		buf->next = (struct recvbuf *) freelist;
		freelist = buf;
		buf++;
	}

	fulllist = 0;
	free_recvbufs = total_recvbufs = nbufs;
	full_recvbufs = lowater_adds = 0;

#if defined(HAVE_IO_COMPLETION_PORT)
	InitializeCriticalSection(&RecvCritSection);
#endif

}


/*
 * getrecvbufs - get receive buffers which have data in them
 *
 * 
 */

struct recvbuf *
getrecvbufs(void)
{
	struct recvbuf *rb = NULL; /* nothing has arrived */;

	RECV_BLOCK_IO();
	if (full_recvbufs == 0)
	{
#ifdef DEBUG
		if (debug > 4)
		    printf("getrecvbufs called, no action here\n");
#endif
	}
	else {

		/*
		 * Get the fulllist chain and mark it empty
		 */
#ifdef DEBUG
		if (debug > 4)
		    printf("getrecvbufs returning %ld buffers\n", full_recvbufs);
#endif
		rb = beginlist;
		fulllist = 0;
		full_recvbufs = 0;

		/*
		 * Check to see if we're below the low water mark.
		 */
		if (free_recvbufs <= RECV_LOWAT)
		{
			if (total_recvbufs >= RECV_TOOMANY)
			    msyslog(LOG_ERR, "too many recvbufs allocated (%ld)",
				    total_recvbufs);
			else
			{
				create_buffers();
			}
		}
	}
	RECV_UNBLOCK_IO();

	/*
	 * Return the chain
	 */
	return rb;
}

/*
 * freerecvbuf - make a single recvbuf available for reuse
 */
void
freerecvbuf(
	struct recvbuf *rb
	)
{
	RECV_BLOCK_IO();
	BLOCKIO();
	rb->next = (struct recvbuf *) freelist;
	freelist = rb;
	free_recvbufs++;
	UNBLOCKIO();
	RECV_UNBLOCK_IO();
}

	
void
add_full_recv_buffer(
	struct recvbuf *rb
	)
{
	RECV_BLOCK_IO();
	if (full_recvbufs == 0)
	{
		beginlist = rb;
		rb->next = 0;
	}
	else
	{
		rb->next = fulllist->next;
		fulllist->next = rb;
	}
	fulllist = rb;
	full_recvbufs++;

	RECV_UNBLOCK_IO();
}

struct recvbuf *
get_free_recv_buffer(void)
{
	struct recvbuf * buffer = NULL;
	RECV_BLOCK_IO();
	if (free_recvbufs <= RECV_LOWAT)
		{
			if (total_recvbufs >= RECV_TOOMANY) {
			    msyslog(LOG_ERR, "too many recvbufs allocated (%ld)",
				    total_recvbufs);
			}
			else
			{
				create_buffers();
			}
		}

	if (free_recvbufs > 0)
	{
		buffer = freelist;
		freelist = buffer->next;
		buffer->next = NULL;
		--free_recvbufs;
	}

	RECV_UNBLOCK_IO();
	return buffer;
}

struct recvbuf *
get_full_recv_buffer(void)
{
	struct recvbuf * buffer = NULL;
	RECV_BLOCK_IO();
	if (full_recvbufs > 0) {
		--full_recvbufs;
		buffer = beginlist;
		beginlist = buffer->next;
		buffer->next = NULL;
	}
	RECV_UNBLOCK_IO();
	return buffer;
}
