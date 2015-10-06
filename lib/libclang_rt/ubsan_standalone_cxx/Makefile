# $FreeBSD$

.include <bsd.init.mk>

LIB=	clang_rt.ubsan_standalone_cxx-${CRTARCH}

CXXFLAGS+=-frtti

.PATH:	${CRTSRC}/lib/ubsan

SRCS=	ubsan_handlers_cxx.cc\
	ubsan_type_hash.cc\
	ubsan_type_hash_itanium.cc\
	ubsan_type_hash_win.cc

.include <bsd.lib.mk>
