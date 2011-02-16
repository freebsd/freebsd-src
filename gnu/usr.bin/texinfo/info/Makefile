# $FreeBSD: src/gnu/usr.bin/texinfo/info/Makefile,v 1.14.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	info
MAN=	info.1 info.5 texinfo.5

SRCS=	dir.c display.c doc.c dribble.c echo-area.c filesys.c \
	footnotes.c gc.c indices.c info-utils.c info.c infodoc.c \
	infomap.c m-x.c man.c nodemenu.c nodes.c search.c session.c \
	signals.c terminal.c tilde.c variables.c window.c

CFLAGS+= -DINFODIR=\"${INFODIR}:/usr/local/info:.\"

DPADD=	${LIBTERMCAP} ${LIBTXI}
LDADD=	-ltermcap ${LIBTXI}

.include <bsd.prog.mk>

.PATH:	${TXIDIR}/info ${TXIDIR}/doc
