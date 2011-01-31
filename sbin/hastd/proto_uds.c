/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
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

/* UDS - UNIX Domain Socket */

#include <sys/un.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hast.h"
#include "pjdlog.h"
#include "proto_impl.h"

#define	UDS_CTX_MAGIC	0xd541c
struct uds_ctx {
	int			uc_magic;
	struct sockaddr_un	uc_sun;
	int			uc_fd;
	int			uc_side;
#define	UDS_SIDE_CLIENT		0
#define	UDS_SIDE_SERVER_LISTEN	1
#define	UDS_SIDE_SERVER_WORK	2
};

static void uds_close(void *ctx);

static int
uds_addr(const char *addr, struct sockaddr_un *sunp)
{

	if (addr == NULL)
		return (-1);

	if (strncasecmp(addr, "uds://", 6) == 0)
		addr += 6;
	else if (strncasecmp(addr, "unix://", 7) == 0)
		addr += 7;
	else if (addr[0] == '/' &&	/* If it starts from /... */
	    strstr(addr, "://") == NULL)/* ...and there is no prefix... */
		;			/* ...we assume its us. */
	else
		return (-1);

	sunp->sun_family = AF_UNIX;
	if (strlcpy(sunp->sun_path, addr, sizeof(sunp->sun_path)) >=
	    sizeof(sunp->sun_path)) {
		return (ENAMETOOLONG);
	}
	sunp->sun_len = SUN_LEN(sunp);

	return (0);
}

static int
uds_common_setup(const char *addr, void **ctxp, int side)
{
	struct uds_ctx *uctx;
	int ret;

	uctx = malloc(sizeof(*uctx));
	if (uctx == NULL)
		return (errno);

	/* Parse given address. */
	if ((ret = uds_addr(addr, &uctx->uc_sun)) != 0) {
		free(uctx);
		return (ret);
	}

	uctx->uc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (uctx->uc_fd == -1) {
		ret = errno;
		free(uctx);
		return (ret);
	}

	uctx->uc_side = side;
	uctx->uc_magic = UDS_CTX_MAGIC;
	*ctxp = uctx;

	return (0);
}

static int
uds_client(const char *addr, void **ctxp)
{

	return (uds_common_setup(addr, ctxp, UDS_SIDE_CLIENT));
}

static int
uds_connect(void *ctx)
{
	struct uds_ctx *uctx = ctx;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);
	PJDLOG_ASSERT(uctx->uc_side == UDS_SIDE_CLIENT);
	PJDLOG_ASSERT(uctx->uc_fd >= 0);

	if (connect(uctx->uc_fd, (struct sockaddr *)&uctx->uc_sun,
	    sizeof(uctx->uc_sun)) < 0) {
		return (errno);
	}

	return (0);
}

static int
uds_server(const char *addr, void **ctxp)
{
	struct uds_ctx *uctx;
	int ret;

	ret = uds_common_setup(addr, ctxp, UDS_SIDE_SERVER_LISTEN);
	if (ret != 0)
		return (ret);

	uctx = *ctxp;

	unlink(uctx->uc_sun.sun_path);
	if (bind(uctx->uc_fd, (struct sockaddr *)&uctx->uc_sun,
	    sizeof(uctx->uc_sun)) < 0) {
		ret = errno;
		uds_close(uctx);
		return (ret);
	}
	if (listen(uctx->uc_fd, 8) < 0) {
		ret = errno;
		uds_close(uctx);
		return (ret);
	}

	return (0);
}

static int
uds_accept(void *ctx, void **newctxp)
{
	struct uds_ctx *uctx = ctx;
	struct uds_ctx *newuctx;
	socklen_t fromlen;
	int ret;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);
	PJDLOG_ASSERT(uctx->uc_side == UDS_SIDE_SERVER_LISTEN);
	PJDLOG_ASSERT(uctx->uc_fd >= 0);

	newuctx = malloc(sizeof(*newuctx));
	if (newuctx == NULL)
		return (errno);

	fromlen = sizeof(uctx->uc_sun);
	newuctx->uc_fd = accept(uctx->uc_fd, (struct sockaddr *)&uctx->uc_sun,
	    &fromlen);
	if (newuctx->uc_fd < 0) {
		ret = errno;
		free(newuctx);
		return (ret);
	}

	newuctx->uc_side = UDS_SIDE_SERVER_WORK;
	newuctx->uc_magic = UDS_CTX_MAGIC;
	*newctxp = newuctx;

	return (0);
}

