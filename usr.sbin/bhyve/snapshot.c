/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016 Flavius Anton
 * Copyright (c) 2016 Mihai Tiganus
 * Copyright (c) 2016-2019 Mihai Carabas
 * Copyright (c) 2017-2019 Darius Mihai
 * Copyright (c) 2017-2019 Elena Mihailescu
 * Copyright (c) 2018-2019 Sergiu Weisz
 * All rights reserved.
 * The bhyve-snapshot feature was developed under sponsorships
 * from Matthew Grooms.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>

#include <machine/atomic.h>
#include <machine/segments.h>

#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <sysexits.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#include <machine/vmm.h>
#ifndef WITHOUT_CAPSICUM
#include <machine/vmm_dev.h>
#endif
#include <machine/vmm_snapshot.h>
#include <vmmapi.h>

#include "bhyverun.h"
#include "acpi.h"
#include "atkbdc.h"
#include "debug.h"
#include "inout.h"
#include "ipc.h"
#include "fwctl.h"
#include "ioapic.h"
#include "mem.h"
#include "mevent.h"
#include "mptbl.h"
#include "pci_emul.h"
#include "pci_irq.h"
#include "pci_lpc.h"
#include "smbiostbl.h"
#include "snapshot.h"
#include "xmsr.h"
#include "spinup_ap.h"
#include "rtc.h"

#include <libxo/xo.h>
#include <ucl.h>

struct spinner_info {
	const size_t *crtval;
	const size_t maxval;
	const size_t total;
};

extern int guest_ncpus;

static struct winsize winsize;
static sig_t old_winch_handler;

#define	KB		(1024UL)
#define	MB		(1024UL * KB)
#define	GB		(1024UL * MB)

#define	SNAPSHOT_CHUNK	(4 * MB)
#define	PROG_BUF_SZ	(8192)

#define	SNAPSHOT_BUFFER_SIZE (20 * MB)

#define	JSON_STRUCT_ARR_KEY		"structs"
#define	JSON_DEV_ARR_KEY		"devices"
#define	JSON_BASIC_METADATA_KEY 	"basic metadata"
#define	JSON_SNAPSHOT_REQ_KEY		"snapshot_req"
#define	JSON_SIZE_KEY			"size"
#define	JSON_FILE_OFFSET_KEY		"file_offset"

#define	JSON_NCPUS_KEY			"ncpus"
#define	JSON_VMNAME_KEY 		"vmname"
#define	JSON_MEMSIZE_KEY		"memsize"
#define	JSON_MEMFLAGS_KEY		"memflags"

#define min(a,b)		\
({				\
 __typeof__ (a) _a = (a);	\
 __typeof__ (b) _b = (b); 	\
 _a < _b ? _a : _b;       	\
 })

const struct vm_snapshot_dev_info snapshot_devs[] = {
	{ "atkbdc",	atkbdc_snapshot,	NULL,		NULL		},
	{ "virtio-net",	pci_snapshot,		pci_pause,	pci_resume	},
	{ "virtio-blk",	pci_snapshot,		pci_pause,	pci_resume	},
	{ "virtio-rnd",	pci_snapshot,		NULL,		NULL		},
	{ "lpc",	pci_snapshot,		NULL,		NULL		},
	{ "fbuf",	pci_snapshot,		NULL,		NULL		},
	{ "xhci",	pci_snapshot,		NULL,		NULL		},
	{ "e1000",	pci_snapshot,		NULL,		NULL		},
	{ "ahci",	pci_snapshot,		pci_pause,	pci_resume	},
	{ "ahci-hd",	pci_snapshot,		pci_pause,	pci_resume	},
	{ "ahci-cd",	pci_snapshot,		pci_pause,	pci_resume	},
};

const struct vm_snapshot_kern_info snapshot_kern_structs[] = {
	{ "vhpet",	STRUCT_VHPET	},
	{ "vm",		STRUCT_VM	},
	{ "vmx",	STRUCT_VMX	},
	{ "vioapic",	STRUCT_VIOAPIC	},
	{ "vlapic",	STRUCT_VLAPIC	},
	{ "vmcx",	STRUCT_VMCX	},
	{ "vatpit",	STRUCT_VATPIT	},
	{ "vatpic",	STRUCT_VATPIC	},
	{ "vpmtmr",	STRUCT_VPMTMR	},
	{ "vrtc",	STRUCT_VRTC	},
};

static cpuset_t vcpus_active, vcpus_suspended;
static pthread_mutex_t vcpu_lock;
static pthread_cond_t vcpus_idle, vcpus_can_run;
static bool checkpoint_active;

/*
 * TODO: Harden this function and all of its callers since 'base_str' is a user
 * provided string.
 */
static char *
strcat_extension(const char *base_str, const char *ext)
{
	char *res;
	size_t base_len, ext_len;

	base_len = strnlen(base_str, NAME_MAX);
	ext_len = strnlen(ext, NAME_MAX);

	if (base_len + ext_len > NAME_MAX) {
		fprintf(stderr, "Filename exceeds maximum length.\n");
		return (NULL);
	}

	res = malloc(base_len + ext_len + 1);
	if (res == NULL) {
		perror("Failed to allocate memory.");
		return (NULL);
	}

	memcpy(res, base_str, base_len);
	memcpy(res + base_len, ext, ext_len);
	res[base_len + ext_len] = 0;

	return (res);
}

void
destroy_restore_state(struct restore_state *rstate)
{
	if (rstate == NULL) {
		fprintf(stderr, "Attempting to destroy NULL restore struct.\n");
		return;
	}

	if (rstate->kdata_map != MAP_FAILED)
		munmap(rstate->kdata_map, rstate->kdata_len);

	if (rstate->kdata_fd > 0)
		close(rstate->kdata_fd);
	if (rstate->vmmem_fd > 0)
		close(rstate->vmmem_fd);

	if (rstate->meta_root_obj != NULL)
		ucl_object_unref(rstate->meta_root_obj);
	if (rstate->meta_parser != NULL)
		ucl_parser_free(rstate->meta_parser);
}

