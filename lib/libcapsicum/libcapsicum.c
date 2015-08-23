/*-
 * Copyright (c) 2012-2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/nv.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libcapsicum.h"
#include "libcapsicum_impl.h"

/*
 * Structure describing communication channel between two separated processes.
 */
#define	CAP_CHANNEL_MAGIC	0xcac8a31
struct cap_channel {
	/*
	 * Magic value helps to ensure that a pointer to the right structure is
	 * passed to our functions.
	 */
	int	cch_magic;
	/* Socket descriptor for IPC. */
	int	cch_sock;
};

bool
fd_is_valid(int fd)
{

	return (fcntl(fd, F_GETFL) != -1 || errno != EBADF);
}

cap_channel_t *
cap_init(void)
{
	cap_channel_t *chan;
	struct sockaddr_un sun;
	int serrno, sock;

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, CASPER_SOCKPATH, sizeof(sun.sun_path));
	sun.sun_len = SUN_LEN(&sun);

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1)
		return (NULL);
	if (connect(sock, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
		serrno = errno;
		close(sock);
		errno = serrno;
		return (NULL);
	}
	chan = cap_wrap(sock);
	if (chan == NULL) {
		serrno = errno;
		close(sock);
		errno = serrno;
		return (NULL);
	}
	return (chan);
}

cap_channel_t *
cap_wrap(int sock)
{
	cap_channel_t *chan;

	if (!fd_is_valid(sock))
		return (NULL);

	chan = malloc(sizeof(*chan));
	if (chan != NULL) {
		chan->cch_sock = sock;
		chan->cch_magic = CAP_CHANNEL_MAGIC;
	}

	return (chan);
}

int
cap_unwrap(cap_channel_t *chan)
{
	int sock;

	assert(chan != NULL);
	assert(chan->cch_magic == CAP_CHANNEL_MAGIC);

	sock = chan->cch_sock;
	chan->cch_magic = 0;
	free(chan);

	return (sock);
}

cap_channel_t *
cap_clone(const cap_channel_t *chan)
{
	cap_channel_t *newchan;
	nvlist_t *nvl;
	int newsock;

	assert(chan != NULL);
	assert(chan->cch_magic == CAP_CHANNEL_MAGIC);

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "clone");
	nvl = cap_xfer_nvlist(chan, nvl, 0);
	if (nvl == NULL)
		return (NULL);
	if (nvlist_get_number(nvl, "error") != 0) {
		errno = (int)nvlist_get_number(nvl, "error");
		nvlist_destroy(nvl);
		return (NULL);
	}
	newsock = nvlist_take_descriptor(nvl, "sock");
	nvlist_destroy(nvl);
	newchan = cap_wrap(newsock);
	if (newchan == NULL) {
		int serrno;

		serrno = errno;
		close(newsock);
		errno = serrno;
	}

	return (newchan);
}

void
cap_close(cap_channel_t *chan)
{

	assert(chan != NULL);
	assert(chan->cch_magic == CAP_CHANNEL_MAGIC);

	chan->cch_magic = 0;
	close(chan->cch_sock);
	free(chan);
}

int
cap_sock(const cap_channel_t *chan)
{

	assert(chan != NULL);
	assert(chan->cch_magic == CAP_CHANNEL_MAGIC);

	return (chan->cch_sock);
}

int
cap_limit_set(const cap_channel_t *chan, nvlist_t *limits)
{
	nvlist_t *nvlmsg;
	int error;

	nvlmsg = nvlist_create(0);
	nvlist_add_string(nvlmsg, "cmd", "limit_set");
	nvlist_add_nvlist(nvlmsg, "limits", limits);
	nvlmsg = cap_xfer_nvlist(chan, nvlmsg, 0);
	if (nvlmsg == NULL) {
		nvlist_destroy(limits);
		return (-1);
	}
	error = (int)nvlist_get_number(nvlmsg, "error");
	nvlist_destroy(nvlmsg);
	nvlist_destroy(limits);
	if (error != 0) {
		errno = error;
		return (-1);
	}
	return (0);
}

int
cap_limit_get(const cap_channel_t *chan, nvlist_t **limitsp)
{
	nvlist_t *nvlmsg;
	int error;

	nvlmsg = nvlist_create(0);
	nvlist_add_string(nvlmsg, "cmd", "limit_get");
	nvlmsg = cap_xfer_nvlist(chan, nvlmsg, 0);
	if (nvlmsg == NULL)
		return (-1);
	error = (int)nvlist_get_number(nvlmsg, "error");
	if (error != 0) {
		nvlist_destroy(nvlmsg);
		errno = error;
		return (-1);
	}
	if (nvlist_exists_null(nvlmsg, "limits"))
		*limitsp = NULL;
	else
		*limitsp = nvlist_take_nvlist(nvlmsg, "limits");
	nvlist_destroy(nvlmsg);
	return (0);
}

int
cap_send_nvlist(const cap_channel_t *chan, const nvlist_t *nvl)
{

	assert(chan != NULL);
	assert(chan->cch_magic == CAP_CHANNEL_MAGIC);

	return (nvlist_send(chan->cch_sock, nvl));
}

nvlist_t *
cap_recv_nvlist(const cap_channel_t *chan, int flags)
{

	assert(chan != NULL);
	assert(chan->cch_magic == CAP_CHANNEL_MAGIC);

	return (nvlist_recv(chan->cch_sock, flags));
}

nvlist_t *
cap_xfer_nvlist(const cap_channel_t *chan, nvlist_t *nvl, int flags)
{

	assert(chan != NULL);
	assert(chan->cch_magic == CAP_CHANNEL_MAGIC);

	return (nvlist_xfer(chan->cch_sock, nvl, flags));
}
