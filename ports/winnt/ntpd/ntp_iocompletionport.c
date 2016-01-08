/*
-----------------------------------------------------------------------
This is the IO completion port handling for async/overlapped IO on
Windows >= Win2000.

Some notes on the implementation:

+ Only one thread is used to serve the IO completion port, for several
  reasons:

  * First, there seems to be (have been?) trouble that locked up NTPD
    when more than one thread was used for IOCPL.

  * Second, for the sake of the time stamp interpolation the threads
    must run on the same CPU as the time interpolation thread. This
    makes using more than one thread useless, as they would compete for
    the same core and create contention.

+ Some IO operations need a possibly lengthy postprocessing. Emulating
  the UN*X line discipline is currently the only but prominent example.
  To avoid the processing in the time-critical IOCPL thread, longer
  processing is offloaded the worker thread pool.

+ A fact that seems not as well-known as it should be is that all
  ressources passed to an overlapped IO operation must be considered
  owned by the OS until the result has been fetched/dequeued. This
  includes all overlapped structures and buffers involved, so cleaning
  up on shutdown must be carefully constructed. (This includes closing
  all the IO handles and waiting for the results to be dequeued.
  'CancleIo()' cannot be used since it's broken beyond repair.)

  If this is not possible, then all ressources should be dropped into
  oblivion -- otherwise "bad things (tm)" are bound to happen.

  Using a private heap that is silently dropped but not deleted is a
  good way to avoid cluttering memory stats with IO context related
  objects. Leak tracing becomes more interesting, though.


The current implementation is based on the work of Danny Mayer who improved
the original implementation and Dave Hart who improved on the serial I/O
routines. The true roots of this file seem to be shrouded by the mist of time...


This version still provides the 'user space PPS' emulation
feature.

Juergen Perlinger (perlinger@ntp.org) Feb 2012

-----------------------------------------------------------------------
*/
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_IO_COMPLETION_PORT

#include <stddef.h>
#include <stdio.h>
#include <process.h>
#include <syslog.h>
#include <limits.h>

#include "ntpd.h"
#include "ntp_machine.h"
#include "ntp_iocompletionport.h"
#include "ntp_request.h"
#include "ntp_assert.h"
#include "ntp_io.h"
#include "ntp_lists.h"


#define CONTAINEROF(p, type, member) \
	((type *)((char *)(p) - offsetof(type, member)))

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 201)		/* nonstd extension nameless union */
#endif

/*
 * ---------------------------------------------------------------------
 * storage type for PPS data (DCD change counts & times)
 * ---------------------------------------------------------------------
 */
struct PpsData {
	u_long	cc_assert;
	u_long	cc_clear;
	l_fp	ts_assert;
	l_fp	ts_clear;
};
typedef struct PpsData PPSData_t;

struct PpsDataEx {
	u_long		cov_count;
	PPSData_t	data;
};
typedef volatile struct PpsDataEx PPSDataEx_t;

/*
 * ---------------------------------------------------------------------
 * device context; uses reference counting to avoid nasty surprises.
 * Currently this stores only the PPS time stamps, but it could be
 * easily extended.
 * ---------------------------------------------------------------------
 */
#define PPS_QUEUE_LEN	8u		  /* must be power of two! */
#define PPS_QUEUE_MSK	(PPS_QUEUE_LEN-1) /* mask for easy MOD ops */

struct DeviceContext {
	volatile long	ref_count;
	volatile u_long	cov_count;
	PPSData_t	pps_data;
	PPSDataEx_t	pps_buff[PPS_QUEUE_LEN];
};

typedef struct DeviceContext DevCtx_t;

/*
 * ---------------------------------------------------------------------
 * I/O context structure
 *
 * This is an extended overlapped structure. Some fields are only used
 * for serial I/O, others are used for all operations. The serial I/O is
 * more interesting since the same context object is used for waiting,
 * actual I/O and possibly offload processing in a worker thread until
 * a complete operation cycle is done.
 *
 * In this case the I/O context is used to gather all the bits that are
 * finally needed for the processing of the buffer.
 * ---------------------------------------------------------------------
 */
//struct IoCtx;
typedef struct IoCtx      IoCtx_t;
typedef struct refclockio RIO_t;

typedef void (*IoCompleteFunc)(ULONG_PTR, IoCtx_t *);

struct IoCtx {
	OVERLAPPED		ol;		/* 'kernel' part of the context	*/
	union {
		recvbuf_t *	recv_buf;	/* incoming -> buffer structure	*/
		void *		trans_buf;	/* outgoing -> char array	*/
		PPSData_t *	pps_buf;	/* for reading PPS seq/stamps	*/
		HANDLE		ppswake;	/* pps wakeup for attach	*/
	};
	IoCompleteFunc		onIoDone;	/* HL callback to execute	*/
	RIO_t *			rio;		/* RIO backlink (for offload)	*/
	DevCtx_t *		devCtx;
	l_fp			DCDSTime;	/* PPS-hack: time of DCD ON	*/
	l_fp			FlagTime;	/* timestamp of flag/event char */
	l_fp			RecvTime;	/* timestamp of callback        */
	DWORD			errCode;	/* error code of last I/O	*/
	DWORD			byteCount;	/* byte count     "             */
	DWORD			com_events;	/* buffer for COM events	*/
	unsigned int		flRawMem : 1;	/* buffer is raw memory -> free */
	unsigned int		flTsDCDS : 1;	/* DCDSTime valid?		*/
	unsigned int		flTsFlag : 1;	/* FlagTime valid?		*/
};

#ifdef _MSC_VER
# pragma warning(pop)
#endif

/*
 * local function definitions
 */
static		void ntpd_addremove_semaphore(HANDLE, int);
static inline	void set_serial_recv_time    (recvbuf_t *, IoCtx_t *);

/* Initiate/Request async IO operations */
static	BOOL QueueSerialWait   (RIO_t *, recvbuf_t *, IoCtx_t *);
static	BOOL QueueSerialRead   (RIO_t *, recvbuf_t *, IoCtx_t *);
static	BOOL QueueRawSerialRead(RIO_t *, recvbuf_t *, IoCtx_t *);
static  BOOL QueueSocketRecv   (SOCKET , recvbuf_t *, IoCtx_t *);


