# Makefile
# $Id: Makefile,v 1.3 1994/05/26 07:52:23 csgr Exp $

all:
	@echo -n

install: all
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/manctl.sh \
	${DESTDIR}/usr/sbin/manctl

.include <bsd.prog.mk>
