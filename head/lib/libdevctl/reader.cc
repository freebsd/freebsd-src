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
 * Authors: Alan Somers         (Spectra Logic Corporation)
 */

/**
 * \file reader.cc
 */

#include <sys/cdefs.h>
#include <sys/ioctl.h>

#include <cstddef>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

#include <iostream>

#include "reader.h"

__FBSDID("$FreeBSD$");

/*============================ Namespace Control =============================*/
namespace DevCtl
{

//- FDReader Public Methods ---------------------------------------------------
FDReader::FDReader(int fd)
 : m_fd(fd)
{
}

ssize_t
FDReader::read(char* buf, size_t count)
{
	return (::read(m_fd, buf, count));
}

ssize_t
FDReader::in_avail() const
{
	int bytes;
	if (ioctl(m_fd, FIONREAD, &bytes)) {
		syslog(LOG_ERR, "ioctl FIONREAD: %s", strerror(errno));
		return (-1);
	}
	return (bytes);
}

//- IstreamReader Inline Public Methods ----------------------------------------
IstreamReader::IstreamReader(std::istream* stream)
 : m_stream(stream)
{
}

ssize_t
IstreamReader::read(char* buf, size_t count)
{
	m_stream->read(buf, count);
	if (m_stream->fail())
		return (-1);
	return (m_stream->gcount());
}

ssize_t
IstreamReader::in_avail() const
{
	return (m_stream->rdbuf()->in_avail());
}

} // namespace DevCtl
