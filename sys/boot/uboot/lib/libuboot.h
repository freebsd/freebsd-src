/*-
 * Copyright (C) 2000 Benno Rice.
 * Copyright (C) 2007 Semihalf, Rafal Jaworowski <raj@semihalf.com>
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
 *
 * $FreeBSD$
 */

struct uboot_devdesc
{
	struct devsw	*d_dev;
	int		d_type;
	int		d_unit;
	union {
		struct {
			void	*data;
			int	slice;
			int	partition;
			off_t	offset;
		} disk;
	} d_kind;
};

#define d_disk d_kind.disk

/*
 * Default network packet alignment in memory
 */
#define	PKTALIGN	32

int uboot_getdev(void **vdev, const char *devspec, const char **path);
char *uboot_fmtdev(void *vdev);
int uboot_setcurrdev(struct env_var *ev, int flags, const void *value);

extern int devs_no;
extern struct netif_driver uboot_net;
extern struct devsw uboot_storage;

void *uboot_vm_translate(vm_offset_t);
ssize_t	uboot_copyin(const void *src, vm_offset_t dest, const size_t len);
ssize_t	uboot_copyout(const vm_offset_t src, void *dest, const size_t len);
ssize_t	uboot_readin(const int fd, vm_offset_t dest, const size_t len);
extern int uboot_autoload(void);

struct preloaded_file;
struct file_format;

extern struct file_format uboot_elf;

void reboot(void);
