# $FreeBSD$
#
# This is a cheap knockoff of libc/Makefile building a subset of libc
# appropriate (and for us thus far necessary) suitable for sandboxed
# applications.  Replicating Makefiles doesn't really seem like the right way
# to do this, but reworking the libc Makefile to build a second libc is quite
# a lot harder.
#
# (Note that there is actually a bit more to this: there is some libcheri
# code, and local libc_cheri code, that is also thrown into the mix as well.)
#

# libcheri
LIB=		c_cheri
SRCS=		cheri_invoke.S

# libc/gen
#SRCS+=		_spinlock_stub.c

# libc/stdio
#
# findfp.c, stdio.c: strong FD assumptions will need fixing
#SRCS+=		fflush.c		\
#		findfp.c		\
#		fprintf.c		\
#		fputc.c			\
#		fputs.c			\
#		fvwrite.c		\
#		fwalk.c			\
#		makebuf.c		\
#		printf.c		\
#		putc.c			\
#		puts.c			\
#		stdio.c			\
#		vfprintf.c		\
#		wsetup.c

# libc/string
SRCS+=		memchr.c		\
		memcpy.S		\
		memset.c		\
		strlen.c

# CHERI system class
SRCS+=		cheri_system_stub.c

INCS=
SHLIB_MAJOR=	1
NO_MAN=		yes
USE_CHERI=	yes
WARNS?= 2

FILES=		cheri_invoke.o
CLEANFILES=	cheri_invoke.o

LIBC_DIR=${.CURDIR}/../libc
LIBCHERI_DIR=${.CURDIR}/../libcheri

.if exists(${LIBC_DIR}/${MACHINE_ARCH})
LIBC_ARCH=${MACHINE_ARCH}
.else
LIBC_ARCH=${MACHINE_CPUARCH}
.endif

CFLAGS+=	-I${LIBC_DIR}/include			\
		-I${LIBC_DIR}/${LIBC_ARCH}		\
		-I${LIBC_DIR}/locale			\
		-I${.CURDIR}/../../contrib/gdtoa	\
		-I${.CURDIR}/../../include

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${LIBCHERI_DIR}
.PATH: ${LIBCHERI_DIR}/${MACHINE_ARCH}

.PATH: ${LIBC_DIR}/gen
.PATH: ${LIBC_DIR}/${MACHINE}/string
.PATH: ${LIBC_DIR}/stdio
.PATH: ${LIBC_DIR}/string

.include <bsd.lib.mk>
