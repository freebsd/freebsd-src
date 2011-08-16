/*-
 * Copyright (c) 2011 Spectra Logic Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * Authors: Justin T. Gibbs     (Spectra Logic Corporation)
 *
 * $FreeBSD$
 */

/**
 * \file zfsd.h
 *
 * Class definitions and supporting data strutures for the ZFS fault
 * management daemon.
 */
#ifndef	_ZFSD_H_
#define	_ZFSD_H_

#include <cstdarg>
#include <list>
#include <map>
#include <utility>

#include <sys/fs/zfs.h>
#include <libzfs.h>

#include "case_file.h"
#include "dev_ctl_event.h"
#include "vdev_iterator.h"

/*============================ Namespace Control =============================*/
using std::auto_ptr;
using std::map;
using std::pair;
using std::string;

/*================================ Global Data ===============================*/
extern const char       g_devdSock[];
extern int              g_debug;
extern libzfs_handle_t *g_zfsHandle;

/*=========================== Forward Declarations ===========================*/
struct EventFactoryRecord;
struct pidfh;

typedef int LeafIterFunc(zpool_handle_t *, nvlist_t *, void *);

/*================================== Macros ==================================*/
#define NUM_ELEMENTS(x) (sizeof(x) / sizeof(*x))

/*============================= Class Definitions ============================*/
/*-------------------------------- EventBuffer -------------------------------*/
/**
 * \brief Class buffering event data from Devd and splitting it
 *        into individual event strings.
 *
 * Users of this class initialize it with the file descriptor associated
 * with the unix domain socket connection with devd.  The lifetime of
 * an EventBuffer instance should match that of the file descriptor passed
 * to it.  This is required as data from partially received events is
 * retained in the EventBuffer in order to allow reconstruction of these
 * events across multiple reads of the Devd file descriptor.
 *
 * Once the program determines that the Devd file descriptor is ready
 * for reading, the EventBuffer::ExtractEvent() should be called in a
 * loop until the method returns false.
 */
class EventBuffer
{
public:
	/**
	 * Constructor
	 *
	 * \param fd  The file descriptor on which to buffer/parse event data.
	 */
	EventBuffer(int fd);

	/**
	 * Pull a single event string out of the event buffer.
	 *
	 * \param eventString  The extracted event data (if available).
	 *
	 * \return  true if event data is available and eventString has
	 *          been populated.  Otherwise false.
	 */
	bool ExtractEvent(string &eventString);

private:
	enum {
		/**
		 * Size of an empty event (start and end token with
		 * no data.  The EventBuffer parsing needs at least
		 * this much data in the buffer for safe event extraction.
		 */
		MIN_EVENT_SIZE = 2,

		/**
		 * The maximum amount of buffer data to read at
		 * a single time from the Devd file descriptor.
		 * This size matches the largest event size allowed
		 * in the system.
		 */
		MAX_READ_SIZE = 1024,

		/**
		 * The size of EventBuffer's buffer of Devd event data.
		 * This is one larger than the maximum event size which
		 * alows us to always include a terminating NUL without
		 * overwriting any received data.
		 */
		EVENT_BUFSIZE = MAX_READ_SIZE + /*NUL*/1
	};

	/** The amount of data in m_buf we have yet to look at. */
	size_t UnParsed();

	/** The amount of data in m_buf available for the next event. */
	size_t NextEventMaxLen();

	/** Fill the event buffer with event data from Devd. */
	bool Fill();

	/** Characters we treat as beginning an event string. */
	static const char   s_eventStartTokens[];

	/** Characters we treat as ending an event string. */
	static const char   s_eventEndTokens[];

	/** Temporary space for event data during our parsing. */
	char		    m_buf[EVENT_BUFSIZE];

	/** Copy of the file descriptor linked to devd's domain socket. */
	int		    m_fd;

	/** Valid bytes in m_buf. */
	size_t		    m_validLen;

	/** The amount of data in m_buf we have looked at. */
	size_t		    m_parsedLen;

	/** Offset to the start token of the next event. */
	size_t		    m_nextEventOffset;
};

//- EventBuffer Inline Private Methods -----------------------------------------
inline size_t
EventBuffer::UnParsed()
{
	return (m_validLen - m_parsedLen);
}

inline size_t
EventBuffer::NextEventMaxLen()
{
	return (m_validLen - m_nextEventOffset);
}

/*--------------------------------- ZfsDaemon --------------------------------*/
/**
 * Static singleton orchestrating the operations of the ZFS daemon program.
 */
