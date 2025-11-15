/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 ConnectWise
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <stdlib.h>
#include <libxo/xo.h>

#include "sockstat.h"

int	*ports;

int
parse_ports(const char *portspec)
{
	const char *p;

	if (ports == NULL)
		if ((ports = calloc(65536 / INT_BIT, sizeof(int))) == NULL)
			xo_err(1, "calloc()");
	p = portspec;
	while (*p != '\0') {
		long port, end;
		char *endptr = NULL;

		errno = 0;
		port = strtol(p, &endptr, 10);
		if (errno)
			return (errno);
		if (port < 0 || port > 65535)
			return (ERANGE);
		SET_PORT(port);
		switch (*endptr) {
		case '-':
			p = endptr + 1;
			end = strtol(p, &endptr, 10);
			break;
		case ',':
			p = endptr + 1;
			continue;
		default:
			p = endptr;
			continue;
		}
		if (errno)
			return (errno);
		if (end < port || end > 65535)
			return (ERANGE);
		while (port++ < end)
			SET_PORT(port);
	}
	return (0);
}
