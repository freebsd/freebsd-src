/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef	__FreeBSD__
# ifndef __FreeBSD_cc_version
#  include <osreldate.h>
# else
#  if __FreeBSD_cc_version < 430000
#   include <osreldate.h>
#  endif
# endif
#endif
#if defined(__sgi) && (IRIX > 602)
# include <sys/ptimers.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#if !defined(__SVR4) && !defined(__GNUC__)
#include <strings.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <sys/time.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_nat.h"
#include "ip_state.h"
#include "ipf.h"
#include "ipl.h"

#if !defined(lint)
static const char sccsid[] = "@(#)ipf.c	1.23 6/5/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipf.c,v 2.10.2.23 2003/06/27 14:39:13 darrenr Exp $";
#endif

#if	SOLARIS
static	void	blockunknown __P((void));
#endif
#if !defined(__SVR4) && defined(__GNUC__)
extern	char	*index __P((const char *, int));
#endif

extern	char	*optarg;
extern	int	optind;

void	frsync __P((void));
void	zerostats __P((void));
int	main __P((int, char *[]));

int	opts = 0;
int	use_inet6 = 0;

static	int	fd = -1;

static	void	procfile __P((char *, char *)), flushfilter __P((char *));
static	int	set_state __P((u_int));
static	void	showstats __P((friostat_t *));
static	void	packetlogon __P((char *)), swapactive __P((void));
static	int	opendevice __P((char *));
static	void	closedevice __P((void));
static	char	*getline __P((char *, size_t, FILE *, int *));
static	char	*ipfname = IPL_NAME;
static	void	usage __P((char *));
static	int	showversion __P((void));
static	int	get_flags __P((int *));


#if SOLARIS
# define	OPTS	"6AdDEf:F:Il:noPrsUvVyzZ"
#else
# define	OPTS	"6AdDEf:F:Il:noPrsvVyzZ"
#endif

static void usage(name)
char *name;
{
	fprintf(stderr, "usage: %s [-%s] %s %s %s\n", name, OPTS,
		"[-l block|pass|nomatch]", "[-F i|o|a|s|S]", "[-f filename]");
	exit(1);
}


int main(argc,argv)
int argc;
char *argv[];
{
	int c;

	if (argc < 2)
		usage(argv[0]);

	while ((c = getopt(argc, argv, OPTS)) != -1) {
		switch (c)
		{
		case '6' :
			use_inet6 = 1;
			break;
		case 'A' :
			opts &= ~OPT_INACTIVE;
			break;
		case 'E' :
			if (set_state((u_int)1))
				exit(1);
			break;
		case 'D' :
			if (set_state((u_int)0))
				exit(1);
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'f' :
			procfile(argv[0], optarg);
			break;
		case 'F' :
			flushfilter(optarg);
			break;
		case 'I' :
			opts |= OPT_INACTIVE;
			break;
		case 'l' :
			packetlogon(optarg);
			break;
		case 'n' :
			opts |= OPT_DONOTHING;
			break;
		case 'o' :
			break;
		case 'P' :
			ipfname = IPL_AUTH;
			break;
		case 'r' :
			opts |= OPT_REMOVE;
			break;
		case 's' :
			swapactive();
			break;
#if SOLARIS
		case 'U' :
			blockunknown();
			break;
#endif
		case 'v' :
			opts += OPT_VERBOSE;
			break;
		case 'V' :
			if (showversion())
				exit(1);
			break;
		case 'y' :
			frsync();
			break;
		case 'z' :
			opts |= OPT_ZERORULEST;
			break;
		case 'Z' :
			zerostats();
			break;
		case '?' :
		default :
			usage(argv[0]);
			break;
		}
	}

	if (optind < 2)
		usage(argv[0]);

	if (fd != -1)
		(void) close(fd);

	exit(0);
	/* NOTREACHED */
}


