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
test_str( const char *str, const struct eui64 *eui)
{
	struct eui64	e;
	char		buf[EUI64_SIZ];

	if (eui64_aton(str, &e) != 0 &&
	    memcmp(&e, &eui, sizeof(struct eui64)) != 0) {
		printf("FAIL: eui64_aton(%s)\n", str);
		eui64_ntoa(&e, buf, sizeof(buf));
		printf("got: %s\n", buf);
		return (0);
	} else {
		printf("PASS: eui64_aton(%s)\n", str);
		return (1);
	}

}

int
main(int argc, char **argv)
{

	test_str(test_eui64_id_ascii, &test_eui64_id);
	test_str(test_eui64_id_colon_ascii, &test_eui64_id);
	test_str(test_eui64_mac_ascii, &test_eui64_eui48);
	test_str(test_eui64_mac_colon_ascii, &test_eui64_eui48);
	test_str(test_eui64_hex_ascii, &test_eui64_id);

	return (0);
}
