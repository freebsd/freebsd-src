# $FreeBSD$

DIALOG=	${.CURDIR}/../../../contrib/dialog
PROG=   dialog

DPADD=	$(LIBDIALOG) $(LIBNCURSES) $(LIBM)
LDADD=	-ldialog -lncursesw -lm
CFLAGS+= -I${.CURDIR} -I${DIALOG}
.PATH:	${DIALOG}

WARNS?= 3

.include <bsd.prog.mk>
