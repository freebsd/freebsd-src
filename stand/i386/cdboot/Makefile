.include <bsd.init.mk>

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
MAN=	cdboot.8
SRCS=	${PROG}.S

CFLAGS+=-I${BOOTSRC}/i386/common

ORG=	0x7c00

LDFLAGS+=${LDFLAGS_BIN}

.include <bsd.prog.mk>