static int opendevice(ipfdev)
char *ipfdev;
{
	if (opts & OPT_DONOTHING)
		return 0;

	if (!ipfdev)
		ipfdev = ipfname;

	/*
	 * shouldn't we really be testing for fd < 0 here and below?
	 */

	if (fd != -1)
		return 0;

	if ((fd = open(ipfdev, O_RDWR)) == -1) {
		if ((fd = open(ipfdev, O_RDONLY)) == -1) {
			perror("open device");
			if (errno == ENODEV)
				fprintf(stderr, "IPFilter enabled?\n");
			return -1;
		}
	}

	return 0;
}


static void closedevice()
{
	if (fd != -1)
		close(fd);
	fd = -1;
}


/*
 * Return codes:
 *	0	Success
 *	!0	Failure (and an error message has already been printed)
 */
static	int	get_flags(i)
int	*i;
{

	if (opts & OPT_DONOTHING)
		return 0;

	if (opendevice(ipfname) < 0)
		return -1;

	if (ioctl(fd, SIOCGETFF, i) == -1) {
		perror("SIOCGETFF");
		return -1;
	}
	return 0;
}


static	int	set_state(enable)
u_int	enable;
{
	if (opts & OPT_DONOTHING)
		return 0;

	if (opendevice(ipfname))
		return -1;

	if (ioctl(fd, SIOCFRENB, &enable) == -1) {
		if (errno == EBUSY)
			/* Not really an error */
			fprintf(stderr,
				"IP Filter: already initialized\n");
		else {
			perror("SIOCFRENB");
			return -1;
		}
	}
	return 0;
}

static	void	procfile(name, file)
char	*name, *file;
{
	FILE	*fp;
	char	line[513], *s;
	struct	frentry	*fr;
	u_int	add, del;
	int     linenum = 0;
	int	parsestatus;

	if (opendevice(ipfname) == -1)
		exit(1);

	if (opts & OPT_INACTIVE) {
		add = SIOCADIFR;
		del = SIOCRMIFR;
	} else {
		add = SIOCADAFR;
		del = SIOCRMAFR;
	}
	if (opts & OPT_DEBUG)
		printf("add %x del %x\n", add, del);

	initparse();

	if (!strcmp(file, "-"))
		fp = stdin;
	else if (!(fp = fopen(file, "r"))) {
		fprintf(stderr, "%s: fopen(%s) failed: %s\n", name, file,
			STRERROR(errno));
		exit(1);
	}

	while (getline(line, sizeof(line), fp, &linenum)) {
		/*
		 * treat CR as EOL.  LF is converted to NUL by getline().
		 */
		if ((s = index(line, '\r')))
			*s = '\0';
		/*
		 * # is comment marker, everything after is a ignored
		 */
		if ((s = index(line, '#')))
			*s = '\0';

		if (!*line)
			continue;

		if (opts & OPT_VERBOSE)
			(void)fprintf(stderr, "[%s]\n", line);

		parsestatus = 1;
		fr = parse(line, linenum, &parsestatus);
		(void)fflush(stdout);

		if (parsestatus != 0) {
			fprintf(stderr, "%s: %s: %s error (%d), quitting\n",
			    name, file,
			    ((parsestatus < 0)? "parse": "internal"),
			    parsestatus);
			exit(1);
		}

		if (fr) {
			if (opts & OPT_ZERORULEST)
				add = SIOCZRLST;
			else if (opts & OPT_INACTIVE)
				add = (u_int)fr->fr_hits ? SIOCINIFR :
							   SIOCADIFR;
			else
				add = (u_int)fr->fr_hits ? SIOCINAFR :
							   SIOCADAFR;
			if (fr->fr_hits)
				fr->fr_hits--;
			if (fr && (opts & OPT_VERBOSE))
				printfr(fr);
			if (fr && (opts & OPT_OUTQUE))
				fr->fr_flags |= FR_OUTQUE;

			if (opts & OPT_DEBUG)
				binprint(fr);

			if ((opts & OPT_ZERORULEST) &&
			    !(opts & OPT_DONOTHING)) {
				if (ioctl(fd, add, &fr) == -1) {
					fprintf(stderr, "%d:", linenum);
					perror("ioctl(SIOCZRLST)");
					exit(1);
				} else {
#ifdef	USE_QUAD_T
					printf("hits %qd bytes %qd ",
						(long long)fr->fr_hits,
						(long long)fr->fr_bytes);
#else
					printf("hits %ld bytes %ld ",
						fr->fr_hits, fr->fr_bytes);
#endif
					printfr(fr);
				}
			} else if ((opts & OPT_REMOVE) &&
				   !(opts & OPT_DONOTHING)) {
				if (ioctl(fd, del, &fr) == -1) {
					fprintf(stderr, "%d:", linenum);
					perror("ioctl(delete rule)");
					exit(1);
				}
			} else if (!(opts & OPT_DONOTHING)) {
				if (ioctl(fd, add, &fr) == -1) {
					fprintf(stderr, "%d:", linenum);
					perror("ioctl(add/insert rule)");
					exit(1);
				}
			}
		}
	}
	if (ferror(fp) || !feof(fp)) {
		fprintf(stderr, "%s: %s: file error or line too long\n",
		    name, file);
		exit(1);
	}
	(void)fclose(fp);
}

