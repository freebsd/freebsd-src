# Makefile for uucico
# $Id: Makefile,v 1.7 1995/05/13 12:29:57 ache Exp $

BINDIR=         $(libxdir)
BINOWN=		$(owner)
BINMODE=        4555

PROG=		uucico
SRCS=		uucico.c trans.c send.c rec.c xcmd.c prot.c protg.c protf.c \
		prott.c prote.c proti.c protj.c proty.c protz.c time.c log.c \
		chat.c conn.c util.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\" -DOWNER=\"$(owner)\"

MAN8=		uucico.8

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
