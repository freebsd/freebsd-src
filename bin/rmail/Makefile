#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/sendmail/rmail

PROG=	rmail
MAN8=	rmail.8

# If you want to have your rmail queuing the mail only, uncomment the
# following:
# CFLAGS+= -DQUEUE_ONLY

# Not much point this being static. It calls a shared sendmail...
NOSHARED?= NO

.include <bsd.prog.mk>
