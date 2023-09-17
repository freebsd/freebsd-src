
SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/tests/fib_lookup

KMOD=	test_lookup
SRCS=	opt_inet.h opt_inet6.h fib_lookup.c

.include <bsd.kmod.mk>