/*
 * Similar to fgets(3) but can handle '\\' and NL is converted to NUL.
 * Returns NULL if error occurred, EOF encounterd or input line is too long.
 */
static char *getline(str, size, file, linenum)
register char	*str;
size_t	size;
FILE	*file;
int	*linenum;
{
	char *p;
	int s, len;

	do {
		for (p = str, s = size;; p += (len - 1), s -= (len - 1)) {
			/*
			 * if an error occurred, EOF was encounterd, or there
			 * was no room to put NUL, return NULL.
			 */
			if (fgets(p, s, file) == NULL)
				return (NULL);
			len = strlen(p);
			if (p[len - 1] != '\n') {
				p[len] = '\0';
				break;
			}
			(*linenum)++;
			p[len - 1] = '\0';
			if (len < 2 || p[len - 2] != '\\')
				break;
			else
				/*
				 * Convert '\\' to a space so words don't
				 * run together
				 */
				p[len - 2] = ' ';
		}
	} while (*str == '\0');
	return (str);
}


static void packetlogon(opt)
char	*opt;
{
	int	flag;

	if (get_flags(&flag))
		exit(1);

	if (flag != 0) {
		if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE)
			printf("log flag is currently %#x\n", flag);
	}

	flag &= ~(FF_LOGPASS|FF_LOGNOMATCH|FF_LOGBLOCK);

	if (index(opt, 'p')) {
		flag |= FF_LOGPASS;
		if (opts & OPT_VERBOSE)
			printf("set log flag: pass\n");
	}
	if (index(opt, 'm') && (*opt == 'n' || *opt == 'N')) {
		flag |= FF_LOGNOMATCH;
		if (opts & OPT_VERBOSE)
			printf("set log flag: nomatch\n");
	}
	if (index(opt, 'b') || index(opt, 'd')) {
		flag |= FF_LOGBLOCK;
		if (opts & OPT_VERBOSE)
			printf("set log flag: block\n");
	}

	if (opendevice(ipfname) == -1) {
		exit(1);
	}

	if (!(opts & OPT_DONOTHING)) {
		if (ioctl(fd, SIOCSETFF, &flag) != 0) {
			perror("ioctl(SIOCSETFF)");
			exit(1);
		}
	}

	if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
		/*
		 * Even though the ioctls above succeeded, it
		 * is possible that a calling script/program
		 * relies on the following verbose mode string.
		 * Thus, we still take an error exit if get_flags
		 * fails here.
		 */
		if (get_flags(&flag))
			exit(1);
		printf("log flag is now %#x\n", flag);
	}
}


