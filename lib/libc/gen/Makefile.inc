#	From: @(#)Makefile.inc	8.3 (Berkeley) 4/16/94
#	$Id: Makefile.inc,v 1.16 1995/05/30 05:40:08 rgrimes Exp $

# machine-independent gen sources
.PATH: ${.CURDIR}/${MACHINE}/gen ${.CURDIR}/gen

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
	wait3.c waitpid.c _thread_init.c

# *rand48 family, from 1.1.5
SRCS+=	_rand48.c drand48.c erand48.c jrand48.c lcong48.c lrand48.c \
	mrand48.c nrand48.c seed48.c srand48.c

# machine-dependent gen sources
.include "${.CURDIR}/${MACHINE}/gen/Makefile.inc"

.if (${MACHINE} == "tahoe" || ${MACHINE} == "vax")
errlst.o errlst.po:
	${CC} -S ${CFLAGS} ${.IMPSRC}
	ed - < ${.CURDIR}/${MACHINE}/:errfix errlst.s
	${AS} -o ${.TARGET} errlst.s
	rm -f errlst.s
.endif

MAN3+=	gen/alarm.3 gen/clock.3 gen/confstr.3 gen/config_open.3 \
	gen/crypt.3 gen/ctermid.3 \
	gen/daemon.3 gen/devname.3 gen/directory.3 gen/err.3 gen/exec.3 \
	gen/fnmatch.3 gen/frexp.3 gen/fts.3 gen/getbsize.3 gen/getbootfile.3 \
	gen/getcap.3 gen/getcwd.3 gen/getdiskbyname.3 gen/getdomainname.3 \
	gen/getfsent.3 gen/getgrent.3 gen/getgrouplist.3 gen/gethostname.3 \
	gen/getloadavg.3 \
	gen/getmntinfo.3 gen/getnetgrent.3 gen/getpagesize.3 gen/getpass.3 \
	gen/getpwent.3 gen/getttyent.3 gen/getvfsent.3 gen/getusershell.3 \
	gen/glob.3 \
	gen/initgroups.3 gen/isinf.3 gen/ldexp.3 \
	gen/msgctl.3 gen/msgget.3 gen/msgrcv.3 gen/msgsnd.3 \
	gen/modf.3 gen/nice.3 \
	gen/nlist.3 gen/pause.3 gen/popen.3 gen/psignal.3 gen/pwcache.3 \
	gen/raise.3 gen/rand48.3 gen/scandir.3 gen/setjmp.3 gen/setmode.3 \
	gen/siginterrupt.3 gen/signal.3 gen/sigsetops.3 gen/sleep.3 \
	gen/sysconf.3 gen/sysctl.3 gen/syslog.3 gen/tcgetpgrp.3 \
	gen/tcsendbreak.3 gen/tcsetattr.3 gen/tcsetpgrp.3 gen/time.3 \
	gen/times.3 gen/timezone.3 gen/ttyname.3 gen/tzset.3 gen/ualarm.3 \
	gen/uname.3 gen/unvis.3 gen/usleep.3 gen/utime.3 gen/valloc.3 gen/vis.3

MLINKS+=config_open.3 config_next.3 config_open.3 config_close.3 \
	config_open.3 config_skip.3
MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3 crypt.3  des_setkey.3 \
	crypt.3 des_cipher.3
MLINKS+=directory.3 closedir.3 directory.3 dirfd.3 directory.3 opendir.3 \
	directory.3 readdir.3 directory.3 rewinddir.3 directory.3 seekdir.3 \
	directory.3 telldir.3
MLINKS+=exec.3 execl.3 exec.3 execle.3 exec.3 execlp.3 exec.3 execv.3 \
	exec.3 execvp.3 exec.3 exect.3
MLINKS+=err.3 verr.3 err.3 errx.3 err.3 verrx.3 err.3 warn.3 err.3 vwarn.3 \
	err.3 warnx.3 err.3 vwarnx.3 err.3 err_set_file.3 \
	err.3 err_set_exit.3
