/*-
 * Copyright (c) 2005, Sam Leffler <sam@errno.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _SYS_FIRMWARE_H_
#define _SYS_FIRMWARE_H_
/*
 * Loadable firmware support.
 *
 * Firmware images are embedded in kernel loadable modules that can
 * be loaded on-demand or pre-loaded as desired.  Modules may contain
 * one or more firmware images that are stored as opaque data arrays
 * and registered with a unique string name.  Consumers request
 * firmware by name with held references counted to use in disallowing
 * module/data unload.
 *
 * When multiple images are stored in one module the one image is
 * treated as the master with the other images holding references
 * to it.  This means that to unload the module each dependent/subimage
 * must first have its references removed.
 */
struct firmware {
	const char	*name;		/* system-wide name */
	const void	*data;		/* location of image */
	size_t		 datasize;	/* size of image in bytes */
	unsigned int	 version;	/* version of the image */
	int		 refcnt;	/* held references */
	struct firmware *parent;	/* not null if a subimage */
	linker_file_t	 file;		/* loadable module */
};

struct firmware	*firmware_register(const char *, const void *, size_t,
    unsigned int, struct firmware *);
int		 firmware_unregister(const char *);
struct firmware *firmware_get(const char *);
#define	FIRMWARE_UNLOAD		0x0001	/* unload if unreferenced */
void		 firmware_put(struct firmware *, int);
#endif /* _SYS_FIRMWARE_H_ */
