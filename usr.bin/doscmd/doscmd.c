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
 *	BSDI doscmd.c,v 2.3 1996/04/08 19:32:30 bostic Exp
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include <machine/param.h>
#include <machine/vmparam.h>

#include <sys/proc.h>
#include <machine/sysarch.h>
#include <machine/vm86.h>

#include "doscmd.h"
#include "cwd.h"
#include "trap.h"
#include "tty.h"
#include "video.h"

/* exports */
int		capture_fd = -1;
int		dead = 0;
int		xmode = 0;
int		booting = 0;
int		raw_kbd = 0;
int		timer_disable = 0;
struct timeval	boot_time;
unsigned long	*ivec = (unsigned long *)0;

#ifndef USE_VM86
#define PRB_V86_FORMAT  0x4242

struct vconnect_area vconnect_area = {
	0,				/* Interrupt state */
	PRB_V86_FORMAT,			/* Magic number */
	{ 0, },				/* Pass through ints */
	{ 0x00000000, 0x00000000 }	/* Magic iret location */
};
#endif

/* local prototypes */
static void	setup_boot(regcontext_t *REGS);
static int	try_boot(int);
static void	setup_command(int argc, char *argv[], regcontext_t *REGS);
static FILE	*find_doscmdrc(void);
static int	do_args(int argc, char *argv[]);
static void	usage(void);
static int	open_name(char *name, char *ext);

/* Local option flags &c. */
static int	zflag = 0;

/* DOS environment emulation */
static unsigned	ecnt = 0;
static char 	*envs[256];

/* Search path and command name */
static char	*dos_path = 0;
char		cmdname[256];	/* referenced from dos.c */

static struct vm86_init_args kargs;

/* lobotomise */
int
main(int argc, char **argv)
{
#ifndef USE_VM86
    ucontext_t uc;
#else
    struct vm86_struct vm86s;
#define	sc	vm86s.substr.regs.vmsc
#endif
    regcontext_t *REGS = (regcontext_t *)&uc.uc_mcontext;
    int fd;
    int i;    
    sigset_t sigset;
    
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGIO);
    sigaddset(&sigset, SIGALRM);
    sigprocmask(SIG_BLOCK, &sigset, 0);

    init_ints();

    debugf = stderr;
    /* XXX should only be for tty mode */
    fd = open (_PATH_DEVNULL, O_RDWR);
    if (fd != 3)
	dup2 (fd, 3); /* stdaux */
    if (fd != 4)
	dup2 (fd, 4); /* stdprt */
    if (fd != 3 && fd != 4)
	close (fd);
    fd = -1;

    debug_set(0);		/* debug any D_TRAPS without intnum */

    /* perform option argument processing */
    do_args(argc, argv);
    argc -= optind;
    argv += optind;

    if (vflag && debugf == stderr) {
	debugf = stdout;
	setbuf (stdout, NULL);
    }

    initHMA();

    /* This needs to happen before the executable is loaded */
    mem_init();

#ifdef USE_VM86	
    memset(&vm86s, 0, sizeof(vm86s));
#endif

    /*
     * With no other arguments we will assume we must boot DOS
     */
    if (argc <= 0)
	booting = 1;

#if 1
    /*
     * Nominate interrupts to handle here when the kernel is 
     * performing interrupt handling.
     *
     * I would like to let INT 2F pass through as well, but I
     * need to get my hands on INT 2F:11 to do file redirection.
     */
    for (i = 0; i <= 0xff; ++i) {
	switch (i) {
	case 0x2f:
	case 0xff:
#if 1
            kargs.int_map[i >> 3] |= (1 << (i & 7));
#ifndef USE_VM86
	    vconnect_area.passthru[i >> 5] &= ~(1 << (i & 0x1f));
#else
	    vm86s.int_byuser[i >> 3] |= (1 << (i & 0x07));
#endif
#endif
	    break;
	default:
#if 1
            kargs.int_map[i >> 3] &= ~(1 << (i & 7));
#ifndef USE_VM86
	    vconnect_area.passthru[i >> 5] |= (1 << (i & 0x1f));
#else
	    vm86s.int_byuser[i >> 3] |= (1 << (i & 0x07));
#endif
#endif
	    break;
	}
    }
