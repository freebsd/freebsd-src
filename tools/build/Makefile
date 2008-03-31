# $FreeBSD$

.PATH: ${.CURDIR}/../../include

LIB=		egacy
SRCS=
INCSGROUPS=	INCS
INCS=		

BOOTSTRAPPING?=	0

# Some tools need <sys/endian.h>.
.if !exists(/usr/include/sys/endian.h) || ${BOOTSTRAPPING} < 500000
INCSGROUPS+=	SYSHDRS
SYSHDRS=	endian.h
SYSHDRSDIR=	${INCLUDEDIR}/sys
.endif

# gnu/usr.bin/binutils/ld needs <elf-hints.h>.
.if !exists(/usr/include/elf-hints.h)
INCS+=		elf-hints.h
.endif

# lib/libncurses needs <stdbool.h>.
.if !exists(/usr/include/stdbool.h)
INCS+=		stdbool.h
.endif

# usr.bin/xargs needs <nl_langinfo.h>.
.if !exists(/usr/include/langinfo.h)
INCS+=		langinfo.h
.endif

# Some tools need basename(3).
.if !exists(/usr/include/libgen.h)
INCS+=		libgen.h
.PATH: ${.CURDIR}/../../lib/libc/gen
SRCS+=		basename.c
.endif

# Some tools need {get,set}progname(3).
.if ( ${BOOTSTRAPPING} < 440001 || \
    ( ${BOOTSTRAPPING} >= 500000 && ${BOOTSTRAPPING} < 500023 ))
SRCS+=		progname.c
.endif

# install(1) needs strtofflags(3).
.if ( ${BOOTSTRAPPING} < 400021 || \
    ( ${BOOTSTRAPPING} >= 500000 && ${BOOTSTRAPPING} < 500007 ))
.PATH: ${.CURDIR}/../../lib/libc/gen
SRCS+=		strtofflags.c
.endif

# Lot of tools need <getopt.h> and getopt_long(3).
.if !exists(/usr/include/getopt.h) || ${BOOTSTRAPPING} < 502104
INCS+=		getopt.h
.PATH: ${.CURDIR}/../../lib/libc/stdlib
SRCS+=		getopt_long.c
.endif

# Some tools depend on a new <regex.h> not requiring <sys/types.h>.
.if ${BOOTSTRAPPING} < 500042
INCS+=		regex.h
.endif

# mklocale(1) requires rune.h
.if ${BOOTSTRAPPING} >= 600016
INCS+=		rune.h
.endif

.if empty(SRCS)
SRCS=		dummy.c
.endif

.include <bsd.lib.mk>
