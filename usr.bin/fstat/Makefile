#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

# Somewhat works, but spews out "fstat: kinfo_getfile(): Memory protection violation"
WANT_CHERI=	none

PROG=	fstat
SRCS=	fstat.c fuser.c main.c
LINKS=	${BINDIR}/fstat ${BINDIR}/fuser
LIBADD=	procstat

MAN1=	fuser.1 fstat.1

.include <bsd.prog.mk>
