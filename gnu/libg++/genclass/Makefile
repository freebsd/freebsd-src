# Makefile for g++ library genclass test

NOMAN=	noman

afterinstall:
	sed -e '/^PROTODIR/s_PROTODIR.*_PROTODIR=${DESTDIR}/usr/include/g++/gen_;' $(.CURDIR)/genclass.sh > $(DESTDIR)$(BINDIR)/genclass
	chown ${BINOWN}.${BINGRP} $(DESTDIR)$(BINDIR)/genclass
	chmod ${BINMODE} $(DESTDIR)$(BINDIR)/genclass

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
