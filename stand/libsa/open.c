/*	$NetBSD: open.c,v 1.16 1997/01/28 09:41:03 pk Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * Copyright (c) 1989, 1990, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Author: Alessandro Forin
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "stand.h"

struct fs_ops *exclusive_file_system;

/*
 * Open file list. The current implementation and assumption is,
 * we only remove entries from tail and we only add new entries to tail.
 * This decision is to keep file id management simple - we get list
 * entries ordered by continiously growing f_id field.
 * If we do have multiple files open and we do close file not from tail,
 * this entry will be marked unused. open() will reuse unused entry, or
 * close will free all unused tail entries.
 *
 * Only case we expect open file list to grow long, is with zfs pools with
 * many disks. 
 */
file_list_t files = TAILQ_HEAD_INITIALIZER(files);

/*
 * Walk file list and return pointer to open_file structure.
 * if fd is < 0, return first unused open_file.
 */
struct open_file *
fd2open_file(int fd)
{
	struct open_file *f;

	TAILQ_FOREACH(f, &files, f_link) {
		if (fd >= 0) {
			if (f->f_id == fd)
				break;
			continue;
		}

		if (f->f_flags == 0)
			break;
	}
	return (f);
}

static int
o_gethandle(struct open_file **ptr)
{
	struct open_file *f, *last;

	/* Pick up unused entry */
	f = fd2open_file(-1);
	if (f != NULL) {
		*ptr = f;
		return (f->f_id);
	}

	/* Add new entry */
	f = calloc(1, sizeof (*f));
	if (f == NULL)
		return (-1);

	last = TAILQ_LAST(&files, file_list);
	if (last != NULL)
		f->f_id = last->f_id + 1;
	TAILQ_INSERT_TAIL(&files, f, f_link);

	*ptr = f;
	return (f->f_id);
}

static void
o_rainit(struct open_file *f)
{
	f->f_rabuf = malloc(SOPEN_RASIZE);
	f->f_ralen = 0;
	f->f_raoffset = 0;
}

int
open(const char *fname, int mode)
{
	struct fs_ops *fs;
	struct open_file *f;
	int fd, i, error, besterror;
	const char *file;

	TSENTER();

	if ((fd = o_gethandle(&f)) == -1) {
		errno = EMFILE;
		return (-1);
	}

	f->f_flags = mode + 1;
	f->f_dev = NULL;
	f->f_ops = NULL;
	f->f_offset = 0;
	f->f_devdata = NULL;
	file = NULL;

	if (exclusive_file_system != NULL) {
		fs = exclusive_file_system;
		error = (fs->fo_open)(fname, f);
		if (error == 0)
			goto ok;
		goto err;
	}

	error = devopen(f, fname, &file);
	if (error ||
	    (((f->f_flags & F_NODEV) == 0) && f->f_dev == NULL))
		goto err;

	/* see if we opened a raw device; otherwise, 'file' is the file name. */
	if (file == NULL || *file == '\0') {
		f->f_flags |= F_RAW;
		f->f_rabuf = NULL;
		TSEXIT();
		return (fd);
	}

	/* pass file name to the different filesystem open routines */
	besterror = ENOENT;
	for (i = 0; file_system[i] != NULL; i++) {
		fs = file_system[i];
		error = (fs->fo_open)(file, f);
		if (error == 0)
			goto ok;
		if (error != EINVAL)
			besterror = error;
	}
	error = besterror;

	if ((f->f_flags & F_NODEV) == 0 && f->f_dev != NULL)
		f->f_dev->dv_close(f);
	if (error)
		devclose(f);

err:
	f->f_flags = 0;
	errno = error;
	TSEXIT();
	return (-1);

ok:
	f->f_ops = fs;
	o_rainit(f);
	TSEXIT();
	return (fd);
}
