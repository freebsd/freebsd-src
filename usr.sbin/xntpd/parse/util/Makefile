#
# $Id$
#

CFLAGS+= -I${.CURDIR}/../../include
CFLAGS+= -DNTP_POSIX_SOURCE -DUSE_PROTOTYPES
CFLAGS+= -DSYS_FREEBSD -DBOEDER -DHAVE_TERMIOS -DHAVE_BSD_NICE

.if exists(${.CURDIR}/../../lib/obj)
LDADD+= -L${.CURDIR}/../../lib/obj
DPADD+= -L${.CURDIR}/../../lib/obj/libntp.a
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
