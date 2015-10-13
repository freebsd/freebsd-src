# $FreeBSD$

.if ${.MAKE.MODE:Unormal:Mmeta*} != ""
.if !empty(SUBDIR) && !defined(LIB) && !defined(PROG) && ${.MAKE.MAKEFILES:M*bsd.prog.mk} == ""
.if ${.MAKE.MODE:Mleaf*} != ""
# we only want leaf dirs to build in meta mode... and we are not one
.MAKE.MODE = normal
.endif
.endif
.endif

.if ${MK_SYSROOT} == "yes" && !empty(SYSROOT) && ${MACHINE} != "host"
CFLAGS_LAST+= --sysroot=${SYSROOT}
CXXFLAGS_LAST+= --sysroot=${SYSROOT}
LDADD+= --sysroot=${SYSROOT}
.elif ${MK_STAGING} == "yes"
CFLAGS+= -nostdinc
CFLAGS+= -I${STAGE_INCLUDEDIR}
LDADD+= -L${STAGE_LIBDIR}
.endif
.if ${MACHINE} == "host"
# we cheat?
LDADD+= -B/usr/lib
CFLAGS_LAST+= -I/usr/include
CXXFLAGS_LAST+= -I/usr/include
.endif

.if ${MACHINE} == "host"
.if ${.MAKE.DEPENDFILE:E} != "host"
UPDATE_DEPENDFILE?= no
.endif
HOST_CC?=	/usr/bin/cc
CC=		${HOST_CC}
HOST_CXX?=	/usr/bin/c++
CXX=		${HOST_CXX}
HOST_CPP?=	/usr/bin/cpp
CPP=		${HOST_CPP}
HOST_CFLAGS+= -DHOSTPROG
CFLAGS+= ${HOST_CFLAGS}
.endif
