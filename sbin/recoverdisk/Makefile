# $FreeBSD$

PACKAGE=runtime
PROG=	recoverdisk

LDFLAGS += -lm

WARNS?=	6

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0
