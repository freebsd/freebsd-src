/*-
 * Copyright (c) 2017-2018, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <libsecureboot.h>

unsigned char *
read_fd(int fd, size_t len)
{
	int m, n, x;
	unsigned char *buf;

	buf = malloc(len + 1);
	if (buf != NULL) {
		for (x = 0, m = len; m > 0; ) {
			n = read(fd, &buf[x], m);
			if (n < 0)
				break;
			if (n > 0) {
				m -= n;
				x += n;
			}
		}
		if (m == 0) {
			buf[len] = '\0';
			return (buf);
		}
		free(buf);
	}
	return (NULL);
}

unsigned char *
read_file(const char *path, size_t *len)
{
	struct stat st;
	unsigned char *ucp;
	int fd;

    	if (len)
		*len = 0;
	if ((fd = open(path, O_RDONLY)) < 0)
		return (NULL);
	fstat(fd, &st);
	ucp = read_fd(fd, st.st_size);
	close(fd);
	if (ucp != NULL) {
		if (len != NULL)
			*len = st.st_size;
	}
#ifdef _STANDALONE
	else
		printf("%s: out of memory! %lu\n", __func__,
		    (unsigned long)len);
#endif

	return (ucp);
}

