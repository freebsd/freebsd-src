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
 *
 * $FreeBSD$
 */

/**
 * \file devctl_event_buffer.h
 */
#ifndef	_DEVCTL_EVENT_BUFFER_H_
#define	_DEVCTL_EVENT_BUFFER_H_

/*============================ Namespace Control =============================*/
namespace DevCtl
{

/*=========================== Forward Declarations ===========================*/
class Reader;

/*============================= Class Definitions ============================*/
/*-------------------------------- EventBuffer -------------------------------*/
/**
 * \brief Class buffering event data from Devd or a similar source and
 *        splitting it into individual event strings.
 *
 * Users of this class initialize it with a Reader associated with the unix
 * domain socket connection with devd or a compatible source.  The lifetime of
 * an EventBuffer instance should match that of the Reader passed to it.  This
 * is required as data from partially received events is retained in the
 * EventBuffer in order to allow reconstruction of these events across multiple
 * reads of the stream.
 *
 * Once the program determines that the Reader is ready for reading, the
 * EventBuffer::ExtractEvent() should be called in a loop until the method
 * returns false.
 */
class EventBuffer
{
public:
	/**
	 * Constructor
	 *
	 * \param reader  The data source on which to buffer/parse event data.
	 */
	EventBuffer(Reader& reader);

	/**
	 * Pull a single event string out of the event buffer.
	 *
	 * \param eventString  The extracted event data (if available).
	 *
	 * \return  true if event data is available and eventString has
	 *          been populated.  Otherwise false.
	 */
	bool ExtractEvent(std::string &eventString);

private:
	enum {
		/**
		 * Size of an empty event (start and end token with
		 * no data.  The EventBuffer parsing needs at least
		 * this much data in the buffer for safe event extraction.
		 */
		MIN_EVENT_SIZE = 2,

		/*
		 * The maximum event size supported by ZFSD.
		 * Events larger than this size (minus 1) are
		 * truncated at the end of the last fully received
		 * key/value pair.
		 */
		MAX_EVENT_SIZE = 8192,

		/**
		 * The maximum amount of buffer data to read at
		 * a single time from the Devd file descriptor.
		 */
		MAX_READ_SIZE = MAX_EVENT_SIZE,

		/**
		 * The size of EventBuffer's buffer of Devd event data.
		 * This is one larger than the maximum supported event
		 * size, which alows us to always include a terminating
		 * NUL without overwriting any received data.
		 */
		EVENT_BUFSIZE = MAX_EVENT_SIZE + /*NUL*/1
	};

	/** The amount of data in m_buf we have yet to look at. */
	size_t UnParsed()        const;

	/** The amount of data in m_buf available for the next event. */
	size_t NextEventMaxLen() const;

	/** Fill the event buffer with event data from Devd. */
	bool Fill();

	/** Characters we treat as beginning an event string. */
	static const char   s_eventStartTokens[];

	/** Characters we treat as ending an event string. */
	static const char   s_eventEndTokens[];

	/** Characters found between successive "key=value" strings. */
	static const char   s_keyPairSepTokens[];

	/** Temporary space for event data during our parsing.  Laid out like
	 * this:
	 *         <--------------------------------------------------------->
	 *         |         |    |           |                              |
	 * m_buf---|         |    |           |                              |
	 * m_nextEventOffset--    |           |                              |
	 * m_parsedLen-------------           |                              |
	 * m_validLen--------------------------                              |
	 * EVENT_BUFSIZE------------------------------------------------------
	 *
	 * Data before m_nextEventOffset has already been processed.
	 *
	 * Data between m_nextEvenOffset and m_parsedLen has been parsed, but
	 * not processed as a single event.
	 *
	 * Data between m_parsedLen and m_validLen has been read from the
	 * source, but not yet parsed.
	 *
	 * Between m_validLen and EVENT_BUFSIZE is empty space.
	 *
	 * */
	char		    m_buf[EVENT_BUFSIZE];

	/** Reference to the reader linked to devd's domain socket. */
	Reader&		    m_reader;

	/** Offset within m_buf to the beginning of free space. */
	size_t		    m_validLen;

	/** Offset within m_buf to the beginning of data not yet parsed */
	size_t		    m_parsedLen;

	/** Offset within m_buf to the start token of the next event. */
	size_t		    m_nextEventOffset;

	/** The EventBuffer is aligned and tracking event records. */
	bool		    m_synchronized;
};

//- EventBuffer Inline Private Methods -----------------------------------------
inline size_t
EventBuffer::UnParsed() const
{
	return (m_validLen - m_parsedLen);
}

inline size_t
EventBuffer::NextEventMaxLen() const
{
	return (m_validLen - m_nextEventOffset);
}

} // namespace DevCtl
#endif	/* _DEVCTL_EVENT_BUFFER_H_ */
