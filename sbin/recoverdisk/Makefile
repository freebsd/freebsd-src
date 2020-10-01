# $FreeBSD$

PACKAGE=runtime
PROG=	recoverdisk

LDFLAGS += -lm

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0
