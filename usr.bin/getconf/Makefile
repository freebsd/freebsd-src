#	$FreeBSD$

PROG=	getconf

SRCS=	confstr.c getconf.c pathconf.c sysconf.c
CLEANFILES+= confstr.c pathconf.c sysconf.c

.SUFFIXES: .gperf

.gperf.c:
	gperf -t -L ANSI-C -C -k 1,2,7-10,21,'$$' ${.IMPSRC} >${.TARGET}

.include <bsd.prog.mk>
