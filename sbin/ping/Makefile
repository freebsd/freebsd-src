#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.6 1998/09/05 15:11:47 dfr Exp $

PROG=	ping
MAN8=	ping.8
BINMODE=4555
COPTS+=	-Wall -Wmissing-prototypes
.if ${MACHINE_ARCH} == "alpha"
COPTS+= -fno-builtin	# GCC's builtin memcpy doesn't do unaligned copies
.endif
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
