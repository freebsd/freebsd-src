#	@(#)Makefile	8.1 (Berkeley) 6/4/93

SUBDIR=	libc libcurses libcompat libedit libkvm libm libmp \
	libplot libresolv librpc/rpc libtelnet libterm libutil liby

.if	${MACHINE} == "tahoe"
SUBDIR+=csu/tahoe.pcc
.elif	${MACHINE} == "vax"
SUBDIR+=csu/vax.pcc
.else
SUBDIR+=csu/${MACHINE}
.endif

.include <bsd.subdir.mk>