static int
load_vmmem_file(const char *filename, struct restore_state *rstate)
{
	struct stat sb;
	int err;

	rstate->vmmem_fd = open(filename, O_RDONLY);
	if (rstate->vmmem_fd < 0) {
		perror("Failed to open restore file");
		return (-1);
	}

	err = fstat(rstate->vmmem_fd, &sb);
	if (err < 0) {
		perror("Failed to stat restore file");
		goto err_load_vmmem;
	}

	if (sb.st_size == 0) {
		fprintf(stderr, "Restore file is empty.\n");
		goto err_load_vmmem;
	}

	rstate->vmmem_len = sb.st_size;

	return (0);

err_load_vmmem:
	if (rstate->vmmem_fd > 0)
		close(rstate->vmmem_fd);
	return (-1);
}

static int
load_kdata_file(const char *filename, struct restore_state *rstate)
{
	struct stat sb;
	int err;

	rstate->kdata_fd = open(filename, O_RDONLY);
	if (rstate->kdata_fd < 0) {
		perror("Failed to open kernel data file");
		return (-1);
	}

	err = fstat(rstate->kdata_fd, &sb);
	if (err < 0) {
		perror("Failed to stat kernel data file");
		goto err_load_kdata;
	}

	if (sb.st_size == 0) {
		fprintf(stderr, "Kernel data file is empty.\n");
		goto err_load_kdata;
	}

	rstate->kdata_len = sb.st_size;
	rstate->kdata_map = mmap(NULL, rstate->kdata_len, PROT_READ,
				 MAP_SHARED, rstate->kdata_fd, 0);
	if (rstate->kdata_map == MAP_FAILED) {
		perror("Failed to map restore file");
		goto err_load_kdata;
	}

	return (0);

err_load_kdata:
	if (rstate->kdata_fd > 0)
		close(rstate->kdata_fd);
	return (-1);
}

static int
load_metadata_file(const char *filename, struct restore_state *rstate)
{
	const ucl_object_t *obj;
	struct ucl_parser *parser;
	int err;

	parser = ucl_parser_new(UCL_PARSER_DEFAULT);
	if (parser == NULL) {
		fprintf(stderr, "Failed to initialize UCL parser.\n");
		goto err_load_metadata;
	}

	err = ucl_parser_add_file(parser, filename);
	if (err == 0) {
		fprintf(stderr, "Failed to parse metadata file: '%s'\n",
			filename);
		err = -1;
		goto err_load_metadata;
	}

	obj = ucl_parser_get_object(parser);
	if (obj == NULL) {
		fprintf(stderr, "Failed to parse object.\n");
		err = -1;
		goto err_load_metadata;
	}

	rstate->meta_parser = parser;
	rstate->meta_root_obj = (ucl_object_t *)obj;

	return (0);

err_load_metadata:
	if (parser != NULL)
		ucl_parser_free(parser);
	return (err);
}

int
load_restore_file(const char *filename, struct restore_state *rstate)
{
	int err = 0;
	char *kdata_filename = NULL, *meta_filename = NULL;

	assert(filename != NULL);
	assert(rstate != NULL);

	memset(rstate, 0, sizeof(*rstate));
	rstate->kdata_map = MAP_FAILED;

	err = load_vmmem_file(filename, rstate);
	if (err != 0) {
		fprintf(stderr, "Failed to load guest RAM file.\n");
		goto err_restore;
	}

	kdata_filename = strcat_extension(filename, ".kern");
	if (kdata_filename == NULL) {
		fprintf(stderr, "Failed to construct kernel data filename.\n");
		goto err_restore;
	}

	err = load_kdata_file(kdata_filename, rstate);
	if (err != 0) {
		fprintf(stderr, "Failed to load guest kernel data file.\n");
		goto err_restore;
	}

	meta_filename = strcat_extension(filename, ".meta");
	if (meta_filename == NULL) {
		fprintf(stderr, "Failed to construct kernel metadata filename.\n");
		goto err_restore;
	}

	err = load_metadata_file(meta_filename, rstate);
	if (err != 0) {
		fprintf(stderr, "Failed to load guest metadata file.\n");
		goto err_restore;
	}

	return (0);

err_restore:
	destroy_restore_state(rstate);
	if (kdata_filename != NULL)
		free(kdata_filename);
	if (meta_filename != NULL)
		free(meta_filename);
	return (-1);
}

#define JSON_GET_INT_OR_RETURN(key, obj, result_ptr, ret)			\
do {										\
	const ucl_object_t *obj__;						\
	obj__ = ucl_object_lookup(obj, key);					\
	if (obj__ == NULL) {							\
		fprintf(stderr, "Missing key: '%s'", key);			\
		return (ret);							\
	}									\
	if (!ucl_object_toint_safe(obj__, result_ptr)) {			\
		fprintf(stderr, "Cannot convert '%s' value to int.", key);	\
		return (ret);							\
	}									\
} while(0)

#define JSON_GET_STRING_OR_RETURN(key, obj, result_ptr, ret)			\
do {										\
	const ucl_object_t *obj__;						\
	obj__ = ucl_object_lookup(obj, key);					\
	if (obj__ == NULL) {							\
		fprintf(stderr, "Missing key: '%s'", key);			\
		return (ret);							\
	}									\
	if (!ucl_object_tostring_safe(obj__, result_ptr)) {			\
		fprintf(stderr, "Cannot convert '%s' value to string.", key);	\
		return (ret);							\
	}									\
} while(0)

