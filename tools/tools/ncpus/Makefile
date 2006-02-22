# $FreeBSD$

PROG=	ncpus
SRCS=	ncpus.c acpi.c biosmptable.c
NO_MAN= noman
BINDIR=	/usr/local/bin

DPADD=	${LIBDEVINFO}
LDADD=	-ldevinfo

.include <bsd.prog.mk>