static	void	flushfilter(arg)
char	*arg;
{
	int	fl = 0, rem;

	if (!arg || !*arg) {
		fprintf(stderr, "-F: no filter specified\n");
		exit(1);
	}

	if (!strcmp(arg, "s") || !strcmp(arg, "S")) {
		if (*arg == 'S')
			fl = 0;
		else
			fl = 1;
		rem = fl;

		closedevice();

		if (opendevice(IPL_STATE) == -1) {
			exit(1);
		}

		if (!(opts & OPT_DONOTHING)) {
			if (use_inet6) {
				if (ioctl(fd, SIOCIPFL6, &fl) == -1) {
					perror("ioctl(SIOCIPFL6)");
					exit(1);
				}
			} else {
				if (ioctl(fd, SIOCIPFFL, &fl) == -1) {
					perror("ioctl(SIOCIPFFL)");
					exit(1);
				}
			}
		}
		if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
			printf("remove flags %s (%d)\n", arg, rem);
			printf("removed %d filter rules\n", fl);
		}
		closedevice();
		return;
	}
	if (strchr(arg, 'i') || strchr(arg, 'I'))
		fl = FR_INQUE;
	if (strchr(arg, 'o') || strchr(arg, 'O'))
		fl = FR_OUTQUE;
	if (strchr(arg, 'a') || strchr(arg, 'A'))
		fl = FR_OUTQUE|FR_INQUE;
	fl |= (opts & FR_INACTIVE);
	rem = fl;

	if (opendevice(ipfname) == -1) {
		exit(1);
	}

	if (!(opts & OPT_DONOTHING)) {
		if (use_inet6) {
			if (ioctl(fd, SIOCIPFL6, &fl) == -1) {
				perror("ioctl(SIOCIPFL6)");
				exit(1);
			}
		} else {
			if (ioctl(fd, SIOCIPFFL, &fl) == -1) {
				perror("ioctl(SIOCIPFFL)");
				exit(1);
			}
		}
	}
	if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
		printf("remove flags %s%s (%d)\n", (rem & FR_INQUE) ? "I" : "",
			(rem & FR_OUTQUE) ? "O" : "", rem);
		printf("removed %d filter rules\n", fl);
	}
	return;
}


static void swapactive()
{
	int in = 2;

	if (opendevice(ipfname) == -1) {
		exit(1);
	}


	if (!(opts & OPT_DONOTHING)) {
		if (ioctl(fd, SIOCSWAPA, &in) == -1) {
			perror("ioctl(SIOCSWAPA)");
			exit(1);
		}
	}
	printf("Set %d now inactive\n", in);
}


void frsync()
{
	int frsyn = 0;

	if (opendevice(ipfname) == -1)
		exit(1);

	if (!(opts & OPT_DONOTHING)) {
		if (ioctl(fd, SIOCFRSYN, &frsyn) == -1) {
			perror("SIOCFRSYN");
			exit(1);
		}
	}
	printf("filter sync'd\n");
}


void zerostats()
{
	friostat_t	fio;
	friostat_t	*fiop = &fio;

	if (opendevice(ipfname) == -1)
		exit(1);

	if (!(opts & OPT_DONOTHING)) {
		if (ioctl(fd, SIOCFRZST, &fiop) == -1) {
			perror("ioctl(SIOCFRZST)");
			exit(-1);
		}
		showstats(fiop);
	}

}


/*
 * Read the kernel stats for packets blocked and passed
 */
