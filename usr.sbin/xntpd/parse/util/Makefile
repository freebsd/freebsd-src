#
# $FreeBSD: src/usr.sbin/xntpd/parse/util/Makefile,v 1.5.2.1 1999/08/29 15:49:30 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../../include
CFLAGS+= -DNTP_POSIX_SOURCE -DUSE_PROTOTYPES
CFLAGS+= -DSYS_FREEBSD -DBOEDER -DHAVE_TERMIOS -DHAVE_BSD_NICE

.if exists(${.OBJDIR}/../../lib)
LDADD+= -L${.OBJDIR}/../../lib
DPADD+= -L${.OBJDIR}/../../lib/libntp.a
.else
LDADD+= -L${.CURDIR}/../../lib
DPADD+= -L${.CURDIR}/../../lib/libntp.a
.endif

LDADD+= -lntp
BINDIR= /usr/sbin
PROG= dcfd

SRCS= dcfd.c
NOMAN=

.include <bsd.prog.mk>
