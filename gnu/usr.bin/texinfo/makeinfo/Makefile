# $FreeBSD: src/gnu/usr.bin/texinfo/makeinfo/Makefile,v 1.16 2003/05/02 00:51:24 ru Exp $

PROG=	makeinfo
SRCS=	cmds.c defun.c files.c footnote.c html.c \
	index.c insertion.c lang.c macro.c makeinfo.c multi.c \
	node.c sectioning.c toc.c xml.c

DPADD=	${LIBTXI}
LDADD=	${LIBTXI}

.include <bsd.prog.mk>

.PATH:	$(TXIDIR)/makeinfo $(TXIDIR)/doc
