/*-
 * Copyright (c) 2012-2013 The FreeBSD Foundation
 * Copyright (c) 2015 Mariusz Zaborski <oshogbo@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef	_LIBCASPER_H_
#define	_LIBCASPER_H_

#include <sys/types.h>

#ifndef	_NVLIST_T_DECLARED
#define	_NVLIST_T_DECLARED
struct nvlist;

typedef struct nvlist nvlist_t;
#endif

#ifndef	_CAP_CHANNEL_T_DECLARED
#define	_CAP_CHANNEL_T_DECLARED
struct cap_channel;

typedef struct cap_channel cap_channel_t;
#endif

/*
 * The functions opens unrestricted communication channel to Casper.
 */
cap_channel_t *cap_init(void);

/*
 * The functions to communicate with service.
 */
cap_channel_t	*cap_service_open(const cap_channel_t *chan, const char *name);
int		 cap_service_limit(const cap_channel_t *chan,
		    const char * const *names, size_t nnames);

/*
 * The function creates cap_channel_t based on the given socket.
 */
cap_channel_t *cap_wrap(int sock);

/*
 * The function returns communication socket and frees cap_channel_t.
 */
int	cap_unwrap(cap_channel_t *chan);

/*
 * The function clones the given capability.
 */
cap_channel_t *cap_clone(const cap_channel_t *chan);

/*
 * The function closes the given capability.
 */
void	cap_close(cap_channel_t *chan);

/*
 * The function returns socket descriptor associated with the given
 * cap_channel_t for use with select(2)/kqueue(2)/etc.
 */
int	cap_sock(const cap_channel_t *chan);

/*
 * The function limits the given capability.
 * It always destroys 'limits' on return.
 */
int	cap_limit_set(const cap_channel_t *chan, nvlist_t *limits);

/*
 * The function returns current limits of the given capability.
 */
int	cap_limit_get(const cap_channel_t *chan, nvlist_t **limitsp);

/*
 * Function sends nvlist over the given capability.
 */
int	cap_send_nvlist(const cap_channel_t *chan, const nvlist_t *nvl);
/*
 * Function receives nvlist over the given capability.
 */
nvlist_t *cap_recv_nvlist(const cap_channel_t *chan, int flags);
/*
 * Function sends the given nvlist, destroys it and receives new nvlist in
 * response over the given capability.
 */
nvlist_t *cap_xfer_nvlist(const cap_channel_t *chan, nvlist_t *nvl, int flags);

#endif	/* !_LIBCASPER_H_ */
