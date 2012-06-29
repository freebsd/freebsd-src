# $FreeBSD$

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.S

CFLAGS+=-I${.CURDIR}/../../i386/common

ORG=	0x0000

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
