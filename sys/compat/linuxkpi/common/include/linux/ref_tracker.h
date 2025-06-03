/*-
 * Copyright (c) 2025 The FreeBSD Foundation
 * Copyright (c) 2025 Jean-Sébastien Pédron
 *
 * This software was developed by Jean-Sébastien Pédron under sponsorship
 * from the FreeBSD Foundation.
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

#ifndef _LINUXKPI_LINUX_REF_TRACKER_H_
#define	_LINUXKPI_LINUX_REF_TRACKER_H_

#include <linux/refcount.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/stackdepot.h>

struct ref_tracker;

struct ref_tracker_dir {
};

/*
 * The following functions currently have dummy implementations that, on Linux,
 * are used when CONFIG_REF_TRACKER is not set at compile time.
 *
 * The ref tracker is a tool to associate a refcount increase to a refcount
 * decrease. This helps developers track, document and debug refcounts. We
 * don't need this feature for now in linuxkpi.
 */

static inline void
ref_tracker_dir_init(struct ref_tracker_dir *dir,
    unsigned int quarantine_count, const char *name)
{
}

static inline void
ref_tracker_dir_exit(struct ref_tracker_dir *dir)
{
}

static inline void
ref_tracker_dir_print_locked(struct ref_tracker_dir *dir,
    unsigned int display_limit)
{
}

static inline void
ref_tracker_dir_print(struct ref_tracker_dir *dir, unsigned int display_limit)
{
}

static inline int
ref_tracker_dir_snprint(struct ref_tracker_dir *dir, char *buf, size_t size)
{
	return (0);
}

static inline int
ref_tracker_alloc(struct ref_tracker_dir *dir, struct ref_tracker **trackerp,
    gfp_t gfp)
{
	return (0);
}

static inline int
ref_tracker_free(struct ref_tracker_dir *dir, struct ref_tracker **trackerp)
{
	return (0);
}

#endif /* !defined(_LINUXKPI_LINUX_REF_TRACKER_H_) */
