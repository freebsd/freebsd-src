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

+ Some IO operations need a possibly lengthy post-processing. Emulating
  the UN*X line discipline is currently the only but prominent example.
  To avoid the processing in the time-critical IOCPL thread, longer
  processing is offloaded the worker thread pool.

+ A fact that seems not as well-known as it should be is that all
  resources passed to an overlapped IO operation must be considered
  owned by the OS until the result has been fetched/dequeued. This
  includes all overlapped structures and buffers involved, so cleaning
  up on shutdown must be carefully constructed. (This includes closing
  all the IO handles and waiting for the results to be dequeued.
  'CancleIo()' cannot be used since it's broken beyond repair.)

  If this is not possible, then all resources should be dropped into
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

#include "ntpd.h"
#include "ntp_request.h"

#include "ntp_iocompletionport.h"
#include "ntp_iocplmem.h"
#include "ntp_iocpltypes.h"


#define CONTAINEROF(p, type, member) \
	((type *)((char *)(p) - offsetof(type, member)))

enum io_packet_handling {
	PKT_OK,
	PKT_DROP,
	PKT_SOCKET_ERROR
};

static const char * const st_packet_handling[3] = {
	"accepted",
	"dropped"
	"error"
};

/*
 * local function definitions
 */
static	void ntpd_addremove_semaphore(HANDLE, int);
static	void set_serial_recv_time    (recvbuf_t *, IoCtx_t *);

/* Initiate/Request async IO operations */
static	BOOL __fastcall QueueSerialWait   (IoCtx_t *, recvbuf_t *);
static	BOOL __fastcall QueueSerialRead(IoCtx_t *, recvbuf_t *);
static	BOOL __fastcall QueueRawSerialRead(IoCtx_t *, recvbuf_t *);
static  BOOL __fastcall QueueSocketRecv(IoCtx_t *, recvbuf_t *);


/* High-level IO callback functions */
static	void OnSocketRecv           (ULONG_PTR, IoCtx_t *);
static	void OnSocketSend           (ULONG_PTR, IoCtx_t *);
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


	HANDLE	WaitableExitEventHandle;
	HANDLE	WaitableIoEventHandle;
static	HANDLE	hndIOCPLPort;
static	HANDLE	hMainThread;
static	HANDLE	hMainRpcDone;

DWORD	ActiveWaitHandles;
HANDLE	WaitHandles[4];


/*
 * -------------------------------------------------------------------
 * Windows 2000 bluescreens with bugcheck 0x76 PROCESS_HAS_LOCKED_PAGES
 * at ntpd process termination when using more than one pending
 * receive per socket.  A runtime version test during startup will
 * allow using more on newer versions of Windows.
 *
 * perlinger@ntp.org: Considering the quirks fixed in the overlapped
 * IO handling in recent years, it could even be that this is no longer
 * an issue. Testing this might be tricky -- who runs a Win2k system
 * in the year 2016?
 */
static size_t	s_SockRecvSched = 1;	/* possibly adjusted later */


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
iocompletionthread(
	void *NotUsed
	)
{
	DWORD		err;
	DWORD		octets;
	ULONG_PTR	key;
	OVERLAPPED *	pol;
	IoCtx_t *	lpo;

	UNUSED_ARG(NotUsed);

	/* Socket and refclock receive call gettimeofday() so the I/O
	 * thread needs to be on the same processor as the main and
	 * timing threads to ensure consistent QueryPerformanceCounter()
	 * results.
	 *
	 * This gets seriously into the way of efficient thread pooling
	 * on multi-core systems.
	 */
	lock_thread_to_processor(GetCurrentThread());

	/* Set the thread priority high enough so I/O will pre-empt
	 * normal recv packet processing, but not higher than the timer
	 * sync thread.
	 */
	if (!SetThreadPriority(GetCurrentThread(),
			       THREAD_PRIORITY_ABOVE_NORMAL))
		msyslog(LOG_ERR, "Can't set thread priority: %m");

	for(;;) {
		if (GetQueuedCompletionStatus(
					hndIOCPLPort, 
					&octets, 
					&key, 
					&pol, 
					INFINITE)) {
			err = ERROR_SUCCESS;
		} else {
			err = GetLastError();
		}
		if (pol == NULL) {
			DPRINTF(2, ("Overlapped IO Thread Exiting\n"));
			break; /* fail */
		}
		lpo = CONTAINEROF(pol, IoCtx_t, ol);
		get_systime(&lpo->aux.RecvTime);
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
	OSVERSIONINFO vi;

#   ifdef DEBUG
	atexit(&free_io_completion_port_mem);
#   endif

	memset(&vi, 0, sizeof(vi));
	vi.dwOSVersionInfoSize = sizeof(vi);

	/* For windows 7 and above, schedule more than one receive */
	if (GetVersionEx(&vi) && vi.dwMajorVersion >= 6)
		s_SockRecvSched = 4;

	/* Create the context pool first. */
	IOCPLPoolInit(20);

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
	hMainRpcDone = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hMainRpcDone == NULL) {
		msyslog(LOG_ERR, "Can't create RPC sync handle: %m");
		exit(1);
	}

	/* Create the IO completion port */
	hndIOCPLPort = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hndIOCPLPort == NULL) {
		msyslog(LOG_ERR, "Can't create I/O completion port: %m");
		exit(1);
	}

	/* Initialize the Wait Handles table */
	WaitHandles[0] = WaitableIoEventHandle;
	WaitHandles[1] = WaitableExitEventHandle; /* exit request */
	WaitHandles[2] = WaitableTimerHandle;
	ActiveWaitHandles = 3;

	/* Supply ntp_worker.c with function to add or remove a
	 * semaphore to the ntpd I/O loop which is signalled by a worker
	 * when a response is ready.  The callback is invoked in the
	 * parent.
	 */
	addremove_io_semaphore = &ntpd_addremove_semaphore;

	/* Create a true handle for the main thread (APC processing) */
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
		GetCurrentProcess(), &hMainThread,
		0, FALSE, DUPLICATE_SAME_ACCESS);

	/* Have one thread servicing I/O. See rationale in front matter. */
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
	DWORD	rc;

	/* do noting if completion port already gone. */
	if (hndIOCPLPort == NULL)
		return;

	/* Service thread seems running. Terminate him with grace
	 * first and force later...
	 */
	if (tidCompletionThread != GetCurrentThreadId()) {
		PostQueuedCompletionStatus(hndIOCPLPort, 0, 0, 0);
		rc = WaitForSingleObject(hIoCompletionThread, 5000);
		if (rc == WAIT_TIMEOUT) {
			/* Thread lost. Kill off with TerminateThread. */
			msyslog(LOG_ERR,
				"IO completion thread refuses to terminate");
			TerminateThread(hIoCompletionThread, ~0UL);
		}
	}

	/* close the additional main thread handle */
	if (hMainThread) {
		CloseHandle(hMainThread);
		hMainThread = NULL;
	}

	/* stop using the memory pool */
	IOCPLPoolDone();

	/* now reap all handles... */
	CloseHandle(hIoCompletionThread);
	hIoCompletionThread = NULL;
	CloseHandle(hndIOCPLPort);
	hndIOCPLPort = NULL;
	CloseHandle(hMainRpcDone);
	hMainRpcDone = NULL;
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
		/* If found, eventually swap with last entry to keep
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
		/* Make sure the entry is not found and there is enough
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
free_io_completion_port_mem(void)
{
	/* At the moment, do absolutely nothing. Returning memory here
	 * requires NO PENDING OVERLAPPED OPERATIONS AT ALL at this
	 * point in time, and as long we cannot be reasonable sure about
	 * that the simple advice is:
	 *
	 * HANDS OFF!
	 */
}
#endif	/* DEBUG */

