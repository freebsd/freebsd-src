# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc
GCCLIB=	${.CURDIR}/../../../contrib/gcclibs
SRCDIR=	${.CURDIR}/../../../contrib/libstdc++/libsupc++

.PATH: ${SRCDIR} ${GCCLIB}/libiberty 

# Static only.
LIB=	supc++
SRCS+=	del_op.cc del_opnt.cc del_opv.cc del_opvnt.cc eh_alloc.cc eh_arm.cc \
	eh_aux_runtime.cc eh_call.cc eh_catch.cc eh_exception.cc eh_globals.cc \
	eh_personality.cc eh_term_handler.cc eh_terminate.cc eh_throw.cc \
	eh_type.cc eh_unex_handler.cc guard.cc new_handler.cc new_op.cc \
	new_opnt.cc new_opv.cc new_opvnt.cc pure.cc tinfo.cc tinfo2.cc \
	vec.cc vterminate.cc

# from libiberty:
SRCS+=	cp-demangle.c

CFLAGS+=	-DIN_GLIBCPP_V3 -DHAVE_CONFIG_H
CFLAGS+=	-I${GCCLIB}/include -I${SRCDIR} -I${GCCDIR}
CFLAGS+=	-I${.CURDIR}/../libstdc++ -I.
CFLAGS+=	-frandom-seed=RepeatabilityConsideredGood
CXXFLAGS+=	-fno-implicit-templates -ffunction-sections -fdata-sections
PO_CXXFLAGS=    ${CXXFLAGS:N-ffunction-sections}

HDRS=	exception new typeinfo cxxabi.h exception_defines.h
INCS=	${HDRS:S;^;${SRCDIR}/;}
INCSDIR=${INCLUDEDIR}/c++/4.2

unwind.h: ${GCCDIR}/unwind-generic.h
	ln -sf ${.ALLSRC} ${.TARGET}

SRCS+=		unwind.h
CLEANFILES+=	unwind.h

.include <bsd.lib.mk>
