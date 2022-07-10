# $FreeBSD$

.include <src.opts.mk>

LIB=	proc

SRCS=	proc_bkpt.c		\
	proc_create.c		\
	proc_regs.c		\
	proc_sym.c		\
	proc_rtld.c		\
	proc_util.c

INCS=	libproc.h

CFLAGS+=	-I${.CURDIR}

.if ${MK_CXX} == "no"
CFLAGS+=	-DNO_CXA_DEMANGLE
.else
LIBADD+=		cxxrt
.endif

LIBADD+=	elf procstat rtld_db util z

.if ${MK_CDDL} != "no"
LIBADD+=	ctf
IGNORE_PRAGMA=	YES
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID -DHAVE_BOOLEAN
CFLAGS+=	-I${SRCTOP}/cddl/contrib/opensolaris/lib/libctf/common \
		-I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common \
		-I${SRCTOP}/sys/cddl/compat/opensolaris
.else
CFLAGS+=	-DNO_CTF
.endif

SHLIB_MAJOR=	5

MAN=

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