#endif

    if (booting) {			/* are we booting? */
	setup_boot(REGS);
    } else {				/* no, load a command */
	setup_command(argc, argv, REGS);
    }

    /* install signal handlers */
    setsignal(SIGFPE, sigfpe);		/* */
    setsignal(SIGALRM, sigalrm);	/* */
    setsignal(SIGILL, sigill);		/* */
    setsignal(SIGTRAP, sigtrap);	/* */
    setsignal(SIGUSR2, sigtrace);	/* */
    setsignal(SIGINFO, sigtrace);	/* */
#ifdef USE_VM86
    setsignal(SIGURG, sigurg);		/* entry from NetBSD vm86 */
#else
    setsignal(SIGBUS, sigbus);		/* entry from FreeBSD, BSD/OS vm86 */
#endif
	
    /* Call init functions */
    if (raw_kbd)
	console_init();
    init_io_port_handlers();
    bios_init();
    cpu_init();
    kbd_init();
    kbd_bios_init();
    video_init();
    if (xmode)
	mouse_init();
    video_bios_init();
    disk_bios_init();
    cmos_init();
    xms_init();
    dos_init();
    net_init();
    speaker_init();
    timer_init();
    /* iomap_init(); */

    gettimeofday(&boot_time, 0);

    if (zflag) for (;;) pause();	/* spin if requested */

    if (raw_kbd) {
	/*
	 * If we have a raw keyboard, and hence, video,
	 * sneak in a call to the video BIOS to reinit the
	 * the video display.
	 */
	u_long video_vector;
	static u_char video_trampoline[] = {
	    0x60,			/* pusha */
	    0xB8, 0x03, 0x00,		/* mov ax,00003h */
	    0xCD, 0x10,			/* int 010h */
	    0x61,			/* popa */
	    0xCF,			/* iret */
	};

	video_vector = insert_generic_trampoline(
	    sizeof(video_trampoline), video_trampoline);
	
	PUSH(R_FLAGS, REGS);
	PUSH(R_CS, REGS);
	PUSH(R_IP, REGS);
	PUTVEC(R_CS, R_IP, video_vector);
    }

    sigemptyset(&uc.uc_sigmask);
    sigaltstack(NULL, &uc.uc_stack);
    uc.uc_mcontext.mc_onstack = 0;

    if (tmode)
	tracetrap(REGS);

#ifndef USE_VM86
    R_EAX = (booting || raw_kbd) ? (int)&vconnect_area : -1;
    R_EFLAGS |= PSL_VM | PSL_VIF;			/* request VM86 mode */

    i386_vm86(VM86_INIT, &kargs);

    sigreturn(&uc);
    debug(D_ALWAYS,"sigreturn failed : %s\n", strerror(errno));
#else
    vm86s.cpu_type = VCPU_586;
    i386_vm86(&vm86s);
#endif

    /* shouldn't get here */
    if (vflag) dump_regs(REGS);
    fatal ("vm86 returned (no kernel support?)\n");
#undef	sc
    /* quiet -Wall */
    return 0;
}

