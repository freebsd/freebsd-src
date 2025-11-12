/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Goran Mekić
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

#include <sys/event.h>

#include "oss.h"

int
main(int argc, char *argv[])
{
	struct config config = {
		.device = "/dev/dsp",
		.mode = O_RDWR,
		.format = AFMT_S32_NE,
		.sample_rate = 48000,
	};
	struct kevent event = {};
	int rc, bytes, kq;

	oss_init(&config);
	bytes = config.buffer_info.bytes;

	if ((kq = kqueue()) < 0)
		err(1, "Failed to allocate kqueue");
	EV_SET(&event, config.fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, 0);
	if (kevent(kq, &event, 1, NULL, 0, NULL) < 0)
		err(1, "Failed to register kevent");
	for (;;) {
		if (kevent(kq, NULL, 0, &event, 1, NULL) < 0) {
			warn("Event error");
			break;
		}
		if (event.flags & EV_ERROR) {
			warn("Event error: %s", strerror(event.data));
			break;
		}
		if ((rc = read(config.fd, config.buf, bytes)) < bytes) {
			warn("Requested %d bytes, but read %d!\n", bytes, rc);
			break;
		}
		if ((rc = write(config.fd, config.buf, bytes)) < bytes) {
			warn("Requested %d bytes, but wrote %d!\n", bytes, rc);
			break;
		}
	}
	EV_SET(&event, config.fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
	if (kevent(kq, &event, 1, NULL, 0, NULL) < 0)
		err(1, "Failed to unregister kevent");
	close(kq);

	free(config.buf);
	close(config.fd);

	return (0);
}