/* High-level IO callback functions */
static	void OnSocketRecv           (ULONG_PTR, IoCtx_t *);
static	void OnSerialWaitComplete   (ULONG_PTR, IoCtx_t *);
static	void OnSerialReadComplete   (ULONG_PTR, IoCtx_t *);
static	void OnRawSerialReadComplete(ULONG_PTR, IoCtx_t *);
static	void OnSerialWriteComplete  (ULONG_PTR, IoCtx_t *);

/* worker pool offload functions */
static DWORD WINAPI OnSerialReadWorker(void * ctx);


/* keep a list to traverse to free memory on debug builds */
#ifdef DEBUG
static void free_io_completion_port_mem(void);
#endif


	HANDLE WaitableExitEventHandle;
	HANDLE WaitableIoEventHandle;
static	HANDLE hIoCompletionPort;

DWORD	ActiveWaitHandles;
HANDLE	WaitHandles[16];

/*
 * -------------------------------------------------------------------
 * We make a pool of our own for IO context objects -- the are owned by
 * the system until a completion result is pulled from the queue, and
 * they seriously go into the way of memory tracking until we can safely
 * cancel an IO request.
 * -------------------------------------------------------------------
 */
static	HANDLE hHeapHandle;

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Create a new heap for IO context objects
 */
static void
IoCtxPoolInit(
	size_t	initObjs
	)
{
	hHeapHandle = HeapCreate(0, initObjs * sizeof(IoCtx_t), 0);
	if (hHeapHandle == NULL) {
		msyslog(LOG_ERR, "Can't initialize Heap: %m");
		exit(1);
	}
}

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Delete the IO context heap
 *
 * Since we do not know what callbacks are pending, we just drop the
 * pool into oblivion. New allocs and frees will fail from this moment,
 * but we simply don't care. At least the normal heap dump stats will
 * show no leaks from IO context blocks. On the downside, we have to
 * track them ourselves if something goes wrong.
 */
static void
IoCtxPoolDone(void)
{
	hHeapHandle = NULL;
}

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Alloc & Free on local heap
 *
 * When the heap handle is NULL, these both will fail; Alloc with a NULL
 * return and Free silently.
 */
static void * __fastcall
LocalPoolAlloc(
	size_t		size,
	const char *	desc
)
{
	void *	ptr;

	/* Windows heaps can't grok zero byte allocation.
	 * We just get one byte.
	 */
	if (size == 0)
		size = 1;
	if (hHeapHandle != NULL)
		ptr = HeapAlloc(hHeapHandle, HEAP_ZERO_MEMORY, size);
	else
		ptr = NULL;
	DPRINTF(3, ("Allocate '%s', heap=%p, ptr=%p\n",
			desc,  hHeapHandle, ptr));

	return ptr;
}

static void __fastcall
LocalPoolFree(
	void *		ptr,
	const char *	desc
	)
{
	DPRINTF(3, ("Free '%s', heap=%p, ptr=%p\n",
			desc, hHeapHandle, ptr));
	if (ptr != NULL && hHeapHandle != NULL)
		HeapFree(hHeapHandle, 0, ptr);
}

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Alloc & Free of Device context
 *
 * When the heap handle is NULL, these both will fail; Alloc with a NULL
 * return and Free silently.
 */
static DevCtx_t * __fastcall
DevCtxAlloc(void)
{
	DevCtx_t *	devCtx;
	u_long		slot;

	/* allocate struct and tag all slots as invalid */
	devCtx = (DevCtx_t *)LocalPoolAlloc(sizeof(DevCtx_t), "DEV ctx");
	if (devCtx != NULL)
	{
		/* The initial COV values make sure there is no busy
		 * loop on unused/empty slots.
		 */
		devCtx->cov_count = 0;
		for (slot = 0; slot < PPS_QUEUE_LEN; slot++)
			devCtx->pps_buff[slot].cov_count = ~slot;
	}
	return devCtx;
}

static void __fastcall
DevCtxFree(
	DevCtx_t *	devCtx
	)
{
	/* this would be the place to get rid of managed ressources. */
	LocalPoolFree(devCtx, "DEV ctx");
}

static DevCtx_t * __fastcall
DevCtxAttach(
	DevCtx_t *	devCtx
	)
{
	if (devCtx != NULL)
		InterlockedIncrement(&devCtx->ref_count);
	return devCtx;
}

static void __fastcall
DevCtxDetach(
	DevCtx_t *	devCtx
	)
{
	if (devCtx && !InterlockedDecrement(&devCtx->ref_count))
		DevCtxFree(devCtx);
}

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Alloc & Free of I/O context
 *
 * When the heap handle is NULL, these both will fail; Alloc with a NULL
 * return and Free silently.
 */
static IoCtx_t * __fastcall
IoCtxAlloc(
	DevCtx_t *	devCtx
	)
{
	IoCtx_t *	ioCtx;

	ioCtx = (IoCtx_t *)LocalPoolAlloc(sizeof(IoCtx_t), "IO ctx");
	if (ioCtx != NULL)
		ioCtx->devCtx = DevCtxAttach(devCtx);
	return ioCtx;
}

static void __fastcall
IoCtxFree(
	IoCtx_t *	ctx
	)
{
	if (ctx)
		DevCtxDetach(ctx->devCtx);
	LocalPoolFree(ctx, "IO ctx");
}

static void __fastcall
IoCtxReset(
	IoCtx_t *	ctx
	)
{
	RIO_t *		rio;
	DevCtx_t *	dev;
	if (ctx) {
		rio = ctx->rio;
		dev = ctx->devCtx;
		ZERO(*ctx);
		ctx->rio    = rio;
		ctx->devCtx = dev;
	}
}

