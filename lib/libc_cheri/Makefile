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
SRCS=		cheri_clock_gettime.c	\
		cheri_invoke_cabi.S	\
		cheri_printf.c		\
		cheri_put.c		\
		cheri_raise.c		\
		cheri_stdio.c		\
		cheri_stub.c		\
		heap.c			\
		malloc.c

CFLAGS.malloc.c=	-Wno-cast-align

CFLAGS+=	-I${.CURDIR}/../libc/include \
		-I${.CURDIR}/../libc/locale \
		-I${.CURDIR}/../libc/mips \
		-I${.CURDIR}/../libcheri \
		-I${.CURDIR}/../libmalloc_simple \
		-I${.CURDIR}/../../contrib/gdtoa \
		-I${.CURDIR}/../../contrib/tzcode/stdtime \
		-mxgot

# libc/gdtoa
CFLAGS.misc.c=	-Wno-sign-compare
SRCS+=		glue.c			\
		machdep_ldisd.c		\
		misc.c
# libc/gen
SRCS+=		assert.c		\
		fmtcheck.c		\
		getprogname.c		\
		errlst.c		\
		errno.c			\
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
		collcmp.c		\
		ctype.c			\
		fix_grouping.c		\
		ldpart.c		\
		lmessages.c		\
		lmonetary.c		\
		lnumeric.c		\
		localeconv.c		\
		mbrtowc.c		\
		runetype.c		\
		setlocale.c		\
		setrunelocale.c		\
		table.c			\
		tolower.c		\
		toupper.c		\
		xlocale.c		\
		wcrtomb.c		\
		wctype.c

# libc/inet
SRCS+=		inet_addr.c		\
		inet_ntop.c		\
		inet_ntoa.c		\
		inet_pton.c

# libc/regex
CFLAGS.regcomp.c=	-DPOSIX_MISTAKE
SRCS+=		regcomp.c		\
		regerror.c		\
		regexec.c		\
		regfree.c

# libc/stdlib
SRCS+=		atoi.c			\
		abs.c			\
		llabs.c			\
		heapsort.c		\
		qsort.c 		\
		reallocf.c		\
		strtol.c		\
		strtol_c.c		\
		strtoll.c		\
		strtoul.c		\
		strtoull.c		\
		strtoq.c		\
		strtouq.c
# libc/string
CFLAGS.strerror.c=	-Wno-incompatible-pointer-types-discards-qualifiers
SRCS+=		bcopy.S			\
		ffs.c			\
		memchr.c		\
		memchr_c.c		\
		memcmp.c		\
		memcmp_c.c		\
		memcpy.c		\
		memcpy_c.S		\
		memcpy_c_tofrom.c	\
		memset.c		\
		memset_c.c		\
		memmove.c		\
		memmove_c.c		\
		strcat.c		\
		strcasecmp.c		\
		strcasestr.c		\
		strchr.S		\
		strchr_c.c		\
		strcmp.c		\
		strcmp_c.c		\
		strcoll.c		\
		strcpy.c		\
		strcpy_c.c		\
		strcspn.c		\
		strdup.c		\
		strerror.c		\
		strlcat.c		\
		strlcpy.c		\
		strlen.c		\
		strncmp.c		\
		strncmp_c.c		\
		strncpy.c		\
		strncpy_c.c		\
		strndup.c		\
		strnlen.c		\
		strnlen_c.c		\
		strpbrk.c		\
		strrchr.c		\
		strspn.c		\
		strstr.c

# libc/sys
SRCS+=		__error.c

# libc/mips/sys
SRCS+=		cerror.S

# contrib/gdtoa
CFLAGS.strtod.c=	-Wno-unused-parameter -Wno-uninitialized \
			-Wno-incompatible-pointer-types-discards-qualifiers
CFLAGS.strtodg.c=	-Wno-unused-parameter -Wno-uninitialized \
			-Wno-incompatible-pointer-types-discards-qualifiers
CFLAGS.strtof.c=	-Wno-unused-parameter -Wno-uninitialized \
			-Wno-incompatible-pointer-types-discards-qualifiers
SRCS+=		gethex.c		\
		gmisc.c			\
		hd_init.c		\
		hexnan.c		\
		smisc.c			\
		strtod.c		\
		strtodg.c		\
		strtof.c		\
		strtord.c		\
		sum.c			\
		ulp.c
# contrib/tzcode/stdtime
CFLAGS.localtime.c=	-fwrapv -Wno-missing-prototypes \
			-Wno-unused-parameter -Wno-unused-function -DTZ_NO_FILES
