#	@(#)Makefile	8.4 (Berkeley) 6/10/97

PROG=	makemap
SRCS=	makemap.c safefile.c snprintf.c
MAN8=	makemap.8
CFLAGS+=-I${.CURDIR}/../../contrib/sendmail/src -DNEWDB -DNOT_SENDMAIL

.PATH:	${.CURDIR}/../../contrib/sendmail/makemap \
	${.CURDIR}/../../contrib/sendmail/src


.include <bsd.prog.mk>
