#	BSDI $Id: Makefile,v 1.4 1994/01/25 01:35:58 nate Exp $
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

beforeinstall:
	@rm -f ${DESTDIR}${BINDIR}/init.old
	@- ln ${DESTDIR}${BINDIR}/init ${DESTDIR}${BINDIR}/init.old
	@rm -f ${DESTDIR}${BINDIR}/init

.include <bsd.prog.mk>
