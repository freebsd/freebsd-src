#	@(#)Makefile	5.7 (Berkeley) 9/30/90

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN5=	syslog.conf.5
MAN8=	syslogd.8
LDADD=	-lutil

.include <bsd.prog.mk>
