/* $FreeBSD: src/sys/boot/arc/include/libarc.h,v 1.2 1999/08/28 00:39:34 peter Exp $ */

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * ARC fully-qualified device descriptor
 */
struct arc_devdesc
{
    struct devsw	*d_dev;
    int			d_type;
#define DEVT_NONE	0
#define DEVT_DISK	1
#define DEVT_NET	2
    union 
    {
	struct 
	{
	    int		unit;
	    int		slice;
	    int		partition;
	} arcdisk;
	struct 
	{
	    int		unit;		/* XXX net layer lives over these? */
	} netif;
    } d_kind;
};

extern int	arc_getdev(void **vdev, const char *devspec, const char **path);
extern char	*arc_fmtdev(void *vdev);
extern int	arc_setcurrdev(struct env_var *ev, int flags, void *value);

#define MAXDEV	31	/* maximum number of distinct devices */

typedef unsigned long physaddr_t;

/* exported devices XXX rename? */
extern struct devsw arcdisk;
extern struct netif_driver arcnet;

/* this is in startup code */
extern void		delay(int);
extern void		reboot(void);

extern int		arc_copyin(void *src, vm_offset_t dest, size_t len);
extern int		arc_copyout(vm_offset_t src, void *dest, size_t len);
extern int		arc_readin(int fd, vm_offset_t dest, size_t len);

extern int		arc_boot(void);
extern int		arc_autoload(void);
