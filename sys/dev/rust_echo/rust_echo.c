/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 FusionBSD
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/uio.h>

#define	RUST_ECHO_IO_CHUNK	128

/*
 * Narrow C/Rust boundary: C owns all kernel ABI objects, locking, and uiomove;
 * Rust owns only the fixed-size echo buffer.
 */
extern void	rust_echo_clear(void);
extern size_t	rust_echo_read(uint8_t *, size_t);
extern size_t	rust_echo_write(const uint8_t *, size_t);

static d_open_t		rust_echo_open;
static d_close_t	rust_echo_close;
static d_read_t		rust_echo_dev_read;
static d_write_t	rust_echo_dev_write;

static struct cdev	*rust_echo_dev;
static struct mtx	rust_echo_mtx;

static struct cdevsw rust_echo_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	rust_echo_open,
	.d_close =	rust_echo_close,
	.d_read =	rust_echo_dev_read,
	.d_write =	rust_echo_dev_write,
	.d_name =	"rust_echo",
};

static int
rust_echo_open(struct cdev *dev __unused, int flags __unused,
    int devtype __unused, struct thread *td __unused)
{
	return (0);
}

static int
rust_echo_close(struct cdev *dev __unused, int flags __unused,
    int devtype __unused, struct thread *td __unused)
{
	return (0);
}

static int
rust_echo_dev_write(struct cdev *dev __unused, struct uio *uio,
    int ioflag __unused)
{
	uint8_t buf[RUST_ECHO_IO_CHUNK];
	size_t done, len;
	int error;

	mtx_lock(&rust_echo_mtx);
	rust_echo_clear();
	mtx_unlock(&rust_echo_mtx);

	while (uio->uio_resid > 0) {
		len = MIN(uio->uio_resid, sizeof(buf));
		error = uiomove(buf, len, uio);
		if (error != 0)
			return (error);

		mtx_lock(&rust_echo_mtx);
		done = rust_echo_write(buf, len);
		mtx_unlock(&rust_echo_mtx);
		if (done == 0)
			break;
	}

	return (0);
}

static int
rust_echo_dev_read(struct cdev *dev __unused, struct uio *uio,
    int ioflag __unused)
{
	uint8_t buf[RUST_ECHO_IO_CHUNK];
	size_t len;
	int error;

	while (uio->uio_resid > 0) {
		mtx_lock(&rust_echo_mtx);
		len = rust_echo_read(buf, MIN(uio->uio_resid, sizeof(buf)));
		mtx_unlock(&rust_echo_mtx);
		if (len == 0)
			break;

		error = uiomove(buf, len, uio);
		if (error != 0)
			return (error);
	}

	return (0);
}

static int
rust_echo_modevent(module_t mod __unused, int type, void *data __unused)
{
	switch (type) {
	case MOD_LOAD:
		mtx_init(&rust_echo_mtx, "rust_echo", NULL, MTX_DEF);
		rust_echo_clear();
		rust_echo_dev = make_dev(&rust_echo_cdevsw, 0, UID_ROOT,
		    GID_WHEEL, 0600, "rust_echo");
		return (0);
	case MOD_UNLOAD:
		destroy_dev(rust_echo_dev);
		rust_echo_dev = NULL;
		mtx_destroy(&rust_echo_mtx);
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t rust_echo_mod = {
	"rust_echo",
	rust_echo_modevent,
	NULL,
};

DECLARE_MODULE(rust_echo, rust_echo_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(rust_echo, 1);
