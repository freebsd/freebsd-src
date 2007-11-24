/* $FreeBSD$ */

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
 * alpha fully-qualified device descriptor
 */
struct alpha_devdesc
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
	} srmdisk;
	struct 
	{
	    int		unit;		/* XXX net layer lives over these? */
	} netif;
    } d_kind;
};

extern int	alpha_getdev(void **vdev, const char *devspec, const char **path);
extern char	*alpha_fmtdev(void *vdev);
extern int	alpha_setcurrdev(struct env_var *ev, int flags, const void *value);

#define MAXDEV	31	/* maximum number of distinct devices */

typedef unsigned long physaddr_t;

/* exported devices XXX rename? */
extern struct devsw srmdisk;
extern struct netif_driver srmnet;

/* this is in startup code */
extern void		delay(int);
extern void		reboot(void);

ssize_t	alpha_copyin(const void *src, vm_offset_t dest, const size_t len);
ssize_t	alpha_copyout(const vm_offset_t src, void *dest, const size_t len);
ssize_t	alpha_readin(const int fd, vm_offset_t dest, const size_t len);

extern int		alpha_boot(void);
extern int		alpha_autoload(void);