void
iocpl_notify(
	IoHndPad_T *	iopad,
	void		(*pfunc)(ULONG_PTR, IoCtx_t *),
	UINT_PTR	fdn
	)
{
	IoCtx_t	xf;

	memset(&xf, 0, sizeof(xf));
	xf.iopad    = iopad;
	xf.ppswake  = hMainRpcDone;
	xf.onIoDone = pfunc;
	xf.io.sfd   = fdn;
	PostQueuedCompletionStatus(hndIOCPLPort, 1, 0, &xf.ol);
	WaitForSingleObject(xf.ppswake, INFINITE);
}

/*
 * -------------------------------------------------------------------
 * APC callback for scheduling interface scans.
 *
 * We get an error when trying to send if the network interface is
 * gone or has lost link. Rescan interfaces to catch on sooner, but no
 * more often than once per minute.  Once ntpd is able to detect
 * changes without polling this should be unnecessary.
 */
static void WINAPI
apcOnUnexpectedNetworkError(
	ULONG_PTR arg
	)
{
	static u_long time_next_ifscan_after_error;

	UNUSED_ARG(arg);

	if (time_next_ifscan_after_error < current_time) {
		time_next_ifscan_after_error = current_time + 60;
		timer_interfacetimeout(current_time);
	}
	DPRINTF(4, ("UnexpectedNetworkError: interface may be down\n"));
}

/* -------------------------------------------------------------------
 *
 * Prelude to madness -- common error checking code
 *
 * -------------------------------------------------------------------
 */
extern char * NTstrerror(int err, BOOL *bfreebuf);

static void
LogIoError(
	const char *	msg,
	HANDLE		hnd,
	DWORD		err
	)
{
	static const char * const rmsg =
		"LogIoError (unknown source)";

	/* -*- format & print the error message -*-
	 * We have to resort to the low level error formatting functions
	 * here, since the error code can come from an overlapped result.
	 * Relying the value to be the same as the 'GetLastError()'
	 * result at this point of execution is shaky at best, and using
	 * 'SetLastError()' to force it seems too nasty.
	 */
	BOOL   dynbuf = FALSE;
	char * msgbuf = NTstrerror(err, &dynbuf);
	msyslog(LOG_ERR, "%s: hnd=%p, err=%u, '%s'",
		(msg ? msg : rmsg), hnd, err, msgbuf);
	if (dynbuf)
		LocalFree(msgbuf);
}

/* -------------------------------------------------------------------
 * synchronous IO request result check (network & serial)
 * -------------------------------------------------------------------
 */
static BOOL
IoResultCheck(
	DWORD		err,
	IoCtx_t * 	ctx,
	const char *	msg
	)
{
	DPRINTF(6, ("in IoResultCheck err = %d\n", err));

	switch (err) {
		/* The first ones are no real errors. */
	case ERROR_SUCCESS:	/* all is good */
	case ERROR_IO_PENDING:	/* callback pending */
		break;

		/* this defers the error processing to the main thread
		 * and continues silently.
		 */
	case ERROR_UNEXP_NET_ERR:
		if (hMainThread) {
			QueueUserAPC(apcOnUnexpectedNetworkError,
				hMainThread, ctx->io.sfd);
		}
		IoCtxRelease(ctx);
		return FALSE;

	default:
		LogIoError(msg, ctx->io.hnd, err);
		/* the next ones go silently -- only clean-up is done */
	case ERROR_INVALID_PARAMETER:	/* handle already closed (clock)*/
	case WSAENOTSOCK	    :	/* handle already closed (socket)*/
		IoCtxRelease(ctx);
		return FALSE;
	}
	return TRUE;
}

/* -------------------------------------------------------------------
 * IO callback context check -- serial (non-network) data streams
 *
 * Attention: deletes the IO context when the clock is dead!
 * -------------------------------------------------------------------
 */
static RIO_t*
getRioFromIoCtx(
	IoCtx_t *	ctx,
	ULONG_PTR	key,
	const char *	msg
	)
{
	/* Make sure the key matches the context info in the shared
	 * lock, the check for errors. If the error indicates the
	 * operation was cancelled, let the operation fail silently.
	 */
	RIO_t *		rio   = NULL;
	IoHndPad_T *	iopad = ctx->iopad;
	if (NULL != iopad) {
		rio = iopad->rsrc.rio;
		if (key != iopad->rsrc.key)
			rio = NULL;
		else if (ctx->io.hnd != iopad->handles[0])
			rio = NULL;
	}
	if (rio != NULL) switch (ctx->errCode) {
		/* When we got cancelled, don't spill messages */
	case ERROR_INVALID_PARAMETER:	/* handle already closed (clock) */
	case ERROR_OPERATION_ABORTED:	/* handle closed while wait      */
	case WSAENOTSOCK:	/* handle already closed (sock?) */
		ctx->errCode = ERROR_SUCCESS;
		rio = NULL;
	case ERROR_SUCCESS:		/* all is good */
		break;
	default:
		/* log error, but return -- caller has to handle this! */
		LogIoError(msg, ctx->io.hnd, ctx->errCode);
		break;
	}
	if (rio == NULL)
		IoCtxRelease(ctx);
	return rio;
}

/* -------------------------------------------------------------------
 * IO callback context check -- network sockets
 *
 * Attention: deletes the IO context when the endpoint is dead!
 * -------------------------------------------------------------------
 */
