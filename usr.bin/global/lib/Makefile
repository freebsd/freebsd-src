# $Id: Makefile,v 1.3 1999/01/18 07:38:13 simokawa Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/lib

LIB=		gloutil
SRCS=		gtagsop.c tab.c strbuf.c mgets.c defined.c getdbpath.c \
		find.c dbop.c test.c makepath.c locatestring.c pathop.c \
		conf.c strmake.c usable.c token.c
NOPROFILE=		noprofile
INTERNALLIB=		true
INTERNALSTATICLIB=	true

.include <bsd.lib.mk>
