# Makefile
# $Id: Makefile,v 1.1.1.1 1994/04/17 21:26:21 csgr Exp $

all: manctl.sh
	/bin/cp ${.CURDIR}/manctl.sh manctl

install: all
	install -o ${BINOWN} -g ${BINGRP} -m 555 manctl \
	${DESTDIR}/usr/sbin/manctl

.include <bsd.prog.mk>
