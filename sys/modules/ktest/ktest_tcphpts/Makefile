PACKAGE=	tests
WARNS?=		6

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/netinet

KMOD=	ktest_tcphpts
SRCS=	tcp_hpts_test.c

.include <bsd.kmod.mk>

