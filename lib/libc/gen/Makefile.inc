#	@(#)Makefile.inc	5.21 (Berkeley) 5/24/91

# gen sources
.PATH: ${.CURDIR}/${MACHINE}/gen ${.CURDIR}/gen

SRCS+=	alarm.c assert.c clock.c crypt.c ctermid.c ctime.c ctype_.c \
	directory.c difftime.c disklabel.c err.c errlst.c exec.c \
	fnmatch.c frexp.c fstab.c fts.c getcap.c getcwd.c getgrent.c \
	getlogin.c getmntinfo.c getpass.c getpwent.c getsubopt.c getttyent.c \
	getusershell.c glob.c infinity.c initgroups.c insque.c isatty.c \
	isinf.c mktemp.c msgctl.c msgget.c msgsnd.c msgrcv.c \
	nice.c nlist.c pause.c popen.c psignal.c raise.c scandir.c \
	semconfig.c semctl.c semget.c semop.c \
	setjmperr.c setmode.c \
	shmat.c shmctl.c shmdt.c shmget.c \
	siginterrupt.c siglist.c signal.c sigsetops.c sleep.c syslog.c \
	termios.c time.c times.c timezone.c ttyname.c ttyslot.c \
	ualarm.c unvis.c usleep.c utime.c valloc.c vis.c \
	wait.c wait3.c waitpid.c

.if   (${MACHINE} == "hp300")
SRCS+=	_setjmp.s alloca.s fabs.s ldexp.s modf.s setjmp.s
SRCS+=	adddf3.s addsf3.s ashlsi3.s ashrsi3.s cmpdf2.s cmpsf2.s divdf3.s \
	divsf3.s divsi3.s extendsfdf2.s fixdfsi.s fixunsdfsi.s \
	floatsidf.s lshlsi3.s lshrsi3.s modsi3.s muldf3.s mulsf3.s mulsi3.s \
	negdf2.s negsf2.s saveregs.c subdf3.s subsf3.s truncdfsf2.s udivsi3.s \
	umodsi3.s umulsi3.s
.elif (${MACHINE} == "i386")
SRCS+=	_setjmp.S alloca.S fabs.S ldexp.c modf.S setjmp.S sigsetjmp.S
.if defined (GCC1_IN_LIBC)
SRCS+=	divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S
.endif
.elif (${MACHINE} == "tahoe")
CFLAGS+=-I/sys
SRCS+=	_setjmp.s alloca.s fabs.s ldexp.s modf.s setjmp.s
SRCS+=	udiv.s urem.s
.elif (${MACHINE} == "vax")
SRCS+=	_setjmp.s alloca.s fabs.s ldexp.s modf.s setjmp.s
SRCS+=	udiv.s urem.s
.endif
.if defined (PW_COMPACT)
CFLAGS+=-DPW_COMPACT
.endif

MAN3+=	gen/alarm.3 gen/clock.3 gen/crypt.3 gen/ctermid.3 gen/ctime.3 \
	gen/ctype.3 gen/directory.3 gen/err.3 gen/exec.3 gen/fnmatch.3 \
	gen/frexp.3 gen/fts.3 gen/getcwd.3 gen/getcap.3 gen/getdiskbyname.3 \
	gen/getfsent.3 gen/getgrent.3 gen/getmntinfo.3 gen/getpass.3 \
	gen/getpwent.3 gen/getsubopt.3 gen/getttyent.3 gen/getusershell.3 \
	gen/glob.3 gen/initgroups.3 gen/insque.3 gen/isalnum.3 gen/isalpha.3 \
	gen/isascii.3 gen/iscntrl.3 gen/isdigit.3 gen/isgraph.3 gen/isinf.3 \
	gen/islower.3 gen/isprint.3 gen/ispunct.3 gen/isspace.3 \
	gen/isupper.3 gen/isxdigit.3 gen/ldexp.3 gen/modf.3 gen/nice.3 \
	gen/nlist.3 gen/pause.3 gen/popen.3 gen/psignal.3 gen/raise.3 \
	gen/scandir.3 gen/setjmp.3 gen/setmode.3 \
	gen/siginterrupt.3 gen/signal.3 gen/sigsetops.3 gen/sleep.3 \
	gen/syslog.3 gen/tcgetpgrp.3 gen/tcsendbreak.3 gen/tcsetattr.3 \
	gen/tcsetpgrp.3 gen/time.3 gen/times.3  gen/timezone.3 gen/tolower.3 \
	gen/toupper.3 gen/ttyname.3 gen/tzset.3 gen/ualarm.3 gen/unvis.3 \
	gen/usleep.3 gen/utime.3 gen/valloc.3 gen/vis.3

