#	@(#)Makefile	8.2 (Berkeley) 4/17/94

PROG=	compress
SRCS=	compress.c zopen.c
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress
MLINKS=	compress.1 uncompress.1

# XXX zopen is not part of libc
# MAN3=zopen.3

.include <bsd.prog.mk>
