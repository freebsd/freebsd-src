/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *	$Id: procfs.h,v 1.2 1994/03/07 11:39:04 davidg Exp $
 */
#ifndef _SYS_PROCFS_H_
#define _SYS_PROCFS_H_

struct procmap {	/* Simplified VM maps */
	vm_offset_t	vaddr;
	vm_size_t	size;
	vm_offset_t	offset;
	vm_prot_t	prot;
};

struct vmfd {		/* Mapped file descriptor */
	vm_offset_t	vaddr;	/* IN */
	int		fd;	/* OUT */
};

enum vmet { PFS_END, PFS_PRMAP, PFS_OBJINFO };

struct objinfo {
	int	ref_count;	/* ref_count for object */
	int	rss_map;	/* rss within the map entry */
	int	persist;	/* persist flag */
	int	internal;	/* internal flag */
	vm_offset_t offset;	/* offset into object */
	vm_offset_t size;	/* total size of object */
	struct vm_page pages[0];	/* pages */
};

struct procvminfo	{	/* Full proc VM info */
	enum	vmet entrytype;	/* type of entry */
	union	{
		struct procmap pm;	/* proc map entry */
		struct objinfo oi;	/* object info entry */
	} u;
};

typedef	unsigned long	fltset_t;

#define PIOCGPINFO	_IOR('P', 0, struct kinfo_proc)
#define PIOCGSIGSET	_IOR('P', 1, sigset_t)
#define PIOCSSIGSET	_IOW('P', 2, sigset_t)
#define PIOCGFLTSET	_IOR('P', 3, fltset_t)
#define PIOCSFLTSET	_IOW('P', 4, fltset_t)
#define PIOCGNMAP	_IOR('P', 5, int)
#define PIOCGMAP	_IO ('P', 6)
#define PIOCGMAPFD	_IOWR('P', 7, struct vmfd)
#define PIOCGNVMINFO	_IOR('P', 8, int)
#define PIOCGVMINFO	_IO ('P', 9)

#endif /* _SYS_PROCFS_H_ */