/*
** setup_boot
**
** Setup to boot DOS
*/
static void
setup_boot(regcontext_t *REGS)
{
    FILE	*fp;		/* doscmdrc handle */
    int		fd;		/* don't close this! */

    fp = find_doscmdrc();	/* get doscmdrc */
    if (!fp) {
	fprintf(stderr, "You must have a doscmdrc to boot\n");
	quit(1);
    }

    booting = read_config(fp);			/* where to boot from? */
    fclose(fp);
    if (booting < 0) {				/* not specified */
	if ((fd = try_boot(booting = 0)) < 0)	/* try A: */
	    fd = try_boot(booting = 2);		/* try C: */
    } else {
	fd = try_boot(booting);	/* do like the man says */
    }

    if (fd < 0)
	errx(1, "Failed to boot");
    
    /* initialise registers for entry to bootblock */
    R_EFLAGS = 0x20202;
    R_CS = 0x0000;
    R_IP = 0x7c00;
    R_SS = 0x9800;
    R_SP = 0x8000 - 2;
    R_DS = 0x0000;
    R_ES = 0x0000;

    R_AX = R_BX = R_CX = R_DX = R_SI = R_DI = R_BP = 0;

#if defined(__FreeBSD__) || defined(__NetBSD__)
    /*
    ** init a few other context registers 
    */
    R_FS = 0x0000;
    R_GS = 0x0000;
#endif	
}

/*
** try_boot
**
** try to read the boot sector from the specified disk
*/
static int
try_boot(int bootdrv)
{
    int fd;

    fd = disk_fd(bootdrv);
    if (fd < 0)	{			/* can we boot it? */
	debug(D_DISK, "Cannot boot from %c\n", drntol(bootdrv));
	return -1;
    }
    
    /* read bootblock */
    if (read(fd, (char *)0x7c00, 512) != 512) {
        debug(D_DISK, "Short read on boot block from %c:\n", drntol(bootdrv));
	return -1;
    }
    
    return fd;
}

/*
** setup_command
**
** Setup to run a single command and emulate DOS
*/
static void
setup_command(int argc, char *argv[], regcontext_t *REGS)
{
    FILE	*fp;
    u_short	param[7] = {0, 0, 0, 0, 0, 0, 0};
    const char	*p;
    char	prog[1024];
    char	buffer[PATH_MAX];
    unsigned	i;
    int		fd;
    
    fp = find_doscmdrc();		/* dig up a doscmdrc */
    if (fp) {
	read_config(fp);		/* load config for non-boot mode */
	fclose(fp);
    }
    
    if (argc <= 0)			/* need some arguments */
	usage();

    /* look for a working directory  XXX ??? */
    if (dos_getcwd(drlton('C')) == NULL) {
	
	/* try to get our current directory, use '/' if desperate */
	p = getcwd(buffer, sizeof(buffer));
	if (!p || !*p) p = getenv("PWD");
	if (!p || !*p) p = "/";
	init_path(drlton('C'), "/", p);

	/* look for PATH= already set, learn from it if possible */
	for (i = 0; i < ecnt; ++i) {
	    if (!strncmp(envs[i], "PATH=", 5)) {
		dos_path = envs[i] + 5;
		break;
	    }
	}
	/* no PATH in DOS environment? put current directory there*/
	if (i >= ecnt) {
	    static char path[256];
	    snprintf(path, sizeof(path), "PATH=C:%s", dos_getcwd(drlton('C')));
	    put_dosenv(path);
	    dos_path = envs[ecnt-1] + 5;
	}
    }

    /* add a COMSPEC if required */
    for (i = 0; i < ecnt; ++i) {
	if (!strncmp(envs[i], "COMSPEC=", 8))
	    break;
    }
    if (i >= ecnt)
	put_dosenv("COMSPEC=C:\\COMMAND.COM");

    /* look for PATH already set, learn from it if possible */
    for (i = 0; i < ecnt; ++i) {
	if (!strncmp(envs[i], "PATH=", 5)) {
	    dos_path = envs[i] + 5;
	    break;
	}
    }
    /* No PATH, default to c:\ */
    if (i >= ecnt) {
	put_dosenv("PATH=C:\\");
	dos_path = envs[ecnt-1] + 5;
    }

    /* if no PROMPT, default to 'DOS>' */
    for (i = 0; i < ecnt; ++i) {
	if (!strncmp(envs[i], "PROMPT=", 7))
	    break;
    }
    if (i >= ecnt)
	put_dosenv("PROMPT=DOS> ");

    /* terminate environment */
    envs[ecnt] = 0;

    /* XXX ??? */
    if (dos_getcwd(drlton('R')) == NULL)
	init_path(drlton('R'), "/", 0);

    /* get program name */
    strncpy(prog, *argv++, sizeof(prog) -1);
    prog[sizeof(prog) -1] = '\0';

    /* try to open program */
    if ((fd = open_prog(prog)) < 0) {
	fprintf (stderr, "%s: command not found\n", prog);
	quit(1);
    }
    
    /* load program */
    load_command(REGS, 1, fd, cmdname, param, argv, envs);
    close(fd);
}

