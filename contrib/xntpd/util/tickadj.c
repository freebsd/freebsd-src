/* tickadj.c,v 3.1 1993/07/06 01:11:05 jbj Exp
 * tickadj - read, and possibly modify, the kernel `tick' and
 *	     `tickadj' variables, as well as `dosynctodr'.  Note that
 *	     this operates on the running kernel only.  I'd like to be
 *	     able to read and write the binary as well, but haven't
 *	     mastered this yet.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>

#if defined(SYS_AUX3) || defined(SYS_AUX2)
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <a.out.h>
#include <sys/var.h>
#else
#include <nlist.h>
#endif

#include "ntp_io.h"
#include "ntp_stdlib.h"

#ifdef RS6000
#undef hz
#endif /* RS6000 */

#if defined(SOLARIS)||defined(RS6000)||defined(SYS_SINIXM)
#if !defined(_SC_CLK_TCK)
#include <unistd.h>
#endif
#endif

#ifdef SYS_PTX
#define L_SET SEEK_SET
#endif

#define	KMEM	"/dev/kmem"
#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

char *progname;
int debug;

int dokmem = 1;
int writetickadj = 0;
int writeopttickadj = 0;
int unsetdosync = 0;
int writetick = 0;
int quiet = 0;
int setnoprintf = 0;

char *kmem = KMEM;
char *kernel = NULL;
char *file = NULL;
int   fd  = -1;

static	char *	getoffsets	P((char *, unsigned long *, unsigned long *, unsigned long *, unsigned long *));
static	int	openfile	P((char *, int));
static	void	writevar	P((int, unsigned long, int));
static	void	readvar		P((int, unsigned long, int *));

/*
 * main - parse arguments and handle options
 */
void
main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int errflg = 0;
	extern int ntp_optind;
	extern char *ntp_optarg;
	unsigned long tickadj_offset;
	unsigned long tick_offset;
	unsigned long dosync_offset;
	unsigned long noprintf_offset;
	int tickadj;
	int tick;
	int dosynctodr;
	int noprintf;
	int hz, hz_hundredths;
	int recommend_tickadj;
	long tmp;
	int openfile();
	char *getoffsets();
	void readvar();
	void writevar();

	progname = argv[0];
	while ((c = ntp_getopt(argc, argv, "a:Adkqpst:")) != EOF)
		switch (c) {
		case 'd':
			++debug;
			break;
		case 'k':
			dokmem = 1;
			break;
		case 'p':
			setnoprintf = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'a':
			writetickadj = atoi(ntp_optarg);
			if (writetickadj <= 0) {
				(void) fprintf(stderr,
				    "%s: unlikely value for tickadj: %s\n",
				    progname, ntp_optarg);
				errflg++;
			}
			break;
		case 'A':
			writeopttickadj = 1;
			break;
		case 's':
			unsetdosync = 1;
			break;
		case 't':
			writetick = atoi(ntp_optarg);
			if (writetick <= 0) {
				(void) fprintf(stderr,
				    "%s: unlikely value for tick: %s\n",
				    progname, ntp_optarg);
				errflg++;
			}
			break;
		default:
			errflg++;
			break;
		}
	if (errflg || ntp_optind != argc) {
		(void) fprintf(stderr,
		    "usage: %s [-Aqsp] [-a newadj] [-t newtick]\n", progname);
		exit(2);
	}
	kernel = getoffsets(kernel, &tick_offset,
	    &tickadj_offset, &dosync_offset, &noprintf_offset);

	if (debug) {
		(void) printf("tick offset = %lu\n", tick_offset);
		(void) printf("tickadj offset = %lu\n", tickadj_offset);
		(void) printf("dosynctodr offset = %lu\n", dosync_offset);
		(void) printf("noprintf offset = %lu\n", noprintf_offset);
	}

	if (setnoprintf && (noprintf_offset == 0)) {
		(void) fprintf(stderr, 
			       "No noprintf kernal variable\n");
		errflg++;
	}

	if (unsetdosync && (dosync_offset == 0)) {
		(void) fprintf(stderr, 
			       "No dosynctodr kernal variable\n");
		errflg++;
	}
	
	if (writeopttickadj && (tickadj_offset == 0)) {
		(void) fprintf(stderr, 
			       "No tickadj kernal variable\n");
		errflg++;
	}

	if (writetick && (tick_offset == 0)) {
		(void) fprintf(stderr, 
			       "No tick kernal variable\n");
		errflg++;
	}
	

	if (tickadj_offset != 0)
		readvar(fd, tickadj_offset, &tickadj);

#if defined(SOLARIS)||defined(RS6000)||defined(SYS_SINIXM)
	tick = 1000000/sysconf(_SC_CLK_TCK);
