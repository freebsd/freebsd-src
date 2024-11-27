/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Jake Freeland <jfree@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 */

#ifndef _SYSLOGD_CAP_H_
#define _SYSLOGD_CAP_H_

#include <sys/nv.h>

#include <libcasper.h>

#include <casper/cap_net.h>

#ifdef WITH_CASPER

#include <sys/capsicum.h>
#include <sys/dnv.h>

#include <capsicum_helpers.h>
#include <libcasper_service.h>

#include <casper/cap_net.h>

#include "syslogd.h"

/*
 * Information used to verify filed integrity when executing outside of the
 * security sandbox.
 */
struct cap_filed {
	size_t idx;
	char pipe_cmd[MAXPATHLEN];
	SLIST_ENTRY(cap_filed) next;
};
extern SLIST_HEAD(cfiled_list, cap_filed) cfiled_head;

int cap_p_open(cap_channel_t *, size_t, const char *, int *);
nvlist_t *cap_readconfigfile(cap_channel_t *, const char *);
const char *cap_ttymsg(cap_channel_t *, struct iovec *, int, const char *, int);
void cap_wallmsg(cap_channel_t *, const struct filed *, struct iovec *,
    const int);

int casper_p_open(nvlist_t *, nvlist_t *);
int casper_readconfigfile(nvlist_t *, nvlist_t *);
int casper_ttymsg(nvlist_t *, nvlist_t *);
int casper_wallmsg(nvlist_t *);

nvlist_t *filed_to_nvlist(const struct filed *);
nvlist_t *prop_filter_to_nvlist(const struct prop_filter *pfilter);

struct filed *nvlist_to_filed(const nvlist_t *);
struct prop_filter *nvlist_to_prop_filter(const nvlist_t *nvl_prop_filter);

#else /* !WITH_CASPER */

#define	cap_p_open(chan, f_idx, prog, rpd) \
	p_open(prog, rpd)
#define	cap_readconfigfile(chan, cf) \
	readconfigfile(cf)
#define	cap_ttymsg(chan, iov, iovcnt, line, tmout) \
	ttymsg(iov, iovcnt, line, tmout)
#define	cap_wallmsg(chan, f, iov, iovcnt) \
	wallmsg(f, iov, iovcnt)

#endif /* WITH_CASPER */

#endif /* !_SYSLOGD_CAP_H_ */
