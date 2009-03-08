# $FreeBSD$

unix		?=	We run FreeBSD, not UNIX.
.FreeBSD	?=	true

# These are the architectures we build.
# 'common' is a special machine type used to build common files which are shared
# by all the other machine types.
MACHINE_LIST?= common host i386

# For the rest MACHINE_ARCH == MACHINE 
.for m in ${MACHINE_LIST}
MACHINE_ARCH.$m?=$m
MACHINE_ARCH_LIST+=${MACHINE_ARCH.$m}
.endfor
MACHINE_ARCH_LIST:=${MACHINE_ARCH_LIST:O:u}

.if ${MACHINE} != ${MACHINE_ARCH}
MACHINE_ARCH=${MACHINE_ARCH.${MACHINE}}
.endif

.if ${MACHINE} == "host"
HOSTPROG= yes
.endif

# On JUNOS, FreeBSD sources are in src/bsd so we need a variable to reference
# them from so that the Buildfiles are compatible.
BSDSRCTOP = ${.SRCTOP}

# HOST_OBJTOP is where we build/find tools needed during the build.
HOST_OBJTOP?=${.OBJROOT}/host
HOSTTOOL_OBJTOP ?= ${HOST_OBJTOP}
HOSTTOOL_STAGEDIR = ${.OBJROOT}/stage/host
SHAREDHOSTTOOL_STAGEDIR = ${.OBJROOT}/../shared/stage/host

.include <bsd.init.mk>

.SUFFIXES:	.out .a .ln .o .c .cc .cpp .cxx .C .m .F .f .e .r .y .l .S .asm .s .cl .p .h .sh .lis

MKPROFILE_${MACHINE_ARCH} ?= no

SHLIB_LDFLAGS.i386 = -Wl,-E -nostdlib

AR		?=	ar
.if defined(%POSIX)
ARFLAGS		?=	-rv
.else
ARFLAGS		?=	crl
.endif

RANLIB		?=	ranlib
NM		?=	nm

AS		?=	as
AFLAGS		?=

DEBUG_FLAGS     ?=      -gdwarf-2
PROFILE_FLAGS	?=	-pg -DNEED_GPROF

.if defined(NEED_GPROF_STATIC)
PROFILE_FLAGS+= -DNEED_GPROF_STATIC
.endif

CC		?=	gcc
CFLAGS_BSD	+=	-DJBUILD

HOST_OS		!=	uname
_HOST_OSREL	!=	uname -r
HOST_OSREL	=	${_HOST_OSREL:R}
HOST_MACHINE	=	host
HOST_TARGET	=	${HOST_OS:L}${HOST_OSREL}-${HOST_MACHINE}

CFLAGS_BSD	+=	-nostdinc -O -pipe -D__${MACHINE_ARCH}__
CFLAGS_BSD	+=	${CPPFLAGS}
CFLAGS_BSD	+=	-fno-builtin-printf

HOSTCC		?=	${HOST_CC} ${HOST_CFLAGS}

HOST_CC		?=	gcc ${DEBUG_FLAGS}
HOST_CPP	?=	cpp
HOST_LD		?=	ld
HOST_CFLAGS	?=	-O
HOST_CFLAGS	+=	${HOST_CPPFLAGS}
HOST_COMPILE.c	?=	${HOST_CC} ${HOST_CFLAGS} -c
HOST_LINK.c	?=	${HOST_CC} ${HOST_CFLAGS} ${HOST_LDFLAGS}
#HOST_CFLAGS	+=	-Wno-pointer-sign

CXX		?=	g++
CXXFLAGS	+=	${CFLAGS:N-Wno-pointer-sign:N-Wmissing-declarations:N-Wmissing-prototypes:N-Wnested-externs:N-Wshadow:N-Wstrict-prototypes}
CXXFLAGS	+=	${CXXINCLUDES}

CPP		?=	cpp

LORDER		?=	lorder
STRIP		?=	strip

OBJCOPY		?=	objcopy

ELFTOOL		?= 	elftool
WRITE_MFS_IN_KERNEL ?=  ${HOSTTOOL_OBJTOP}/juniper/host-utils/wmfs/write_mfs_in_kernel
STRFILE=	${HOSTTOOL_OBJTOP}/juniper/host-utils/strfile/strfile

.if !empty(.MAKEFLAGS:M-n) && ${.MAKEFLAGS:M-n} == "-n"
_+_		?=
.else
_+_		?=	+
.endif

FC		?=	f77
FFLAGS		?=	-O
EFLAGS		?=

