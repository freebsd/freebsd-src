/*-
 * Copyright (c) 2010 Marcel Moolenaar
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/iodev.h>

static int iodev_pio_read(struct iodev_pio_req *req);
static int iodev_pio_write(struct iodev_pio_req *req);

/* ARGSUSED */
int
ioopen(struct cdev *dev __unused, int flags __unused, int fmt __unused,
    struct thread *td)
{
	int error;

	error = priv_check(td, PRIV_IO);
	if (error == 0)
		error = securelevel_gt(td->td_ucred, 0);

	return (error);
}

/* ARGSUSED */
int
ioclose(struct cdev *dev __unused, int flags __unused, int fmt __unused,
    struct thread *td __unused)
{

	return (0);
}

/* ARGSUSED */
int
ioioctl(struct cdev *dev __unused, u_long cmd, caddr_t data,
    int fflag __unused, struct thread *td __unused)
{
	struct iodev_pio_req *pio_req;
	int error;

	error = ENOIOCTL;
	switch (cmd) {
	case IODEV_PIO:
		pio_req = (struct iodev_pio_req *)data;
		switch (pio_req->access) {
		case IODEV_PIO_READ:
			error = iodev_pio_read(pio_req);
			break;
		case IODEV_PIO_WRITE:
			error = iodev_pio_write(pio_req);
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	}

	return (error);
}

static int
iodev_pio_read(struct iodev_pio_req *req)
{

	switch (req->width) {
	case 1:
		req->val = bus_space_read_io_1(req->port);
		break;
	case 2:
		if (req->port & 1) {
			req->val = bus_space_read_io_1(req->port);
			req->val |= bus_space_read_io_1(req->port + 1) << 8;
		} else
			req->val = bus_space_read_io_2(req->port);
		break;
	case 4:
		if (req->port & 1) {
			req->val = bus_space_read_io_1(req->port);
			req->val |= bus_space_read_io_2(req->port + 1) << 8;
			req->val |= bus_space_read_io_1(req->port + 3) << 24;
		} else if (req->port & 2) {
			req->val = bus_space_read_io_2(req->port);
			req->val |= bus_space_read_io_2(req->port + 2) << 16;
		} else
			req->val = bus_space_read_io_4(req->port);
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
iodev_pio_write(struct iodev_pio_req *req)
{

	switch (req->width) {
	case 1:
		bus_space_write_io_1(req->port, req->val);
		break;
	case 2:
		if (req->port & 1) {
			bus_space_write_io_1(req->port, req->val);
			bus_space_write_io_1(req->port + 1, req->val >> 8);
		} else
			bus_space_write_io_2(req->port, req->val);
		break;
	case 4:
		if (req->port & 1) {
			bus_space_write_io_1(req->port, req->val);
			bus_space_write_io_2(req->port + 1, req->val >> 8);
			bus_space_write_io_1(req->port + 3, req->val >> 24);
		} else if (req->port & 2) {
			bus_space_write_io_2(req->port, req->val);
			bus_space_write_io_2(req->port + 2, req->val >> 16);
		} else
			bus_space_write_io_4(req->port, req->val);
		break;
	default:
		return (EINVAL);
	}

	return (0);
}
