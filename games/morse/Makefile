#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	morse
NOMAN=	noman

.if ${MACHINE_ARCH} == "i386"
CFLAGS += -DSPEAKER=\"/dev/speaker\"
.endif

.include <bsd.prog.mk>
