#	@(#)Makefile	5.2 (Berkeley) 4/27/91

PROG=	kdump
CFLAGS+=-I/sys -I${.CURDIR}/../ktrace
SRCS=	kdump.c ioctl.c subr.c
.PATH:	${.CURDIR}/../ktrace
#CLEANFILES+=ioctl.c

#ioctl.c: mkioctls
#	/bin/sh ${.CURDIR}/mkioctls > ioctl.c

.include "../../Makefile.inc"
.include <bsd.prog.mk>
