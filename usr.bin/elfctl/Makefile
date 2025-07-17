ELFTCDIR=	${SRCTOP}/contrib/elftoolchain

PROG=	elfctl

LIBADD=	elftc elf

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/common

.include <bsd.prog.mk>
