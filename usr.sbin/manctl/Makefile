# Makefile
# $Id: Makefile,v 1.4 1994/05/26 20:16:49 csgr Exp $

all:
	@echo -n

install: all
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/manctl.sh \
	${DESTDIR}/usr/sbin/manctl

.include <bsd.prog.mk>
