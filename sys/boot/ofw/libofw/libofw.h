/*
 * Copyright (C) 2000 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "openfirm.h"

/* Note: Must match the 'struct devdesc' in bootstrap.h */
struct ofw_devdesc {
	struct devsw	*d_dev;
	int		d_type;
	union {
		struct {
			phandle_t	handle;		/* OFW handle */
			unsigned long	partoff;	/* sector offset */
			int	unit;			/* disk number */
			char	path[64];		/* OFW path */
			int	slice;			/* slice# */
			int	partition;		/* partition in slice */
			int	bsize;			/* block size */
		} ofwdisk;
		struct {
			int	unit;
			char	path[64];
			void	*dmabuf;
		} netif;
	} d_kind;
/*
 * Keeping this around so I know what came from the NetBSD stuff.
 * I've made a wild guess as to what goes where, but I have no idea if it's
 * right.
 *
 *	void *dmabuf;
 */
};

#define	MAXDEV		31		/* Maximum number of devices. */

/* Known types.  Use the same as alpha for consistancy. */
#define	DEVT_NONE	0
#define	DEVT_DISK	1
#define	DEVT_NET	2

extern int	ofw_getdev(void **vdev, const char *devspec, const char **path);
extern char	*ofw_fmtdev(void *vdev);
extern int	ofw_parseofwdev(struct ofw_devdesc *, const char *devspec);
extern int	ofw_setcurrdev(struct env_var *ev, int flags, void *value);

extern struct devsw		ofwdisk;
extern struct netif_driver	ofwnet;

int	ofwd_getunit(const char *);
int	ofwn_getunit(const char *);

ssize_t	ofw_copyin(const void *src, vm_offset_t dest, const size_t len);
ssize_t ofw_copyout(const vm_offset_t src, void *dest, const size_t len);
ssize_t ofw_readin(const int fd, vm_offset_t dest, const size_t len);

void	ofw_devsearch_init(void);
int	ofw_devsearch(const char *, char *);
int	ofw_devicetype(char *);

extern int	ofw_boot(void);
extern int	ofw_autoload(void);

void	ofw_memmap(void);
void	*ofw_alloc_heap(unsigned int);
void	ofw_release_heap(void);

struct preloaded_file;
struct file_format;

int	ofw_elf_loadfile(char *, vm_offset_t, struct preloaded_file **);
int	ofw_elf_exec(struct preloaded_file *);

extern struct file_format	ofw_elf;

extern void	reboot(void);

extern int	main(int (*openfirm)(void *));

struct ofw_reg
{
	cell_t		base;
	cell_t		size;
};
