#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	strip
BINDIR= /usr/libexec/aout
MAN1=	strip.1
CLEANFILES += maybe_stripped

all: maybe_stripped

maybe_stripped: strip
	cp -p strip maybe_stripped
.if defined(STRIP)
.if ${STRIP:M-s} != ""
	-strip maybe_stripped
.endif
.endif

install: maninstall
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    maybe_stripped ${DESTDIR}${BINDIR}/strip

.include <bsd.prog.mk>
