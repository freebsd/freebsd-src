.PATH:		${.CURDIR}/..

PROG=	ssh-keygen
BINOWN=	root

.if (${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "powerpc" || \
	${MACHINE_ARCH} == "hppa")
BINMODE=0000
.else
BINMODE?=555
.endif

BINDIR=	/usr/bin
MAN=	ssh-keygen.1

SRCS=	ssh-keygen.c log-client.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBDES} ${LIBUTIL} ${LIBZ}
