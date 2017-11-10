# $FreeBSD$

HAVE_FDT=yes

SRCS+=	exec.c \
	start.S

.PATH:	${BOOTSRC}/arm64/libarm64
CFLAGS+=-I${BOOTSRC}/arm64/libarm64
SRCS+=	cache.c

CFLAGS+=	-mgeneral-regs-only

CLEANFILES+=	loader.help

loader.help: help.common
	cat ${.ALLSRC} | \
	    awk -f ${LDRSRC}/merge_help.awk > ${.TARGET}

.if !defined(LOADER_ONLY)
.PATH: ${BOOTSRC}/forth
.include	"${BOOTSRC}/forth/Makefile.inc"

FILES+=	loader.rc
.endif