static int
uds_send(void *ctx, const unsigned char *data, size_t size)
{
	struct uds_ctx *uctx = ctx;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);
	PJDLOG_ASSERT(uctx->uc_fd >= 0);

	return (proto_common_send(uctx->uc_fd, data, size));
}

static int
uds_recv(void *ctx, unsigned char *data, size_t size)
{
	struct uds_ctx *uctx = ctx;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);
	PJDLOG_ASSERT(uctx->uc_fd >= 0);

	return (proto_common_recv(uctx->uc_fd, data, size));
}

static int
uds_descriptor_send(void *ctx, int fd)
{
	struct uds_ctx *uctx = ctx;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);
	PJDLOG_ASSERT(uctx->uc_fd >= 0);
	PJDLOG_ASSERT(fd >= 0);

	return (proto_common_descriptor_send(uctx->uc_fd, fd));
}

static int
uds_descriptor_recv(void *ctx, int *fdp)
{
	struct uds_ctx *uctx = ctx;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);
	PJDLOG_ASSERT(uctx->uc_fd >= 0);
	PJDLOG_ASSERT(fdp != NULL);

	return (proto_common_descriptor_recv(uctx->uc_fd, fdp));
}

static int
uds_descriptor(const void *ctx)
{
	const struct uds_ctx *uctx = ctx;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);

	return (uctx->uc_fd);
}

static void
uds_local_address(const void *ctx, char *addr, size_t size)
{
	const struct uds_ctx *uctx = ctx;
	struct sockaddr_un sun;
	socklen_t sunlen;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);
	PJDLOG_ASSERT(addr != NULL);

	sunlen = sizeof(sun);
	if (getsockname(uctx->uc_fd, (struct sockaddr *)&sun, &sunlen) < 0) {
		PJDLOG_VERIFY(strlcpy(addr, "N/A", size) < size);
		return;
	}
	PJDLOG_ASSERT(sun.sun_family == AF_UNIX);
	if (sun.sun_path[0] == '\0') {
		PJDLOG_VERIFY(strlcpy(addr, "N/A", size) < size);
		return;
	}
	PJDLOG_VERIFY(snprintf(addr, size, "uds://%s", sun.sun_path) < (ssize_t)size);
}

static void
uds_remote_address(const void *ctx, char *addr, size_t size)
{
	const struct uds_ctx *uctx = ctx;
	struct sockaddr_un sun;
	socklen_t sunlen;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);
	PJDLOG_ASSERT(addr != NULL);

	sunlen = sizeof(sun);
	if (getpeername(uctx->uc_fd, (struct sockaddr *)&sun, &sunlen) < 0) {
		PJDLOG_VERIFY(strlcpy(addr, "N/A", size) < size);
		return;
	}
	PJDLOG_ASSERT(sun.sun_family == AF_UNIX);
	if (sun.sun_path[0] == '\0') {
		PJDLOG_VERIFY(strlcpy(addr, "N/A", size) < size);
		return;
	}
	snprintf(addr, size, "uds://%s", sun.sun_path);
}

static void
uds_close(void *ctx)
{
	struct uds_ctx *uctx = ctx;

	PJDLOG_ASSERT(uctx != NULL);
	PJDLOG_ASSERT(uctx->uc_magic == UDS_CTX_MAGIC);

	if (uctx->uc_fd >= 0)
		close(uctx->uc_fd);
	unlink(uctx->uc_sun.sun_path);
	uctx->uc_magic = 0;
	free(uctx);
}

static struct hast_proto uds_proto = {
	.hp_name = "uds",
	.hp_client = uds_client,
	.hp_connect = uds_connect,
	.hp_server = uds_server,
	.hp_accept = uds_accept,
	.hp_send = uds_send,
	.hp_recv = uds_recv,
	.hp_descriptor_send = uds_descriptor_send,
	.hp_descriptor_recv = uds_descriptor_recv,
	.hp_descriptor = uds_descriptor,
	.hp_local_address = uds_local_address,
	.hp_remote_address = uds_remote_address,
	.hp_close = uds_close
};

static __constructor void
uds_ctor(void)
{

	proto_register(&uds_proto, false);
}
