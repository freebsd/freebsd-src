.include <src.opts.mk>

.PATH:	${.CURDIR}/..

ATF_TESTS_C=	addrmerge_test
CFLAGS+=	-I${.CURDIR:H} -Wno-cast-qual
SRCS.addrmerge_test=	addrmerge_test.c util.c

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

WARNS?=	3

.if ${MK_ASAN} != "no"
# Work around "error: duplicate symbol: getifaddrs" when building with ASAN.
# The ASAN interceptors also define getifaddrs, but we want to prefer the local
# stub symbol here, so using a shared sanitizer runtime moves the local
# definition first in the symbol resolution order.
LDFLAGS+=-shared-libasan
.endif

.include <bsd.test.mk>
