/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI doscmd.h,v 2.3 1996/04/08 19:32:32 bostic Exp
 *
 * $Id: doscmd.h,v 1.1 1997/08/09 01:43:09 dyson Exp $
 */


#ifdef __NetBSD__
#define USE_VM86
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/npx.h>
#ifdef USE_VM86
#include <machine/vm86.h>
#endif

#include "register.h"
#include "dos.h"
#include "callback.h"
#include "cwd.h"


/* 
** assorted hardware/scope constants 
*/

#define	MAX_AVAIL_SEG	0xa000

#define MAXPORT		0x400

#define N_PARALS_MAX	3
#define N_COMS_MAX	4	/* DOS restriction (sigh) */

struct vconnect_area {
        int     int_state;
        int     magic;                  /* 0x4242 -> PRB format */
        u_long  passthru[256>>5];       /* bitmap of INTs to handle */
        u_long  magiciret[2];           /* Bounds of "magic" IRET */
};
extern struct vconnect_area vconnect_area;
#define IntState vconnect_area.int_state


/* debug.c */
extern int	vflag;
extern int	tmode;
extern FILE	*debugf;
extern int	debug_flags;

/* Lower 8 bits are int number */
#define D_ALWAYS 	0x0000100	/* always emit this message */
#define D_TRAPS 	0x0000200	/* trap-related activity */
#define D_FILE_OPS	0x0000400	/* file-related activity */
#define D_MEMORY	0x0000800	/* memory-related activity */
#define D_HALF		0x0001000 	/* for "half-implemented" system calls */
#define	D_FLOAT		0x0002000	/* ??? */
#define	D_DISK		0x0004000	/* disk (not file) operations */
#define	D_TRAPS2	0x0008000
#define	D_PORT		0x0010000	/* port accesses */
#define	D_EXEC		0x0020000
#define	D_ITRAPS	0x0040000
#define	D_REDIR		0x0080000	/* redirector functions */
#define	D_PRINTER	0x0100000
#define	D_TRAPS3	0x0200000
#define	D_DEBUGIN	0x0400000
#define D_DOSCALL	0x0800000	/* MS-DOS function results */
#define D_XMS		0x1000000	/* XMS calls */

#define	TTYF_ECHO	0x00000001
#define	TTYF_ECHONL	0x00000003
#define	TTYF_CTRL	0x00000004
#define	TTYF_BLOCK	0x00000008
#define	TTYF_POLL	0x00000010
#define	TTYF_REDIRECT	0x00010000	/* Cannot have 0xffff bits set */

#define	TTYF_ALL	(TTYF_ECHO | TTYF_CTRL | TTYF_REDIRECT)
#define	TTYF_BLOCKALL	(TTYF_ECHO | TTYF_CTRL | TTYF_REDIRECT | TTYF_BLOCK)

extern void	unknown_int2(int, int, regcontext_t *REGS);
extern void	unknown_int3(int, int, int, regcontext_t *REGS);
extern void	unknown_int4(int, int, int, int, regcontext_t *REGS);
extern void	fatal (char *fmt, ...);
extern void	debug (int flags, char *fmt, ...);
extern void	dump_regs(regcontext_t *REGS);
extern void	debug_set(int x);
extern void	debug_unset(int x);
extern u_long	debug_isset(int x);

/* doscmd.c */
extern int		capture_fd;
extern int		dead;
extern int		xmode;
extern int		booting;
extern int		raw_kbd;
extern int		timer_disable;
extern char		cmdname[];
extern struct timeval	boot_time;
extern unsigned long	*ivec;

extern int		open_prog(char *name);
extern void		done(regcontext_t *REGS, int val);
extern void 		quit(int);
extern void		call_on_quit(void (*)(void *), void *);

/* signal.c */
extern struct sigframe	*saved_sigframe;
extern regcontext_t	*saved_regcontext;
extern int		saved_valid;
extern void		setsignal(int s, void (*h)(struct sigframe *));

/* cmos.c */
extern time_t	delta_clock;

extern void	cmos_init(void);

/* config.c */
extern int	read_config(FILE *fp);

/* tty.c */
extern char	*xfont;

/* setver.c */
extern void	setver(char *, short);
extern short	getver(char *);

/* mem.c */
extern char	*dosmem;

extern void	mem_init(void);
extern int	mem_alloc(int size, int owner, int *biggestp);
extern int	mem_adjust(int addr, int size, int *availp);
extern void	mem_free_owner(int owner);
extern void	mem_change_owner(int addr, int owner);


/* intff.c */
extern int	int2f_11(regcontext_t *REGS);
extern void	intff(regcontext_t *REGS);

/* trap.c */
extern void	fake_int(regcontext_t *REGS, int);
extern void	sigtrap(struct sigframe *sf);
extern void	sigtrace(struct sigframe *sf);
extern void	sigalrm(struct sigframe *sf);
extern void	sigill(struct sigframe *sf);
extern void	sigfpe(struct sigframe *sf);
extern void	breakpoint(struct sigframe *sf);
#ifdef USE_VM86
extern void	sigurg(struct sigframe *sf);
#else
extern void	sigbus(struct sigframe *sf);
#endif

/* int.c */
extern void	softint(int intnum);
extern void	hardint(int intnum);

extern void	delay_interrupt(int intnum, void (*func)(int));
extern void	resume_interrupt(void);


/* bios.c */
#define	BIOSDATA	((u_char *)0x400)
extern unsigned long	rom_config;
extern int nfloppies;
extern int ndisks;
extern int nserial;
extern int nparallel;

extern volatile int	poll_cnt;
extern void		wakeup_poll(void);
extern void		reset_poll(void);
extern void		sleep_poll(void);

/* int13.c */
extern int	init_hdisk(int drive, int cyl, int head, int tracksize,
			   char *file, char *boot_sector);
extern int	init_floppy(int drive, int type, char *file);
extern int	disk_fd(int drive);
extern void	make_readonly(int drive);
extern int	search_floppy(int i);
extern void	disk_bios_init(void);

/* int17.c */
extern void	lpt_poll(void);
extern void	printer_direct(int printer);
extern void	printer_spool(int printer, char *print_queue);
extern void	printer_timeout(int printer, char *time_out);

/* xms.c */
extern int	int2f_43(regcontext_t *REGS);
extern void	get_raw_extmemory_info(regcontext_t *REGS);
extern void	initHMA(void);
extern u_long	xms_maxsize;

/****************************** dirty below here ******************************/

extern u_long	pending[];		/* pending interrupts */
extern int	n_pending;

u_char	*VREG;

extern int nmice;

extern int redirect0;
extern int redirect1; 
extern int redirect2;
extern int kbd_fd;
extern int jmp_okay;



void put_dosenv(char *value);


/* TTY subsystem   XXX rewrite! */
int tty_eread(REGISTERS, int, int);
void tty_write(int, int);
void tty_rwrite(int, int, int);
void tty_move(int, int);
void tty_report(int *, int *);
void tty_flush();
void tty_index();
void tty_pause();
int tty_peek(REGISTERS, int);
int tty_state();
void tty_scroll(int, int, int, int, int, int);
void tty_rscroll(int, int, int, int, int, int);
int tty_char(int, int);
void video_setborder(int);

void outb_traceport(int, unsigned char);
unsigned char inb_traceport(int);