/*
** find_doscmdrc
**
** Try to find a doscmdrc file
*/
static FILE *
find_doscmdrc(void)
{
    FILE	*fp;
    char 	buffer[4096];
    
    if ((fp = fopen(".doscmdrc", "r")) == NULL) {
	struct passwd *pwd = getpwuid(geteuid());
	if (pwd) {
	    snprintf(buffer, sizeof(buffer), "%s/.doscmdrc", pwd->pw_dir);
	    fp = fopen(buffer, "r");
	}
	if (!fp) {
	    char *home = getenv("HOME");
            if (home) {
	        snprintf(buffer, sizeof(buffer), "%s/.doscmdrc", home);
                fp = fopen(buffer, "r");
	    }
        }
	if (!fp)
	    fp = fopen("/etc/doscmdrc", "r");
    }
    return(fp);
}

/*
** do_args
**
** commandline argument processing
*/
static int
do_args(int argc, char *argv[])
{
    int 	i,c,p;
    FILE	*fp;
    char 	*col;

    while ((c = getopt(argc, argv, "234AbCc:Dd:EGHIi:kLMOo:Pp:RrS:TtU:vVxXYz")) != -1) {
	switch (c) {
	case '2':
	    debug_flags |= D_TRAPS2;
	    break;
	case '3':
	    debug_flags |= D_TRAPS3;
	    break;
	case '4':
	    debug_flags |= D_DEBUGIN;
	    break;
	case 'A':
	    debug_flags |= D_TRAPS | D_ITRAPS;
	    for (c = 0; c < 256; ++c)
		debug_set(c);
	    break;
	case 'b':
	    booting = 1;
	    break;
	case 'C':
	    debug_flags |= D_DOSCALL;
	    break;
	case 'c':
	    if ((capture_fd = creat(optarg, 0666)) < 0) {
		perror(optarg);
		quit(1);
	    }
	    break;
	case 'D':
	    debug_flags |= D_DISK | D_FILE_OPS;
	    break;
	case 'd':
	    if ((fp = fopen(optarg, "w")) != 0) {
		debugf = fp;
		setbuf (fp, NULL);
	    } else
		perror(optarg);
	    break;
	case 'E':
	    debug_flags |= D_EXEC;
	    break;
	case 'G':
	    debug_flags |= D_VIDEO;
	    break;
	case 'H':
	    debug_flags |= D_HALF;
	    break;
	case 'I':
	    debug_flags |= D_ITRAPS;
	    for (c = 0; c < 256; ++c)
		debug_set(c);
	    break;
	case 'i':
	    i = 1;
	    if ((col = strchr(optarg, ':')) != 0) {
		*col++ = 0;
		i = strtol(col, 0, 0);
	    }
	    p = strtol(optarg, 0, 0);
	    iomap_port(p, i);

	    while (i-- > 0)
		define_input_port_handler(p++, inb_traceport);
	    break;
	case 'k':
            kargs.debug = 1;
	    break;
	case 'L':
	    debug_flags |= D_PRINTER;
	    break;
	case 'M':
	    debug_flags |= D_MEMORY;
	    break;
	case 'O':
	    debugf = stdout;
	    setbuf (stdout, NULL);
	    break;
	case 'o':
	    i = 1;
	    if ((col = strchr(optarg, ':')) != 0) {
		*col++ = 0;
		i = strtol(col, 0, 0);
	    }
	    p = strtol(optarg, 0, 0);
	    iomap_port(p, i);

	    while (i-- > 0)
		define_output_port_handler(p++, outb_traceport);
	    break;
	case 'P':
	    debug_flags |= D_PORT;
	    break;
	case 'p':
	    i = 1;
	    if ((col = strchr(optarg, ':')) != 0) {
		*col++ = 0;
		i = strtol(col, 0, 0);
	    }
	    p = strtol(optarg, 0, 0);
	    iomap_port(p, i);

	    while (i-- > 0) {
		define_input_port_handler(p++, inb_port);
		define_output_port_handler(p++, outb_port);
	    }
	    break;
	case 'R':
	    debug_flags |= D_REDIR;
	    break;
	case 'r':
	    raw_kbd = 1;
	    break;
	case 'S':
	    debug_flags |= D_TRAPS | D_ITRAPS;
	    debug_set(strtol(optarg, 0, 0));
	    break;
	case 'T':
	    timer_disable = 1;
	    break;
	case 't':
	    tmode = 1;
	    break;
	case 'U':
	    debug_unset(strtol(optarg, 0, 0));
	    break;
	case 'V':
	    vflag = 1;
	    break;
	case 'v':
	    debug_flags |= D_TRAPS | D_ITRAPS | D_HALF | 0xff;
	    break;
	case 'X':
	    debug_flags |= D_XMS;
	    break;
	case 'x':
#ifdef NO_X
	    fatal("X11 support not compiled in.\n");
#endif
	    xmode = 1;
	    break;
	case 'Y':
	    debug_flags |= D_EMS;
	    break;
	case 'z':
	    zflag = 1;
	    break;
	default:
	    usage ();
	}
    }
    return(optind);
}

