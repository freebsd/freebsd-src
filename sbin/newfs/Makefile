#	@(#)Makefile	7.13 (Berkeley) 2/26/91

PROG=	newfs
CFLAGS+=-DMFS
SRCS=	dkcksum.c newfs.c mkfs.c
MAN8=	newfs.8
LINKS=	${BINDIR}/newfs ${BINDIR}/mount_mfs
MLINKS=	newfs.8 mfs.8
.PATH:	${.CURDIR}/../disklabel

.include <bsd.prog.mk>
