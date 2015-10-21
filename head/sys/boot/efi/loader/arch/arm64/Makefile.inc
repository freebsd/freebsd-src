# $FreeBSD$

LOADER_FDT_SUPPORT=yes
SRCS+=	exec.c \
	start.S

.PATH:	${.CURDIR}/../../arm64/libarm64
CFLAGS+=-I${.CURDIR}/../../arm64/libarm64
SRCS+=	cache.c

CFLAGS+=	-msoft-float -mgeneral-regs-only

CLEANFILES+=	loader.help

loader.help: help.common
	cat ${.ALLSRC} | \
	    awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

.if !defined(LOADER_ONLY)
.PATH: ${.CURDIR}/../../forth
.include	"${.CURDIR}/../../forth/Makefile.inc"

FILES+=	loader.rc
.endif
