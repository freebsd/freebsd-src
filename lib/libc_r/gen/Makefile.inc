#	From: @(#)Makefile.inc	8.3 (Berkeley) 4/16/94
#	$Id: Makefile.inc,v 1.16 1995/05/30 05:40:08 rgrimes Exp $

# machine-independent gen sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/gen ${.CURDIR}/../libc/gen

SRCS+=	alarm.c assert.c clock.c closedir.c config.c confstr.c crypt.c \
	ctermid.c daemon.c devname.c disklabel.c err.c errlst.c \
	exec.c fnmatch.c frexp.c fstab.c fts.c getbootfile.c getbsize.c \
	getcap.c getcwd.c getdomainname.c getgrent.c getgrouplist.c \
	gethostname.c getloadavg.c getlogin.c getmntinfo.c getnetgrent.c \
	getosreldate.c getpagesize.c getpass.c getpwent.c getttyent.c \
	getusershell.c getvfsent.c glob.c initgroups.c isatty.c msgctl.c \
	msgget.c msgrcv.c msgsnd.c nice.c nlist.c ntp_gettime.c opendir.c \
	pause.c popen.c psignal.c pwcache.c raise.c readdir.c rewinddir.c \
	scandir.c seekdir.c semconfig.c semctl.c semget.c semop.c \
	setdomainname.c sethostname.c setjmperr.c setmode.c shmat.c \
	shmctl.c shmdt.c shmget.c siginterrupt.c siglist.c signal.c \
	sigsetops.c sleep.c sysconf.c sysctl.c syslog.c \
	telldir.c termios.c time.c times.c timezone.c ttyname.c ttyslot.c \
	ualarm.c uname.c unvis.c usleep.c utime.c valloc.c vis.c wait.c \
	wait3.c waitpid.c

# *rand48 family, from 1.1.5
SRCS+=	_rand48.c drand48.c erand48.c jrand48.c lcong48.c lrand48.c \
	mrand48.c nrand48.c seed48.c srand48.c

# machine-dependent gen sources
.include "${.CURDIR}/${MACHINE}/gen/Makefile.inc"