static void *
lookup_struct(enum snapshot_req struct_id, struct restore_state *rstate,
	      size_t *struct_size)
{
	const ucl_object_t *structs = NULL, *obj = NULL;
	ucl_object_iter_t it = NULL;
	int64_t snapshot_req, size, file_offset;

	structs = ucl_object_lookup(rstate->meta_root_obj, JSON_STRUCT_ARR_KEY);
	if (structs == NULL) {
		fprintf(stderr, "Failed to find '%s' object.\n",
			JSON_STRUCT_ARR_KEY);
		return (NULL);
	}

	if (ucl_object_type((ucl_object_t *)structs) != UCL_ARRAY) {
		fprintf(stderr, "Object '%s' is not an array.\n",
		JSON_STRUCT_ARR_KEY);
		return (NULL);
	}

	while ((obj = ucl_object_iterate(structs, &it, true)) != NULL) {
		snapshot_req = -1;
		JSON_GET_INT_OR_RETURN(JSON_SNAPSHOT_REQ_KEY, obj,
				       &snapshot_req, NULL);
		assert(snapshot_req >= 0);
		if ((enum snapshot_req) snapshot_req == struct_id) {
			JSON_GET_INT_OR_RETURN(JSON_SIZE_KEY, obj,
					       &size, NULL);
			assert(size >= 0);

			JSON_GET_INT_OR_RETURN(JSON_FILE_OFFSET_KEY, obj,
					       &file_offset, NULL);
			assert(file_offset >= 0);
			assert(file_offset + size <= rstate->kdata_len);

			*struct_size = (size_t)size;
			return (rstate->kdata_map + file_offset);
		}
	}

	return (NULL);
}

static void *
lookup_check_dev(const char *dev_name, struct restore_state *rstate,
		 const ucl_object_t *obj, size_t *data_size)
{
	const char *snapshot_req;
	int64_t size, file_offset;

	snapshot_req = NULL;
	JSON_GET_STRING_OR_RETURN(JSON_SNAPSHOT_REQ_KEY, obj,
				  &snapshot_req, NULL);
	assert(snapshot_req != NULL);
	if (!strcmp(snapshot_req, dev_name)) {
		JSON_GET_INT_OR_RETURN(JSON_SIZE_KEY, obj,
				       &size, NULL);
		assert(size >= 0);

		JSON_GET_INT_OR_RETURN(JSON_FILE_OFFSET_KEY, obj,
				       &file_offset, NULL);
		assert(file_offset >= 0);
		assert(file_offset + size <= rstate->kdata_len);

		*data_size = (size_t)size;
		return (rstate->kdata_map + file_offset);
	}

	return (NULL);
}

static void*
lookup_dev(const char *dev_name, struct restore_state *rstate,
	   size_t *data_size)
{
	const ucl_object_t *devs = NULL, *obj = NULL;
	ucl_object_iter_t it = NULL;
	void *ret;

	devs = ucl_object_lookup(rstate->meta_root_obj, JSON_DEV_ARR_KEY);
	if (devs == NULL) {
		fprintf(stderr, "Failed to find '%s' object.\n",
			JSON_DEV_ARR_KEY);
		return (NULL);
	}

	if (ucl_object_type((ucl_object_t *)devs) != UCL_ARRAY) {
		fprintf(stderr, "Object '%s' is not an array.\n",
			JSON_DEV_ARR_KEY);
		return (NULL);
	}

	while ((obj = ucl_object_iterate(devs, &it, true)) != NULL) {
		ret = lookup_check_dev(dev_name, rstate, obj, data_size);
		if (ret != NULL)
			return (ret);
	}

	return (NULL);
}

static const ucl_object_t *
lookup_basic_metadata_object(struct restore_state *rstate)
{
	const ucl_object_t *basic_meta_obj = NULL;

	basic_meta_obj = ucl_object_lookup(rstate->meta_root_obj,
					   JSON_BASIC_METADATA_KEY);
	if (basic_meta_obj == NULL) {
		fprintf(stderr, "Failed to find '%s' object.\n",
			JSON_BASIC_METADATA_KEY);
		return (NULL);
	}

	if (ucl_object_type((ucl_object_t *)basic_meta_obj) != UCL_OBJECT) {
		fprintf(stderr, "Object '%s' is not a JSON object.\n",
		JSON_BASIC_METADATA_KEY);
		return (NULL);
	}

	return (basic_meta_obj);
}

const char *
lookup_vmname(struct restore_state *rstate)
{
	const char *vmname;
	const ucl_object_t *obj;

	obj = lookup_basic_metadata_object(rstate);
	if (obj == NULL)
		return (NULL);

	JSON_GET_STRING_OR_RETURN(JSON_VMNAME_KEY, obj, &vmname, NULL);
	return (vmname);
}

int
lookup_memflags(struct restore_state *rstate)
{
	int64_t memflags;
	const ucl_object_t *obj;

	obj = lookup_basic_metadata_object(rstate);
	if (obj == NULL)
		return (0);

	JSON_GET_INT_OR_RETURN(JSON_MEMFLAGS_KEY, obj, &memflags, 0);

	return ((int)memflags);
}

size_t
lookup_memsize(struct restore_state *rstate)
{
	int64_t memsize;
	const ucl_object_t *obj;

	obj = lookup_basic_metadata_object(rstate);
	if (obj == NULL)
		return (0);

	JSON_GET_INT_OR_RETURN(JSON_MEMSIZE_KEY, obj, &memsize, 0);
	if (memsize < 0)
		memsize = 0;

	return ((size_t)memsize);
}


int
lookup_guest_ncpus(struct restore_state *rstate)
{
	int64_t ncpus;
	const ucl_object_t *obj;

	obj = lookup_basic_metadata_object(rstate);
	if (obj == NULL)
		return (0);

	JSON_GET_INT_OR_RETURN(JSON_NCPUS_KEY, obj, &ncpus, 0);
	return ((int)ncpus);
}

static void
winch_handler(int signal)
{
#ifdef TIOCGWINSZ
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize);
#endif /* TIOCGWINSZ */
}

