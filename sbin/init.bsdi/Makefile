#	BSDI $Id: Makefile,v 1.3 1993/09/26 12:26:15 rgrimes Exp $
#	@(#)Makefile	5.3 (Berkeley) 5/11/90

PROG=	init
MAN8=	init.8
# XXX fix this when we have KERN_SECURELVL
CFLAGS+=-DSECURE -DNOSYSCTL
DPADD=	${LIBUTIL}
LDADD=	-lutil
BINMODE=500

.if exists(/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

.include <bsd.prog.mk>
