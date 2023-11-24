
PACKAGE=	tests

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/tests

KMOD=	ktest_example
SRCS=	ktest_example.c

.include <bsd.kmod.mk>