static int
print_progress(size_t crtval, const size_t maxval)
{
	size_t rc;
	double crtval_gb, maxval_gb;
	size_t i, win_width, prog_start, prog_done, prog_end;
	int mval_len;

	static char prog_buf[PROG_BUF_SZ];
	static const size_t len = sizeof(prog_buf);

	static size_t div;
	static char *div_str;

	static char wip_bar[] = { '/', '-', '\\', '|' };
	static int wip_idx = 0;

	if (maxval == 0) {
		printf("[0B / 0B]\r\n");
		return (0);
	}

	if (crtval > maxval)
		crtval = maxval;

	if (maxval > 10 * GB) {
		div = GB;
		div_str = "GiB";
	} else if (maxval > 10 * MB) {
		div = MB;
		div_str = "MiB";
	} else {
		div = KB;
		div_str = "KiB";
	}

	crtval_gb = (double) crtval / div;
	maxval_gb = (double) maxval / div;

	rc = snprintf(prog_buf, len, "%.03lf", maxval_gb);
	if (rc == len) {
		fprintf(stderr, "Maxval too big\n");
		return (-1);
	}
	mval_len = rc;

	rc = snprintf(prog_buf, len, "\r[%*.03lf%s / %.03lf%s] |",
		mval_len, crtval_gb, div_str, maxval_gb, div_str);

	if (rc == len) {
		fprintf(stderr, "Buffer too small to print progress\n");
		return (-1);
	}

	win_width = min(winsize.ws_col, len);
	prog_start = rc;

	if (prog_start < (win_width - 2)) {
		prog_end = win_width - prog_start - 2;
		prog_done = prog_end * (crtval_gb / maxval_gb);

		for (i = prog_start; i < prog_start + prog_done; i++)
			prog_buf[i] = '#';

		if (crtval != maxval) {
			prog_buf[i] = wip_bar[wip_idx];
			wip_idx = (wip_idx + 1) % sizeof(wip_bar);
			i++;
		} else {
			prog_buf[i++] = '#';
		}

		for (; i < win_width - 2; i++)
			prog_buf[i] = '_';

		prog_buf[win_width - 2] = '|';
	}

	prog_buf[win_width - 1] = '\0';
	write(STDOUT_FILENO, prog_buf, win_width);

	return (0);
}

static void *
snapshot_spinner_cb(void *arg)
{
	int rc;
	size_t crtval, maxval, total;
	struct spinner_info *si;
	struct timespec ts;

	si = arg;
	if (si == NULL)
		pthread_exit(NULL);

	ts.tv_sec = 0;
	ts.tv_nsec = 50 * 1000 * 1000; /* 50 ms sleep time */

	do {
		crtval = *si->crtval;
		maxval = si->maxval;
		total = si->total;

		rc = print_progress(crtval, total);
		if (rc < 0) {
			fprintf(stderr, "Failed to parse progress\n");
			break;
		}

		nanosleep(&ts, NULL);
	} while (crtval < maxval);

	pthread_exit(NULL);
	return NULL;
}

static int
vm_snapshot_mem_part(const int snapfd, const size_t foff, void *src,
		     const size_t len, const size_t totalmem, const bool op_wr)
{
	int rc;
	size_t part_done, todo, rem;
	ssize_t done;
	bool show_progress;
	pthread_t spinner_th;
	struct spinner_info *si;

	if (lseek(snapfd, foff, SEEK_SET) < 0) {
		perror("Failed to change file offset");
		return (-1);
	}

	show_progress = false;
	if (isatty(STDIN_FILENO) && (winsize.ws_col != 0))
		show_progress = true;

	part_done = foff;
	rem = len;

	if (show_progress) {
		si = &(struct spinner_info) {
			.crtval = &part_done,
			.maxval = foff + len,
			.total = totalmem
		};

		rc = pthread_create(&spinner_th, 0, snapshot_spinner_cb, si);
		if (rc) {
			perror("Unable to create spinner thread");
			show_progress = false;
		}
	}

	while (rem > 0) {
		if (show_progress)
			todo = min(SNAPSHOT_CHUNK, rem);
		else
			todo = rem;

		if (op_wr)
			done = write(snapfd, src, todo);
		else
			done = read(snapfd, src, todo);
		if (done < 0) {
			perror("Failed to write in file");
			return (-1);
		}

		src += done;
		part_done += done;
		rem -= done;
	}

	if (show_progress) {
		rc = pthread_join(spinner_th, NULL);
		if (rc)
			perror("Unable to end spinner thread");
	}

	return (0);
}

static size_t
vm_snapshot_mem(struct vmctx *ctx, int snapfd, size_t memsz, const bool op_wr)
{
	int ret;
	size_t lowmem, highmem, totalmem;
	char *baseaddr;

	ret = vm_get_guestmem_from_ctx(ctx, &baseaddr, &lowmem, &highmem);
	if (ret) {
		fprintf(stderr, "%s: unable to retrieve guest memory size\r\n",
			__func__);
		return (0);
	}
	totalmem = lowmem + highmem;

	if ((op_wr == false) && (totalmem != memsz)) {
		fprintf(stderr, "%s: mem size mismatch: %ld vs %ld\r\n",
			__func__, totalmem, memsz);
		return (0);
	}

	winsize.ws_col = 80;
#ifdef TIOCGWINSZ
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize);
#endif /* TIOCGWINSZ */
	old_winch_handler = signal(SIGWINCH, winch_handler);

	ret = vm_snapshot_mem_part(snapfd, 0, baseaddr, lowmem,
		totalmem, op_wr);
	if (ret) {
		fprintf(stderr, "%s: Could not %s lowmem\r\n",
			__func__, op_wr ? "write" : "read");
		totalmem = 0;
		goto done;
	}

	if (highmem == 0)
		goto done;

	ret = vm_snapshot_mem_part(snapfd, lowmem, baseaddr + 4*GB,
		highmem, totalmem, op_wr);
	if (ret) {
		fprintf(stderr, "%s: Could not %s highmem\r\n",
		        __func__, op_wr ? "write" : "read");
		totalmem = 0;
		goto done;
	}

done:
	printf("\r\n");
	signal(SIGWINCH, old_winch_handler);

	return (totalmem);
}

int
restore_vm_mem(struct vmctx *ctx, struct restore_state *rstate)
{
	size_t restored;

	restored = vm_snapshot_mem(ctx, rstate->vmmem_fd, rstate->vmmem_len,
				   false);

	if (restored != rstate->vmmem_len)
		return (-1);

	return (0);
}

