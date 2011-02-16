# $FreeBSD: src/gnu/usr.bin/texinfo/makeinfo/Makefile,v 1.17.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	makeinfo
SRCS=	cmds.c defun.c files.c float.c footnote.c html.c \
	index.c insertion.c lang.c macro.c makeinfo.c multi.c \
	node.c sectioning.c toc.c xml.c xref.c

DPADD=	${LIBTXI}
LDADD=	${LIBTXI}

.include <bsd.prog.mk>

.PATH:	$(TXIDIR)/makeinfo $(TXIDIR)/doc
