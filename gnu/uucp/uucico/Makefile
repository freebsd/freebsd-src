# Makefile for uucico
# $Id: Makefile,v 1.2 1993/08/06 23:38:23 rgrimes Exp $

BINDIR=		$(sbindir)
BINOWN=		$(owner)
BINMODE=	4555

PROG=		uucico
SRCS=		uucico.c trans.c send.c rec.c xcmd.c prot.c protg.c protf.c \
		prott.c prote.c proti.c protj.c protz.c time.c chat.c \
		conn.c copy.c log.c tcp.c tli.c util.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

MAN8=		uucico.8

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