static int
vm_restore_kern_struct(struct vmctx *ctx, struct restore_state *rstate,
		       const struct vm_snapshot_kern_info *info)
{
	void *struct_ptr;
	size_t struct_size;
	int ret;
	struct vm_snapshot_meta *meta;

	struct_ptr = lookup_struct(info->req, rstate, &struct_size);
	if (struct_ptr == NULL) {
		fprintf(stderr, "%s: Failed to lookup struct %s\r\n",
			__func__, info->struct_name);
		ret = -1;
		goto done;
	}

	if (struct_size == 0) {
		fprintf(stderr, "%s: Kernel struct size was 0 for: %s\r\n",
			__func__, info->struct_name);
		ret = -1;
		goto done;
	}

	meta = &(struct vm_snapshot_meta) {
		.ctx = ctx,
		.dev_name = info->struct_name,
		.dev_req  = info->req,

		.buffer.buf_start = struct_ptr,
		.buffer.buf_size = struct_size,

		.buffer.buf = struct_ptr,
		.buffer.buf_rem = struct_size,

		.op = VM_SNAPSHOT_RESTORE,
	};

	ret = vm_snapshot_req(meta);
	if (ret != 0) {
		fprintf(stderr, "%s: Failed to restore struct: %s\r\n",
			__func__, info->struct_name);
		goto done;
	}

done:
	return (ret);
}

int
vm_restore_kern_structs(struct vmctx *ctx, struct restore_state *rstate)
{
	int ret;
	int i;

	for (i = 0; i < nitems(snapshot_kern_structs); i++) {
		ret = vm_restore_kern_struct(ctx, rstate,
					     &snapshot_kern_structs[i]);
		if (ret != 0)
			return (ret);
	}

	return (0);
}

int
vm_restore_user_dev(struct vmctx *ctx, struct restore_state *rstate,
		    const struct vm_snapshot_dev_info *info)
{
	void *dev_ptr;
	size_t dev_size;
	int ret;
	struct vm_snapshot_meta *meta;

	dev_ptr = lookup_dev(info->dev_name, rstate, &dev_size);
	if (dev_ptr == NULL) {
		fprintf(stderr, "Failed to lookup dev: %s\r\n", info->dev_name);
		fprintf(stderr, "Continuing the restore/migration process\r\n");
		return (0);
	}

	if (dev_size == 0) {
		fprintf(stderr, "%s: Device size is 0. "
			"Assuming %s is not used\r\n",
			__func__, info->dev_name);
		return (0);
	}

	meta = &(struct vm_snapshot_meta) {
		.ctx = ctx,
		.dev_name = info->dev_name,

		.buffer.buf_start = dev_ptr,
		.buffer.buf_size = dev_size,

		.buffer.buf = dev_ptr,
		.buffer.buf_rem = dev_size,

		.op = VM_SNAPSHOT_RESTORE,
	};

	ret = (*info->snapshot_cb)(meta);
	if (ret != 0) {
		fprintf(stderr, "Failed to restore dev: %s\r\n",
			info->dev_name);
		return (-1);
	}

	return (0);
}


int
vm_restore_user_devs(struct vmctx *ctx, struct restore_state *rstate)
{
	int ret;
	int i;

	for (i = 0; i < nitems(snapshot_devs); i++) {
		ret = vm_restore_user_dev(ctx, rstate, &snapshot_devs[i]);
		if (ret != 0)
			return (ret);
	}

	return 0;
}

int
vm_pause_user_devs(struct vmctx *ctx)
{
	const struct vm_snapshot_dev_info *info;
	int ret;
	int i;

	for (i = 0; i < nitems(snapshot_devs); i++) {
		info = &snapshot_devs[i];
		if (info->pause_cb == NULL)
			continue;

		ret = info->pause_cb(ctx, info->dev_name);
		if (ret != 0)
			return (ret);
	}

	return (0);
}

int
vm_resume_user_devs(struct vmctx *ctx)
{
	const struct vm_snapshot_dev_info *info;
	int ret;
	int i;

	for (i = 0; i < nitems(snapshot_devs); i++) {
		info = &snapshot_devs[i];
		if (info->resume_cb == NULL)
			continue;

		ret = info->resume_cb(ctx, info->dev_name);
		if (ret != 0)
			return (ret);
	}

	return (0);
}

static int
vm_snapshot_kern_struct(int data_fd, xo_handle_t *xop, const char *array_key,
			struct vm_snapshot_meta *meta, off_t *offset)
{
	int ret;
	size_t data_size;
	ssize_t write_cnt;

	ret = vm_snapshot_req(meta);
	if (ret != 0) {
		fprintf(stderr, "%s: Failed to snapshot struct %s\r\n",
			__func__, meta->dev_name);
		ret = -1;
		goto done;
	}

	data_size = vm_get_snapshot_size(meta);

	write_cnt = write(data_fd, meta->buffer.buf_start, data_size);
	if (write_cnt != data_size) {
		perror("Failed to write all snapshotted data.");
		ret = -1;
		goto done;
	}

	/* Write metadata. */
	xo_open_instance_h(xop, array_key);
	xo_emit_h(xop, "{:debug_name/%s}\n", meta->dev_name);
	xo_emit_h(xop, "{:" JSON_SNAPSHOT_REQ_KEY "/%d}\n",
		  meta->dev_req);
	xo_emit_h(xop, "{:" JSON_SIZE_KEY "/%lu}\n", data_size);
	xo_emit_h(xop, "{:" JSON_FILE_OFFSET_KEY "/%lu}\n", *offset);
	xo_close_instance_h(xop, JSON_STRUCT_ARR_KEY);

	*offset += data_size;

done:
	return (ret);
}

static int
vm_snapshot_kern_structs(struct vmctx *ctx, int data_fd, xo_handle_t *xop)
{
	int ret, i, error;
	size_t offset, buf_size;
	char *buffer;
	struct vm_snapshot_meta *meta;

	error = 0;
	offset = 0;
	buf_size = SNAPSHOT_BUFFER_SIZE;

	buffer = malloc(SNAPSHOT_BUFFER_SIZE * sizeof(char));
	if (buffer == NULL) {
		error = ENOMEM;
		perror("Failed to allocate memory for snapshot buffer");
		goto err_vm_snapshot_kern_data;
	}

	meta = &(struct vm_snapshot_meta) {
		.ctx = ctx,

		.buffer.buf_start = buffer,
		.buffer.buf_size = buf_size,

		.op = VM_SNAPSHOT_SAVE,
	};

	xo_open_list_h(xop, JSON_STRUCT_ARR_KEY);
	for (i = 0; i < nitems(snapshot_kern_structs); i++) {
		meta->dev_name = snapshot_kern_structs[i].struct_name;
		meta->dev_req  = snapshot_kern_structs[i].req;

		memset(meta->buffer.buf_start, 0, meta->buffer.buf_size);
		meta->buffer.buf = meta->buffer.buf_start;
		meta->buffer.buf_rem = meta->buffer.buf_size;

		ret = vm_snapshot_kern_struct(data_fd, xop, JSON_DEV_ARR_KEY,
					      meta, &offset);
		if (ret != 0) {
			error = -1;
			goto err_vm_snapshot_kern_data;
		}
	}
	xo_close_list_h(xop, JSON_STRUCT_ARR_KEY);

err_vm_snapshot_kern_data:
	if (buffer != NULL)
		free(buffer);
	return (error);
}

