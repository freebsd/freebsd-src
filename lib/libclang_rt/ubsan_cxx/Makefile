# $FreeBSD$

.include <bsd.init.mk>

LIB=	clang_rt.ubsan_cxx-${CRTARCH}

CXXFLAGS+=-frtti

.PATH:	${CRTSRC}/lib/ubsan

SRCS=	ubsan_handlers_cxx.cc\
	ubsan_type_hash.cc

.include <bsd.lib.mk>
