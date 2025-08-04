PACKAGE=	lib${LIB}
LIB_CXX=	util++
INTERNALLIB=	true
SHLIB_MAJOR=	1
SRCS=		stringf.cc

MAN+=	freebsd::FILE_up.3 \
	freebsd::addrinfo_up.3 \
	freebsd::malloc_up.3 \
	freebsd::stringf.3

.include <src.opts.mk>

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
