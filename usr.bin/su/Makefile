#	@(#)Makefile	5.5 (Berkeley) 5/11/90


.if exists(/usr/lib/libcrypt.a)
#CFLAGS+=-DKERBEROS
DPADD+= ${LIBCRYPT} #${LIBKRB}
LDADD+= -lcrypt #-lkrb
.endif

PROG=	su
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