/*
 * -------------------------------------------------------------------
 * The IO completion thread and support functions
 *
 * There is only one completion thread, because it is locked to the same
 * core as the time interpolation. Having more than one causes core
 * contention and is not useful.
 * -------------------------------------------------------------------
 */
static HANDLE hIoCompletionThread;
static UINT   tidCompletionThread;

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * The IO completion worker thread
 *
 * Note that this thread does not enter an alertable wait state and that
 * the only waiting point is the IO completion port. If stopping this
 * thread with a special queued result packet does not work,
 * 'TerminateThread()' is the only remaining weapon in the arsenal. A
 * dangerous weapon -- it's like SIGKILL.
 */
static unsigned WINAPI
iocompletionthread(void *NotUsed)
{
	DWORD		err;
	DWORD		octets;
	ULONG_PTR	key;
	OVERLAPPED *	pol;
	IoCtx_t *	lpo;

	UNUSED_ARG(NotUsed);

	/*
	 * Socket and refclock receive call gettimeofday() so the I/O
	 * thread needs to be on the same processor as the main and
	 * timing threads to ensure consistent QueryPerformanceCounter()
	 * results.
	 *
	 * This gets seriously into the way of efficient thread pooling
	 * on multicore systems.
	 */
	lock_thread_to_processor(GetCurrentThread());

	/*
	 * Set the thread priority high enough so I/O will preempt
	 * normal recv packet processing, but not higher than the timer
	 * sync thread.
	 */
	if (!SetThreadPriority(GetCurrentThread(),
			       THREAD_PRIORITY_ABOVE_NORMAL))
		msyslog(LOG_ERR, "Can't set thread priority: %m");

	for(;;) {
		if (GetQueuedCompletionStatus(
					hIoCompletionPort, 
					&octets, 
					&key, 
					&pol, 
					INFINITE)) {
			err = ERROR_SUCCESS;
		} else {
			err = GetLastError();
		}
		if (NULL == pol) {
			DPRINTF(2, ("Overlapped IO Thread Exiting\n"));
			break; /* fail */
		}
		lpo = CONTAINEROF(pol, IoCtx_t, ol);
		get_systime(&lpo->RecvTime);
		lpo->byteCount = octets;
		lpo->errCode = err;
		handler_calls++;
		(*lpo->onIoDone)(key, lpo);
	}

	return 0;
}

/*
 * -------------------------------------------------------------------
 * Create/initialise the I/O creation port
 */
void
init_io_completion_port(void)
{
#ifdef DEBUG
	atexit(&free_io_completion_port_mem);
#endif

	/* Create the context pool first. */
	IoCtxPoolInit(20);

	/* Create the event used to signal an IO event */
	WaitableIoEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (WaitableIoEventHandle == NULL) {
		msyslog(LOG_ERR, "Can't create I/O event handle: %m");
		exit(1);
	}
	/* Create the event used to signal an exit event */
	WaitableExitEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (WaitableExitEventHandle == NULL) {
		msyslog(LOG_ERR, "Can't create exit event handle: %m");
		exit(1);
	}

	/* Create the IO completion port */
	hIoCompletionPort = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIoCompletionPort == NULL) {
		msyslog(LOG_ERR, "Can't create I/O completion port: %m");
		exit(1);
	}

	/* Initialize the Wait Handles table */
	WaitHandles[0] = WaitableIoEventHandle;
	WaitHandles[1] = WaitableExitEventHandle; /* exit request */
	WaitHandles[2] = WaitableTimerHandle;
	ActiveWaitHandles = 3;

	/*
	 * Supply ntp_worker.c with function to add or remove a
	 * semaphore to the ntpd I/O loop which is signalled by a worker
	 * when a response is ready.  The callback is invoked in the
	 * parent.
	 */
	addremove_io_semaphore = &ntpd_addremove_semaphore;

	/*
	 * Have one thread servicing I/O. See rationale in front matter.
 	 */
	hIoCompletionThread = (HANDLE)_beginthreadex(
		NULL, 
		0, 
		iocompletionthread, 
		NULL, 
		0, 
		&tidCompletionThread);
}


/*
 * -------------------------------------------------------------------
 * completion port teardown
 */
void
uninit_io_completion_port(
	void
	)
{
        DWORD rc;

	/* do noting if completion port already gone. */
	if (NULL == hIoCompletionPort)
		return;

	/*
	 * Service thread seems running. Terminate him with grace
	 * first and force later...
	 */
        if (tidCompletionThread != GetCurrentThreadId()) {
	        PostQueuedCompletionStatus(hIoCompletionPort, 0, 0, 0);
                rc = WaitForSingleObject(hIoCompletionThread, 5000);
                if (rc == WAIT_TIMEOUT) {
		        /* Thread lost. Kill off with TerminateThread. */
		        msyslog(LOG_ERR,
                                "IO completion thread refuses to terminate");
		        TerminateThread(hIoCompletionThread, ~0UL);
                }
	}

         /* stop using the memory pool */
	IoCtxPoolDone();

	/* now reap all handles... */
	CloseHandle(hIoCompletionThread);
	hIoCompletionThread = NULL;
	CloseHandle(hIoCompletionPort);
	hIoCompletionPort = NULL;
}


/*
 * -------------------------------------------------------------------
 * external worker thread support (wait handle stuff)
 *
 * !Attention!
 *
 *  - This function must only be called from the main thread. Changing
 *    a set of wait handles while someone is waiting on it creates
 *    undefined behaviour. Also there's no provision for mutual
 *    exclusion when accessing global values. 
 *
 *  - It's not possible to register a handle that is already in the table.
 */
static void
ntpd_addremove_semaphore(
	HANDLE	sem,
	int	remove
	)
{
	DWORD	hi;

	/* search for a matching entry first. */
	for (hi = 3; hi < ActiveWaitHandles; hi++)
		if (sem == WaitHandles[hi])
			break;

	if (remove) {
		/*
		 * If found, eventually swap with last entry to keep
		 * the table dense.
		 */
		if (hi < ActiveWaitHandles) {
			ActiveWaitHandles--;
			if (hi < ActiveWaitHandles)
				WaitHandles[hi] =
				    WaitHandles[ActiveWaitHandles];
			WaitHandles[ActiveWaitHandles] = NULL;
		}
	} else {
		/*
		 * Make sure the entry is not found and there is enough
		 * room, then append to the table array.
		 */
		if (hi >= ActiveWaitHandles) {
			INSIST(ActiveWaitHandles < COUNTOF(WaitHandles));
			WaitHandles[ActiveWaitHandles] = sem;
			ActiveWaitHandles++;
		}
	}
}