/*
** Very helpful 8(
*/
void
usage (void)
{
	fprintf (stderr, "usage: doscmd cmd args...\n");
	quit (1);
}

/*
** look up a DOS command name
**
** XXX ordering is wrong!
*/
static int
open_name(char *name, char *ext)
{
    int fd;
    char *p = name + strlen(name);
    char *q;

    *ext = 0;

    q = strrchr(name, '/');
    if (q)
	q++;
    else
	q = name;

    if (!strchr(q, '.')) {
	strcpy(ext, ".exe");
	strcpy(p, ".exe");

	if ((fd = open (name, O_RDONLY)) >= 0)
	    return (fd);

	strcpy(ext, ".com");
	strcpy(p, ".com");

	if ((fd = open (name, O_RDONLY)) >= 0)
	    return (fd);
    } else {
	if ((fd = open (name, O_RDONLY)) >= 0)
	    return (fd);
    }

    return (-1);
}

/*
** look up a DOS command, search the path as well.
*/
int
open_prog(char *name)
{
    int fd;
    char fullname[1024], tmppath[1024];
    char *p;
    char *e;
    char ext[5];
    int error;
    int drive;
    char *path;

    if (strpbrk(name, ":/\\")) {
	error = translate_filename(name, fullname, &drive);
	if (error)
	    return (-1);

	fd = open_name(fullname, ext);

	strcpy(cmdname, name);
	if (*ext)
	    strcat(cmdname, ext);
	return (fd);
    }

    path = dos_path;

    while (*path) {
	p = path;
	while (*p && *p != ';')
	    ++p;

	memcpy(tmppath, path, p - path);
	e = tmppath + (p - path);
	*e++ = '\\';
	strcpy(e, name);

	path = *p ? p + 1 : p;

	error = translate_filename(tmppath, fullname, &drive);
	if (error)
	    continue;

	fd = open_name(fullname, ext);

	if (fd >= 0) {
	    strcpy(cmdname, tmppath);
	    if (*ext)
		strcat(cmdname, ext);
	    return (fd);
	}
    }

    return (-1);
}

