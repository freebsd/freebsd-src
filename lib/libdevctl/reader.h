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
 * \file devctl_reader.h
 */
#ifndef	_DEVCTL_READER_H_
#define	_DEVCTL_READER_H_

/*=========================== Forward Declarations ===========================*/

/*============================ Namespace Control =============================*/
namespace DevCtl
{

/*============================= Class Definitions ============================*/

/*-------------------------------- Reader  -------------------------------*/
/**
 * \brief A class that presents a common interface to both file descriptors
 *        and istreams.
 *
 * Standard C++ provides no way to create an iostream from a file descriptor or
 * a FILE.  The GNU, Apache, HPUX, and Solaris C++ libraries all provide
 * non-standard ways to construct such a stream using similar semantics, but
 * FreeBSD's C++ library does not.  This class supports only the functionality
 * needed by ZFSD; it does not implement the iostream API.
 */
class Reader
{
public:
	/**
	 * \brief Return the number of bytes immediately available for reading
	 */
	virtual ssize_t	in_avail() const = 0;

	/**
	 * \brief Reads up to count bytes
	 *
	 * Whether this call blocks depends on the underlying input source.
	 * On error, -1 is returned, and errno will be set by the underlying
	 * source.
	 *
	 * \param buf    Destination for the data
	 * \param count  Maximum amount of data to read
	 * \returns      Amount of data that was actually read
	 */
	virtual ssize_t read(char* buf, size_t count) = 0;

	virtual ~Reader() = 0;
};

inline Reader::~Reader() {}


/*--------------------------------- FDReader ---------------------------------*/
/**
 * \brief Specialization of Reader that uses a file descriptor
 */
class FDReader : public Reader
{
public:
	/**
	 * \brief Constructor
	 *
	 * \param fd  An open file descriptor.  It will not be garbage
	 *            collected by the destructor.
	 */
	FDReader(int fd);

	virtual ssize_t  in_avail() const;

	virtual ssize_t read(char* buf, size_t count);

protected:
	/** Copy of the underlying file descriptor */
	int m_fd;
};

/*-------------------------------- IstreamReader------------------------------*/
/**
 * \brief Specialization of Reader that uses a std::istream
 */
class IstreamReader : public Reader
{
public:
	/**
	 * Constructor
	 *
	 * \param stream  Pointer to an open istream.  It will not be
	 *                garbage collected by the destructor.
	 */
	IstreamReader(std::istream* stream);

	virtual ssize_t in_avail() const;

	virtual ssize_t read(char* buf, size_t count);

protected:
	/** Copy of the underlying stream */
	std::istream *m_stream;
};

} // namespace DevCtl
#endif	/* _DEVCTL_READER_H_ */
