# $FreeBSD$

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
MAN=
SRCS=	${PROG}.S

CFLAGS+=-I${.CURDIR}/../common

ORG=	0x7c00

LDFLAGS=${LDFLAGS_BIN}

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.cdboot.S=	${CLANG_NO_IAS}
