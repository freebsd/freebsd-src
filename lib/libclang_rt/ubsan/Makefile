# $FreeBSD$

.include <bsd.init.mk>

LIB=	clang_rt.ubsan-${CRTARCH}

.PATH:	${CRTSRC}/lib/ubsan

SRCS=	ubsan_diag.cc\
	ubsan_init.cc\
	ubsan_flags.cc\
	ubsan_handlers.cc\
	ubsan_value.cc

.include <bsd.lib.mk>


