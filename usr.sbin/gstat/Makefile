# $FreeBSD$

PROG=	gstat

SRCS=	gstat.c subr_sbuf.c

.PATH:	${CURDIR}/../../../../sys/kern

WARNS?=	5

LDADD += -lgeom
LDADD += -ldevstat
LDADD += -lkvm
LDADD += -lbsdxml
LDADD += -lcurses

NOMAN=	sorry

.include <bsd.prog.mk>
