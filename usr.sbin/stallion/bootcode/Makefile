# $Id: Makefile,v 1.1 1996/05/04 08:02:10 peter Exp $

FILES	= 	2681.sys cdk.sys

LIBMODE	=	444
LIBOWN	=	bin
LIBGRP	=	bin
CLEANFILES= 	${FILES}

all: $(FILES)

install:
	@if [ ! -d ${DESTDIR}${BOOTDIR} ]; then mkdir ${DESTDIR}${BOOTDIR};fi
	@for i in ${FILES}; do \
		${ECHO} "installing $$i into ${DESTDIR}${BOOTDIR}"; \
		${INSTALL} -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
		$$i ${DESTDIR}${BOOTDIR}; \
	done

clean:
	rm -f ${CLEANFILES}

.include <bsd.prog.mk>

cdk.sys: ${.CURDIR}/cdk.sys.uu
	@rm -f $@
	uudecode ${.CURDIR}/$@.uu

2681.sys: ${.CURDIR}/2681.sys.uu
	@rm -f $@
	uudecode ${.CURDIR}/$@.uu