#else
	readvar(fd, tick_offset, &tick);
#endif

	if (dosync_offset != 0)
		readvar(fd, dosync_offset, &dosynctodr);
	if (noprintf_offset != 0)
		readvar(fd, noprintf_offset, &noprintf);
	(void) close(fd);

	if (unsetdosync && dosync_offset == 0) {
		(void) fprintf(stderr,
		    "%s: can't find dosynctodr in namelist\n", progname);
		exit(1);
	}

	if (!quiet) {
		(void) printf("tick = %d us",tick);
		if (tickadj_offset != 0)
			(void) printf(", tickadj = %d us", tickadj);
		if (dosync_offset != 0)
			(void) printf(", dosynctodr is %s", dosynctodr ? "on" : "off");
		(void) printf("\n");
		if (noprintf_offset != 0)
			(void) printf("kernel level printf's: %s\n", noprintf ? "off" : "on");
	}

	if (tick <= 0) {
		(void) fprintf(stderr, "%s: the value of tick is silly!\n",
			progname);
		exit(1);
	}

	hz = (int)(1000000L / (long)tick);
	hz_hundredths = (int)((100000000L / (long)tick) - ((long)hz * 100L));
	if (!quiet)
		(void) printf("calculated hz = %d.%02d Hz\n", hz,
		    hz_hundredths);
	tmp = (long) tick * 500L;
	recommend_tickadj = (int)(tmp / 1000000L);
	if (tmp % 1000000L > 0)
		recommend_tickadj++;

#if defined(RS6000)
	if (recommend_tickadj < 40) recommend_tickadj = 40;
#endif

	if ((!quiet) && (tickadj_offset != 0))
		(void) printf("recommended value of tickadj = %d us\n",
		    recommend_tickadj);
	
	if (writetickadj == 0 && !writeopttickadj &&
	    !unsetdosync && writetick == 0 && !setnoprintf)
		exit(errflg ? 1 : 0);

	if (writetickadj == 0 && writeopttickadj)
		writetickadj = recommend_tickadj;

	fd = openfile(file, O_WRONLY);

	if (setnoprintf && (dosync_offset != 0)) {
		if (!quiet) {
			(void) fprintf(stderr, "setting noprintf: ");
			(void) fflush(stderr);
		}
		writevar(fd, noprintf_offset, 1);
		if (!quiet)
			(void) fprintf(stderr, "done!\n");
	}

	if ((writetick > 0) && (tick_offset != 0)) {
		if (!quiet) {
			(void) fprintf(stderr, "writing tick, value %d: ",
			    writetick);
			(void) fflush(stderr);
		}
		writevar(fd, tick_offset, writetick);
		if (!quiet)
			(void) fprintf(stderr, "done!\n");
	}

	if ((writetickadj > 0) && (tickadj_offset != 0)) {
		if (!quiet) {
			(void) fprintf(stderr, "writing tickadj, value %d: ",
			    writetickadj);
			(void) fflush(stderr);
		}
		writevar(fd, tickadj_offset, writetickadj);
		if (!quiet)
			(void) fprintf(stderr, "done!\n");
	}

	if (unsetdosync && (dosync_offset != 0)) {
		if (!quiet) {
			(void) fprintf(stderr, "zeroing dosynctodr: ");
			(void) fflush(stderr);
		}
		writevar(fd, dosync_offset, 0);
		if (!quiet)
			(void) fprintf(stderr, "done!\n");
	}
	(void) close(fd);
	exit(errflg ? 1 : 0);
}

/*
 * getoffsets - read the magic offsets from the specified file
 */