/*
** append a value to the DOS environment
*/
void
put_dosenv(const char *value)
{
    if (ecnt < sizeof(envs)/sizeof(envs[0])) {
	if ((envs[ecnt++] = strdup(value)) == NULL) {
	    perror("put_dosenv");
	    quit(1);
	}
    } else {
	fprintf(stderr, "Environment full, ignoring %s\n", value);
    }
}

/*
** replicate a fd up at the top of the range
*/
int
squirrel_fd(int fd)
{
    int sfd = sysconf(_SC_OPEN_MAX);
    struct stat sb;

    do {
	errno = 0;
	fstat(--sfd, &sb);
    } while (sfd > 0 && errno != EBADF);

    if (errno == EBADF && dup2(fd, sfd) >= 0) {
	close(fd);
	return(sfd);
    }
    return(fd);
}

/*
** Exit-time stuff
*/

/*
** Going away time
**
** XXX belongs somewhere else perhaps
*/
void
done(regcontext_t *REGS, int val)
{
    if (curpsp < 2) {
	if (xmode) {
	    const char *m;

	    tty_move(24, 0);
	    for (m = "END OF PROGRAM"; *m; ++m)
		tty_write(*m, 0x8400);

	    for (m = "(PRESS <CTRL-ALT> ANY MOUSE BUTTON TO exit)"; *m; ++m)
		tty_write(*m, 0x0900);
	    tty_move(-1, -1);
	    for (;;)
		tty_pause();
	} else {
	    quit(val);
	}
    }
    exec_return(REGS, val);
}

typedef struct COQ {
    void	(*func)(void *);
    void	*arg;
    struct COQ	*next;
} COQ;

COQ *coq = 0;

void
quit(int status)
{
    while (coq) {
	COQ *c = coq;
	coq = coq->next;
	c->func(c->arg);
    }
    if (!xmode)		/* XXX not for bootmode */
	puts("\n");
    exit(status);
}

void
call_on_quit(void (*func)(void *), void *arg)
{
    COQ *c = (COQ *)malloc(sizeof(COQ));
    if (!c) {
	perror("call_on_quit");
	quit(1);
    }
    c->func = func;
    c->arg = arg;
    c->next = coq;
    coq = c;
}

struct io_range {
	u_int start;
	u_int length;
	int enable;
};

/* This is commented out as it is never called.  Turn it back on if needed.
 */
#if COMMENTED_OUT
static void
iomap_init(void)
{
        int i;
        struct io_range io[] = {
#if 0
                { 0x200, 0x200, 1 },            /* 0x200 - 0x400 */
                { 0x1c80, 2, 1 },               /* 0x1c80 - 0x1c81 */
                { 0x2c80, 2, 1 },               /* 0x2c80 - 0x2c81 */
                { 0x3c80, 2, 1 },               /* 0x3c80 - 0x3c81 */
                { 0x378,  8, 1 },               /* 0x378 - 0x37F */
                { 0x3c4,  2, 1 },               /* 0x3c4 - 0x3c5 */
                { 0x3c5,  2, 1 },               /* 0x3ce - 0x3cf */
#else
		{ 0x0, 0x10000, 1 },		/* entire i/o space */
#endif
                { 0, 0, 0 }
        };
	
        for (i = 0; io[i].length; i++)
                if (i386_set_ioperm(io[i].start, io[i].length, io[i].enable) < 0)
                        err(1, "i386_set_ioperm");
}
#endif

/* This is used to map in only the specified port range, instead of all
   the ports or only certain port ranges.
 */
void
iomap_port(int port, int count)
{
    if (i386_set_ioperm(port, count, 1) < 0)
	err(1, "i386_set_ioperm");

    debug(D_PORT,"mapped I/O port: port=%#x count=%d\n", port, count);
}
