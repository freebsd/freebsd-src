/*-
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
 */

#include "openfirm.h"
#include <readin.h>

#define DEVT_OFDISK	1001

struct ofw_devdesc {
	struct devdesc			dd;
	union {
		struct {
			ihandle_t	d_handle;	
			char		d_path[256];
		};
		struct {
			uint64_t	pool_guid;
			uint64_t	root_guid;
		};
	};
};

extern int	ofw_getdev(void **vdev, const char *devspec, const char **path);

extern struct devsw		ofwdisk;
extern struct devsw		ofw_netdev;
extern struct netif_driver	ofwnet;

int	ofwn_getunit(const char *);

ssize_t	ofw_copyin(const void *src, vm_offset_t dest, const size_t len);
ssize_t ofw_copyout(const vm_offset_t src, void *dest, const size_t len);
ssize_t ofw_readin(readin_handle_t fd, vm_offset_t dest, const size_t len);

extern int	ofw_boot(void);
extern int	ofw_autoload(void);

void	ofw_memmap(int);

phandle_t ofw_path_to_handle(const char *ofwpath, const char *want_type, const char **path);
int ofw_common_parsedev(struct devdesc **dev, const char *devspec, const char **path,
    const char *ofwtype);

struct preloaded_file;
struct file_format;

extern void	reboot(void);

struct ofw_reg
{
	cell_t		base;
	cell_t		size;
};

struct ofw_reg2
{
	cell_t		base_hi;
	cell_t		base_lo;
	cell_t		size;
};

extern int (*openfirmware)(void *);
