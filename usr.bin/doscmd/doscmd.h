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
 * $FreeBSD$
 */


#ifdef __NetBSD__
#define USE_VM86
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <errno.h>

#include <sys/signalvar.h>
#include <machine/sigframe.h>

#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/npx.h>
#ifdef USE_VM86
#include <machine/vm86.h>
#endif

#include "register.h"
#include "dos.h"
#include "callback.h"

#define drlton(a)	((islower((a)) ? toupper((a)) : (a)) - 'A')
#define drntol(a)	((a) + 'A')

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

/* ParseBuffer.c */
int	ParseBuffer(char *, char **, int);

/* bios.c */
#define	BIOSDATA	((u_char *)0x400)
extern unsigned long	rom_config;
extern int nfloppies;
extern int ndisks;
extern int nserial;
extern int nparallel;

extern volatile int	poll_cnt;
void	bios_init(void);
void	wakeup_poll(void);
void	reset_poll(void);
void	sleep_poll(void);

/* cmos.c */
extern time_t	delta_clock;

void	cmos_init(void);

/* config.c */
int	read_config(FILE *fp);

/* cpu.c */
void	cpu_init(void);
int	emu_instr(regcontext_t *);
void	int00(regcontext_t *);
void	int01(regcontext_t *);
void	int03(regcontext_t *);
void	int0d(regcontext_t *);

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
#define D_HALF		0x0001000 	/* "half-implemented" system calls */
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
#define D_EMS		0x2000000	/* EMS calls */
#define D_VIDEO		0x4000000	/* video-related activity */

#define	TTYF_ECHO	0x00000001
#define	TTYF_ECHONL	0x00000003
#define	TTYF_CTRL	0x00000004
#define	TTYF_BLOCK	0x00000008
#define	TTYF_POLL	0x00000010
#define	TTYF_REDIRECT	0x00010000	/* Cannot have 0xffff bits set */

#define	TTYF_ALL	(TTYF_ECHO | TTYF_CTRL | TTYF_REDIRECT)
#define	TTYF_BLOCKALL	(TTYF_ECHO | TTYF_CTRL | TTYF_REDIRECT | TTYF_BLOCK)

void	unknown_int2(int, int, regcontext_t *);
void	unknown_int3(int, int, int, regcontext_t *);
void	unknown_int4(int, int, int, int, regcontext_t *);
void	fatal(const char *, ...) __printflike(1, 2);
void	debug(int, const char *, ...) __printflike(2, 3);
void	dump_regs(regcontext_t *);
void	debug_set(int);
void	debug_unset(int);
u_long	debug_isset(int);

/* disktab.c */
int	map_type(int, int *, int *, int *);

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

int	_prog(char *);
void	call_on_quit(void (*)(void *), void *);
void	done(regcontext_t *, int);
void	iomap_port(int, int);
int 	open_prog(char *);
void	put_dosenv(const char *);
void 	quit(int);
int	squirrel_fd(int);

/* ems.c */
int	ems_init(void);
void	ems_entry(regcontext_t *);

/* emuint.c */
extern void	emuint(regcontext_t *REGS);

/* i386-pinsn.c */
extern int	i386dis(unsigned short, unsigned short,
    unsigned char *, char *, int);

/* int.c */
void	init_ints(void);
int	isinhardint(int);
void	softint(int);
void	hardint(int);
void	resume_interrupt(void);
void	unpend(int);
void	send_eoi(void);
void	set_eoir(int, void (*)(void *), void *);

/* int10.c */
extern void	int10(regcontext_t *);

/* int13.c */
extern int	init_hdisk(int drive, int cyl, int head, int tracksize,
			   char *file, char *boot_sector);
extern int	init_floppy(int drive, int type, char *file);
extern int	disk_fd(int drive);
extern void	make_readonly(int drive);
extern int	search_floppy(int i);
extern void	disk_bios_init(void);

/* int16.c */
void	int16(regcontext_t *);

/* int17.c */
void	int17(regcontext_t *);
void	lpt_poll(void);
void	printer_direct(int printer);
void	printer_spool(int printer, char *print_queue);
void	printer_timeout(int printer, char *time_out);

/* int1a.c */
void	int1a(regcontext_t *);

/* int2f.c */
extern void	int2f(regcontext_t *);

/* intff.c */
extern int	int2f_11(regcontext_t *REGS);
extern void	intff(regcontext_t *REGS);

/* mem.c */
extern char	*dosmem;

extern void	mem_init(void);
extern int	mem_alloc(int size, int owner, int *biggestp);
extern int	mem_adjust(int addr, int size, int *availp);
extern void	mem_free_owner(int owner);
extern void	mem_change_owner(int addr, int owner);

/* mouse.c */
void	int33(regcontext_t *);
void	mouse_init(void);

/* net.c */
void	net_init(void);

/* port.c */
void	define_input_port_handler(int, unsigned char (*)(int));
void	define_output_port_handler(int, void (*)(int, unsigned char));
void	inb(regcontext_t *, int);
unsigned char	inb_port(int);
unsigned char	inb_speaker(int);
unsigned char	inb_traceport(int);
void	init_io_port_handlers(void);
void	insb(regcontext_t *, int);
void	insx(regcontext_t *, int);
void	inx(regcontext_t *, int);
void	outb(regcontext_t *, int);
void	outb_port(int, unsigned char);
void	outb_speaker(int, unsigned char);
void	outb_traceport(int, unsigned char);
void	outsb(regcontext_t *, int);
void	outsx(regcontext_t *, int);
void	outx(regcontext_t *, int);
void	speaker_init(void);

/* setver.c */
extern void	setver(char *, short);
extern short	getver(char *);

/* signal.c */
extern struct sigframe	*saved_sigframe;
extern regcontext_t	*saved_regcontext;
extern int		saved_valid;
extern void		setsignal(int s, void (*h)(struct sigframe *));

/* timer.c */
extern void	timer_init(void);

/* trace.c */
extern int	resettrace(regcontext_t *);
extern void	tracetrap(regcontext_t *);

/* xms.c */
extern void	get_raw_extmemory_info(regcontext_t *REGS);
extern int	int2f_43(regcontext_t *REGS);
extern void	initHMA(void);
extern void	xms_init(void);
extern u_long	xms_maxsize;

/****************************** dirty below here *****************************/extern int nmice;