static void showstats(fp)
friostat_t	*fp;
{
#if SOLARIS
	printf("dropped packets:\tin %lu\tout %lu\n",
			fp->f_st[0].fr_drop, fp->f_st[1].fr_drop);
	printf("non-ip packets:\t\tin %lu\tout %lu\n",
			fp->f_st[0].fr_notip, fp->f_st[1].fr_notip);
	printf("   bad packets:\t\tin %lu\tout %lu\n",
			fp->f_st[0].fr_bad, fp->f_st[1].fr_bad);
#endif
	printf(" input packets:\t\tblocked %lu passed %lu nomatch %lu",
			fp->f_st[0].fr_block, fp->f_st[0].fr_pass,
			fp->f_st[0].fr_nom);
	printf(" counted %lu\n", fp->f_st[0].fr_acct);
	printf("output packets:\t\tblocked %lu passed %lu nomatch %lu",
			fp->f_st[1].fr_block, fp->f_st[1].fr_pass,
			fp->f_st[1].fr_nom);
	printf(" counted %lu\n", fp->f_st[0].fr_acct);
	printf(" input packets logged:\tblocked %lu passed %lu\n",
			fp->f_st[0].fr_bpkl, fp->f_st[0].fr_ppkl);
	printf("output packets logged:\tblocked %lu passed %lu\n",
			fp->f_st[1].fr_bpkl, fp->f_st[1].fr_ppkl);
	printf(" packets logged:\tinput %lu-%lu output %lu-%lu\n",
			fp->f_st[0].fr_pkl, fp->f_st[0].fr_skip,
			fp->f_st[1].fr_pkl, fp->f_st[1].fr_skip);
}


#if SOLARIS
static void blockunknown()
{
	int	flag;

	if (opendevice(ipfname) == -1)
		exit(1);

	if (get_flags(&flag))
		exit(1);

	if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE)
		printf("log flag is currently %#x\n", flag);

	flag ^= FF_BLOCKNONIP;

	if (opendevice(ipfname) == -1)
		exit(1);

	if (!(opts & OPT_DONOTHING)) {
		if (ioctl(fd, SIOCSETFF, &flag))
			perror("ioctl(SIOCSETFF)");
	}

	if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
		if (ioctl(fd, SIOCGETFF, &flag))
			perror("ioctl(SIOCGETFF)");

		printf("log flag is now %#x\n", flag);
	}
}
#endif


/*
 * nonzero return value means caller should exit with error
 */
static int showversion()
{
	struct friostat fio;
	struct friostat *fiop=&fio;
	int flags, vfd;
	char *s;

	printf("ipf: %s (%d)\n", IPL_VERSION, (int)sizeof(frentry_t));

	if ((vfd = open(ipfname, O_RDONLY)) == -1) {
		perror("open device");
		return 1;
	}

	if (ioctl(vfd, SIOCGETFS, &fiop)) {
		perror("ioctl(SIOCGETFS)");
		close(vfd);
		return 1;
	}
	close(vfd);

	printf("Kernel: %-*.*s\n", (int)sizeof(fio.f_version),
		(int)sizeof(fio.f_version), fio.f_version);
	printf("Running: %s\n", fio.f_running ? "yes" : "no");

	if (get_flags(&flags)) {
		return 1;
	}
	printf("Log Flags: %#x = ", flags);
	s = "";
	if (flags & FF_LOGPASS) {
		printf("pass");
		s = ", ";
	}
	if (flags & FF_LOGBLOCK) {
		printf("%sblock", s);
		s = ", ";
	}
	if (flags & FF_LOGNOMATCH) {
		printf("%snomatch", s);
		s = ", ";
	}
	if (flags & FF_BLOCKNONIP) {
		printf("%snonip", s);
		s = ", ";
	}
	if (!*s)
		printf("none set");
	putchar('\n');

	printf("Default: ");
	if (fio.f_defpass & FR_PASS)
		s = "pass";
	else if (fio.f_defpass & FR_BLOCK)
		s = "block";
	else
		s = "nomatch -> block";
	printf("%s all, Logging: %savailable\n", s, fio.f_logging ? "" : "un");
	printf("Active list: %d\n", fio.f_active);

	return 0;
}