static char *
getoffsets(filex, tick_off, tickadj_off, dosync_off, noprintf_off)
	char *filex;
	unsigned long *tick_off;
	unsigned long *tickadj_off;
	unsigned long *dosync_off;
	unsigned long *noprintf_off;
{
	char **kname;

#if defined(SYS_AUX3) || defined(SYS_AUX2)
#define X_TICKADJ       0
#define X_V             1
#define X_TICK          2
#define X_DEF	
	static struct nlist	nl[4];
#endif

#ifdef	NeXT
#define	X_TICKADJ	0
#define	X_TICK		1
#define	X_DOSYNC	2
#define	X_NOPRINTF	3
#define X_DEF	
	static struct nlist nl[] =
	{	{{"_tickadj"}},
		{{"_tick"}},
		{{"_dosynctodr"}},
		{{"_noprintf"}},
		{{""}},
	};
#endif

#if	defined(SYS_SVR4) || defined(SYS_PTX)
#define	X_TICKADJ	0
#define	X_TICK		1
#define	X_DOSYNC	2
#define	X_NOPRINTF	3
#define X_DEF	
	static struct nlist nl[] =
	{	{{"tickadj"}},
		{{"tick"}},
		{{"doresettodr"}},
		{{"noprintf"}},
		{{""}},
	};
#endif /* SYS_SVR4 */

#if defined(SOLARIS)||defined(RS6000)||defined(SYS_SINIXM)
#ifndef SOLARIS_HRTIME
#define	X_TICKADJ	0
#endif
#define	X_DOSYNC	1
#define	X_NOPRINTF	2
#define X_DEF	
	static struct nlist nl[] =
	{	{"tickadj"},
		{"dosynctodr"},
		{"noprintf"},
		{""},
	};

#if  defined(RS6000)
	int i;
#endif
#endif

#if !defined(X_DEF)
#define	X_TICKADJ	0
#define	X_TICK		1
#define	X_DOSYNC	2
#define	X_NOPRINTF	3
	static struct nlist nl[] =
	{	{"_tickadj"},
		{"_tick"},
		{"_dosynctodr"},
		{"_noprintf"},
		{""},
	};
#endif
	static char *kernels[] = {
		"/vmunix",
		"/unix",
		"/mach",
		"/kernel/unix",
		"/386bsd",
		"/netbsd",
		NULL
	};
	struct stat stbuf;

#if defined(SYS_AUX3) || defined(SYS_AUX2)
	strcpy (nl[X_TICKADJ].n_name, "tickadj");
	strcpy (nl[X_V].n_name, "v");
	strcpy (nl[X_TICK].n_name, "tick");
	nl[3].n_name[0] = '\0';
#endif

	for (kname = kernels; *kname != NULL; kname++) {
		if (stat(*kname, &stbuf) == -1)
			continue;
		if (nlist(*kname, nl) >= 0) 
			break;
	}
	if (*kname == NULL) {
		(void) fprintf(stderr,
		    "%s: nlist fails: can't find/read /vmunix or /unix\n",
		    progname);
		exit(1);
	}

	if (dokmem)
		file = kmem;
	else
		file = kernel;

	fd = openfile(file, O_RDONLY);
#if defined(RS6000)
	/*
	 * Go one more round of indirection.
	 */
	for (i=0; i<(sizeof(nl)/sizeof(struct nlist)); i++) {
		if (nl[i].n_value) {
		   	readvar(fd, nl[i].n_value, &nl[i].n_value);
		}
	}
#endif
	*tickadj_off  = 0;
	*tick_off     = 0;
	*dosync_off   = 0;
	*noprintf_off = 0;

#if defined(X_TICKADJ)
	*tickadj_off = nl[X_TICKADJ].n_value;
#endif

#if defined(X_TICK)
	*tick_off = nl[X_TICK].n_value;
#endif

#if defined(X_DOSYNC)
	*dosync_off = nl[X_DOSYNC].n_value;
#endif

#if defined(X_NOPRINTF)
	*noprintf_off = nl[X_NOPRINTF].n_value;
#endif
	return *kname;
}

#undef X_TICKADJ
#undef X_TICK
#undef X_DOSYNC
#undef X_NOPRINTF


/*
 * openfile - open the file, check for errors
 */
static int
openfile(name, mode)
	char *name;
	int mode;
{
	int fd;

	fd = open(name, mode);
	if (fd < 0) {
		(void) fprintf(stderr, "%s: open %s: ", progname, name);
		perror("");
		exit(1);
	}
	return fd;
}


/*
 * writevar - write a variable into the file
 */
static void
writevar(fd, off, var)
	int fd;
	unsigned long off;
	int var;
{
	
	if (lseek(fd, off, L_SET) == -1) {
		(void) fprintf(stderr, "%s: lseek fails: ", progname);
		perror("");
		exit(1);
	}
	if (write(fd, (char *)&var, sizeof(int)) != sizeof(int)) {
		(void) fprintf(stderr, "%s: write fails: ", progname);
		perror("");
		exit(1);
	}
}


/*
 * readvar - read a variable from the file
 */
static void
readvar(fd, off, var)
	int fd;
	unsigned long off;
	int *var;
{
	int i;
	
	if (lseek(fd, off, L_SET) == -1) {
		(void) fprintf(stderr, "%s: lseek fails: ", progname);
		perror("");
		exit(1);
	}
	i = read(fd, (char *)var, sizeof(int));
	if (i < 0) {
		(void) fprintf(stderr, "%s: read fails: ", progname);
		perror("");
		exit(1);
	}
	if (i != sizeof(int)) {
		(void) fprintf(stderr, "%s: read expected %d, got %d\n",
		    progname, sizeof(int), i);
		exit(1);
	}
}