MLINKS+=isinf.3 isnan.3
MLINKS+=getcap.3 cgetcap.3 getcap.3 cgetclose.3 getcap.3 cgetent.3 \
	getcap.3 cgetfirst.3 getcap.3 cgetmatch.3 getcap.3 cgetnext.3 \
	getcap.3 cgetnum.3 getcap.3 cgetset.3 getcap.3 cgetstr.3 \
	getcap.3 cgetustr.3
MLINKS+=getcwd.3 getwd.3
MLINKS+=getfsent.3 endfsent.3 getfsent.3 getfsfile.3 getfsent.3 getfsspec.3 \
	getfsent.3 getfstype.3 getfsent.3 setfsent.3
MLINKS+=getgrent.3 endgrent.3 getgrent.3 setgroupent.3 getgrent.3 getgrgid.3 \
	getgrent.3 getgrnam.3 getgrent.3 setgrent.3 getgrent.3 setgrfile.3
MLINKS+=gethostname.3 sethostname.3
MLINKS+=getdomainname.3 setdomainname.3
MLINKS+=getnetgrent.3 endnetgrent.3 getnetgrent.3 setnetgrent.3 \
	getnetgrent.3 innetgr.3
MLINKS+=getpwent.3 endpwent.3 getpwent.3 setpassent.3 getpwent.3 getpwnam.3 \
	getpwent.3 getpwuid.3 getpwent.3 setpwent.3 getpwent.3 setpwfile.3
MLINKS+=getttyent.3 endttyent.3 getttyent.3 getttynam.3 getttyent.3 setttyent.3
MLINKS+=getvfsent.3 getvfsbyname.3 getvfsent.3 getvfsbytype.3 \
	getvfsent.3 setvfsent.3 getvfsent.3 endvfsent.3 \
	getvfsent.3 vfsisloadable.3 getvfsent.3 vfsload.3
MLINKS+=getusershell.3 endusershell.3 getusershell.3 setusershell.3
MLINKS+=glob.3 globfree.3
MLINKS+=popen.3 pclose.3
MLINKS+=psignal.3 sys_siglist.3 psignal.3 sys_signame.3
MLINKS+=pwcache.3 user_from_uid.3 pwcache.3 group_from_gid.3
MLINKS+=rand48.3 _rand48.3 rand48.3 drand48.3 rand48.3 erand48.3 \
	rand48.3 jrand48.3 rand48.3 lcong48.3 rand48.3 lrand48.3 \
	rand48.3 mrand48.3 rand48.3 nrand48.3 rand48.3 seed48.3 \
	rand48.3 srand48.3
MLINKS+=scandir.3 alphasort.3
MLINKS+=setjmp.3 _longjmp.3 setjmp.3 _setjmp.3 setjmp.3 longjmp.3 \
	setjmp.3 longjmperr.3 setjmp.3 longjmperror.3 \
	setjmp.3 sigsetjmp.3 setjmp.3 siglongjmp.3
MLINKS+=setmode.3 getmode.3
MLINKS+=sigsetops.3 sigemptyset.3 sigsetops.3 sigfillset.3 \
	sigsetops.3 sigaddset.3 sigsetops.3 sigdelset.3 \
	sigsetops.3 sigismember.3
MLINKS+=syslog.3 closelog.3 syslog.3 openlog.3 syslog.3 setlogmask.3 \
	syslog.3 vsyslog.3
MLINKS+=tcsendbreak.3 tcdrain.3 tcsendbreak.3 tcflush.3 tcsendbreak.3 tcflow.3
MLINKS+=tcsetattr.3 tcgetattr.3 tcsetattr.3 cfsetospeed.3 \
	tcsetattr.3 cfgetospeed.3 tcsetattr.3 cfgetispeed.3 \
	tcsetattr.3 cfsetispeed.3 tcsetattr.3 cfsetspeed.3 \
	tcsetattr.3 cfmakeraw.3
MLINKS+=ttyname.3 isatty.3 ttyname.3 ttyslot.3
MLINKS+=tzset.3 tzsetwall.3
MLINKS+=vis.3 strvis.3 vis.3 strvisx.3
MLINKS+=unvis.3 strunvis.3 
MLINKS+=fts.3 fts_open.3 fts.3 fts_read.3 fts.3 fts_children.3 \
	fts.3 fts_close.3
