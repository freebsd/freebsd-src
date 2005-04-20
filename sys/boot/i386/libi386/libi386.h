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
 * $FreeBSD$
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
	    int		unit;
	    void	*data;
	} bioscd;
	struct 
	{
	    int		unit;		/* XXX net layer lives over these? */
	} netif;
    } d_kind;
};

int	i386_getdev(void **vdev, const char *devspec, const char **path);
char	*i386_fmtdev(void *vdev);
int	i386_setcurrdev(struct env_var *ev, int flags, void *value);

extern struct devdesc	currdev;	/* our current device */

#define MAXDEV	31			/* maximum number of distinct devices */

/* exported devices XXX rename? */
extern struct devsw bioscd;
extern struct devsw biosdisk;
extern struct devsw pxedisk;
extern struct fs_ops pxe_fsops;

int	bc_add(int biosdev);		/* Register CD booted from. */
int	bc_getdev(struct i386_devdesc *dev);	/* return dev_t for (dev) */
int	bc_bios2unit(int biosdev);	/* xlate BIOS device -> bioscd unit */
int	bc_unit2bios(int unit);		/* xlate bioscd unit -> BIOS device */
u_int32_t	bd_getbigeom(int bunit);	/* return geometry in bootinfo format */
int	bd_bios2unit(int biosdev);		/* xlate BIOS device -> biosdisk unit */
int	bd_unit2bios(int unit);			/* xlate biosdisk unit -> BIOS device */
int	bd_getdev(struct i386_devdesc *dev);	/* return dev_t for (dev) */

ssize_t	i386_copyin(const void *src, vm_offset_t dest, const size_t len);
ssize_t	i386_copyout(const vm_offset_t src, void *dest, const size_t len);
ssize_t	i386_readin(const int fd, vm_offset_t dest, const size_t len);

struct preloaded_file;
void	bios_addsmapdata(struct preloaded_file *);
void	bios_getsmap(void);

void	bios_getmem(void);
extern u_int32_t	bios_basemem;				/* base memory in bytes */
extern u_int32_t	bios_extmem;				/* extended memory in bytes */
extern vm_offset_t	memtop;

void	biosacpi_detect();

void	gateA20(void);

int	i386_autoload(void);

int	bi_getboothowto(char *kargs);
vm_offset_t	bi_copyenv(vm_offset_t addr);
int	bi_load32(char *args, int *howtop, int *bootdevp, vm_offset_t *bip,
	    vm_offset_t *modulep, vm_offset_t *kernend);
int	bi_load64(char *args, vm_offset_t *modulep, vm_offset_t *kernend);

void	pxe_enable(void *pxeinfo);
