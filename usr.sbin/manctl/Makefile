# Makefile
# $Id: Makefile,v 1.2 1994/04/19 06:43:36 csgr Exp $

all:
	@echo -n

install: all
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/manctl.sh \
	${DESTDIR}/usr/sbin/manctl
