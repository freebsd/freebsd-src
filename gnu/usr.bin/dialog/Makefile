# $FreeBSD$

DIALOG=	${.CURDIR}/../../../contrib/dialog
PROG=   dialog

DPADD=	$(LIBDIALOG) $(LIBNCURSES)
LDADD=	-ldialog -lncursesw -lm
CFLAGS+= -I${.CURDIR} -I${DIALOG}
.PATH:	${DIALOG}

WARNS?= 3

.include <bsd.prog.mk>
