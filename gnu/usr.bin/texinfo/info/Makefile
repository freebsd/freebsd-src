#	$Id$

PROG=	info
NOMAN=  yes

SRCS=	dir.c display.c display.h doc.c doc.h dribble.c dribble.h
SRCS+=	echo-area.c echo-area.h filesys.c filesys.h footnotes.c
SRCS+=	footnotes.h funs.h gc.c gc.h indices.c indices.h
SRCS+=	info-utils.c info-utils.h info.c info.h infodoc.c
SRCS+=	infomap.c infomap.h m-x.c man.c man.h nodemenu.c nodes.c
SRCS+=	nodes.h search.c search.h session.c session.h signals.c
SRCS+=	signals.h termdep.h terminal.c terminal.h tilde.c tilde.h
SRCS+=	variables.c variables.h window.c window.h

CFLAGS+= -DINFODIR=\"${INFODIR}:/usr/local/info:.\"
CFLAGS+= -DLOCALEDIR=\"/usr/share/locale\" -DHAVE_CONFIG_H
CFLAGS+= -I${TXIDIR} -I${TXIDIR}/lib -I../libintl

DPADD=	${LIBTERMCAP} ${LIBTXI} ${LIBINTL}
LDADD=	-ltermcap ${LIBTXI} ${LIBINTL}

.include <bsd.prog.mk>

.PATH: ${TXIDIR}/info