static int
vm_snapshot_basic_metadata(struct vmctx *ctx, xo_handle_t *xop, size_t memsz)
{

	xo_open_container_h(xop, JSON_BASIC_METADATA_KEY);
	xo_emit_h(xop, "{:" JSON_NCPUS_KEY "/%ld}\n", guest_ncpus);
	xo_emit_h(xop, "{:" JSON_VMNAME_KEY "/%s}\n", vm_get_name(ctx));
	xo_emit_h(xop, "{:" JSON_MEMSIZE_KEY "/%lu}\n", memsz);
	xo_emit_h(xop, "{:" JSON_MEMFLAGS_KEY "/%d}\n", vm_get_memflags(ctx));
	xo_close_container_h(xop, JSON_BASIC_METADATA_KEY);

	return (0);
}

static int
vm_snapshot_dev_write_data(int data_fd, xo_handle_t *xop, const char *array_key,
			   struct vm_snapshot_meta *meta, off_t *offset)
{
	int ret;
	size_t data_size;

	data_size = vm_get_snapshot_size(meta);

	ret = write(data_fd, meta->buffer.buf_start, data_size);
	if (ret != data_size) {
		perror("Failed to write all snapshotted data.");
		return (-1);
	}

	/* Write metadata. */
	xo_open_instance_h(xop, array_key);
	xo_emit_h(xop, "{:" JSON_SNAPSHOT_REQ_KEY "/%s}\n", meta->dev_name);
	xo_emit_h(xop, "{:" JSON_SIZE_KEY "/%lu}\n", data_size);
	xo_emit_h(xop, "{:" JSON_FILE_OFFSET_KEY "/%lu}\n", *offset);
	xo_close_instance_h(xop, array_key);

	*offset += data_size;

	return (0);
}

static int
vm_snapshot_user_dev(const struct vm_snapshot_dev_info *info,
		     int data_fd, xo_handle_t *xop,
		     struct vm_snapshot_meta *meta, off_t *offset)
{
	int ret;

	ret = (*info->snapshot_cb)(meta);
	if (ret != 0) {
		fprintf(stderr, "Failed to snapshot %s; ret=%d\r\n",
			meta->dev_name, ret);
		return (ret);
	}

	ret = vm_snapshot_dev_write_data(data_fd, xop, JSON_DEV_ARR_KEY, meta,
					 offset);
	if (ret != 0)
		return (ret);

	return (0);
}

static int
vm_snapshot_user_devs(struct vmctx *ctx, int data_fd, xo_handle_t *xop)
{
	int ret, i;
	off_t offset;
	void *buffer;
	size_t buf_size;
	struct vm_snapshot_meta *meta;

	buf_size = SNAPSHOT_BUFFER_SIZE;

	offset = lseek(data_fd, 0, SEEK_CUR);
	if (offset < 0) {
		perror("Failed to get data file current offset.");
		return (-1);
	}

	buffer = malloc(buf_size);
	if (buffer == NULL) {
		perror("Failed to allocate memory for snapshot buffer");
		ret = ENOSPC;
		goto snapshot_err;
	}

	meta = &(struct vm_snapshot_meta) {
		.ctx = ctx,

		.buffer.buf_start = buffer,
		.buffer.buf_size = buf_size,

		.op = VM_SNAPSHOT_SAVE,
	};

	xo_open_list_h(xop, JSON_DEV_ARR_KEY);

	/* Restore other devices that support this feature */
	for (i = 0; i < nitems(snapshot_devs); i++) {
		meta->dev_name = snapshot_devs[i].dev_name;

		memset(meta->buffer.buf_start, 0, meta->buffer.buf_size);
		meta->buffer.buf = meta->buffer.buf_start;
		meta->buffer.buf_rem = meta->buffer.buf_size;

		ret = vm_snapshot_user_dev(&snapshot_devs[i], data_fd, xop,
					   meta, &offset);
		if (ret != 0)
			goto snapshot_err;
	}

	xo_close_list_h(xop, JSON_DEV_ARR_KEY);

snapshot_err:
	if (buffer != NULL)
		free(buffer);
	return (ret);
}

void
checkpoint_cpu_add(int vcpu)
{

	pthread_mutex_lock(&vcpu_lock);
	CPU_SET(vcpu, &vcpus_active);

	if (checkpoint_active) {
		CPU_SET(vcpu, &vcpus_suspended);
		while (checkpoint_active)
			pthread_cond_wait(&vcpus_can_run, &vcpu_lock);
		CPU_CLR(vcpu, &vcpus_suspended);
	}
	pthread_mutex_unlock(&vcpu_lock);
}

/*
 * When a vCPU is suspended for any reason, it calls
 * checkpoint_cpu_suspend().  This records that the vCPU is idle.
 * Before returning from suspension, checkpoint_cpu_resume() is
 * called.  In suspend we note that the vCPU is idle.  In resume we
 * pause the vCPU thread until the checkpoint is complete.  The reason
 * for the two-step process is that vCPUs might already be stopped in
 * the debug server when a checkpoint is requested.  This approach
 * allows us to account for and handle those vCPUs.
 */
void
checkpoint_cpu_suspend(int vcpu)
{

	pthread_mutex_lock(&vcpu_lock);
	CPU_SET(vcpu, &vcpus_suspended);
	if (checkpoint_active && CPU_CMP(&vcpus_active, &vcpus_suspended) == 0)
		pthread_cond_signal(&vcpus_idle);
	pthread_mutex_unlock(&vcpu_lock);
}

