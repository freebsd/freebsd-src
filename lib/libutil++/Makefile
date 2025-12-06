LIB_CXX=	util++
INTERNALLIB=	true
SHLIB_MAJOR=	1
SRCS=		stringf.cc

MAN+=	freebsd::FILE_up.3 \
	freebsd::addrinfo_up.3 \
	freebsd::fd_up.3 \
	freebsd::malloc_up.3 \
	freebsd::nvlist_up.3 \
	freebsd::pidfile.3 \
	freebsd::stringf.3

.for page in ${MAN}
MANSRC.${page}=	${page:S/:/_/g}
.endfor

.include <src.opts.mk>

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
