# $Id: Makefile.inc,v 1.3 1997/12/15 02:28:05 skynyrd Exp $

BINDIR?= 	/usr/bin
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes

LIBGLOBALUTIL=	${.OBJDIR}/../lib/libutil.a
