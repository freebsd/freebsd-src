#
# $Id: Makefile,v 1.1.1.1 1998/03/01 23:41:17 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		strip
SRCS=		objcopy.c is-strip.c
CFLAGS+=	-I${SRCDIR}/binutils
LDADD+=		-L${RELTOP}/libbinutils -lbinutils
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
CLEANFILES+=	maybe_stripped

all: maybe_stripped

maybe_stripped: strip
	cp strip maybe_stripped
.if defined(STRIP)
.if ${STRIP:M-s} != ""
	strip maybe_stripped
.endif
.endif

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    maybe_stripped ${DESTDIR}${BINDIR}/strip

.include <bsd.prog.mk>
