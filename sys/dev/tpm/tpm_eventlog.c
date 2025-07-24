/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Netflix, Inc.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/uio.h>

#include <dev/tpm/tpm_eventlog.h>

static MALLOC_DEFINE(M_TPM_EVENTLOG, "tpmeventlog", "TPM firmware event log");

struct tpm_eventlog {
	struct cdev	*cdev;
	size_t		length;
	uint8_t		data[];
};

static d_read_t tpm_eventlog_read;

static struct cdevsw tpm_eventlog_cdevsw = {
	.d_version = D_VERSION,
	.d_read = tpm_eventlog_read,
	.d_name = "tpm_eventlog",
};

static int
tpm_eventlog_read(struct cdev *dev, struct uio *uio, int flags)
{
	struct tpm_eventlog *log = dev->si_drv1;

	/* uiomove maintains the offset, including after a partial fault. */
	return (uiomove_frombuf(log->data, log->length, uio));
}

struct tpm_eventlog *
tpm_eventlog_create(device_t dev)
{
	struct tpm_eventlog_header hdr;
	struct tpm_eventlog *log;
	struct make_dev_args args;
	const uint8_t *data;
	uint32_t size;
	int error;

	data = preload_search_info(preload_kmdp,
	    MODINFO_METADATA | MODINFOMD_TPM_EVENTLOG);
	if (data == NULL)
		return (NULL);
	/* preload_search_info guarantees a native uint32_t size before data. */
	memcpy(&size, data - sizeof(size), sizeof(size));
	if (size < sizeof(hdr))
		goto invalid;
	memcpy(&hdr, data, sizeof(hdr));
	data += sizeof(hdr);
	if (hdr.version != TPM_EVENTLOG_VERSION ||
	    (hdr.flags & ~TPM_EVENTLOG_TRUNCATED) != 0 ||
	    hdr.length != size - sizeof(hdr) ||
	    !tpm_eventlog_valid(data, hdr.length, hdr.format))
		goto invalid;
	log = malloc(sizeof(*log) + hdr.length, M_TPM_EVENTLOG, M_WAITOK);
	log->length = hdr.length;
	memcpy(log->data, data, hdr.length);
	make_dev_args_init(&args);
	args.mda_devsw = &tpm_eventlog_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_WHEEL;
	args.mda_mode = 0400;
	args.mda_si_drv1 = log;
	error = make_dev_s(&args, &log->cdev, "tpm%d_eventlog",
	    device_get_unit(dev));
	if (error != 0) {
		device_printf(dev, "cannot create event log device: %d\n",
		    error);
		free(log, M_TPM_EVENTLOG);
		return (NULL);
	}
	if ((hdr.flags & TPM_EVENTLOG_TRUNCATED) != 0)
		device_printf(dev, "firmware TPM event log is truncated\n");
	return (log);
invalid:
	device_printf(dev, "invalid firmware TPM event log metadata\n");
	return (NULL);
}

void
tpm_eventlog_destroy(struct tpm_eventlog *log)
{

	if (log == NULL)
		return;
	/* Drain all readers before releasing their immutable backing buffer. */
	destroy_dev(log->cdev);
	free(log, M_TPM_EVENTLOG);
}