class ZfsDaemon
{
public:
	/**
	 * Used by signal handlers to ensure, in a race free way, that
	 * the event loop will perform at least one more full loop
	 * before sleeping again.
	 */
	static void WakeEventLoop();

	/**
	 * Schedules a rescan of devices in the system for potential
	 * candidates to replace a missing vdev.  The scan is performed
	 * during the next run of the event loop.
	 */
	static void RequestSystemRescan();

	/**
	 * Queue an event for replay after the next ZFS configuration
	 * sync event is received.  This facility is used when an event
	 * is received for a pool or vdev that is not visible in the
	 * current ZFS configuration, but may "arrive" once the kernel
	 * commits the configuration change that emitted the event.
	 */
	static bool SaveEvent(const DevCtlEvent &event);

	/**
	 * Reprocess any events saved via the SaveEvent() facility.
	 */
	static void ReplayUnconsumedEvents();

	/** Daemonize and perform all functions of the ZFS daemon. */
	static void Run();

private:
	/** Initialize the daemon. */
	static void Init();

	/** Perform any necessary cleanup at daemon shutdown. */
	static void Fini();

	/** Process incoming devctl events from devd. */
	static void ProcessEvents(EventBuffer &eventBuffer);

	/** Discard all data pending in s_devdSockFD. */
	static void FlushEvents();

	static VdevCallback_t VdevAddCaseFile;

	/**
	 * Test for data pending in s_devdSockFD
	 *
	 * \return  True if data is pending.  Otherwise false.
	 */
	static bool EventsPending();

	/** Purge our cache of outstanding ZFS issues in the system. */
	static void PurgeCaseFiles();

	/** Build a cache of outstanding ZFS issues in the system. */
	static void BuildCaseFiles();

	/**
	 * Iterate over all known issues and attempt to solve them
	 * given resources currently available in the system.
	 */
	static void RescanSystem();

	/**
	 * Interrogate the system looking for previously unknown
	 * faults that occurred either before ZFSD was started,
	 * or during a period of lost communication with Devd.
	 */
	static void DetectMissedEvents();

	/**
	 * Wait for and process event source activity.
	 */
	static void EventLoop();

	/**
	 * Signal handler for which our response is to
	 * log the current state of the daemon.
	 *
	 * \param sigNum  The signal caught.
	 */
	static void InfoSignalHandler(int sigNum);

	/**
	 * Signal handler for which our response is to
	 * request a case rescan.
	 *
	 * \param sigNum  The signal caught.
	 */
	static void RescanSignalHandler(int sigNum);

	/**
	 * Signal handler for which our response is to
	 * gracefully terminate.
	 *
	 * \param sigNum  The signal caught.
	 */
	static void QuitSignalHandler(int sigNum);

	/**
	 * Open and lock our PID file.
	 */
	static void OpenPIDFile();

	/**
	 * Update our PID file with our PID.
	 */
	static void UpdatePIDFile();

	/**
	 * Close and release the lock on our PID file.
	 */
	static void ClosePIDFile();

	/**
	 * Perform syslog configuraiton.
	 */
	static void InitializeSyslog();

	/**
	 * Open a connection to devd's unix domain socket.
	 *
	 * \return  True if the connection attempt is successsful.  Otherwise
	 *          false.
	 */
	static bool ConnectToDevd();

	/**
	 * Close a connection (if any) to devd's unix domain socket.
	 */
	static void DisconnectFromDevd();

	/**
	 * Set to true when our program is signaled to
	 * gracefully exit.
	 */
	static bool   s_logCaseFiles;

	/**
	 * Set to true when our program is signaled to
	 * gracefully exit.
	 */
	static bool   s_terminateEventLoop;

	/**
	 * The canonical path and file name of zfsd's PID file.
	 */
	static char   s_pidFilePath[];

	/**
	 * Control structure for PIDFILE(3) API.
	 */
	static pidfh *s_pidFH;

	/**
	 * File descriptor representing the unix domain socket
	 * connection with devd.
	 */
	static int    s_devdSockFD;

	/**
	 * Pipe file descriptors used to close races with our
	 * signal handlers.
	 */
	static int    s_signalPipeFD[2];

	/** Queued events for replay. */
	static DevCtlEventList s_unconsumedEvents;

	/**
	 * Flag controlling a rescan from ZFSD's event loop of all
	 * GEOM providers in the system to find candidates for solving
	 * cases.
	 */
	static bool   s_systemRescanRequested;

	/**
	 * Flag controlling whether events can be queued.  This boolean
	 * is set during event replay to ensure that events for pools or
	 * devices no longer in the system are not retained forever.
	 */
	static bool   s_consumingEvents;
};

#endif	/* _ZFSD_H_ */
