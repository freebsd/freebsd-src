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

#include <sys/types.h>
#include <sys/socket.h>

#include <libcasper.h>
#include <string.h>

#include <casper/cap_net.h>

#include "syslogd_cap.h"

/* This is where libcasper receives commands via nvlist. */
static int
casper_command(const char *cmd, const nvlist_t *limits __unused,
    nvlist_t *nvlin, nvlist_t *nvlout)
{
	int error = EINVAL;

	if (strcmp(cmd, "p_open") == 0)
		error = casper_p_open(nvlin, nvlout);
	else if (strcmp(cmd, "readconfigfile") == 0)
		error = casper_readconfigfile(nvlin, nvlout);
	else if (strcmp(cmd, "ttymsg") == 0)
		error = casper_ttymsg(nvlin, nvlout);
	else if (strcmp(cmd, "wallmsg") == 0)
		error = casper_wallmsg(nvlin);

	return (error);
}

CREATE_SERVICE("syslogd.casper", NULL, casper_command, CASPER_SERVICE_STDIO);
