MAN1=		afmtodit.1

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		$(.CURDIR)/afmtodit.pl $(DESTDIR)$(BINDIR)/afmtodit

.include <bsd.prog.mk>
.include <../../../usr.bin/Makefile.inc>
.include <../Makefile.cfg>
