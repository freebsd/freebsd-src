# $FreeBSD$

LOADER_FDT_SUPPORT=yes
SRCS+=	exec.c \
	start.S

.PATH:	${.CURDIR}/../../arm64/libarm64
CFLAGS+=-I${.CURDIR}/../../arm64/libarm64
SRCS+=	cache.c

CFLAGS+=	-msoft-float -mgeneral-regs-only