MLINKS+=crypt.3 encrypt.3 crypt.3 setkey.3
MLINKS+=ctime.3 asctime.3 ctime.3 difftime.3 ctime.3 gmtime.3 \
	ctime.3 localtime.3 ctime.3 mktime.3
MLINKS+=directory.3 closedir.3 directory.3 dirfd.3 directory.3 opendir.3 \
	directory.3 readdir.3 directory.3 rewinddir.3 directory.3 seekdir.3 \
	directory.3 telldir.3
MLINKS+=err.3 verr.3 err.3 errx.3 err.3 verrx.3 err.3 warn.3 err.3 vwarn.3 \
	err.3 warnx.3 err.3 vwarnx.3
MLINKS+=exec.3 execl.3 exec.3 execle.3 exec.3 execlp.3 exec.3 execv.3 \
	exec.3 execvp.3
MLINKS+=getcwd.3 getwd.3
MLINKS+=getcap.3 cgetent.3 getcap.3 cgetset.3 getcap.3 cgetmatch.3 \
	getcap.3 cgetcap.3 getcap.3 cgetnum.3 getcap.3 cgetstr.3 \
	getcap.3 cgetustr.3 getcap.3 cgetfirst.3 getcap.3 cgetnext.3 \
	getcap.3 cgetclose.3
MLINKS+=getfsent.3 endfsent.3 getfsent.3 getfsfile.3 getfsent.3 getfsspec.3 \
	getfsent.3 getfstype.3 getfsent.3 setfsent.3
MLINKS+=getgrent.3 endgrent.3 getgrent.3 setgroupent.3 getgrent.3 getgrgid.3 \
	getgrent.3 getgrnam.3 getgrent.3 setgrent.3 getgrent.3 setgrfile.3
MLINKS+=getpwent.3 endpwent.3 getpwent.3 setpassent.3 getpwent.3 getpwnam.3 \
	getpwent.3 getpwuid.3 getpwent.3 setpwent.3 getpwent.3 setpwfile.3
MLINKS+=getttyent.3 endttyent.3 getttyent.3 getttynam.3 getttyent.3 setttyent.3
MLINKS+=getusershell.3 endusershell.3 getusershell.3 setusershell.3
MLINKS+=glob.3 globfree.3
MLINKS+=insque.3 remque.3
MLINKS+=isinf.3 isnan.3
MLINKS+=popen.3 pclose.3
MLINKS+=psignal.3 sys_siglist.3
MLINKS+=scandir.3 alphasort.3
MLINKS+=setjmp.3 _longjmp.3 setjmp.3 _setjmp.3 setjmp.3 longjmp.3 \
	setjmp.3 sigsetjmp.3 setjmp.3 siglongjmp.3 setjmp.3 longjmperror.3
MLINKS+=setmode.3 getmode.3
MLINKS+=sigsetops.3 sigemptyset.3 sigsetops.3 sigfillset.3 \
	sigsetops.3 sigaddset.3 sigsetops.3 sigdelset.3 \
	sigsetops.3 sigismember.3
MLINKS+=syslog.3 closelog.3 syslog.3 openlog.3 syslog.3 setlogmask.3 \
	syslog.3 vsyslog.3
MLINKS+=tcsendbreak.3 tcdrain.3 tcsendbreak.3 tcflush.3 tcsendbreak.3 tcflow.3
MLINKS+=tcsetattr.3 cfgetispeed.3 tcsetattr.3 cfsetispeed.3 \
	tcsetattr.3 cfgetospeed.3 tcsetattr.3 cfsetospeed.3 \
	tcsetattr.3 cfsetspeed.3 tcsetattr.3 cfmakeraw.3 \
	tcsetattr.3 tcgetattr.3
MLINKS+=ttyname.3 isatty.3 ttyname.3 ttyslot.3
MLINKS+=tzset.3 tzsetwall.3

errlst.o errlst.po:
	${CC} -S ${CFLAGS} ${.IMPSRC}
	sed -e "s/.data/.text/" <errlst.s >xx
	mv xx errlst.s
	${AS} -o ${.TARGET} errlst.s
	rm -f errlst.s xx
