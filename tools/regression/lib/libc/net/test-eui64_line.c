/*
 * Copyright 2004 The Aerospace Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions, and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions, and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  The name of The Aerospace Corporation may not be used to endorse or
 *     promote products derived from this software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AEROSPACE CORPORATION "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AEROSPACE CORPORATION BE LIABLE
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
#include <sys/eui64.h>
#include <stdio.h>
#include <string.h>

#include "test-eui64.h"

static int
test_line(const char *line, const struct eui64 *eui, const char *host)
{
	struct eui64	e;
	char		buf[256];

	if (eui64_line(line, &e, buf, sizeof(buf)) != 0 ||
	    memcmp(&e, eui, sizeof(struct eui64)) != 0 ||
	    strcmp(buf, host) != 0) {
		printf("FAIL: eui64_line(\"%s\")\n", line);
		printf("host = %s\n", buf);
		eui64_ntoa(&e, buf, sizeof(buf));
		printf("e = %s\n", buf);
		return (0);
	} else {
		printf("PASS: eui64_line(\"%s\")\n", line);
		return (1);
	}
}

int
main(int argc, char **argv)
{

	test_line(test_eui64_line_id, &test_eui64_id,
	    test_eui64_id_host);
	test_line(test_eui64_line_id_colon, &test_eui64_id,
	    test_eui64_id_host);
	test_line(test_eui64_line_eui48, &test_eui64_eui48,
	    test_eui64_eui48_host);
	test_line(test_eui64_line_mac48, &test_eui64_mac48,
	    test_eui64_mac48_host);
	test_line(test_eui64_line_eui48_6byte, &test_eui64_eui48,
	    test_eui64_eui48_host);
	test_line(test_eui64_line_eui48_6byte_c, &test_eui64_eui48,
	    test_eui64_eui48_host);

	return (0);
}