static endpt*
getEndptFromIoCtx(
	IoCtx_t *	ctx,
	ULONG_PTR	key
	)
{
	/* Make sure the key matches the context info in the shared
	 * lock, then check for errors. If the error indicates the
	 * operation was cancelled, let the operation fail silently.
	 *
	 * !Note! Since we use the lowest bit of the key to distinguish
	 * between regular and broadcast socket, we must make sure the
	 * LSB is not used in the reverse-link check. Hence we shift
	 * it out in both the input key and the registered source.
	 */
	endpt *		ep    = NULL;
	IoHndPad_T *	iopad = ctx->iopad;
	if (iopad != NULL) {
		ep = iopad->rsrc.ept;
		if ((key >> 1) != (iopad->rsrc.key >> 1))
			ep = NULL;
		else if (ctx->io.hnd != iopad->handles[key & 1])
			ep = NULL;
	}
	if (ep == NULL)
		IoCtxRelease(ctx);
	return ep;
}


static int
socketErrorCheck(
	IoCtx_t *	ctx,
	const char *	msg
	)
{
	int oval, olen; /* getsockopt params */
	int retCode;

	switch (ctx->errCode) {
	case ERROR_SUCCESS:		/* all is good */
		retCode = PKT_OK;
		break;
	case ERROR_UNEXP_NET_ERR:
		if (hMainThread)
			QueueUserAPC(apcOnUnexpectedNetworkError,
				hMainThread, ctx->io.sfd);
	case ERROR_INVALID_PARAMETER:	/* handle already closed (clock?)*/
	case ERROR_OPERATION_ABORTED:	/* handle closed while wait      */
	case WSAENOTSOCK            :	/* handle already closed (sock)  */
		retCode = PKT_SOCKET_ERROR;
		break;

	/* [Bug 3019] is hard to squash.
	 * We should not get this, but we do, unfortunately. Obviously
	 * Windows insists in terminating one overlapped I/O request
	 * when it receives a TTL-expired ICMP message, and since the
	 * write that caused it is long finished, this unfortunately
	 * hits the pending receive.
	 *
	 * The only way out seems to be to silently ignore this error
	 * and restart another round, in the hope this condition does
	 * not prevail. Clear any pending socket level errors, too.
	 */
	case ERROR_HOST_UNREACHABLE:
		oval = 0;
		olen = sizeof(oval);
		getsockopt(ctx->io.sfd, SOL_SOCKET, SO_ERROR, (char*)&oval, &olen);
		retCode = PKT_DROP;
		break;

	/* [Bug 3110] On POSIX systems, reading UDP data into too small
	 * a buffers silently truncates the message. Under Windows the
	 * data is also truncated, but it blarts loudly about that.
	 * Just pretend all is well, and all will be well.
	 *
	 * Note: We accept the truncated packet -- this is consistent with the
	 * POSIX / UNIX case where we have no notification about this at all.
	 */
	case ERROR_MORE_DATA:		/* Too Much data for Buffer	 */
	case WSAEMSGSIZE:
		retCode = PKT_OK; /* or PKT_DROP ??? */
		break;

	/* For any other error, log the error, clear the byte count, but
	 * return the endpoint. This prevents processing the packet and
	 * keeps the read-chain running -- otherwise NTPD will play
	 * dead duck!
	 */
	default:
		LogIoError(msg, ctx->io.hnd, ctx->errCode);
		retCode = PKT_DROP;
		break;
	}
	return retCode;
}

/*
 * -------------------------------------------------------------------
 * Serial IO stuff
 *
 * Part 1 -- COMM event handling
 *
 * This is the initial step for serial line input: wait for COM event.
 * We always check for DCD changes (for user-mode PPS time stamps) and
 * either a flag char (line feed, for line mode emulation) or any
 * input character (raw mode). In the callback we decide if we just
 * have to go on with waiting, or if there is data we must read.
 * Depending on the mode, we either queue a raw read or a 'regular'
 * read request.
 *
 * !Note! Currently on single IO context circles through the WAIT,
 * READ and PROCESS stages. For better performance, it might make
 * sense to have on cycle for the wait, spinning off new read requests
 * when there is data. There are actually two problems that must be
 * solved:
 *  - We would need a queue on post-processing.
 *  - We have to take care of the order of read results. While the
 *    IOCPL queue guarantees delivery in the order of enque, the
 *    order of enque is not guaranteed once multiple reads are in
 *    flight.
 *
 * So, for the time being, we have one request cycling...
 * -------------------------------------------------------------------
 */

