/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *	
 *	from: @(#)boot.c	7.3 (Berkeley) 5/4/91
 *	$Id: boot.c,v 1.2 1993/10/16 18:49:23 rgrimes Exp $
 */

#ifdef lint
char copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#include "param.h"
#include "reboot.h"
#include <a.out.h>
#include "saio.h"
#include "disklabel.h"
#include "dinode.h"

/*
 * Boot program, loaded by boot block from remaing 7.5K of boot area.
 * Sifts through disklabel and attempts to load an program image of
 * a standalone program off the disk. If keyboard is hit during load,
 * or if an error is encounter, try alternate files.
 */

char *files[] = { "386bsd", "386bsd.alt", "386bsd.old", "boot" , "vmunix", 0};
int	retry = 0;
extern struct disklabel disklabel;
extern	int bootdev, cyloffset;
static unsigned char *biosparams = (char *) 0x9ff00; /* XXX */

/*
 * Boot program... loads /boot out of filesystem indicated by arguements.
 * We assume an autoboot unless we detect a misconfiguration.
 */

main(dev, unit, off)
{
	register struct disklabel *lp;
	register int io;
	register char **bootfile = files;
	int howto = 0;
	extern int scsisn; /* XXX */


	/* are we a disk, if so look at disklabel and do things */
	lp = &disklabel;
	if (lp->d_type == DTYPE_SCSI)		/* XXX */
		off = htonl(scsisn);		/* XXX */

/*printf("cyl %x %x hd %x sect %x ", biosparams[0], biosparams[1], biosparams[2], biosparams[0xe]);
	printf("dev %x unit %x off %d\n", dev, unit, off);*/

	if (lp->d_magic == DISKMAGIC) {
	    /*
	     * Synthesize bootdev from dev, unit, type and partition
	     * information from the block 0 bootstrap.
	     * It's dirty work, but someone's got to do it.
	     * This will be used by the filesystem primatives, and
	     * drivers. Ultimately, opendev will be created corresponding
	     * to which drive to pass to top level bootstrap.
	     */
	    for (io = 0; io < lp->d_npartitions; io++) {
		int sn;

		if (lp->d_partitions[io].p_size == 0)
			continue;
		if (lp->d_type == DTYPE_SCSI)
			sn = off;
		else
			sn = off * lp->d_secpercyl;
		if (lp->d_partitions[io].p_offset == sn)
			break;
	    }

	    if (io == lp->d_npartitions) goto screwed;
            cyloffset = off;
	} else {
screwed:
		/* probably a bad or non-existant disklabel */
		io = 0 ;
		howto |= RB_SINGLE|RB_ASKNAME ;
	}

	/* construct bootdev */
	/* currently, PC has no way of booting off alternate controllers */
	bootdev = MAKEBOOTDEV(/*i_dev*/ dev, /*i_adapt*/0, /*i_ctlr*/0,
	    unit, /*i_part*/io);

	for (;;) {

/*printf("namei %s", *bootfile);*/
		io = namei(*bootfile);
		if (io > 2) {
			copyunix(io, howto, off);
		} else
			printf("File not found");

		printf(" - didn't load %s, ",*bootfile);
		if(*++bootfile == 0) bootfile = files;
		printf("will try %s\n", *bootfile);

		wait(1<<((retry++) + 10));
	}
}

/*ARGSUSED*/
copyunix(io, howto, cyloff)
	register io;
{
	struct exec x;
	int i;
	char *addr,c;
	struct dinode fil;
	int off;

	fetchi(io, &fil);
/*printf("mode %o ", fil.di_mode);*/
	i = iread(&fil, 0,  (char *)&x, sizeof x);
	off = sizeof x;
	if (i != sizeof x || x.a_magic != 0413) {
		printf("Not an executable format");
		return;
	}

	if (roundup(x.a_text, 4096) + x.a_data + x.a_bss > (unsigned)&fil) {
		printf("File too big to load");
		return;
	}

	off = 4096;
	if (iread(&fil, off, (char *)0, x.a_text) != x.a_text)
		goto shread;
	off += x.a_text;

	addr = (char *)x.a_text;
	while ((int)addr & CLOFSET)
		*addr++ = 0;
	
	if (iread(&fil, off, addr, x.a_data) != x.a_data)
		goto shread;

	addr += x.a_data;

	if (addr + x.a_bss > (unsigned) &fil) {
		printf("Warning: bss overlaps bootstrap");
		x.a_bss = (unsigned)addr - (unsigned)&fil;
	}
	bzero(addr, x.a_bss);

	/* mask high order bits corresponding to relocated system base */
	x.a_entry &= ~0xfff00000;

	/*if (scankbd()) {
		printf("Operator abort");
		kbdreset();
		return;
	}*/

	/* howto, bootdev, cyl */
	/*printf("entry %x [%x] ", x.a_entry, *(int *) x.a_entry);*/
	bcopy(0x9ff00, 0x300, 0x20); /* XXX */
	i = (*((int (*)()) x.a_entry))(howto, bootdev, off);

	if (i) printf("Program exits with %d", i) ; 
	return;
shread:
	printf("Read of file is incomplete");
	return;
}
