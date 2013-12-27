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
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/efi.h>
#include <machine/iodev.h>

static int iodev_efivar_getvar(struct iodev_efivar_req *req);
static int iodev_efivar_nextname(struct iodev_efivar_req *req);
static int iodev_efivar_setvar(struct iodev_efivar_req *req);

/* ARGSUSED */
int
iodev_open(struct thread *td __unused)
{

	return (0);
}

/* ARGSUSED */
int
iodev_close(struct thread *td __unused)
{

	return (0);
}

int
iodev_ioctl(u_long cmd, caddr_t data)
{
	struct iodev_efivar_req *efivar_req;
	int error;

	switch (cmd) {
	case IODEV_EFIVAR:
		efivar_req = (struct iodev_efivar_req *)data;
		efivar_req->result = 0;		/* So it's well-defined */
		switch (efivar_req->access) {
		case IODEV_EFIVAR_GETVAR:
			error = iodev_efivar_getvar(efivar_req);
			break;
		case IODEV_EFIVAR_NEXTNAME:
			error = iodev_efivar_nextname(efivar_req);
			break;
		case IODEV_EFIVAR_SETVAR:
			error = iodev_efivar_setvar(efivar_req);
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	default:
		error = ENOIOCTL;
	}

	return (error);
}

static int
iodev_efivar_getvar(struct iodev_efivar_req *req)
{
	void *data;
	efi_char *name;
	int error;

	if ((req->namesize & 1) != 0 || req->namesize < 4)
		return (EINVAL);
	if (req->datasize == 0)
		return (EINVAL);

	/*
	 * Pre-zero the allocated memory and don't copy the last 2 bytes
	 * of the name. That should be the closing nul character (ucs-2)
	 * and if not, then we ensured a nul-terminating string. This is
	 * to protect the firmware and thus ourselves.
	 */
	name = malloc(req->namesize, M_TEMP, M_WAITOK | M_ZERO);
	error = copyin(req->name, name, req->namesize - 2);
	if (error) {
		free(name, M_TEMP);
		return (error);
	}

	data = malloc(req->datasize, M_TEMP, M_WAITOK);
	error = efi_var_get(name, &req->vendor, &req->attrib, &req->datasize,
	    data);
	if (error == EOVERFLOW || error == ENOENT) {
		req->result = error;
		error = 0;
	}
	if (!error && !req->result)
		error = copyout(data, req->data, req->datasize);

	free(data, M_TEMP);
	free(name, M_TEMP);
	return (error);
}

static int 
iodev_efivar_nextname(struct iodev_efivar_req *req) 
{
	efi_char *name;
	int error;

	/* Enforce a reasonable minimum size of the name buffer. */
	if (req->namesize < 4)
		return (EINVAL);

	name = malloc(req->namesize, M_TEMP, M_WAITOK);
	error = copyin(req->name, name, req->namesize);
	if (error) {
		free(name, M_TEMP);
		return (error);
	}

	error = efi_var_nextname(&req->namesize, name, &req->vendor);
	if (error == EOVERFLOW || error == ENOENT) {
		req->result = error;
		error = 0;
	}
	if (!error && !req->result)
		error = copyout(name, req->name, req->namesize);

	free(name, M_TEMP);
	return (error);
}

static int 
iodev_efivar_setvar(struct iodev_efivar_req *req) 
{
	void *data;
	efi_char *name;
	int error;

	if ((req->namesize & 1) != 0 || req->namesize < 4)
		return (EINVAL);

	/*
	 * Pre-zero the allocated memory and don't copy the last 2 bytes
	 * of the name. That should be the closing nul character (ucs-2)
	 * and if not, then we ensured a nul-terminating string. This is
	 * to protect the firmware and thus ourselves.
	 */
	name = malloc(req->namesize, M_TEMP, M_WAITOK | M_ZERO);
	error = copyin(req->name, name, req->namesize - 2);
	if (error) {
		free(name, M_TEMP);
		return (error);
	}

	if (req->datasize) {
		data = malloc(req->datasize, M_TEMP, M_WAITOK);
		error = copyin(req->data, data, req->datasize);
		if (error) {
			free(data, M_TEMP);
			free(name, M_TEMP);
			return (error);
		}
	} else
		data = NULL;

	error = efi_var_set(name, &req->vendor, req->attrib, req->datasize,
	    data);
	if (error == EAGAIN || error == ENOENT) {
		req->result = error;
		error = 0;
	}

	free(data, M_TEMP);
	free(name, M_TEMP);
	return (error);
}
