# $FreeBSD$

PROG=	ncpus
SRCS=	ncpus.c acpi.c
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
SRCS+=	biosmptable.c
.endif
NO_MAN= noman
BINDIR=	/usr/local/bin

DPADD=	${LIBDEVINFO}
LDADD=	-ldevinfo

.include <bsd.prog.mk>
