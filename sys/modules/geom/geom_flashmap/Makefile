# $FreeBSD$

.PATH: ${SRCTOP}/sys/geom

KMOD=	geom_flashmap
SRCS=	geom_flashmap.c

.if !empty(OPT_FDT)
.PATH:	${SRCTOP}/sys/dev/fdt
SRCS+=	fdt_slicer.c
.endif

.include <bsd.kmod.mk>