void
checkpoint_cpu_resume(int vcpu)
{

	pthread_mutex_lock(&vcpu_lock);
	while (checkpoint_active)
		pthread_cond_wait(&vcpus_can_run, &vcpu_lock);
	CPU_CLR(vcpu, &vcpus_suspended);
	pthread_mutex_unlock(&vcpu_lock);
}

static void
vm_vcpu_pause(struct vmctx *ctx)
{

	pthread_mutex_lock(&vcpu_lock);
	checkpoint_active = true;
	vm_suspend_cpu(ctx, -1);
	while (CPU_CMP(&vcpus_active, &vcpus_suspended) != 0)
		pthread_cond_wait(&vcpus_idle, &vcpu_lock);
	pthread_mutex_unlock(&vcpu_lock);
}

static void
vm_vcpu_resume(struct vmctx *ctx)
{

	pthread_mutex_lock(&vcpu_lock);
	checkpoint_active = false;
	pthread_mutex_unlock(&vcpu_lock);
	vm_resume_cpu(ctx, -1);
	pthread_cond_broadcast(&vcpus_can_run);
}

static int
vm_checkpoint(struct vmctx *ctx, const char *checkpoint_file, bool stop_vm)
{
	int fd_checkpoint = 0, kdata_fd = 0;
	int ret = 0;
	int error = 0;
	size_t memsz;
	xo_handle_t *xop = NULL;
	char *meta_filename = NULL;
	char *kdata_filename = NULL;
	FILE *meta_file = NULL;

	kdata_filename = strcat_extension(checkpoint_file, ".kern");
	if (kdata_filename == NULL) {
		fprintf(stderr, "Failed to construct kernel data filename.\n");
		return (-1);
	}

	kdata_fd = open(kdata_filename, O_WRONLY | O_CREAT | O_TRUNC, 0700);
	if (kdata_fd < 0) {
		perror("Failed to open kernel data snapshot file.");
		error = -1;
		goto done;
	}

	fd_checkpoint = open(checkpoint_file, O_RDWR | O_CREAT | O_TRUNC, 0700);

	if (fd_checkpoint < 0) {
		perror("Failed to create checkpoint file");
		error = -1;
		goto done;
	}

	meta_filename = strcat_extension(checkpoint_file, ".meta");
	if (meta_filename == NULL) {
		fprintf(stderr, "Failed to construct vm metadata filename.\n");
		goto done;
	}

	meta_file = fopen(meta_filename, "w");
	if (meta_file == NULL) {
		perror("Failed to open vm metadata snapshot file.");
		goto done;
	}

	xop = xo_create_to_file(meta_file, XO_STYLE_JSON, XOF_PRETTY);
	if (xop == NULL) {
		perror("Failed to get libxo handle on metadata file.");
		goto done;
	}

	vm_vcpu_pause(ctx);

	ret = vm_pause_user_devs(ctx);
	if (ret != 0) {
		fprintf(stderr, "Could not pause devices\r\n");
		error = ret;
		goto done;
	}

	memsz = vm_snapshot_mem(ctx, fd_checkpoint, 0, true);
	if (memsz == 0) {
		perror("Could not write guest memory to file");
		error = -1;
		goto done;
	}

	ret = vm_snapshot_basic_metadata(ctx, xop, memsz);
	if (ret != 0) {
		fprintf(stderr, "Failed to snapshot vm basic metadata.\n");
		error = -1;
		goto done;
	}


	ret = vm_snapshot_kern_structs(ctx, kdata_fd, xop);
	if (ret != 0) {
		fprintf(stderr, "Failed to snapshot vm kernel data.\n");
		error = -1;
		goto done;
	}

	ret = vm_snapshot_user_devs(ctx, kdata_fd, xop);
	if (ret != 0) {
		fprintf(stderr, "Failed to snapshot device state.\n");
		error = -1;
		goto done;
	}

	xo_finish_h(xop);

	if (stop_vm) {
		vm_destroy(ctx);
		exit(0);
	}

done:
	ret = vm_resume_user_devs(ctx);
	if (ret != 0)
		fprintf(stderr, "Could not resume devices\r\n");
	vm_vcpu_resume(ctx);
	if (fd_checkpoint > 0)
		close(fd_checkpoint);
	if (meta_filename != NULL)
		free(meta_filename);
	if (kdata_filename != NULL)
		free(kdata_filename);
	if (xop != NULL)
		xo_destroy(xop);
	if (meta_file != NULL)
		fclose(meta_file);
	if (kdata_fd > 0)
		close(kdata_fd);
	return (error);
}

static int
handle_message(struct vmctx *ctx, nvlist_t *nvl)
{
	const char *cmd;
	struct ipc_command **ipc_cmd;

	if (!nvlist_exists_string(nvl, "cmd"))
		return (EINVAL);

	cmd = nvlist_get_string(nvl, "cmd");
	IPC_COMMAND_FOREACH(ipc_cmd, ipc_cmd_set) {
		if (strcmp(cmd, (*ipc_cmd)->name) == 0)
			return ((*ipc_cmd)->handler(ctx, nvl));
	}

	return (EOPNOTSUPP);
}

/*
 * Listen for commands from bhyvectl
 */
void *
checkpoint_thread(void *param)
{
	int fd;
	struct checkpoint_thread_info *thread_info;
	nvlist_t *nvl;

	pthread_set_name_np(pthread_self(), "checkpoint thread");
	thread_info = (struct checkpoint_thread_info *)param;

	while ((fd = accept(thread_info->socket_fd, NULL, NULL)) != -1) {
		nvl = nvlist_recv(fd, 0);
		if (nvl != NULL)
			handle_message(thread_info->ctx, nvl);
		else
			EPRINTLN("nvlist_recv() failed: %s", strerror(errno));

		close(fd);
		nvlist_destroy(nvl);
	}

	return (NULL);
}