#ifdef DEBUG
static void
free_io_completion_port_mem(
	void
	)
{
	/*
	 * At the moment, do absolutely nothing. Returning memory here
	 * requires NO PENDING OVERLAPPED OPERATIONS AT ALL at this
	 * point in time, and as long we cannot be reasonable sure about
	 * that the simple advice is:
	 *
	 * HANDS OFF!
	 */
}
#endif	/* DEBUG */


/*
 * -------------------------------------------------------------------
 * Serial IO stuff
 *
 * Prelude -- common error checking code
 * -------------------------------------------------------------------
 */
extern char * NTstrerror(int err, BOOL *bfreebuf);

static BOOL
IoResultCheck(
	DWORD		err,
	IoCtx_t * 	ctx,
	const char *	msg
	)
{
	char * msgbuf;
	BOOL   dynbuf;

	/* If the clock is not / no longer active, assume
	 * 'ERROR_OPERATION_ABORTED' and do the necessary cleanup.
	 */
	if (ctx->rio && !ctx->rio->active)
		err = ERROR_OPERATION_ABORTED;
	
	switch (err)
	{
		/* The first ones are no real errors. */
	case ERROR_SUCCESS:	/* all is good */
	case ERROR_IO_PENDING:	/* callback pending */
		return TRUE;

		/* the next ones go silently -- only cleanup is done */
	case ERROR_INVALID_PARAMETER:	/* handle already closed */
	case ERROR_OPERATION_ABORTED:	/* handle closed while wait */
		break;


	default:
		/*
		 * We have to resort to the low level error formatting
		 * functions here, since the error code can be an
		 * overlapped result. Relying the value to be the same
		 * as the 'GetLastError()' result at this point of
		 * execution is shaky at best, and using SetLastError()
		 * to force it seems too nasty.
		 */
		msgbuf = NTstrerror(err, &dynbuf);
		msyslog(LOG_ERR, "%s: err=%u, '%s'", msg, err, msgbuf);
		if (dynbuf)
			LocalFree(msgbuf);
		break;
	}

	/* If we end here, we have to mop up the buffer and context */
	if (ctx->flRawMem) {
		if (ctx->trans_buf)
			free(ctx->trans_buf);
	} else {
		if (ctx->recv_buf)
			freerecvbuf(ctx->recv_buf);
	}
	IoCtxFree(ctx);
	return FALSE;
}

/*
 * -------------------------------------------------------------------
 * Serial IO stuff
 *
 * Part 1 -- COMM event handling
 * -------------------------------------------------------------------
 */

