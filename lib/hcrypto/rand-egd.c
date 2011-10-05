/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <sys/types.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>

#include <rand.h>
#include <randi.h>

#include <roken.h>

static const char *egd_path = "/var/run/egd-pool";

#define MAX_EGD_DATA 255

static int
connect_egd(const char *path)
{
    struct sockaddr_un addr;
    int fd;

    memset(&addr, 0, sizeof(addr));

    if (strlen(path) > sizeof(addr.sun_path))
	return -1;

    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, path, sizeof(addr.sun_path));

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
	return -1;

    rk_cloexec(fd);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
	close(fd);
	return -1;
    }

    return fd;
}

static int
get_entropy(int fd, void *data, size_t len)
{
    unsigned char msg[2];

    assert(len <= MAX_EGD_DATA);

    msg[0] = 0x02; /* read blocking data */
    msg[1] = len; /* wanted length */

    if (net_write(fd, msg, sizeof(msg)) != sizeof(msg))
	return 0;

    if (net_read(fd, data, len) != len)
	return 0;

    return 1;
}

static int
put_entropy(int fd, const void *data, size_t len)
{
    unsigned char msg[4];

    assert (len <= MAX_EGD_DATA);

    msg[0] = 0x03; /* write data */
    msg[1] = 0; /* dummy */
    msg[2] = 0; /* entropy */
    msg[3] = len; /* length */

    if (net_write(fd, msg, sizeof(msg)) != sizeof(msg))
	return 0;
    if (net_write(fd, data, len) != len)
	return 0;

    return 1;
}

/*
 *
 */

static void
egd_seed(const void *indata, int size)
{
    size_t len;
    int fd, ret = 1;

    fd = connect_egd(egd_path);
    if (fd < 0)
	return;

    while(size) {
	len = size;
	if (len > MAX_EGD_DATA)
	    len = MAX_EGD_DATA;
	ret = put_entropy(fd, indata, len);
	if (ret != 1)
	    break;
	indata = ((unsigned char *)indata) + len;
	size -= len;
    }
    close(fd);
}

static int
get_bytes(const char *path, unsigned char *outdata, int size)
{
    size_t len;
    int fd, ret = 1;

    if (path == NULL)
	path = egd_path;

    fd = connect_egd(path);
    if (fd < 0)
	return 0;

    while(size) {
	len = size;
	if (len > MAX_EGD_DATA)
	    len = MAX_EGD_DATA;
	ret = get_entropy(fd, outdata, len);
	if (ret != 1)
	    break;
	outdata += len;
	size -= len;
    }
    close(fd);

    return ret;
}

static int
egd_bytes(unsigned char *outdata, int size)
{
    return get_bytes(NULL, outdata, size);
}

static void
egd_cleanup(void)
{
}

static void
egd_add(const void *indata, int size, double entropi)
{
    egd_seed(indata, size);
}

static int
egd_pseudorand(unsigned char *outdata, int size)
{
    return get_bytes(NULL, outdata, size);
}

static int
egd_status(void)
{
    int fd;
    fd = connect_egd(egd_path);
    if (fd < 0)
	return 0;
    close(fd);
    return 1;
}

const RAND_METHOD hc_rand_egd_method = {
    egd_seed,
    egd_bytes,
    egd_cleanup,
    egd_add,
    egd_pseudorand,
    egd_status
};

const RAND_METHOD *
RAND_egd_method(void)
{
    return &hc_rand_egd_method;
}


int
RAND_egd(const char *filename)
{
    return RAND_egd_bytes(filename, 128);
}

int
RAND_egd_bytes(const char *filename, int size)
{
    void *data;
    int ret;

    if (size <= 0)
	return 0;

    data = malloc(size);
    if (data == NULL)
	return 0;

    ret = get_bytes(filename, data, size);
    if (ret != 1) {
	free(data);
	return ret;
    }

    RAND_seed(data, size);

    memset(data, 0, size);
    free(data);

    return 1;
}
