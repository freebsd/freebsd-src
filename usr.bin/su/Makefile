#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	su
SRCS=	su.c

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.if !defined(NOPAM)
CFLAGS+= -DUSE_PAM
DPADD+= ${LIBPAM}
LDADD+= ${MINUSLPAM}
.else
COPTS+= -DSKEY
DPADD+= ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD+= -lskey -lmd -lcrypt
.endif

.if defined(WHEELSU)
COPTS+=	-DWHEELSU
.endif
CFLAGS+= -Wall

BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
