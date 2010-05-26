/*-
 * Copyright (C) 2005 M. Warner Losh. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _PC98_INCLUDE_MD_VAR_H_
#define _PC98_INCLUDE_MD_VAR_H_

#include <i386/md_var.h>

/*
 * PC98 machines are based on Intel CPUs.  Some add-in boards offer
 * different CPUs than came with the processor.  These CPUs sometimes
 * require additional flushing before and/or after DMAs.
 */
extern	int	need_pre_dma_flush;
extern	int	need_post_dma_flush;

/*
 * The ad driver maps the IDE disk's actual geometry to the firmware's
 * notion of geometry.  However, PC98 machines need to do something
 * different sometimes, so override the hook so we can do so.
 */
struct disk;
void	pc98_ata_disk_firmware_geom_adjust(struct disk *);
#define	ata_disk_firmware_geom_adjust(disk)				\
	pc98_ata_disk_firmware_geom_adjust(disk)

#endif /* !_PC98_INCLUDE_MD_VAR_H_ */
