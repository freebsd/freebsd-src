/*
 * Copyright (c) 1999
 *      Mark Murray.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MARK MURRAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL MARK MURRAY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$FreeBSD$";
#endif /* LIBC_SCCS and not lint */

#include <string.h>
#include "crypt.h"

static const struct {
	const char *const name;
	char *(*const func)(const char *, const char *);
	const char *const magic;
} crypt_types[] = {
	{
		"des",
		crypt_des,
		NULL
	},
	{
		"md5",
		crypt_md5,
		"$1$"
	},
	{
		NULL,
		NULL
	}
};

static int crypt_type = 0;

const char *
crypt_get_format(void) {

	return (crypt_types[crypt_type].name);
}

int
crypt_set_format(char *type) {
	int i;

	for (i = 0; i < sizeof(crypt_types) / sizeof(crypt_types[0]) - 1; i++) {
		if (strcmp(type, crypt_types[i].name) == 0) {
			crypt_type = i;
			return (1);
		}
	}
	return (0);
}

char *
crypt(char *passwd, char *salt)
{
	int i;

	for (i = 0; i < sizeof(crypt_types) / sizeof(crypt_types[0]) - 1; i++) {
		if (crypt_types[i].magic != NULL && strncmp(salt,
		    crypt_types[i].magic, strlen(crypt_types[i].magic)) == 0)
			return (crypt_types[i].func(passwd, salt));
	}
	return (crypt_types[crypt_type].func(passwd, salt));
}