static BOOL __fastcall
QueueSerialWait(
	IoCtx_t *	lpo,
	recvbuf_t *	buff
	)
{
	static const char * const msg =
		"QueueSerialWait: cannot wait for COM event";

	BOOL	rc;

	lpo->onIoDone = OnSerialWaitComplete;
	lpo->recv_buf = buff;
	lpo->flRawMem = 0;

	buff->fd = lpo->iopad->riofd;
	/* keep receive position for continuation of partial lines! */
	rc  = WaitCommEvent(lpo->io.hnd, &lpo->aux.com_events, &lpo->ol);
	return rc || IoResultCheck(GetLastError(), lpo, msg);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void 
OnSerialWaitComplete(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	static const char * const msg =
		"OnSerialWaitComplete: wait for COM event failed";

	DevCtx_t *	dev;
	PPSDataEx_t *	ppsbuf;
	DWORD 		modem_status;
	u_long		covc;

	/* Make sure this RIO is not closed. */
	if (NULL == getRioFromIoCtx(lpo, key, msg))
		return;

	/* start next IO and leave if we hit an error */
	if (lpo->errCode != ERROR_SUCCESS) {
		memset(&lpo->aux, 0, sizeof(lpo->aux));
		IoCtxStartChecked(lpo, QueueSerialWait, lpo->recv_buf);
		return;
	}

#ifdef DEBUG
	if (~(EV_RXFLAG | EV_RLSD | EV_RXCHAR) & lpo->aux.com_events) {
		msyslog(LOG_ERR, "WaitCommEvent returned unexpected mask %x",
			lpo->aux.com_events);
		exit(-1);
	}
#endif
	/* Take note of changes on DCD; 'user mode PPS hack'.
	 * perlinger@ntp.org suggested a way of solving several problems
	 * with this code that makes a lot of sense: move to a putative
	 * dcdpps-ppsapi-provider.dll.
	 *
	 * perlinger@ntp.org: It came out as loopback-ppsapi-provider
	 * (because it loops back into NTPD), but I had to maintain the
	 * old hack for backward compatibility.
	 */
	if (EV_RLSD & lpo->aux.com_events) {
		modem_status = 0;
		GetCommModemStatus(lpo->io.hnd, &modem_status);
		if (NULL != (dev = lpo->devCtx)) {
			/* PPS-context available -- use it! */
			if (MS_RLSD_ON & modem_status) {
				dev->pps_data.cc_assert++;
				dev->pps_data.ts_assert = lpo->aux.RecvTime;
				DPRINTF(2, ("upps-real: fd %d DCD PPS Rise at %s\n",
					lpo->iopad->rsrc.rio->fd,
					ulfptoa(&lpo->aux.RecvTime, 6)));
			} else {
				dev->pps_data.cc_clear++;
				dev->pps_data.ts_clear = lpo->aux.RecvTime;
				DPRINTF(2, ("upps-real: fd %d DCD PPS Fall at %s\n",
					lpo->iopad->rsrc.rio->fd,
					ulfptoa(&lpo->aux.RecvTime, 6)));
			}
			/* Update PPS buffer, writing from low to high, with index
			 * update as last action. We use interlocked ops and a
			 * volatile data destination to avoid reordering on compiler
			 * and CPU level. The interlocked instruction act as full
			 * barriers -- we need only release semantics, but we don't
			 * have them before VS2010.
			 */
			covc   = dev->cov_count + 1u;
			ppsbuf = dev->pps_buff + (covc & PPS_QUEUE_MSK);
			InterlockedExchange((PLONG)&ppsbuf->cov_count, covc);
			ppsbuf->data = dev->pps_data;
			InterlockedExchange((PLONG)&dev->cov_count, covc);
		}
		/* perlinger@ntp.org, 2012-11-19
		 * It can be argued that once you have the PPS API active, you can
		 * disable the old pps hack. This would give a behaviour that's much
		 * more like the behaviour under a UN*Xish OS. On the other hand, it
		 * will give a nasty surprise for people which have until now happily
		 * taken the pps hack for granted, and after the first complaint, I have
		 * decided to keep the old implementation.
		 *
		 * perlinger@ntp.org, 2017-03-04
		 * If the loopback PPS API provider is active on this channel, the
		 * PPS hack will be *disabled*.
		 *
		 * backward compat: 'usermode-pps-hack'
		 */
		if ((MS_RLSD_ON & modem_status) && !(dev && dev->pps_active)) {
			lpo->aux.DCDSTime = lpo->aux.RecvTime;
			lpo->aux.flTsDCDS = 1;
			DPRINTF(2, ("upps-hack: fd %d DCD PPS Rise at %s\n",
				lpo->iopad->rsrc.rio->fd,
				ulfptoa(&lpo->aux.RecvTime, 6)));
		}
	}

	/* If IO ready, read data. Go back waiting else. */
	if (EV_RXFLAG & lpo->aux.com_events) {		/* line discipline */
		lpo->aux.FlagTime = lpo->aux.RecvTime;
		lpo->aux.flTsFlag = 1;
		IoCtxStartChecked(lpo, QueueSerialRead, lpo->recv_buf);
	} else if (EV_RXCHAR & lpo->aux.com_events) {	/* raw discipline */
		lpo->aux.FlagTime = lpo->aux.RecvTime;
		lpo->aux.flTsFlag = 1;
		IoCtxStartChecked(lpo, QueueRawSerialRead, lpo->recv_buf);
	} else {					/* idle... */
		IoCtxStartChecked(lpo, QueueSerialWait, lpo->recv_buf);
	}
}

/*
 * -------------------------------------------------------------------
 * Serial IO stuff
 *
 * common for both modes
 * -------------------------------------------------------------------
 */
static BOOL __fastcall
QueueSerialReadCommon(
	IoCtx_t *	lpo,
	recvbuf_t *	buff
	)
{
	static const char * const msg =
		"QueueSerialRead: cannot schedule device read";

	BOOL	rc;

	/* 'lpo->onIoDone' must be set already! */
	lpo->recv_buf = buff;
	lpo->flRawMem = 0;

	/* 'buff->recv_length' must be set already! */
	buff->fd        = lpo->iopad->riofd;
	buff->dstadr    = NULL;
	buff->receiver  = process_refclock_packet;
	buff->recv_peer = lpo->iopad->rsrc.rio->srcclock;

	rc = ReadFile(lpo->io.hnd,
		(char*)buff->recv_buffer + buff->recv_length,
		sizeof(buff->recv_buffer) - buff->recv_length,
		NULL, &lpo->ol);
	return rc || IoResultCheck(GetLastError(), lpo, msg);
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Start & Queue a serial read for line discipline emulation.
 */
static BOOL __fastcall
QueueSerialRead(
	IoCtx_t *	lpo,
	recvbuf_t *	buff
	)
{
	lpo->onIoDone = &OnSerialReadComplete;
	/* keep 'buff->recv_length' for line continuation! */
	return QueueSerialReadCommon(lpo, buff);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * IO completion thread callback. Takes a time stamp and offloads the
 * real work to the worker pool ASAP.
 */
static void
OnSerialReadComplete(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	static const char * const msg =
		"OnSerialReadComplete: read from device failed";

	/* Make sure this RIO is not closed. */
	if (NULL == getRioFromIoCtx(lpo, key, msg))
		return;

	/* start next IO and leave if we hit an error */
	if (lpo->errCode != ERROR_SUCCESS)
		goto wait_again;

	/* Offload to worker pool, if there is data */
	if (lpo->byteCount == 0)
		goto wait_again;

	if (QueueUserWorkItem(&OnSerialReadWorker, lpo, WT_EXECUTEDEFAULT))
		return;	/* successful regular exit! */

	/* croak as we're throwing away data */
	msyslog(LOG_ERR,
		"Can't offload to worker thread, will skip data: %m");

wait_again:
	/* make sure the read is issued again */
	memset(&lpo->aux, 0, sizeof(lpo->aux));
	IoCtxStartChecked(lpo, QueueSerialWait, lpo->recv_buf);
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
 *
 * !!ATTENTION!!
 * This function runs on an arbitrary worker thread. The resource
 * management with regard to IO is synchronised only between the main
 * thread and the IO worker thread, so decisions about queueing and
 * starting new IO must be made by either of them.
 *
 * Since the IO thread sticks in the IOCPL queue and is not alertable,
 * we could either use the APC queue to the main thread or the IOCPL
 * queue to the IO thread.
 *
 * We separate the effort -- filtering based on the RIO state is done
 * by the main thread, restarting the IO by the IO thread to reduce
 * delays.
 */

/* -------------------------------------------------------------------
 * IOCPL deferred bouncer -- start a new serial wait from IOCPL thread
 */
static void
OnDeferredStartWait(
	ULONG_PTR	key,
	IoCtx_t *	lpo
)
{
	IoCtxStartChecked(lpo, QueueSerialWait, lpo->recv_buf);
}

/* -------------------------------------------------------------------
 * APC deferred bouncer -- put buffer to receive queueor eventually
 * discard it if source is already disabled. Runs in the context
 * of the main thread exclusively.
 */
static void WINAPI
OnEnqueAPC(
	ULONG_PTR arg
)
{
	recvbuf_t *	buff  = (recvbuf_t*)arg;
	IoHndPad_T *	iopad = (IoHndPad_T*)buff->recv_peer;
	RIO_t *         rio   = iopad->rsrc.rio;

	/* Down below we make a nasty hack to transport the iopad
	 * pointer in the buffer so we can avoid another temporary
	 * allocation. We must undo this here.
	*/
	if (NULL != rio) {
		/* OK, refclock still attached */
		buff->recv_peer = rio->srcclock;
		if (iohpQueueLocked(iopad, iohpRefClockOK, buff))
			++rio->srcclock->received;
	} else {
		/* refclock detached while in flight... */
		freerecvbuf(buff);
	}
	iohpDetach(iopad); /* one unit owned by this callback! */
}

/* -------------------------------------------------------------------
 * worker pool thread worker doing the string processing
 */
static DWORD WINAPI
OnSerialReadWorker(
	void *	ctx
	)
{
	IoCtx_t *	lpo  = (IoCtx_t*)ctx;
	IoHndPad_T *	iop  = lpo->iopad;
	recvbuf_t *	buff = lpo->recv_buf;
	recvbuf_t *	obuf = NULL;
	char		*sptr, *send, *dptr;
	BOOL		eol;
	int		ch;

	/* We should never gat a zero-byte read here. If we do, nothing
	 * really bad happens, just a useless rescan of data we have
	 * already processed. But somethings not quite right in logic
	 * and we croak loudly in debug builds.
	 */
	DEBUG_INSIST(lpo->byteCount > 0);

	/* Account for additional input and then mimic the UNIX line
	 * discipline. This is an implict state machine -- the
	 * implementation is very low-level to gather speed.
	 */
	buff->recv_length += (int)lpo->byteCount;
	sptr = (char *)buff->recv_buffer;
	send = sptr + buff->recv_length;
	if (sptr == send)
		goto st_read_fresh;

st_new_obuf:
	/* Get new receive buffer to store the line. */
	obuf = get_free_recv_buffer_alloc();
	obuf->fd        = buff->fd;
	obuf->receiver  = buff->receiver;
	obuf->dstadr    = NULL;
	obuf->recv_peer = buff->recv_peer;
	set_serial_recv_time(obuf, lpo);

st_copy_start:
	/* Copy data to new buffer, convert CR to LF on the fly.
	 * Stop after either.
	 */
	dptr = (char *)obuf->recv_buffer;
	do {
		ch = *sptr++;
		if ('\r' == ch)
			ch = '\n';
		*dptr++ = ch;
		eol = ('\n' == ch);
	} while (!(eol || sptr == send));
	obuf->recv_length = (int)(dptr - (char *)obuf->recv_buffer);

	/* If we're not at EOL, we need more data to continue the line.
	 * But this can only be done if there's more room in the buffer;
	 * if we have already reached the maximum size, treat the whole
	 * buffer as part of a mega-line and pass it on.
	 */
	if (!eol) {
		if (obuf->recv_length < sizeof(obuf->recv_buffer))
			goto st_read_more;
		else
			goto st_pass_buffer;
	}

	/* if we should drop empty lines, do it here. */
	if (obuf->recv_length < 2 && iop->flDropEmpty) {
		obuf->recv_length = 0;
		if (sptr != send)
			goto st_copy_start;
		else
			goto st_read_more;
	}

	if ( ! iop->flFirstSeen) {
		iop->flFirstSeen = 1;
		obuf->recv_length = 0;
		if (sptr != send)
			goto st_copy_start;
		else
			goto st_read_more;
	}

st_pass_buffer:
	/* if we arrive here, we can spin off another text line to the
	 * receive queue. We use a hack to supplant the RIO pointer in
	 * the receive buffer with the IOPAD to save us a temporary
	 * workspace allocation. Note the callback owns one refcount
	 * unit to keep the IOPAD alive! Also checking that the RIO in
	 * the IOPAD matches the RIO in the buffer is dangerous: That
	 * pointer is manipulated by the other threads!
	 */
	obuf->recv_peer = (struct peer*)iohpAttach(lpo->iopad);
	QueueUserAPC(OnEnqueAPC, hMainThread, (ULONG_PTR)obuf);
	if (sptr != send)
		goto st_new_obuf;
	buff->recv_length = 0;
	goto st_read_fresh;

st_read_more:
	/* read more data into current OBUF, which is valid and will
	 * replace BUFF.
	 */
	lpo->recv_buf = obuf;
	freerecvbuf(buff);

st_read_fresh:
	/* Start next round. This is deferred to the IOCPL thread, as
	 * read access to the IOPAD is unsafe from a worker thread
	 * for anything but the flags. If the IOCPL handle is gone,
	 * just mop up the pieces.
	 */
	lpo->onIoDone = OnDeferredStartWait;
	if (!(hndIOCPLPort && PostQueuedCompletionStatus(hndIOCPLPort, 1, 0, &lpo->ol)))
		IoCtxRelease(lpo);
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

static BOOL __fastcall
QueueRawSerialRead(
	IoCtx_t *	lpo,
	recvbuf_t *	buff
	)
{
	lpo->onIoDone     = OnRawSerialReadComplete;
	buff->recv_length = 0;
	return QueueSerialReadCommon(lpo, buff);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * IO completion thread callback. Takes a time stamp and offloads the
 * real work to the worker pool ASAP.
 */
static void
OnRawSerialReadComplete(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	static const char * const msg =
		"OnRawSerialReadComplete: read from device failed";

	recvbuf_t *	buff = lpo->recv_buf;
	RIO_t *		rio  = getRioFromIoCtx(lpo, key, msg);
	/* Make sure this RIO is not closed. */
	if (rio == NULL)
		return;

	/* start next IO and leave if we hit an error */
	if (lpo->errCode == ERROR_SUCCESS && lpo->byteCount > 0) {
		buff->recv_length = (int)lpo->byteCount;
		set_serial_recv_time(buff, lpo);
		iohpQueueLocked(lpo->iopad, iohpRefClockOK, buff);
		buff = get_free_recv_buffer_alloc();
	}
	IoCtxStartChecked(lpo, QueueSerialWait, buff);
}


static void
set_serial_recv_time(
	recvbuf_t *	obuf,
	IoCtx_t *	lpo
	)
{
	/* Time stamp assignment is interesting.  If we
	 * have a DCD stamp, we use it, otherwise we use
	 * the FLAG char event time, and if that is also
	 * not / no longer available we use the arrival
	 * time.
	 */
	if (lpo->aux.flTsDCDS)
		obuf->recv_time = lpo->aux.DCDSTime;
	else if (lpo->aux.flTsFlag)
		obuf->recv_time = lpo->aux.FlagTime;
	else
		obuf->recv_time = lpo->aux.RecvTime;

	lpo->aux.flTsDCDS = lpo->aux.flTsFlag = 0; /* use only once! */
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
	static const char * const msg =
		"async_write: cannot schedule device write";
	static const char * const dmsg =
		"overlapped IO data buffer";

	IoCtx_t *	lpo  = NULL;
	void *		buff = NULL;
	HANDLE		hnd  = NULL;
	BOOL		rc;

	hnd = (HANDLE)_get_osfhandle(fd);
	if (hnd == INVALID_HANDLE_VALUE)
		goto fail;
	if (NULL == (buff = IOCPLPoolMemDup(data, count, dmsg)))
		goto fail;
	if (NULL == (lpo = IoCtxAlloc(NULL, NULL)))
		goto fail;

	lpo->io.hnd    = hnd;
	lpo->onIoDone  = OnSerialWriteComplete;
	lpo->trans_buf = buff;
	lpo->flRawMem  = 1;

	rc = WriteFile(lpo->io.hnd, lpo->trans_buf, count,
		       NULL, &lpo->ol);
	if (rc || IoResultCheck(GetLastError(), lpo, msg))
		return count;	/* normal/success return */

	errno = EBADF;
	return -1;

fail:
	IoCtxFree(lpo);
	IOCPLPoolFree(buff, dmsg);
	return -1;
}

static void
OnSerialWriteComplete(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	/* This is really trivial: Let 'getRioFromIoCtx()' do all the
	 * error processing, and it returns with a valid RIO, just
	 * drop the complete context.
	 */
	static const char * const msg =
		"OnSerialWriteComplete: serial output failed";

	if (NULL != getRioFromIoCtx(lpo, key, msg))
		IoCtxRelease(lpo);
}


/*
 * -------------------------------------------------------------------
 * Serial IO stuff
 *
 * Part 5 -- read PPS time stamps
 *
 * -------------------------------------------------------------------
 */

__declspec(dllexport) void* __stdcall
ntp_pps_attach_device(
	HANDLE	hndIo
	)
{
	DevCtx_t *	dev = NULL;

	dev = DevCtxAttach(serial_devctx(hndIo));
	if (NULL == dev)
		SetLastError(ERROR_INVALID_HANDLE);
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
	/* Reading from shared memory in a lock-free fashion can be
	 * a bit tricky, since we have to read the components in the
	 * opposite direction from the write, and the compiler must
	 * not reorder the read sequence.
	 * We use interlocked ops and a volatile data source to avoid
	 * reordering on compiler and CPU level. The interlocked
	 * instruction act as full barriers -- we need only acquire
	 * semantics, but we don't have them before VS2010.
	 */
	repc = 3;
	do {
		covc = InterlockedExchangeAdd((PLONG)&dev->cov_count, 0);
		ppsbuf = dev->pps_buff + (covc & PPS_QUEUE_MSK);
		*data = ppsbuf->data;
		guard = InterlockedExchangeAdd((PLONG)&ppsbuf->cov_count, 0);
		guard ^= covc;
	} while (guard && ~guard && --repc);

	if (guard) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}
	return TRUE;
}

/* --------------------------------------------------------------------
 * register and unregister refclock IOs with the IO engine
 * --------------------------------------------------------------------
 */

/* Add a reference clock data structures I/O handles to
 * the I/O completion port. Return FALSE if any error,
 * TRUE on success
 */  
BOOL
io_completion_port_add_clock_io(
	RIO_t *rio
	)
{
	static const char * const msgh =
		"io_completion_port_add_clock_io";

	IoCtx_t *	lpo;
	HANDLE		h;
	IoHndPad_T *	iopad = NULL;

	/* preset to clear state for error cleanup:*/
	rio->ioreg_ctx  = NULL;
	rio->device_ctx = NULL;

	h = (HANDLE)_get_osfhandle(rio->fd);
	if (h == INVALID_HANDLE_VALUE) {
		msyslog(LOG_ERR, "%s: COM port FD not valid",
			msgh);
		goto fail;
	}

	if (NULL == (rio->ioreg_ctx = iopad = iohpCreate(rio))) {
		msyslog(LOG_ERR, "%s: Failed to create shared lock",
			msgh);
		goto fail;
	}
	iopad->handles[0] = h;
	iopad->riofd      = rio->fd;
	iopad->rsrc.rio   = rio;

	if (NULL == (rio->device_ctx = DevCtxAttach(serial_devctx(h)))) {
		msyslog(LOG_ERR, "%s: Failed to allocate device context",
			msgh);
		goto fail;
	}

	if (NULL == (lpo = IoCtxAlloc(iopad, rio->device_ctx))) {
		msyslog(LOG_ERR, "%: Failed to allocate IO context",
			msgh);
		goto fail;
	}

	if ( ! CreateIoCompletionPort(h, hndIOCPLPort, (ULONG_PTR)rio, 0)) {
		msyslog(LOG_ERR, "%s: Can't add COM port to i/o completion port: %m",
			msgh);
		goto fail;
	}
	lpo->io.hnd = h;
	memset(&lpo->aux, 0, sizeof(lpo->aux));
	return QueueSerialWait(lpo, get_free_recv_buffer_alloc());

fail:
	rio->ioreg_ctx  = iohpDetach(rio->ioreg_ctx);
	rio->device_ctx = DevCtxDetach(rio->device_ctx);
	return FALSE;
}

/* ----------------------------------------------------------------- */
static void
OnSerialDetach(
	ULONG_PTR	key,
	IoCtx_t *	lpo
)
{
	/* Make sure the key matches the context info in the shared
	* lock, the check for errors. If the error indicates the
	* operation was cancelled, let the operation fail silently.
	*/
	IoHndPad_T *	iopad = lpo->iopad;

	INSIST(NULL != iopad);
	if (iopad->handles[0] == lpo->io.hnd) {
		iopad->handles[0] = INVALID_HANDLE_VALUE;
		iopad->handles[1] = INVALID_HANDLE_VALUE;
		iopad->rsrc.rio   = NULL;
		iopad->riofd      = -1;
	}
	SetEvent(lpo->ppswake);
}


void
io_completion_port_remove_clock_io(
	RIO_t *rio
	)
{
	IoHndPad_T *	iopad = (IoHndPad_T*)rio->ioreg_ctx;

	INSIST(hndIOCPLPort && hMainRpcDone);
	if (iopad)
		iocpl_notify(iopad, OnSerialDetach, _get_osfhandle(rio->fd));
}

/*
 * -------------------------------------------------------------------
 * Socket IO stuff
 * -------------------------------------------------------------------
 */

/* Queue a receiver on a socket. Returns 0 if no buffer can be queued 
 *
 *  Note: As per the WINSOCK documentation, we use WSARecvFrom. Using
 *	  ReadFile() is less efficient. Also, WSARecvFrom delivers
 *	  the remote network address. With ReadFile, getting this
 *	  becomes a chore.
 */
static BOOL __fastcall
QueueSocketRecv(
	IoCtx_t *	lpo,
	recvbuf_t *	buff
	)
{
	static const char * const msg =
		"QueueSocketRecv: cannot schedule socket receive";

	WSABUF	wsabuf;
	int	rc;

	lpo->onIoDone = OnSocketRecv;
	lpo->recv_buf = buff;
	lpo->flRawMem = 0;
	lpo->ioFlags  = 0;

	buff->fd              = lpo->io.sfd;
	buff->recv_srcadr_len = sizeof(buff->recv_srcadr);
	buff->receiver        = receive;
	buff->dstadr          = lpo->iopad->rsrc.ept;

	wsabuf.buf = (char *)buff->recv_buffer;
	wsabuf.len = sizeof(buff->recv_buffer);

	rc = WSARecvFrom(lpo->io.sfd, &wsabuf, 1, NULL, &lpo->ioFlags,
			 &buff->recv_srcadr.sa, &buff->recv_srcadr_len, 
			 &lpo->ol, NULL);
	return !rc || IoResultCheck((DWORD)WSAGetLastError(), lpo, msg);
}

/* ----------------------------------------------------------------- */
static void
OnSocketRecv(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	static const char * const msg =
		"OnSocketRecv: receive from socket failed";

	recvbuf_t *	buff	= NULL;
	IoHndPad_T *	iopad	= NULL;
	endpt *		ep	= NULL;
	int		rc;

	/* order is important -- check first, then get endpoint! */
	rc = socketErrorCheck(lpo, msg);
	ep = getEndptFromIoCtx(lpo, key);

	/* Make sure this endpoint is not closed. */
	if (ep == NULL)
		return;

	/* We want to start a new read before we process the buffer.
	 * Since we must not use the context object once it is in
	 * another IO, we go through some pains to read everything
	 * before going out for another read request.
	 * We also need an extra hold to the IOPAD structure.
	 */
	iopad = iohpAttach(lpo->iopad);
	if (rc == PKT_OK && lpo->byteCount > 0) {
		/* keep input buffer, create new one for IO */
		buff              = lpo->recv_buf;
		lpo->recv_buf     = get_free_recv_buffer_alloc();

		buff->recv_time   = lpo->aux.RecvTime;
		buff->recv_length = (int)lpo->byteCount;

	} /* Note: else we use the current buffer again */

	if (rc != PKT_SOCKET_ERROR) {
		IoCtxStartChecked(lpo, QueueSocketRecv, lpo->recv_buf);
	}  else {
		freerecvbuf(lpo->recv_buf);
		IoCtxFree(lpo);
	}
	/* below this, any usage of 'lpo' is invalid! */

	/* If we have a buffer, do some bookkeeping and other chores,
	 * then feed it to the input queue. And we can be sure we have
	 * a packet here, so we can update the stats.
	 */
	if (buff != NULL) {
		INSIST(buff->recv_srcadr_len <= sizeof(buff->recv_srcadr));
		DPRINTF(4, ("%sfd %d %s recv packet mode is %d\n",
			(MODE_BROADCAST == get_packet_mode(buff))
			? " **** Broadcast "
			: "",
			(int)buff->fd, stoa(&buff->recv_srcadr),
			get_packet_mode(buff)));

		if (iohpEndPointOK(iopad)) {
			InterlockedIncrement(&ep->received);
			InterlockedIncrement(&packets_received);
			InterlockedIncrement(&handler_pkts);
		}

		DPRINTF(2, ("Received %d bytes fd %d in buffer %p from %s, state = %s\n",
			buff->recv_length, (int)buff->fd, buff,
			stoa(&buff->recv_srcadr), st_packet_handling[rc]));
		iohpQueueLocked(iopad, iohpEndPointOK, buff);
	}
	iohpDetach(iopad);
}

/* ----------------------------------------------------------------- */
static void
OnSocketSend(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	/* this is somewhat easier: */
	static const char * const msg =
		"OnSocketSend: send to socket failed";

	endpt *		ep	= NULL;
	int		rc;

	/* order is important -- check first, then get endpoint! */
	rc = socketErrorCheck(lpo, msg);
	ep = getEndptFromIoCtx(lpo, key);

	/* Make sure this endpoint is not closed. */
	if (ep == NULL)
		return;

	if (rc != PKT_OK) {
		InterlockedIncrement(&ep->notsent);
		InterlockedDecrement(&ep->sent);
		InterlockedIncrement(&packets_notsent);
		InterlockedDecrement(&packets_sent);
	}
	IoCtxRelease(lpo);
}

/* --------------------------------------------------------------------
 * register and de-register interface endpoints with the IO engine
 * --------------------------------------------------------------------
 */
static void
OnInterfaceDetach(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	IoHndPad_T *	iopad = lpo->iopad;

	INSIST(NULL != iopad);
	iopad->handles[0] = INVALID_HANDLE_VALUE;
	iopad->handles[1] = INVALID_HANDLE_VALUE;
	iopad->rsrc.ept = NULL;

	SetEvent(lpo->ppswake);
}

/* ----------------------------------------------------------------- */
BOOL
io_completion_port_add_interface(
	endpt *	ep
	)
{
	/* Registering an endpoint is simple: allocate a shared lock for
	 * the enpoint and return if the allocation was successful.
	 */
	ep->ioreg_ctx = iohpCreate(ep);
	return ep->ioreg_ctx != NULL;
}
/* ----------------------------------------------------------------- */
void
io_completion_port_remove_interface(
	endpt *	ep
	)
{
	/* Removing an endpoint is simple, too: Lock the shared lock
	 * for write access, then invalidate the handles and the
	 * endpoint pointer. Do an additional detach and leave the
	 * write lock.
	 */
	IoHndPad_T *	iopad = (IoHndPad_T*)ep->ioreg_ctx;

	INSIST(hndIOCPLPort && hMainRpcDone);
	if (iopad)
		iocpl_notify(iopad, OnInterfaceDetach, (UINT_PTR)-1);
}

/* --------------------------------------------------------------------
 * register and de-register sockets for an endpoint
 * --------------------------------------------------------------------
 */

static void
OnSocketDetach(
	ULONG_PTR	key,
	IoCtx_t *	lpo
	)
{
	IoHndPad_T *	iopad = lpo->iopad;

	INSIST(NULL != iopad);
	if (iopad->handles[0] == lpo->io.hnd)
		iopad->handles[0] = INVALID_HANDLE_VALUE;
	if (iopad->handles[1] == lpo->io.hnd)
		iopad->handles[1] = INVALID_HANDLE_VALUE;

	SetEvent(lpo->ppswake);
}

/* Add a socket handle to the I/O completion port, and send
 * NTP_RECVS_PER_SOCKET receive requests to the kernel.
 */
BOOL
io_completion_port_add_socket(
	SOCKET	sfd,
	endpt *	ep,
	BOOL	bcast
	)
{
	/* Assume the endpoint is already registered. Set the socket
	 * handle into the proper slot, and then start up the IO engine.
	 */
	static const char * const msg =
		"Can't add socket to i/o completion port";

	IoCtx_t *	lpo;
	size_t		n;
	ULONG_PTR	key;
	IoHndPad_T *	iopad = NULL;

	key = ((ULONG_PTR)ep & ~(ULONG_PTR)1u) + !!bcast;

	if (NULL == (iopad = (IoHndPad_T*)ep->ioreg_ctx)) {
		msyslog(LOG_CRIT, "io_completion_port_add_socket: endpt = %p not registered, exiting",
			ep);
		exit(1);
	} else {
		endpt *	rep = iopad->rsrc.ept;
		iopad->handles[!!bcast] = (HANDLE)sfd;
		INSIST(rep == ep);
	}

	if (NULL == CreateIoCompletionPort((HANDLE)sfd,
		hndIOCPLPort, key, 0))
	{
		msyslog(LOG_ERR, "%s: %m", msg);
		goto fail;
	}
	for (n = s_SockRecvSched; n > 0; --n) {
		if (NULL == (lpo = IoCtxAlloc(ep->ioreg_ctx, NULL))) {
			msyslog(LOG_ERR, "%s: no read buffer: %m", msg);
			goto fail;
		}
		lpo->io.sfd = sfd;
		if (!QueueSocketRecv(lpo, get_free_recv_buffer_alloc()))
			goto fail;
	}
	return TRUE;

fail:
	ep->ioreg_ctx = iohpDetach(ep->ioreg_ctx);
	return FALSE;
}
/* ----------------------------------------------------------------- */
void
io_completion_port_remove_socket(
	SOCKET	fd,
	endpt *	ep
	)
{
	/* Lock the shared lock for write, then search the given
	 * socket handle and replace it with an invalid handle value.
	 */
	IoHndPad_T *	iopad = (IoHndPad_T*)ep->ioreg_ctx;

	INSIST(hndIOCPLPort && hMainRpcDone);
	if (iopad)
		iocpl_notify(iopad, OnSocketDetach, fd);
}


/* --------------------------------------------------------------------
 * I/O API functions for endpoints / interfaces
 * --------------------------------------------------------------------
 */

/* io_completion_port_sendto() -- sendto() replacement for Windows
 *
 * Returns len after successful send.
 * Returns -1 for any error, with the error code available via
 *	msyslog() %m, or GetLastError().
 */
int
io_completion_port_sendto(
	endpt *		ep,
	SOCKET		sfd,
	void  *		pkt,
	size_t		len,
	sockaddr_u *	dest
	)
{
	static const char * const msg =
		"sendto: cannot schedule socket send";
	static const char * const dmsg =
		"overlapped IO data buffer";

	IoCtx_t *	lpo  = NULL;
	void *		dbuf = NULL;
	WSABUF		wsabuf;
	int		rc;

	if (len > INT_MAX)
		len = INT_MAX;

	if (NULL == (dbuf = IOCPLPoolMemDup(pkt, len, dmsg)))
		goto fail;
	/* We register the IO operation against the shared lock here.
	 * This is not strictly necessary, since the callback does not
	 * access the endpoint structure in any way...
	 */
	if (NULL == (lpo = IoCtxAlloc(ep->ioreg_ctx, NULL)))
		goto fail;

	lpo->onIoDone  = OnSocketSend;
	lpo->trans_buf = dbuf;
	lpo->flRawMem  = 1;
	lpo->io.sfd    = sfd;

	wsabuf.buf = (void*)lpo->trans_buf;
	wsabuf.len = (DWORD)len;

	rc  = WSASendTo(sfd, &wsabuf, 1, NULL, 0,
			&dest->sa, SOCKLEN(dest),
			&lpo->ol, NULL);
	if (!rc || IoResultCheck((DWORD)WSAGetLastError(), lpo, msg))
		return (int)len;	/* normal/success return */

	errno = EBADF;
	return -1;

fail:
	IoCtxFree(lpo);
	IOCPLPoolFree(dbuf, dmsg);
	return -1;
}

/* --------------------------------------------------------------------
 * GetReceivedBuffers
 * Note that this is in effect the main loop for processing requests
 * both send and receive. This should be reimplemented
 */
int
GetReceivedBuffers(void)
{
	DWORD	index;
	HANDLE	ready;
	int	errcode;
	BOOL	dynbuf;
	BOOL	have_packet;
	char *	msgbuf;

	have_packet = FALSE;
	while (!have_packet) {
		index = WaitForMultipleObjectsEx(
			ActiveWaitHandles, WaitHandles,
			FALSE, INFINITE, TRUE);
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

		case WAIT_IO_COMPLETION: /* there might be something after APC */
			have_packet = !!full_recvbuffs();
			break;

		case WAIT_TIMEOUT:
			msyslog(LOG_ERR,
				"WaitForMultipleObjectsEx INFINITE timed out.");
			break;

		case WAIT_FAILED:
			dynbuf = FALSE;
			errcode = GetLastError();
			msgbuf = NTstrerror(errcode, &dynbuf);
			msyslog(LOG_ERR,
				"WaitForMultipleObjectsEx Failed: Errcode = %n, msg = %s", errcode, msgbuf);
			if (dynbuf)
				LocalFree(msgbuf);
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
