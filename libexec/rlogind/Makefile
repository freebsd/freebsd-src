#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	rlogind
SRCS=	rlogind.c
MAN8=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+= -Wall -DINET6

.if defined(NOPAM)
CFLAGS+= -DNO_PAM
.else
SRCS+=	auth_pam.c
DPADD+= ${LIBPAM}
LDADD+= ${MINUSLPAM}
.endif

.include <bsd.prog.mk>
