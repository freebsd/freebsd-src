# $FreeBSD: src/gnu/usr.bin/texinfo/makeinfo/Makefile,v 1.15 2002/06/05 06:58:32 ru Exp $

PROG=	makeinfo
SRCS=	cmds.c docbook.c defun.c files.c footnote.c html.c \
	index.c insertion.c lang.c macro.c makeinfo.c multi.c \
	node.c sectioning.c toc.c xml.c

DPADD=	${LIBTXI}
LDADD=	${LIBTXI}

.include <bsd.prog.mk>

.PATH:	$(TXIDIR)/makeinfo $(TXIDIR)/doc
