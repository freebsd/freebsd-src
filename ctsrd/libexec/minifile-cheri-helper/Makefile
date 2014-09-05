# $FreeBSD$

PROG=	minifile-cheri-helper

.PATH: ${.CURDIR}/${MACHINE_ARCH}
SRCS=	minifile-cheri.c			\
	chsbrt.S				\
	malloc.c				\
	stub.c					\
	subr_prf.c

#	lib.S					\

.PATH: ${.CURDIR}/../../../lib/libc/${MACHINE}/gen
.PATH: ${.CURDIR}/../../../lib/libc/${MACHINE}/string
.PATH: ${.CURDIR}/../../../lib/libc/${MACHINE}/sys
.PATH: ${.CURDIR}/../../../lib/libc/gen
.PATH: ${.CURDIR}/../../../lib/libc/stdio
.PATH: ${.CURDIR}/../../../lib/libc/stdlib
.PATH: ${.CURDIR}/../../../lib/libc/stdtime
.PATH: ${.CURDIR}/../../../lib/libc/string
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/include
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/softfloat
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/mips/softfloat
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/stdtime
SRCS+=	_flock_stub.c				\
	_once_stub.c				\
	_pthread_stubs.c			\
	cerror.S				\
	ffs.S					\
	fileno.c				\
	fmtcheck.c				\
	getprogname.c				\
	memchr.c				\
	memcmp.c				\
	memcpy.S				\
	memcpy_c.s				\
	memmove.S				\
	memmove_c.c				\
	memset.c				\
	memset_c.c				\
	qsort.c					\
	setprogname.c				\
	strcasecmp.c				\
	strcasestr.c				\
	strcat.c				\
	strchr.S				\
	strcmp.S				\
	strcoll.c				\
	strcpy.c				\
	strcspn.c				\
	strdup.c				\
	strlcat.c				\
	strlcpy.c				\
	strlen.S				\
	strncmp.c				\
	strncpy.c				\
	strndup.c				\
	strnlen.c				\
	strrchr.c				\
	strstr.c				\
	strtol.c				\
	strtoul.c				\
	strtoull.c				\
#	memcpy.S				\

# Locale support
.PATH: ${.CURDIR}/../../../lib/libc/locale
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/locale
SRCS+=	ascii.c					\
	big5.c					\
	collcmp.c				\
	collate.c				\
	euc.c					\
	fix_grouping.c				\
	gbk.c					\
	gb18030.c				\
	gb2312.c				\
	ldpart.c				\
	lmessages.c				\
	lmonetary.c				\
	lnumeric.c				\
	localeconv.c				\
	mbrtowc.c				\
	mbsnrtowcs.c				\
	mskanji.c				\
	none.c					\
	nomacros.c				\
	rune.c					\
	runetype.c				\
	setlocale.c				\
	setrunelocale.c				\
	table.c					\
	tolower.c				\
	toupper.c				\
	utf8.c					\
	xlocale.c				\
	wcsnrtombs.c				\
	wcrtomb.c				\
	wctype.c

.PATH: ${.CURDIR}/../../../contrib/compiler-rt/lib
SRCS+=	adddf3.c				\
	addsf3.c				\
	clzdi2.c				\
	clzsi2.c				\
	divdf3.c				\
	divsf3.c				\
	extendsfdf2.c				\
	fixdfsi.c				\
	fixdfdi.c				\
	fixunsdfsi.c				\
	floatdidf.c				\
	floatsidf.c				\
	floatundidf.c				\
	floatundisf.c				\
	floatunsidf.c				\
	muldf3.c				\
	mulsf3.c				\
	subdf3.c				\
	subsf3.c

.PATH: ${.CURDIR}/../../../contrib/gdtoa
.PATH: ${.CURDIR}/../../../lib/libc/gdtoa
CFLAGS+= -I${.CURDIR}/../../../contrib/gdtoa
SRCS+=	gethex.c				\
	glue.c					\
	hd_init.c				\
	hexnan.c				\
	machdep_ldisx.c				\
	misc.c					\
	gmisc.c					\
	smisc.c					\
	strtod.c				\
	strtof.c				\
	strtodg.c				\
	strtord.c				\
	strtorx.c				\
	sum.c					\
	ulp.c

.PATH: ${.CURDIR}/../../../lib/libc/softfloat
.PATH: ${.CURDIR}/../../../lib/libc/softfloat/bits64
SRCS+=	gedf2.c					\
	gtdf2.c					\
	gtsf2.c					\
	ledf2.c					\
	ltdf2.c					\
	ltsf2.c					\
	eqdf2.c					\
	eqsf2.c					\
	nedf2.c					\
	nesf2.c					\
	softfloat.c

# Regular expression support
.PATH: ${.CURDIR}/../../../lib/libc/regex
SRCS+=	regcomp.c				\
	regerror.c				\
	regexec.c				\
	regfree.c

# Time string manipulation
.PATH: ${.CURDIR}/../../../contrib/tzcode/stdtime
CFLAGS+= -I${.CURDIR}/../../../contrib/tzcode/stdtime
SRCS+=	asctime.c				\
	localtime.c				\
	strftime.c				\
	timelocal.c

OTHER=						\
	setjmp.S				\
	strcpy.c				\
	execve.S				\
	regcomp.c				\
	regerror.c				\
	regexec.c				\
	regfree.c				\

MAN=

LDFLAGS+=	-Wl,--script=${.CURDIR}/sandbox.ld -nostdlib -Wl,-dead_strip
NO_MAN=
#STRIP=
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../usr.bin/minifile
LDADD+=		-lvuln_magic -lz -lcompiler_rt

NO_SHARED=	yes

USE_CHERI=	yes
USE_CHERI_STACK=yes

WARNS=4

#
# For libc files!
#
NO_WERROR=	yes

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		${PROG}.bin ${PROG}.dump
CLEANFILES+=	${PROG}.bin ${PROG}.dump

chsbrt.o:
	${AS} -G0 -o ${.TARGET} ${.ALLSRC}

${PROG}.bin: ${PROG}
	${OBJCOPY} -S -O binary ${.ALLSRC} ${.TARGET}

${PROG}.dump: ${PROG}
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>
