/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_VIDEO_VIDEO_INTERNAL_H_
#define _DEV_VIDEO_VIDEO_INTERNAL_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/queue.h>
#include <sys/selinfo.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <dev/video/video.h>

MALLOC_DECLARE(M_VIDEO);

#define	VIDEO_MAX_BUFFERS	8
#define	VIDEO_READ_BUFFERS	3

enum video_buf_state {
	VB_IDLE,
	VB_QUEUED,
	VB_ACTIVE,
	VB_DONE,
	VB_ERROR,
};

struct video_buf {
	STAILQ_ENTRY(video_buf) entry;
	struct video_buf_pool	*pool;
	struct video_device	*vd;

	uint32_t		index;
	enum video_buf_state	state;

	void			*buf;
	size_t			length;

	size_t			bytesused;
	uint32_t		sequence;
	struct timeval		timestamp;
	uint32_t		flags;
};

STAILQ_HEAD(video_buf_list, video_buf);

/*
 * All buffers must stay carved out of one OBJT_PHYS vm_object, mapped into
 * the kernel map and handed to userspace mmap as that same object.  Giving
 * each buffer its own object breaks the mmap path.
 */
struct video_buf_pool {
	vm_object_t		obj;
	vm_offset_t		kva;

	u_int			nbufs;
	size_t			buf_size;
	size_t			map_size;
	struct video_buf	bufs[VIDEO_MAX_BUFFERS];
};

struct video_file {
	struct video_device	*vd;
	bool			is_owner;
	bool			reading;	/* read(2) in progress */
	size_t			read_offset;
	struct video_buf	*read_buf;
};

enum video_mode {
	VMODE_NONE,
	VMODE_READ,
	VMODE_MMAP,
};

/* Lock order: cfg_sx -> mtx. */
struct video_device {
	device_t		dev;
	struct cdev		*cdev;
	int			unit;

	struct sx		cfg_sx;
	struct mtx		mtx;

	bool			dying;

	struct video_file	*owner;
	enum video_mode		mode;
	bool			streaming;
	bool			stopping;

	struct video_format	format;

	struct video_buf_pool	*pool;
	struct video_buf_list	queued;
	struct video_buf_list	done;
	u_int			readers;	/* copies in flight from pool */

	struct selinfo		sel;
};


#endif /* _DEV_VIDEO_VIDEO_INTERNAL_H_ */