static int
vm_do_checkpoint(struct vmctx *ctx, const nvlist_t *nvl)
{
	int error;

	if (!nvlist_exists_string(nvl, "filename") ||
	    !nvlist_exists_bool(nvl, "suspend"))
		error = EINVAL;
	else
		error = vm_checkpoint(ctx, nvlist_get_string(nvl, "filename"),
		    nvlist_get_bool(nvl, "suspend"));

	return (error);
}
IPC_COMMAND(ipc_cmd_set, checkpoint, vm_do_checkpoint);

void
init_snapshot(void)
{
	int err;

	err = pthread_mutex_init(&vcpu_lock, NULL);
	if (err != 0)
		errc(1, err, "checkpoint mutex init");
	err = pthread_cond_init(&vcpus_idle, NULL);
	if (err != 0)
		errc(1, err, "checkpoint cv init (vcpus_idle)");
	err = pthread_cond_init(&vcpus_can_run, NULL);
	if (err != 0)
		errc(1, err, "checkpoint cv init (vcpus_can_run)");
}

/*
 * Create the listening socket for IPC with bhyvectl
 */
int
init_checkpoint_thread(struct vmctx *ctx)
{
	struct checkpoint_thread_info *checkpoint_info = NULL;
	struct sockaddr_un addr;
	int socket_fd;
	pthread_t checkpoint_pthread;
	int err;

	memset(&addr, 0, sizeof(addr));

	socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		EPRINTLN("Socket creation failed: %s", strerror(errno));
		err = -1;
		goto fail;
	}

	addr.sun_family = AF_UNIX;

	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s%s",
		 BHYVE_RUN_DIR, vm_get_name(ctx));
	addr.sun_len = SUN_LEN(&addr);
	unlink(addr.sun_path);

	if (bind(socket_fd, (struct sockaddr *)&addr, addr.sun_len) != 0) {
		EPRINTLN("Failed to bind socket \"%s\": %s\n",
		    addr.sun_path, strerror(errno));
		err = -1;
		goto fail;
	}

	if (listen(socket_fd, 10) < 0) {
		EPRINTLN("ipc socket listen: %s\n", strerror(errno));
		err = errno;
		goto fail;
	}

	checkpoint_info = calloc(1, sizeof(*checkpoint_info));
	checkpoint_info->ctx = ctx;
	checkpoint_info->socket_fd = socket_fd;

	err = pthread_create(&checkpoint_pthread, NULL, checkpoint_thread,
		checkpoint_info);
	if (err != 0)
		goto fail;

	return (0);
fail:
	free(checkpoint_info);
	if (socket_fd > 0)
		close(socket_fd);
	unlink(addr.sun_path);

	return (err);
}

void
vm_snapshot_buf_err(const char *bufname, const enum vm_snapshot_op op)
{
	const char *__op;

	if (op == VM_SNAPSHOT_SAVE)
		__op = "save";
	else if (op == VM_SNAPSHOT_RESTORE)
		__op = "restore";
	else
		__op = "unknown";

	fprintf(stderr, "%s: snapshot-%s failed for %s\r\n",
		__func__, __op, bufname);
}

int
vm_snapshot_buf(volatile void *data, size_t data_size,
		struct vm_snapshot_meta *meta)
{
	struct vm_snapshot_buffer *buffer;
	int op;

	buffer = &meta->buffer;
	op = meta->op;

	if (buffer->buf_rem < data_size) {
		fprintf(stderr, "%s: buffer too small\r\n", __func__);
		return (E2BIG);
	}

	if (op == VM_SNAPSHOT_SAVE)
		memcpy(buffer->buf, (uint8_t *) data, data_size);
	else if (op == VM_SNAPSHOT_RESTORE)
		memcpy((uint8_t *) data, buffer->buf, data_size);
	else
		return (EINVAL);

	buffer->buf += data_size;
	buffer->buf_rem -= data_size;

	return (0);
}

size_t
vm_get_snapshot_size(struct vm_snapshot_meta *meta)
{
	size_t length;
	struct vm_snapshot_buffer *buffer;

	buffer = &meta->buffer;

	if (buffer->buf_size < buffer->buf_rem) {
		fprintf(stderr, "%s: Invalid buffer: size = %zu, rem = %zu\r\n",
			__func__, buffer->buf_size, buffer->buf_rem);
		length = 0;
	} else {
		length = buffer->buf_size - buffer->buf_rem;
	}

	return (length);
}

int
vm_snapshot_guest2host_addr(void **addrp, size_t len, bool restore_null,
			    struct vm_snapshot_meta *meta)
{
	int ret;
	vm_paddr_t gaddr;

	if (meta->op == VM_SNAPSHOT_SAVE) {
		gaddr = paddr_host2guest(meta->ctx, *addrp);
		if (gaddr == (vm_paddr_t) -1) {
			if (!restore_null ||
			    (restore_null && (*addrp != NULL))) {
				ret = EFAULT;
				goto done;
			}
		}

		SNAPSHOT_VAR_OR_LEAVE(gaddr, meta, ret, done);
	} else if (meta->op == VM_SNAPSHOT_RESTORE) {
		SNAPSHOT_VAR_OR_LEAVE(gaddr, meta, ret, done);
		if (gaddr == (vm_paddr_t) -1) {
			if (!restore_null) {
				ret = EFAULT;
				goto done;
			}
		}

		*addrp = paddr_guest2host(meta->ctx, gaddr, len);
	} else {
		ret = EINVAL;
	}

done:
	return (ret);
}

int
vm_snapshot_buf_cmp(volatile void *data, size_t data_size,
		    struct vm_snapshot_meta *meta)
{
	struct vm_snapshot_buffer *buffer;
	int op;
	int ret;

	buffer = &meta->buffer;
	op = meta->op;

	if (buffer->buf_rem < data_size) {
		fprintf(stderr, "%s: buffer too small\r\n", __func__);
		ret = E2BIG;
		goto done;
	}

	if (op == VM_SNAPSHOT_SAVE) {
		ret = 0;
		memcpy(buffer->buf, (uint8_t *) data, data_size);
	} else if (op == VM_SNAPSHOT_RESTORE) {
		ret = memcmp((uint8_t *) data, buffer->buf, data_size);
	} else {
		ret = EINVAL;
		goto done;
	}

	buffer->buf += data_size;
	buffer->buf_rem -= data_size;

done:
	return (ret);
}
