# Makefile for indxbib

PROG=		indxbib
SRCS=		indxbib.cc dirnamemax.c signal.c
CFLAGS+=	-I$(.CURDIR)/../include
CXXFLAGS+=	-I$(.CURDIR)/../include
LDADD+= 	$(LIBBIB) $(LIBGROFF) -lm
DPADD+= 	$(LIBBIB) $(LIBGROFF) $(LIBMATH)

# Ugh - I can't grab /usr/share from ../../../share/Makefile.inc
# since I've already grabbed a BINDIR file for installing indxbib.
# Sigh... Hardcode it.  	- jkh
#
afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/eign \
		$(DESTDIR)/usr/share/dict

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
