/**
 ** Copyright (c) 1995
 **      Michael Smith, msmith@freebsd.org.  All rights reserved.
 **
 ** This code contains a module marked :

 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 Jordan K. Hubbard
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 *
 * Many additional changes by Bruce Evans
 *
 * This code is derived from software contributed by the
 * University of California Berkeley, Jordan K. Hubbard,
 * David Greenman and Bruce Evans.

 ** As such, it contains code subject to the above copyrights.
 ** The module and its copyright can be found below.
 ** 
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer as
 **    the first lines of this file unmodified.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. All advertising materials mentioning features or use of this software
 **    must display the following acknowledgment:
 **      This product includes software developed by Michael Smith.
 ** 4. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY MICHAEL SMITH ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL MICHAEL SMITH BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **
 ** $FreeBSD$
 **/

#ifndef _I386_MACHINE_UC_DEVICE_H
#define _I386_MACHINE_UC_DEVICE_H

/*
 * Per device structure.  This just happens to resemble the old isa_device
 * but that is by accident.  It is NOT the same.
 */
struct uc_device {
	int	id_id;		/* device id */
	char	*id_name;	/* device name */
	int	id_iobase;	/* base i/o address */
	u_int	id_irq;		/* interrupt request */
	int	id_drq;		/* DMA request */
	caddr_t id_maddr;	/* physical i/o memory address on bus (if any)*/
	int	id_msize;	/* size of i/o memory */
	int	id_unit;	/* unit number */
	int	id_flags;	/* flags */
	int	id_enabled;	/* is device enabled */
	struct uc_device *id_next; /* used in uc_devlist in userconfig() */
};

#endif
