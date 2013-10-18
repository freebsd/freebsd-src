/*-
 * Copyright (c) 2011, 2012, 2013 Spectra Logic Corporation
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
 */

/**
 * \file consumer.cc
 */

#include <sys/cdefs.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

#include <list>
#include <map>
#include <string>

#include "guid.h"
#include "event.h"
#include "event_buffer.h"
#include "event_factory.h"
#include "exception.h"
#include "reader.h"

#include "consumer.h"

__FBSDID("$FreeBSD$");

/*================================== Macros ==================================*/
#define NUM_ELEMENTS(x) (sizeof(x) / sizeof(*x))

/*============================ Namespace Control =============================*/
using std::string;
namespace DevCtl
{

/*============================= Class Definitions ============================*/
/*----------------------------- DevCtl::Consumer -----------------------------*/
//- Consumer Static Private Data -----------------------------------------------
const char Consumer::s_devdSockPath[] = "/var/run/devd.pipe";

//- Consumer Public Methods ----------------------------------------------------
Consumer::Consumer(Event::BuildMethod *defBuilder,
		   EventFactory::Record *regEntries,
		   size_t numEntries)
 : m_devdSockFD(-1),
   m_reader(NULL),
   m_eventBuffer(NULL),
   m_eventFactory(defBuilder),
   m_replayingEvents(false)
{
	m_eventFactory.UpdateRegistry(regEntries, numEntries);
}

Consumer::~Consumer()
{
	DisconnectFromDevd();
	delete m_reader;
	m_reader = NULL;
}

bool
Consumer::ConnectToDevd()
{
	struct sockaddr_un devdAddr;
	int		   sLen;
	int		   result;

	if (m_devdSockFD != -1) {
		/* Already connected. */
		syslog(LOG_DEBUG, "%s: Already connected.", __func__);
		return (true);
	}
	syslog(LOG_INFO, "%s: Connecting to devd.", __func__);

	memset(&devdAddr, 0, sizeof(devdAddr));
	devdAddr.sun_family= AF_UNIX;
	strlcpy(devdAddr.sun_path, s_devdSockPath, sizeof(devdAddr.sun_path));
	sLen = SUN_LEN(&devdAddr);

	m_devdSockFD = socket(AF_UNIX, SOCK_STREAM, 0);
	if (m_devdSockFD == -1)
		err(1, "Unable to create socket");
	result = connect(m_devdSockFD,
			 reinterpret_cast<sockaddr *>(&devdAddr),
			 sLen);
	if (result == -1) {
		syslog(LOG_INFO, "Unable to connect to devd");
		DisconnectFromDevd();
		return (false);
	}

	/* Connect the stream to the file descriptor */
	m_reader = new FDReader(m_devdSockFD);
	m_eventBuffer = new EventBuffer(*m_reader);
	syslog(LOG_INFO, "Connection to devd successful");
	return (true);
}

void
Consumer::DisconnectFromDevd()
{
	if (m_devdSockFD != -1)
		syslog(LOG_INFO, "Disconnecting from devd.");

	delete m_eventBuffer;
	m_eventBuffer = NULL;
	delete m_reader;
	m_reader = NULL;
	close(m_devdSockFD);
	m_devdSockFD = -1;
}

void
Consumer::ReplayUnconsumedEvents(bool discardUnconsumed)
{
	EventList::iterator event(m_unconsumedEvents.begin());
	bool replayed_any = (event != m_unconsumedEvents.end());

	m_replayingEvents = true;
	if (replayed_any)
		syslog(LOG_INFO, "Started replaying unconsumed events");
	while (event != m_unconsumedEvents.end()) {
		bool consumed((*event)->Process());
		if (consumed || discardUnconsumed) {
			delete *event;
			event = m_unconsumedEvents.erase(event);
		} else {
			event++;
		}
	}
	if (replayed_any)
		syslog(LOG_INFO, "Finished replaying unconsumed events");
	m_replayingEvents = false;
}

bool
Consumer::SaveEvent(const Event &event)
{
        if (m_replayingEvents)
                return (false);
        m_unconsumedEvents.push_back(event.DeepCopy());
        return (true);
}

Event *
Consumer::NextEvent(EventBuffer *eventBuffer)
{
	if (!Connected())
		return(NULL);

	if (eventBuffer == NULL)
		eventBuffer = m_eventBuffer;

	Event *event(NULL);
	try {
		string evString;

		if (eventBuffer->ExtractEvent(evString))
			event = Event::CreateEvent(m_eventFactory, evString);
	} catch (const Exception &exp) {
		exp.Log();
		DisconnectFromDevd();
	}
	return (event);
}

/* Capture and process buffered events. */
void
Consumer::ProcessEvents(EventBuffer *eventBuffer)
{
	Event *event;
	while ((event = NextEvent(eventBuffer)) != NULL) {
		if (event->Process())
			SaveEvent(*event);
		delete event;
	}
}

void
Consumer::FlushEvents()
{
	char discardBuf[256];

	while (m_reader->in_avail() > 0)
		m_reader->read(discardBuf, sizeof(discardBuf));
}

bool
Consumer::EventsPending()
{
	struct pollfd fds[1];
	int	      result;

	do {
		fds->fd      = m_devdSockFD;
		fds->events  = POLLIN;
		fds->revents = 0;
		result = poll(fds, NUM_ELEMENTS(fds), /*timeout*/0);
	} while (result == -1 && errno == EINTR);

	if (result == -1)
		err(1, "Polling for devd events failed");

	if ((fds->revents & POLLERR) != 0)
		throw Exception("Consumer::EventsPending(): "
				"POLLERR detected on devd socket.");

	if ((fds->revents & POLLHUP) != 0)
		throw Exception("Consumer::EventsPending(): "
				"POLLHUP detected on devd socket.");

	return ((fds->revents & POLLIN) != 0);
}

} // namespace DevCtl
