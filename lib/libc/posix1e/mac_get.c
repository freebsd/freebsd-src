/*
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/mac.h>

#include <errno.h>
#include <stdlib.h>

mac_t
mac_get_file(const char *path_p)
{
	struct mac *label;
	int error;

	label = (mac_t) malloc(sizeof(*label));
	if (label == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	error = __mac_get_file(path_p, label);
	if (error) {
		mac_free(label);
		return (NULL);
	}

	return (label);
}

mac_t
mac_get_fd(int fd)
{
	struct mac *label;
	int error;

	label = (mac_t) malloc(sizeof(*label));
	if (label == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	error = __mac_get_fd(fd, label);
	if (error) {
		mac_free(label);
		return (NULL);
	}

	return (label);
}

mac_t
mac_get_proc()
{
	struct mac *label;
	int error;

	label = (mac_t) malloc(sizeof(*label));
	if (label == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	error = __mac_get_proc(label);
	if (error) {
		mac_free(label);
		return (NULL);
	}

	return (label);
}
