#	@(#)Makefile	5.4 (Berkeley) 10/26/90

PROG=	lpd
CFLAGS+=-I${.CURDIR}/../common_source
LDADD=	-lutil
MAN8=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c displayq.c rmjob.c startdaemon.c \
	lpdchar.c common.c printcap.c
.PATH:	${.CURDIR}/../common_source

.include "../../Makefile.inc"
.include <bsd.prog.mk>
