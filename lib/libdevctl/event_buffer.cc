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
 * \file event_buffer.cc
 */

#include <sys/cdefs.h>
#include <sys/time.h>

#include <cstddef>
#include <err.h>
#include <errno.h>
#include <syslog.h>

#include <iostream>
#include <sstream>
#include <string>

#include "event_buffer.h"
#include "exception.h"
#include "reader.h"

__FBSDID("$FreeBSD$");

/*============================ Namespace Control =============================*/
using std::string;
using std::stringstream;
namespace DevCtl
{

/*============================= Class Definitions ============================*/
/*-------------------------------- EventBuffer -------------------------------*/
//- EventBuffer Static Data ----------------------------------------------------
/**
 * NOTIFY, NOMATCH, ATTACH, DETACH.  See DevCtlEvent::Type.
 */
const char EventBuffer::s_eventStartTokens[] = "!?+-";

/**
 * Events are terminated by a newline.
 */
const char EventBuffer::s_eventEndTokens[] = "\n";

/**
 * Key=Value pairs are terminated by whitespace.
 */
const char EventBuffer::s_keyPairSepTokens[] = " \t\n";

//- EventBuffer Public Methods -------------------------------------------------
EventBuffer::EventBuffer(Reader& reader)
 : m_reader(reader),
   m_validLen(0),
   m_parsedLen(0),
   m_nextEventOffset(0),
   m_synchronized(true)
{
}

bool
EventBuffer::ExtractEvent(string &eventString)
{
	stringstream tsField;
	timeval now;

	gettimeofday(&now, NULL);
	tsField << " timestamp=" << now.tv_sec;

	while (UnParsed() > 0 || Fill()) {

		/*
		 * If the valid data in the buffer isn't enough to hold
		 * a full event, try reading more.
		 */
		if (NextEventMaxLen() < MIN_EVENT_SIZE) {
			m_parsedLen += UnParsed();
			continue;
		}

		char  *nextEvent(m_buf + m_nextEventOffset);
		bool   truncated(true);
		size_t eventLen(strcspn(nextEvent, s_eventEndTokens));

		if (!m_synchronized) {
			/* Discard data until an end token is read. */
			if (nextEvent[eventLen] != '\0')
				m_synchronized = true;
			m_nextEventOffset += eventLen;
			m_parsedLen = m_nextEventOffset;
			continue;
		} else if (nextEvent[eventLen] == '\0') {

			m_parsedLen += eventLen;
			if (m_parsedLen < MAX_EVENT_SIZE) {
				/*
				 * Ran out of buffer before hitting
				 * a full event. Fill() and try again.
				 */
				continue;
			}
			syslog(LOG_WARNING, "Overran event buffer\n\tm_nextEventOffset"
			       "=%zd\n\tm_parsedLen=%zd\n\tm_validLen=%zd",
			       m_nextEventOffset, m_parsedLen, m_validLen);
		} else {
			/*
			 * Include the normal terminator in the extracted
			 * event data.
			 */
			eventLen += 1;
			truncated = false;
		}

		m_nextEventOffset += eventLen;
		m_parsedLen = m_nextEventOffset;
		eventString.assign(nextEvent, eventLen);

		if (truncated) {
			size_t fieldEnd;

			/* Break cleanly at the end of a key<=>value pair. */
			fieldEnd = eventString.find_last_of(s_keyPairSepTokens);
			if (fieldEnd != string::npos)
				eventString.erase(fieldEnd);
			eventString += '\n';

			m_synchronized = false;
			syslog(LOG_WARNING,
			       "Truncated %zd characters from event.",
			       eventLen - fieldEnd);
		}

		/*
		 * Add a timestamp as the final field of the event if it is
		 * not already present.
		 */
		if (eventString.find("timestamp=") == string::npos) {
			size_t eventEnd(eventString.find_last_not_of('\n') + 1);

			eventString.insert(eventEnd, tsField.str());
		}

		return (true);
	}
	return (false);
}

//- EventBuffer Private Methods ------------------------------------------------
bool
EventBuffer::Fill()
{
	ssize_t avail;
	ssize_t consumed(0);

	/* Compact the buffer. */
	if (m_nextEventOffset != 0) {
		memmove(m_buf, m_buf + m_nextEventOffset,
			m_validLen - m_nextEventOffset);
		m_validLen -= m_nextEventOffset;
		m_parsedLen -= m_nextEventOffset;
		m_nextEventOffset = 0;
	}

	/* Fill any empty space. */
	avail = m_reader.in_avail();
	if (avail > 0) {
		size_t want;

		want = std::min((size_t)avail, MAX_READ_SIZE - m_validLen);
		consumed = m_reader.read(m_buf + m_validLen, want);
		if (consumed == -1) {
			if (errno == EINTR)
				return (false);
			else
				err(1, "EventBuffer::Fill(): Read failed");
		}
	} else if (avail == -1) {
		throw Exception("EventBuffer::Fill(): Reader disconnected");	
	}

	m_validLen += consumed;
	/* Guarantee our buffer is always NUL terminated. */
	m_buf[m_validLen] = '\0';

	return (consumed > 0);
}

} // namespace DevCtl
