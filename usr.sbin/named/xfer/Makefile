#	@(#)Makefile	5.1 (Berkeley) 5/28/90

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c
CFLAGS+=-I${.CURDIR}/..
.PATH:	${.CURDIR}/..
BINDIR=	/usr/libexec
MAN8= named-xfer.8

.include "../../Makefile.inc"
.include <bsd.prog.mk>
