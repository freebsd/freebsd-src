
PACKAGE=	tests

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/tests

KMOD=	ktest
SRCS=	ktest.c

.include <bsd.kmod.mk>