static BOOL
QueueSerialWait(
	RIO_t *		rio,
	recvbuf_t *	buff,
	IoCtx_t *	lpo
	)
{
	BOOL   rc;

	lpo->onIoDone = OnSerialWaitComplete;
	lpo->recv_buf = buff;
	lpo->flRawMem = 0;
	lpo->rio      = rio;
	buff->fd      = rio->fd;

	rc = WaitCommEvent((HANDLE)_get_osfhandle(rio->fd),
			   &lpo->com_events, &lpo->ol);
	if (!rc)
		return IoResultCheck(GetLastError(), lpo,
				     "Can't wait on Refclock");
	return TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void 
OnSerialWaitComplete(
	ULONG_PTR	key,       
	IoCtx_t *	lpo
	)
{
	RIO_t *		rio;
	DevCtx_t *	dev;
	recvbuf_t * 	buff;
	PPSDataEx_t *	ppsbuf;
	DWORD 		modem_status;
	u_long		covc;

	/* check and bail out if operation failed */
	if (!IoResultCheck(lpo->errCode, lpo,
		"WaitCommEvent failed"))
		return;

	/* get & validate context and buffer. */
	rio  = (RIO_t *)key;
	buff = lpo->recv_buf;
	dev  = lpo->devCtx;

	INSIST(rio == lpo->rio);

#ifdef DEBUG
	if (~(EV_RXFLAG | EV_RLSD | EV_RXCHAR) & lpo->com_events) {
		msyslog(LOG_ERR, "WaitCommEvent returned unexpected mask %x",
			lpo->com_events);
		exit(-1);
	}
#endif
	/*
	 * Take note of changes on DCD; 'user mode PPS hack'.
	 * perlinger@ntp.org suggested a way of solving several problems with
	 * this code that makes a lot of sense: move to a putative
	 * dcdpps-ppsapi-provider.dll.
	 */
	if (EV_RLSD & lpo->com_events) {
		modem_status = 0;
		GetCommModemStatus((HANDLE)_get_osfhandle(rio->fd),
				   &modem_status);

		if (dev != NULL) {
			/* PPS-context available -- use it! */
			if (MS_RLSD_ON & modem_status) {
				dev->pps_data.cc_assert++;
				dev->pps_data.ts_assert = lpo->RecvTime;
				DPRINTF(2, ("upps-real: fd %d DCD PPS Rise at %s\n", rio->fd,
					ulfptoa(&lpo->RecvTime, 6)));
			} else {
				dev->pps_data.cc_clear++;
				dev->pps_data.ts_clear = lpo->RecvTime;
				DPRINTF(2, ("upps-real: fd %d DCD PPS Fall at %s\n", rio->fd,
					ulfptoa(&lpo->RecvTime, 6)));
			}
			/*
			** Update PPS buffer, writing from low to high, with index
			** update as last action. We use interlocked ops and a
			** volatile data destination to avoid reordering on compiler
			** and CPU level. The interlocked instruction act as full
			** barriers -- we need only release semantics, but we don't
			** have them before VS2010.
			*/
			covc   = dev->cov_count + 1u;
			ppsbuf = dev->pps_buff + (covc & PPS_QUEUE_MSK);
			InterlockedExchange((PLONG)&ppsbuf->cov_count, covc);
			ppsbuf->data = dev->pps_data;
			InterlockedExchange((PLONG)&dev->cov_count, covc);
		}
		/* perlinger@ntp.org, 2012-11-19
		It can be argued that once you have the PPS API active, you can
		disable the old pps hack. This would give a behaviour that's much
		more like the behaviour under a UN*Xish OS. On the other hand, it
		will give a nasty surprise for people which have until now happily
		taken the pps hack for granted, and after the first complaint, I have
		decided to keep the old implementation unconditionally. So here it is:

		/* backward compat: 'usermode-pps-hack' */
		if (MS_RLSD_ON & modem_status) {
			lpo->DCDSTime = lpo->RecvTime;
			lpo->flTsDCDS = 1;
			DPRINTF(2, ("upps-hack: fd %d DCD PPS Rise at %s\n", rio->fd,
				ulfptoa(&lpo->RecvTime, 6)));
		}
	}

	/* If IO ready, read data. Go back waiting else. */
	if (EV_RXFLAG & lpo->com_events) {		/* line discipline */
		lpo->FlagTime = lpo->RecvTime;
		lpo->flTsFlag = 1;
		QueueSerialRead(rio, buff, lpo);
	} else if (EV_RXCHAR & lpo->com_events) {	/* raw discipline */
		lpo->FlagTime = lpo->RecvTime;
		lpo->flTsFlag = 1;
		QueueRawSerialRead(rio, buff, lpo);
	} else {					/* idle... */
		QueueSerialWait(rio, buff, lpo);
	}
}

/*
 * -------------------------------------------------------------------
 * Serial IO stuff
 *
 * Part 2 -- line discipline emulation
 *
 * Ideally this should *not* be done in the IO completion thread.
 * We use a worker pool thread to offload the low-level processing.
 * -------------------------------------------------------------------
 */

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Start & Queue a serial read for line discipline emulation.
 */
static BOOL
QueueSerialRead(
	RIO_t *		rio,
	recvbuf_t *	buff,
	IoCtx_t *	lpo
	)
{
	BOOL   rc;

	lpo->onIoDone = &OnSerialReadComplete;
	lpo->recv_buf = buff;
	lpo->flRawMem = 0;
	lpo->rio      = rio;
	buff->fd      = rio->fd;

	rc = ReadFile((HANDLE)_get_osfhandle(rio->fd),
		      (char*)buff->recv_buffer  + buff->recv_length,
		      sizeof(buff->recv_buffer) - buff->recv_length,
		      NULL, &lpo->ol);
	if (!rc)
		return IoResultCheck(GetLastError(), lpo,
				     "Can't read from Refclock");
	return TRUE;
}

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * IO completion thread callback. Takes a time stamp and offloads the
 * real work to the worker pool ASAP.
 */
static void
OnSerialReadComplete(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	RIO_t *		rio;
	recvbuf_t *	buff;

	/* check and bail out if operation failed */
	if (!IoResultCheck(lpo->errCode, lpo,
			   "Read from Refclock failed"))
		return;

	/* get & validate context and buffer. */
	rio  = lpo->rio;
	buff = lpo->recv_buf;
	INSIST((ULONG_PTR)rio == key);

	/* Offload to worker pool */
	if (!QueueUserWorkItem(&OnSerialReadWorker, lpo, WT_EXECUTEDEFAULT)) {
		msyslog(LOG_ERR,
			"Can't offload to worker thread, will skip data: %m");
		IoCtxReset(lpo);
		buff->recv_length = 0;
		QueueSerialWait(rio, buff, lpo);
	}
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Worker pool offload function -- avoid lengthy operations in the IO
 * completion thread (affects timing...)
 *
 * This function does the real work of emulating the UN*X line
 * discipline. Since this involves allocation of additional buffers and
 * string parsing/copying, it is offloaded to the worker thread pool so
 * the IO completion thread can resume faster.
 */
static DWORD WINAPI
OnSerialReadWorker(void * ctx)
{
	IoCtx_t *	lpo;
	recvbuf_t *	buff, *obuf;
	RIO_t *		rio;
	char		*sptr, *send, *dptr;
	BOOL		eol;
	char		ch;

	/* Get context back */
	lpo  = (IoCtx_t*)ctx;
	buff = lpo->recv_buf;
	rio  = lpo->rio;
	/*
	 * ignore 0 bytes read due to closure on fd.
	 * Eat the first line of input as it's possibly partial.
	 */
	if (lpo->byteCount && rio->recvcount++) {
		/* account for additional input */
		buff->recv_length += (int)lpo->byteCount;

		/*
		 * Now mimic the Unix line discipline. 
		 */
		sptr = (char *)buff->recv_buffer;
		send = sptr + buff->recv_length;
		obuf = NULL;
		dptr = NULL;

		/* hack #1: eat away leading CR/LF if here is any */
		while (sptr != send) {
			ch = *sptr;
			if (ch != '\n' && ch != '\r')
				break;
			sptr++;
		}

		while (sptr != send)
		{
			/* get new buffer to store line */
			obuf = get_free_recv_buffer_alloc();
			obuf->fd          = rio->fd;
			obuf->receiver    = &process_refclock_packet;
			obuf->dstadr      = NULL;
			obuf->recv_peer   = rio->srcclock;
			set_serial_recv_time(obuf, lpo);

			/*
			 * Copy data to new buffer, convert CR to LF on
			 * the fly.  Stop after either.
			 */
			dptr = (char *)obuf->recv_buffer;
			eol  = FALSE;
			while (sptr != send && !eol) {
				ch  = *sptr++;
				if ('\r' == ch) {
					ch = '\n';
				}
				*dptr++ = ch;
				eol = ('\n' == ch);
			}
			obuf->recv_length =
			    (int)(dptr - (char *)obuf->recv_buffer);

			/*
			 * If NL found, push this buffer and prepare to
			 * get a new one.
			 */
			if (eol) {
				add_full_recv_buffer(obuf);
				SetEvent(WaitableIoEventHandle);
				obuf = NULL;
			}
		}

		/*
		 * If we still have an output buffer, continue to fill
		 * it again.
		 */
		if (obuf) {
			obuf->recv_length =
			    (int)(dptr - (char *)obuf->recv_buffer);
			freerecvbuf(buff);
			buff = obuf;
		} else {
			/* clear the current buffer, continue */
			buff->recv_length = 0;
		}
	} else {
		buff->recv_length = 0;
	}

	IoCtxReset(lpo);
	QueueSerialWait(rio, buff, lpo);
	return 0;
}


/*
 * -------------------------------------------------------------------
 * Serial IO stuff
 *
 * Part 3 -- raw data input
 *
 * Raw data processing is fast enough to do without offloading to the
 * worker pool, so this is rather short'n sweet...
 * -------------------------------------------------------------------
 */

static BOOL
QueueRawSerialRead(
	RIO_t *		rio,
	recvbuf_t *	buff,
	IoCtx_t *	lpo
	)
{
	BOOL   rc;

	lpo->onIoDone = OnRawSerialReadComplete;
	lpo->recv_buf = buff;
	lpo->flRawMem = 0;
	lpo->rio      = rio;
	buff->fd      = rio->fd;

	rc = ReadFile((HANDLE)_get_osfhandle(rio->fd),
		      buff->recv_buffer,
		      sizeof(buff->recv_buffer),
		      NULL, &lpo->ol);
	if (!rc)
		return IoResultCheck(GetLastError(), lpo,
				     "Can't read raw from Refclock");
	return TRUE;
}


static void 
OnRawSerialReadComplete(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	RIO_t *		rio;
	recvbuf_t *	buff;

	/* check and bail out if operation failed */
	if (!IoResultCheck(lpo->errCode, lpo,
			   "Raw read from Refclock failed"))
		return;

	/* get & validate context and buffer. */
	rio  = lpo->rio;
	buff = lpo->recv_buf;
	INSIST((ULONG_PTR)rio == key);

	/* ignore 0 bytes read. */
	if (lpo->byteCount > 0) {
		buff->recv_length = (int)lpo->byteCount;
		buff->dstadr      = NULL;
		buff->receiver    = process_refclock_packet;
		buff->recv_peer   = rio->srcclock;
		set_serial_recv_time(buff, lpo);
		add_full_recv_buffer(buff);
		SetEvent(WaitableIoEventHandle);
		buff = get_free_recv_buffer_alloc();
	}

	buff->recv_length = 0;
	QueueSerialWait(rio, buff, lpo);
}


static inline void
set_serial_recv_time(
	recvbuf_t *	obuf,
	IoCtx_t *	lpo
	)
{
	/*
	 * Time stamp assignment is interesting.  If we
	 * have a DCD stamp, we use it, otherwise we use
	 * the FLAG char event time, and if that is also
	 * not / no longer available we use the arrival
	 * time.
	 */
	if (lpo->flTsDCDS)
		obuf->recv_time = lpo->DCDSTime;
	else if (lpo->flTsFlag)
		obuf->recv_time = lpo->FlagTime;
	else
		obuf->recv_time = lpo->RecvTime;

	lpo->flTsDCDS = lpo->flTsFlag = 0; /* use only once... */
}


/*
 * -------------------------------------------------------------------
 * Serial IO stuff
 *
 * Part 4 -- Overlapped serial output
 *
 * Again, no need to offload any work.
 * -------------------------------------------------------------------
 */

/*
 * async_write, clone of write(), used by some reflock drivers
 */
int	
async_write(
	int		fd,
	const void *	data,
	unsigned int	count
	)
{
	IoCtx_t *	lpo;
	BOOL		rc;

	lpo  = IoCtxAlloc(NULL);
	if (lpo == NULL) {
		DPRINTF(1, ("async_write: out of memory\n"));
		errno = ENOMEM;
		return -1;
	}

	lpo->onIoDone  = OnSerialWriteComplete;
	lpo->trans_buf = emalloc(count);
	lpo->flRawMem  = 1;
	memcpy(lpo->trans_buf, data, count);

	rc = WriteFile((HANDLE)_get_osfhandle(fd),
		       lpo->trans_buf, count,
		       NULL, &lpo->ol);
	if (!rc && !IoResultCheck(GetLastError(), lpo,
				  "Can't write to Refclock")) {
		errno = EBADF;
		return -1;
	}
	return count;
}

static void
OnSerialWriteComplete(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	/* set RIO and force silent cleanup if no error */
	lpo->rio = (RIO_t *)key;
	if (ERROR_SUCCESS == lpo->errCode)
		lpo->errCode = ERROR_OPERATION_ABORTED;
	IoResultCheck(lpo->errCode, lpo,
		      "Write to Refclock failed");
}


/*
 * -------------------------------------------------------------------
 * Serial IO stuff
 *
 * Part 5 -- read PPS time stamps
 *
 * -------------------------------------------------------------------
 */

/* The dummy read procedure is used for getting the device context
 * into the IO completion thread, using the IO completion queue for
 * transport. There are other ways to accomplish what we need here,
 * but using the IO machine is handy and avoids a lot of trouble.
 */
static void
OnPpsDummyRead(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	RIO_t *	rio;

	rio = (RIO_t *)key;
	lpo->devCtx = DevCtxAttach(rio->device_context);
	SetEvent(lpo->ppswake);
}

__declspec(dllexport) void* __stdcall
ntp_pps_attach_device(
	HANDLE	hndIo
	)
{
	IoCtx_t		myIoCtx;
	HANDLE		myEvt;
	DevCtx_t *	dev;
	DWORD		rc;

	if (!isserialhandle(hndIo)) {
		SetLastError(ERROR_INVALID_HANDLE);
		return NULL;
	}

	ZERO(myIoCtx);
	dev   = NULL;
	myEvt = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == myEvt)
		goto done;

	myIoCtx.ppswake   = myEvt;
	myIoCtx.onIoDone  = OnPpsDummyRead;
	rc = ReadFile(hndIo, &myIoCtx.byteCount, 0,
			&myIoCtx.byteCount, &myIoCtx.ol);
	if (!rc && (GetLastError() != ERROR_IO_PENDING))
		goto done;
	if (WaitForSingleObject(myEvt, INFINITE) == WAIT_OBJECT_0)
		if (NULL == (dev = myIoCtx.devCtx))
			SetLastError(ERROR_INVALID_HANDLE);
done:
	rc = GetLastError();
	CloseHandle(myEvt);
	SetLastError(rc);
	return dev;
}

__declspec(dllexport) void __stdcall
ntp_pps_detach_device(
	DevCtx_t *	dev
	)
{
	DevCtxDetach(dev);
}

__declspec(dllexport) BOOL __stdcall
ntp_pps_read(
	DevCtx_t *	dev,
	PPSData_t *	data,
	size_t		dlen
	)
{
	u_long		guard, covc;
	int		repc;
	PPSDataEx_t *	ppsbuf;


	if (dev == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	if (data == NULL || dlen != sizeof(PPSData_t)) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	/*
	** Reading from shared memory in a lock-free fashion can be
	** a bit tricky, since we have to read the components in the
	** opposite direction from the write, and the compiler must
	** not reorder the read sequence.
	** We use interlocked ops and a volatile data source to avoid
	** reordering on compiler and CPU level. The interlocked
	** instruction act as full barriers -- we need only aquire
	** semantics, but we don't have them before VS2010.
	*/
	repc = 3;
	do {
		InterlockedExchange((PLONG)&covc, dev->cov_count);
		ppsbuf = dev->pps_buff + (covc & PPS_QUEUE_MSK);
		*data = ppsbuf->data;
		InterlockedExchange((PLONG)&guard, ppsbuf->cov_count);
		guard ^= covc;
	} while (guard && ~guard && --repc);

	if (guard) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}
	return TRUE;
}

/*
 * Add a reference clock data structures I/O handles to
 * the I/O completion port. Return 1 if any error.
 */  
int
io_completion_port_add_clock_io(
	RIO_t *rio
	)
{
	IoCtx_t *	lpo;
	DevCtx_t *	dev;
	recvbuf_t *	buff;
	HANDLE		h;

	h = (HANDLE)_get_osfhandle(rio->fd);
	if (NULL == CreateIoCompletionPort(
			h, 
			hIoCompletionPort, 
			(ULONG_PTR)rio,
			0)) {
		msyslog(LOG_ERR, "Can't add COM port to i/o completion port: %m");
		return 1;
	}

	dev = DevCtxAlloc();
	if (NULL == dev) {
		msyslog(LOG_ERR, "Can't allocate device context for i/o completion port: %m");
		return 1;
	}
	rio->device_context = DevCtxAttach(dev);
	lpo = IoCtxAlloc(dev);
	if (NULL == lpo) {
		msyslog(LOG_ERR, "Can't allocate heap for completion port: %m");
		return 1;
	}
	buff = get_free_recv_buffer_alloc();
	buff->recv_length = 0;
	QueueSerialWait(rio, buff, lpo);

	return 0;
}

void
io_completion_port_remove_clock_io(
	RIO_t *rio
	)
{
	if (rio)
		DevCtxDetach((DevCtx_t *)rio->device_context);
}

/*
 * Queue a receiver on a socket. Returns 0 if no buffer can be queued 
 *
 *  Note: As per the winsock documentation, we use WSARecvFrom. Using
 *	  ReadFile() is less efficient.
 */
static BOOL 
QueueSocketRecv(
	SOCKET		s,
	recvbuf_t *	buff,
	IoCtx_t *	lpo
	)
{
	WSABUF wsabuf;
	DWORD  Flags;
	int    rc;

	lpo->onIoDone = OnSocketRecv;
	lpo->recv_buf = buff;
	lpo->flRawMem = 0;
	lpo->rio      = NULL;

	Flags = 0;
	buff->fd = s;
	buff->recv_srcadr_len = sizeof(buff->recv_srcadr);
	wsabuf.buf = (char *)buff->recv_buffer;
	wsabuf.len = sizeof(buff->recv_buffer);

	rc = WSARecvFrom(buff->fd, &wsabuf, 1, NULL, &Flags, 
			 &buff->recv_srcadr.sa, &buff->recv_srcadr_len, 
			 &lpo->ol, NULL);
	if (SOCKET_ERROR == rc) 
		return IoResultCheck(GetLastError(), lpo,
				     "Can't read from Socket");
	return TRUE;
}


static void 
OnSocketRecv(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	recvbuf_t * buff;
	recvbuf_t * newbuff;
	struct interface * inter = (struct interface *)key;
	
	REQUIRE(NULL != lpo);
	REQUIRE(NULL != lpo->recv_buf);

	/* check and bail out if operation failed */
	if (!IoResultCheck(lpo->errCode, lpo,
			   "Read from Socket failed"))
		return;

	/*
	 * Convert the overlapped pointer back to a recvbuf pointer.
	 * Fetch items that are lost when the context is queued again.
	 */
	buff = lpo->recv_buf;
	buff->recv_time   = lpo->RecvTime;
	buff->recv_length = (int)lpo->byteCount;

	/*
	 * Get a new recv buffer for the replacement socket receive
	 */
	newbuff = get_free_recv_buffer_alloc();
	if (NULL != newbuff) {
		QueueSocketRecv(inter->fd, newbuff, lpo);
	} else {
		IoCtxFree(lpo);
		msyslog(LOG_ERR, "Can't add I/O request to socket");
	}
	DPRINTF(4, ("%sfd %d %s recv packet mode is %d\n", 
		    (MODE_BROADCAST == get_packet_mode(buff))
			? " **** Broadcast "
			: "",
		    (int)buff->fd, stoa(&buff->recv_srcadr),
		    get_packet_mode(buff)));

	/*
	 * If we keep it add some info to the structure
	 */
	if (buff->recv_length && !inter->ignore_packets) {
		INSIST(buff->recv_srcadr_len <= sizeof(buff->recv_srcadr));
		buff->receiver = &receive; 
		buff->dstadr   = inter;
		packets_received++;
		handler_pkts++;
		inter->received++;
		add_full_recv_buffer(buff);

		DPRINTF(2, ("Received %d bytes fd %d in buffer %p from %s\n", 
			    buff->recv_length, (int)buff->fd, buff,
			    stoa(&buff->recv_srcadr)));

		/*
		 * Now signal we have something to process
		 */
		SetEvent(WaitableIoEventHandle);
	} else
		freerecvbuf(buff);
}


/*
 * Add a socket handle to the I/O completion port, and send 
 * NTP_RECVS_PER_SOCKET recv requests to the kernel.
 */
int
io_completion_port_add_socket(
	SOCKET			fd,
	struct interface *	inter
	)
{
	IoCtx_t *	lpo;
	recvbuf_t *	buff;
	int		n;

	if (fd != INVALID_SOCKET) {
		if (NULL == CreateIoCompletionPort((HANDLE)fd, 
		    hIoCompletionPort, (ULONG_PTR)inter, 0)) {
			msyslog(LOG_ERR,
				"Can't add socket to i/o completion port: %m");
			return 1;
		}
	}

	/*
	 * Windows 2000 bluescreens with bugcheck 0x76
	 * PROCESS_HAS_LOCKED_PAGES at ntpd process
	 * termination when using more than one pending
	 * receive per socket.  A runtime version test
	 * would allow using more on newer versions
	 * of Windows.
	 */

#define WINDOWS_RECVS_PER_SOCKET 1

	for (n = 0; n < WINDOWS_RECVS_PER_SOCKET; n++) {

		buff = get_free_recv_buffer_alloc();
		lpo = IoCtxAlloc(NULL);
		if (lpo == NULL)
		{
			msyslog(LOG_ERR
				, "Can't allocate IO completion context: %m");
			return 1;
		}

		QueueSocketRecv(fd, buff, lpo);

	}
	return 0;
}


/*
 * io_completion_port_sendto() -- sendto() replacement for Windows
 *
 * Returns len after successful send.
 * Returns -1 for any error, with the error code available via
 *	msyslog() %m, or GetLastError().
 */
int
io_completion_port_sendto(
	SOCKET		fd,
	void  *		pkt,
	size_t		len,
	sockaddr_u *	dest
	)
{
	static u_long time_next_ifscan_after_error;
	WSABUF wsabuf;
	DWORD octets_sent;
	DWORD Result;
	int errval;
	int AddrLen;

	if (len > INT_MAX)
		len = INT_MAX;
	wsabuf.buf = (void *)pkt;
	wsabuf.len = (DWORD)len;
	AddrLen = SOCKLEN(dest);
	octets_sent = 0;

	Result = WSASendTo(fd, &wsabuf, 1, &octets_sent, 0,
			   &dest->sa, AddrLen, NULL, NULL);
	errval = GetLastError();
	if (SOCKET_ERROR == Result) {
		if (ERROR_UNEXP_NET_ERR == errval) {
			/*
			 * We get this error when trying to send if the
			 * network interface is gone or has lost link.
			 * Rescan interfaces to catch on sooner, but no
			 * more often than once per minute.  Once ntpd
			 * is able to detect changes without polling
			 * this should be unneccessary
			 */
			if (time_next_ifscan_after_error < current_time) {
				time_next_ifscan_after_error = current_time + 60;
				timer_interfacetimeout(current_time);
			}
			DPRINTF(4, ("sendto unexpected network error, interface may be down\n"));
		} else {
			msyslog(LOG_ERR, "WSASendTo(%s) error %m",
				stoa(dest));
		}
		SetLastError(errval);
		return -1;
	}

	if ((DWORD)len != octets_sent) {
		msyslog(LOG_ERR, "WSASendTo(%s) sent %u of %d octets",
			stoa(dest), octets_sent, len);
		SetLastError(ERROR_BAD_LENGTH);
		return -1;
	}

	DPRINTF(4, ("sendto %s %d octets\n", stoa(dest), len));

	return (int)len;
}



/*
 * GetReceivedBuffers
 * Note that this is in effect the main loop for processing requests
 * both send and receive. This should be reimplemented
 */
int
GetReceivedBuffers()
{
	DWORD	index;
	HANDLE	ready;
	int	have_packet;

	have_packet = FALSE;
	while (!have_packet) {
		index = WaitForMultipleObjects(ActiveWaitHandles,
					       WaitHandles, FALSE,
					       INFINITE);
		switch (index) {

		case WAIT_OBJECT_0 + 0: /* Io event */
			DPRINTF(4, ("IoEvent occurred\n"));
			have_packet = TRUE;
			break;

		case WAIT_OBJECT_0 + 1: /* exit request */
			exit(0);
			break;

		case WAIT_OBJECT_0 + 2: /* timer */
			timer();
			break;

		case WAIT_IO_COMPLETION: /* loop */
			break;

		case WAIT_TIMEOUT:
			msyslog(LOG_ERR,
				"WaitForMultipleObjects INFINITE timed out.");
			exit(1);
			break;

		case WAIT_FAILED:
			msyslog(LOG_ERR,
				"WaitForMultipleObjects Failed: Error: %m");
			exit(1);
			break;

		default:
			DEBUG_INSIST((index - WAIT_OBJECT_0) <
				     ActiveWaitHandles);
			ready = WaitHandles[index - WAIT_OBJECT_0];
			handle_blocking_resp_sem(ready);
			break;
				
		} /* switch */
	}

	return (full_recvbuffs());	/* get received buffers */
}

#else /*defined(HAVE_IO_COMPLETION_PORT) */
  static int NonEmptyCompilationUnit;
#endif  /*!defined(HAVE_IO_COMPLETION_PORT) */