CFLAGS.strftime.c=	-Wno-unused-const-variable \
		-DNO_RUN_TIME_WARNINGS_ABOUT_YEAR_2000_PROBLEMS_THANK_YOU
SRCS+=		asctime.c		\
		localtime.c		\
		strftime.c		\
		timelocal.c

# contrib/compiler-rt/lib
SRCS+=	adddf3.c			\
	addsf3.c			\
	clzdi2.c			\
	clzsi2.c			\
	divdf3.c			\
	divsf3.c			\
	extendsfdf2.c			\
	fixdfdi.c			\
	fixdfsi.c			\
	fixsfdi.c			\
	fixsfsi.c			\
	fixunsdfdi.c			\
	fixunsdfsi.c			\
	floatdidf.c			\
	floatdisf.c			\
	floatsidf.c			\
	floatsisf.c			\
	floatundidf.c			\
	floatundisf.c			\
	floatunsidf.c			\
	floatunsisf.c			\
	muldf3.c			\
	mulsf3.c			\
	subdf3.c			\
	subsf3.c			\
	truncdfsf2.c

# lib/libc/softfloat
SRCS+=	gedf2.c				\
	gesf2.c				\
	gtdf2.c				\
	gtsf2.c				\
	ledf2.c				\
	lesf2.c				\
	ltdf2.c				\
	ltsf2.c				\
	eqdf2.c				\
	eqsf2.c				\
	nedf2.c				\
	nesf2.c				\
	softfloat.c			\
	unorddf2.c			\
	unordsf2.c

# sys/kern
CFLAGS.subr_scanf.c=	-Wno-sign-compare
SRCS+=		subr_scanf.c

# CHERI system classes
SRCS+=		cheri_system_stub.c

# Pull in ../Makefile.inc to define SHLIB_MAJOR so we can undef it.
.include <bsd.init.mk>

INCS=
NO_MAN=		yes
.undef SHLIB_MAJOR
MK_PROFILE=	no
NEED_CHERI=	sandbox
WARNS?= 2

NO_WERROR=	yes

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
		-I${LIBC_DIR}/${LIBC_ARCH}/softfloat	\
		-I${LIBC_DIR}/locale			\
		-I${LIBC_DIR}/regex			\
		-I${LIBC_DIR}/softfloat			\
		-I${LIBC_DIR}/string			\
		-I${LIBC_DIR}/stdlib			\
		-I${LIBC_DIR}/stdtime			\
		-I${.CURDIR}/../../contrib/gdtoa	\
		-I${.CURDIR}/../../include

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${LIBCHERI_DIR}
.PATH: ${LIBCHERI_DIR}/${MACHINE_ARCH}

.PATH: ${.CURDIR}/../libmalloc_simple

.PATH: ${LIBC_DIR}/gen
.PATH: ${LIBC_DIR}/${MACHINE}/string
.PATH: ${LIBC_DIR}/${MACHINE}/sys
.PATH: ${LIBC_DIR}/stdio
.PATH: ${LIBC_DIR}/string
.PATH: ${LIBC_DIR}/regex
.PATH: ${LIBC_DIR}/sys
.PATH: ${LIBC_DIR}/gdtoa
.PATH: ${LIBC_DIR}/locale
.PATH: ${LIBC_DIR}/inet
.PATH: ${LIBC_DIR}/softfloat
.PATH: ${LIBC_DIR}/softfloat/bits64
.PATH: ${LIBC_DIR}/stdlib
.PATH: ${LIBC_DIR}/stdtime
.PATH: ${.CURDIR}/../../contrib/compiler-rt/lib/builtins
.PATH: ${.CURDIR}/../../contrib/gdtoa
.PATH: ${.CURDIR}/../../sys/kern
.PATH: ${.CURDIR}/../../contrib/tzcode/stdtime

CFLAGS+=	-Wno-cast-align -DFORCE_C_LOCALE -DFORCE_UTC_TZ

.ifdef OAKLAND2015
CFLAGS+=	-DCHERI_NO_OUTPUT
.endif

.include <bsd.lib.mk>

CFLAGS+= ${CFLAGS.${.IMPSRC:T}}

SCRIPTDIR=	/usr/libdata/ldscripts
LDSCRIPTS=	sandbox.ld

afterinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${.CURDIR}/${LDSCRIPTS} ${DESTDIR}${SCRIPTDIR}