GENSETDEFS	?=	${HOST_OBJTOP}/usr.bin/gensetdefs/gensetdefs

HOST_GDB	?=	gdb
GDB		?=	gdb

INSTALL		?=	install

# Use a pkg_create which uses a known version of tar
PKG_CREATE	?=	${HOST_OBJTOP}/usr.sbin/pkg_install/create/pkg_create

LFLAGS		?=

LD		?=	ld
LDFLAGS		?=

LEX		?=	lex

LINT		?=	lint
LINTFLAGS	?=	-cghapbx
LINTKERNFLAGS	?=	${LINTFLAGS}
LINTOBJFLAGS	?=	-cghapbxu -i
LINTOBJKERNFLAGS?=	${LINTOBJFLAGS}
LINTLIBFLAGS	?=	-cghapbxu -C ${LIB}

MTREE		?=	mtree

OBJC		?=	cc
OBJCFLAGS	?=	${OBJCINCLUDES} ${CFLAGS} -Wno-import

OBJCOPY		?=	objcopy

OBJDUMP		?=	objdump

PC		?=	pc
PFLAGS		?=

RC		?=	f77
RFLAGS		?=

RPCGEN		?=	${HOSTTOOL_STAGEDIR}/usr/bin/rpcgen

MV		?=	/bin/mv

RM		?=	/bin/rm

SHELL		=	/bin/sh

SIZE		?=	size

TMPDIR		?=	/tmp

YACC		?=	yacc
YFLAGS		?=	

MD5		?= 	md5
SHA1		?=	sha1
SHA256		?=	sha256

.c:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}

.sh:
	cp -p ${.IMPSRC} ${.TARGET}
	chmod a+x ${.TARGET}

.c.ln:
	${LINT} ${LINTOBJFLAGS} ${CFLAGS:M-[DIU]*} ${.IMPSRC} || \
	    touch ${.TARGET}

.cc.ln .C.ln .cpp.ln .cxx.ln:
	${LINT} ${LINTOBJFLAGS} ${CXXFLAGS:M-[DIU]*} ${.IMPSRC} || \
	    touch ${.TARGET}

.c.o:
	${CC} ${${.TARGET}_CFLAGS} ${CFLAGS} ${${.TARGET}_CFLAGS_LAST} -c ${.IMPSRC}

.cc.o .cpp.o .cxx.o .C.o:
	${CXX} ${CXXFLAGS} ${CFLAGS} -c ${.IMPSRC}

.m.o:
	${OBJC} ${OBJCFLAGS} -c ${.IMPSRC}

.p.o:
	${PC} ${PFLAGS} -c ${.IMPSRC}

.e.o .r.o .F.o .f.o:
	${FC} ${RFLAGS} ${EFLAGS} ${FFLAGS} -c ${.IMPSRC}

.S.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}

.asm.o:
	${CC} -x assembler-with-cpp ${CFLAGS} -c ${.IMPSRC}

.s.o:
	${AS} ${${.TARGET}_AFLAGS} ${AFLAGS} ${${.TARGET}_AFLAGS_LAST} -o ${.TARGET} ${.IMPSRC}

.s.out .c.out .o.out:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} \
		${LDLIBS} -o ${.TARGET}

.f.out .F.out .r.out .e.out:
	${FC} ${EFLAGS} ${RFLAGS} ${FFLAGS} ${LDFLAGS} ${.IMPSRC} \
	    ${LDLIBS} -o ${.TARGET}
	${RM} -f ${.PREFIX}.o

.y.out:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} ${LDFLAGS} y.tab.c \
		${LDLIBS} -ly -o ${.TARGET}
	${RM} -f y.tab.c

.l.out:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.PREFIX}.tmp.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.PREFIX}.tmp.c ${LDLIBS} \
		-ll -o ${.TARGET}
	${RM} -f ${.PREFIX}.tmp.c

.o.lis:
	${OBJDUMP} --prefix-addresses -S ${.IMPSRC} > ${.TARGET}



# This makes it easier to host compile utils used in generating
# components in libs.  netbsd's bsd.hostprog.mk uses .lo as the object
# extention for local objects

.SUFFIXES:	.lo

.c.lo:
	${HOST_COMPILE.c} -o ${.TARGET} ${.IMPSRC}

.SUFFIXES:	.cpp-out

.c.cpp-out:
	@${CC} -E ${CFLAGS} ${.IMPSRC} | grep -v '^[ 	]*$$'

.include <bsd.cpu.mk>
