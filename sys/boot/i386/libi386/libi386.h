/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 *	$Id: libi386.h,v 1.3 1998/09/03 02:10:09 msmith Exp $
 */


/*
 * i386 fully-qualified device descriptor.
 * Note, this must match the 'struct devdesc' declaration
 * in bootstrap.h.
 */
struct i386_devdesc
{
    struct devsw	*d_dev;
    int			d_type;
    union 
    {
	struct 
	{
	    int		unit;
	    int		slice;
	    int		partition;
	    void	*data;
	} biosdisk;
	struct 
	{
	    int		unit;		/* XXX net layer lives over these? */
	} netif;
    } d_kind;
};

extern int	i386_getdev(void **vdev, char *devspec, char **path);
extern char	*i386_fmtdev(void *vdev);
extern int	i386_setcurrdev(struct env_var *ev, int flags, void *value);

extern struct devdesc	currdev;	/* our current device */

#define MAXDEV	31			/* maximum number of distinct devices */

/* exported devices XXX rename? */
extern struct devsw biosdisk;

/* from crt module */
extern void		vpbcopy(void *src, vm_offset_t dest, size_t size);
extern void		pvbcopy(vm_offset_t src, void *dest, size_t size);
extern void		pbzero(vm_offset_t dest, size_t size);
extern vm_offset_t	vtophys(void *addr);

extern int		i386_copyin(void *src, vm_offset_t dest, size_t len);
extern int		i386_copyout(vm_offset_t src, void *dest, size_t len);
extern int		i386_readin(int fd, vm_offset_t dest, size_t len);

extern void		startprog(vm_offset_t entry, int argc, u_int32_t *argv, vm_offset_t stack);

extern int		getbasemem(void);
extern int		getextmem(void);

extern void		reboot(void);
extern void		gateA20(void);

extern int		i386_autoload(void);

extern int		bi_getboothowto(char *kargs);
extern vm_offset_t	bi_copyenv(vm_offset_t addr);
