MAINTAINER=ken@FreeBSD.ORG

PROG=   camcontrol
SRCS=   camcontrol.c modeedit.c
MAN8=   camcontrol.8

CFLAGS+= -I${.CURDIR}/../../sys
DPADD=   ${LIBCAM}
LDADD+=  -lcam

.include <bsd.prog.mk>
