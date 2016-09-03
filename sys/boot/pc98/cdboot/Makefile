# $FreeBSD$

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
MAN=
SRCS=	${PROG}.S

CFLAGS+=-I${.CURDIR}/../../i386/common

ORG=	0x0000

LDFLAGS=${LDFLAGS_BIN}

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.cdboot.S=	${CLANG_NO_IAS}
