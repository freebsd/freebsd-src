/* $NetBSD: prom.h,v 1.7 1997/04/06 08:47:37 cgd Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Keith Bostic, Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD$
 */

#ifndef	ASSEMBLER
struct prom_vec {
	u_int64_t	routine;
	void		*routine_arg;
};

/* The return value from a prom call. */
typedef union {
	struct {
		u_int64_t
			retval	: 32,		/* return value. */
			unit	: 8,
			mbz	: 8,
			error	: 13,
			status	: 3;
	} u;
	u_int64_t bits;
} prom_return_t;

#ifdef STANDALONE
int	getchar __P((void));
int	prom_open __P((char *, int));
void	putchar __P((int));
#endif

void	prom_halt __P((int)) __attribute__((__noreturn__));
int	prom_getenv __P((int, char *, int));

#endif

/* Prom operation values. */
#define	PROM_R_CLOSE		0x11
#define	PROM_R_GETC		0x01
#define	PROM_R_GETENV		0x22
#define	PROM_R_OPEN		0x10
#define	PROM_R_PUTS		0x02
#define	PROM_R_READ		0x13
#define	PROM_R_WRITE		0x14

/* Environment variable values. */
#define	PROM_E_BOOTED_DEV	0x4
#define	PROM_E_BOOTED_FILE	0x6
#define	PROM_E_BOOTED_OSFLAGS	0x8
#define	PROM_E_TTY_DEV		0xf

/*
 * There have to be stub routines to do the copying that ensures that the
 * PROM doesn't get called with an address larger than 32 bits.  Calls that
 * either don't need to copy anything, or don't need the copy because it's
 * already being done elsewhere, are defined here.
 */
#define	prom_close(chan)						\
	prom_dispatch(PROM_R_CLOSE, chan, 0, 0, 0)
#define	prom_read(chan, len, buf, blkno)				\
	prom_dispatch(PROM_R_READ, chan, len, (u_int64_t)buf, blkno)
#define	prom_write(chan, len, buf, blkno)				\
	prom_dispatch(PROM_R_WRITE, chan, len, (u_int64_t)buf, blkno)
#define	prom_putstr(chan, str, len)					\
	prom_dispatch(PROM_R_PUTS, chan, (u_int64_t)str, len, 0)
#define	prom_getc(chan)							\
	prom_dispatch(PROM_R_GETC, chan, 0, 0, 0)
#define prom_getenv_disp(id, buf, len)					\
	prom_dispatch(PROM_R_GETENV, id, (u_int64_t)buf, len, 0)

#ifndef ASSEMBLER
#ifdef _KERNEL
void	promcnattach __P((int));
void	promcnputc __P((dev_t, int));
int	promcngetc __P((dev_t));
int	promcncheckc __P((dev_t));

u_int64_t	prom_dispatch __P((u_int64_t, u_int64_t, u_int64_t, u_int64_t,
		    u_int64_t));
void		init_bootstrap_console __P((void));
#endif /* _KERNEL */
#endif /* ASSEMBLER */
