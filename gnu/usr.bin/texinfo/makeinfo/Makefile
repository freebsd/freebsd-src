# $FreeBSD: src/gnu/usr.bin/texinfo/makeinfo/Makefile,v 1.13.2.1 2002/03/30 17:09:25 ru Exp $

PROG=	makeinfo
SRCS=	cmds.c docbook.c defun.c files.c footnote.c html.c \
	index.c insertion.c lang.c macro.c makeinfo.c multi.c \
	node.c sectioning.c toc.c xml.c

DPADD+=	${LIBTXI}
LDADD+=	${LIBTXI}

.include <bsd.prog.mk>

.PATH:	$(TXIDIR)/makeinfo $(TXIDIR)/doc
