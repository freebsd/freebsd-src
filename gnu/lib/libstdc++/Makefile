# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc
SRCDIR=	${.CURDIR}/../../../contrib/libstdc++
SUPDIR=	${SRCDIR}/libsupc++

.PATH: ${SRCDIR}/src ${SRCDIR}/libmath ${SRCDIR}/config/io \
       ${SRCDIR}/config/locale/generic ${SRCDIR} ${SRCDIR}/std \
       ${SUPDIR}

SUBDIR=	doc

LIB=		stdc++
SHLIB_MAJOR=	4
SHLIB_MINOR=	0

CFLAGS+=	-DIN_GLIBCPP_V3 -DHAVE_CONFIG_H
CFLAGS+=	-I${.CURDIR} -I${SUPDIR} -I${GCCDIR}
CXXFLAGS+=	-fno-implicit-templates -ffunction-sections -fdata-sections -Wno-deprecated
#LDFLAGS+=	-Wl,-version-info,4:0:0 -Wl,--version-script=${SRCDIR}/src/linker.map

LDADD+=		-lm
DPADD+=		${LIBM}

# libstdc++ sources (libstdc++/src/Makefile.in, do not reorder)
SRCS+=	globals.cc limits.cc basic_file_stdio.cc complex_io.cc ios.cc \
	strstream-fixed.cc cmath.cc bitset.cc functexcept.cc stdexcept.cc \
	vterminate.cc c_locale.cc locale.cc localename.cc codecvt.cc \
	collate_members.cc ctype_members.cc messages_members.cc \
	monetary_members.cc numeric_members.cc time_members.cc concept-inst.cc \
	locale-inst.cc misc-inst.cc stl-inst.cc string-inst.cc wstring-inst.cc \
	valarray-inst.cc ext-inst.cc

# C parts of math
SRCS+=	nan.c signbit.c signbitf.c signbitl.c # stubs.c

# Embedded copy of libsupc++
SRCS+=	del_op.cc del_opnt.cc del_opv.cc del_opvnt.cc \
	eh_alloc.cc eh_aux_runtime.cc eh_catch.cc eh_exception.cc \
	eh_globals.cc eh_personality.cc eh_terminate.cc eh_throw.cc eh_type.cc \
	new_handler.cc new_op.cc new_opnt.cc new_opv.cc new_opvnt.cc \
	pure.cc tinfo.cc tinfo2.cc vec.cc

# from libiberty:
SRCS+=	${GCCDIR}/cp-demangle.c ${GCCDIR}/dyn-string.c

strstream-fixed.cc:	${SRCDIR}/src/strstream.cc
	sed -e 's,^#include <strstream.h>,#include <backward/strstream.h>,g' \
	    < ${SRCDIR}/src/strstream.cc > strstream-fixed.cc

CLEANFILES+=	strstream-fixed.cc

INCSDIR=	${INCLUDEDIR}/g++
BUILDHDRS=	gthr.h gthr-single.h gthr-posix.h gthr-default.h

BASELINKS!=	awk '$$0 !~ /^\#/ {print "include/" $$1, $$2}' ${.CURDIR}/includes.map
CONFIGLINKS!=	awk '$$0 !~ /^\#/ {print "config/" $$1, $$2}' ${.CURDIR}/configs.map
CPULINKS!=	awk '$$0 !~ /^\#/ {print "config/" $$1, $$2}' ${.CURDIR}/${MACHINE_ARCH}.map
INCLUDES=	${BASELINKS} ${CONFIGLINKS} ${CPULINKS}

gthr.h: ${GCCDIR}/gthr.h
	sed -e '/^#/s/\([ABCDEFGHIJKLMNOPQRSTUVWXYZ_][ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*\)/_GLIBCPP_\1/g' \
	    -e 's,^#include "\(.*\)",#include <bits/\1>,g' \
	    < ${GCCDIR}/gthr.h > gthr.h

gthr-single.h: ${GCCDIR}/gthr-single.h
	sed -e 's/\(UNUSED\)/_GLIBCPP_\1/g' \
	    -e 's/\(GCC[ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*_H\)/_GLIBCPP_\1/g' \
	    < ${GCCDIR}/gthr-single.h > gthr-single.h

gthr-posix.h: ${GCCDIR}/gthr-posix.h
	sed -e 's/\(UNUSED\)/_GLIBCPP_\1/g' \
	    -e 's/\(GCC[ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*_H\)/_GLIBCPP_\1/g' \
	    -e 's/\([ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*WEAK\)/_GLIBCPP_\1/g' \
	    < ${GCCDIR}/gthr-posix.h > gthr-posix.h

gthr-default.h: ${GCCDIR}/gthr-posix.h
	sed -e 's/\(UNUSED\)/_GLIBCPP_\1/g' \
	    -e 's/\(GCC[ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*_H\)/_GLIBCPP_\1/g' \
	    -e 's/\([ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*WEAK\)/_GLIBCPP_\1/g' \
	    -e 's,^#include "\(.*\)",#include <bits/\1>,g' \
	    < ${GCCDIR}/gthr-posix.h > gthr-default.h

CLEANFILES+=	${BUILDHDRS}
buildincludes:	${BUILDHDRS}

installincludes:
	@set ${INCLUDES}; \
	while test $$# -ge 2; do \
		l=${SRCDIR}/$$1; \
		shift; \
		t=$$1; \
		shift; \
		${ECHO} $$l -\> ${DESTDIR}${INCSDIR}/$$t; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
		     $$l ${DESTDIR}${INCSDIR}/$$t; \
	done; true
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${BUILDHDRS} ${.CURDIR}/c++config.h ${DESTDIR}${INCSDIR}/bits

.include <bsd.lib.mk>
