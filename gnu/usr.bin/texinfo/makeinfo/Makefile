# $FreeBSD$

PROG=	makeinfo
SRCS=	cmds.c defun.c files.c footnote.c html.c index.c insertion.c lang.c \
	macro.c makeinfo.c multi.c node.c sectioning.c toc.c

DPADD+=	${LIBTXI}
LDADD+=	${LIBTXI}

.include <bsd.prog.mk>

.PATH:	$(TXIDIR)/makeinfo $(TXIDIR)/doc
