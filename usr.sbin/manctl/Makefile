# Makefile
# $Id: Makefile,v 1.5 1995/07/17 14:39:09 jkh Exp $

all:
	@echo -n

install: all
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/manctl.sh \
	${DESTDIR}${BINDIR}/manctl

.include <bsd.prog.mk>
