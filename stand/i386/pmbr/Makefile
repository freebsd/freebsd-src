# $FreeBSD$

PROG=	pmbr
STRIP=
BINMODE=${NOBINMODE}
SRCS=	${PROG}.s

ORG=	0x600

AFLAGS+=--defsym FLAGS=${BOOT_MBR_FLAGS}
LDFLAGS+=${LDFLAGS_BIN}

.include <bsd.prog.mk>
