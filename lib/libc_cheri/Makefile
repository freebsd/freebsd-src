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
SRCS=		cheri_invoke.S		\
		cheri_printf.c		\
		cmemcpy.s

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

CFLAGS+=	-I${.CURDIR}/../libc/include \
		-I${.CURDIR}/../libc/locale \
		-I${.CURDIR}/../libc/mips \
		-I${.CURDIR}/../../contrib/gdtoa \
		-I${.CURDIR}/../../contrib/tzcode/stdtime

# libc/gdtoa
CFLAGS.misc.c=	-Wno-sign-compare
SRCS+=		glue.c machdep_ldisd.c misc.c
# libc/gen
SRCS+=		assert.c		\
		getprogname.c		\
		errlst.c		\
		setprogname.c		\
		_once_stub.c		\
		_pthread_stubs.c
# libc/locale
CFLAGS.collate.c=	-Wno-sign-compare \
			-Wno-incompatible-pointer-types-discards-qualifiers \
			-Wno-missing-field-initializers \
			-Wno-unused-parameter
CFLAGS.euc.c=	-Wno-sign-compare
CFLAGS.fix_grouping.c=	-Wno-missing-prototypes
CFLAGS.gb18030.c=	-Wno-sign-compare
CFLAGS.gb2312.c=	-Wno-sign-compare
CFLAGS.utf8.c=	-Wno-sign-compare
CFLAGS.none.c=	-Wno-missing-field-initializers
CFLAGS.rune.c=	-Wno-cast-align
CFLAGS.setlocale.c=	-Wno-incompatible-pointer-types-discards-qualifiers \
			-Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
CFLAGS.setrunelocale.c=	-Wno-unused-parameter
CFLAGS.table.c=	-Wno-missing-field-initializers
CFLAGS.wcsnrtombs.c=	-Wno-sign-compare
CFLAGS.xlocale.c=	-Wno-missing-field-initializers \
			-Wno-unused-parameter -Wno-unused-function
SRCS+=		ascii.c big5.c gbk.c gb2312.c gb18030.c euc.c utf8.c \
		none.c mbsnrtowcs.c mskanji.c rune.c setlocale.c wcsnrtombs.c

SRCS+=		collate.c		\
		fix_grouping.c		\
		ldpart.c		\
		lmessages.c		\
		lmonetary.c		\
		lnumeric.c		\
		localeconv.c		\
		runetype.c		\
		setlocale.c		\
		setrunelocale.c		\
		table.c			\
		xlocale.c
# libc/inet
SRCS+=		inet_ntop.c		\
		inet_ntoa.c
# libc/stdlib
SRCS+=		atoi.c			\
		strtol.c		\
		strtoll.c		\
		strtoull.c		\
		strtoq.c		\
		strtouq.c
# libc/string
CFLAGS.strerror.c=	-Wno-incompatible-pointer-types-discards-qualifiers
SRCS+=		bcopy.S			\
		memchr.c		\
		memcmp.c		\
		memcpy.S		\
		memset.c		\
		strcat.c		\
		strchr.S		\
		strcmp.S		\
		strcpy.c		\
		strdup.c		\
		strerror.c		\
		strlcat.c		\
		strlcpy.c		\
		strlen.S		\
		strncmp.c		\
		strncpy.c

# contrib/gdtoa
CFLAGS.strtod.c=	-Wno-unused-parameter -Wno-uninitialized \
			-Wno-incompatible-pointer-types-discards-qualifiers
CFLAGS.strtodg.c=	-Wno-unused-parameter -Wno-uninitialized \
			-Wno-incompatible-pointer-types-discards-qualifiers
SRCS+=		gethex.c		\
		gmisc.c			\
		hd_init.c		\
		hexnan.c		\
		smisc.c			\
		strtod.c		\
		strtodg.c		\
		strtord.c		\
		sum.c			\
		ulp.c
# contrib/tzcode/stdtime
CFLAGS.localtime.c=	-fwrapv -Wno-missing-prototypes \
			-Wno-unused-parameter -Wno-unused-function -DTZ_NO_FILES
CFLAGS.strftime.c=	-Wno-unused-const-variable
SRCS+=		asctime.c		\
		localtime.c		\
		strftime.c		\
		timelocal.c

# sys/kern
CFLAGS.subr_scanf.c=	-Wno-sign-compare
SRCS+=		subr_scanf.c

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
.PATH: ${LIBC_DIR}/gdtoa
.PATH: ${LIBC_DIR}/locale
.PATH: ${LIBC_DIR}/inet
.PATH: ${LIBC_DIR}/stdlib
.PATH: ${LIBC_DIR}/stdtime
.PATH: ${.CURDIR}/../../contrib/gdtoa
.PATH: ${.CURDIR}/../../sys/kern
.PATH: ${.CURDIR}/../../contrib/tzcode/stdtime

CFLAGS+=	-Wno-cast-align -DFORCE_C_LOCALE

.include <bsd.lib.mk>

CFLAGS+= ${CFLAGS.${.IMPSRC:T}}
