# $Id: Makefile,v 1.2 1995/04/21 10:05:36 jkh Exp $

PROG=   pkg_manage
SRCS=   pkg_main.c pkg_manage.c pkg_ui.c version.c
LDADD+= -ldialog -lncurses -lmytinfo
DPADD+= ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO} 
CFLAGS+= -Wall -Wstrict-prototypes -DHELP_PATH=\"${HELPDIR}/\" \
	 -I${.CURDIR}/../../gnu/lib/libdialog \
	 -I${.CURDIR}/../../lib/libncurses

HELPDIR = /usr/share/misc/pkg_manage

HELPS=  pkg_view-inst.hlp pkg_del-inst.hlp pkg_preview.hlp \
	pkg_install.hlp pkg_main.hlp

afterinstall:
	@if [ ! -d ${HELPDIR} ]; then rm -f ${HELPDIR}; mkdir -p ${HELPDIR}; fi
	@cd ${.CURDIR}; \
	for m in ${HELPS} ; do \
	  cmp -s $$m ${DESTDIR}${HELPDIR}/$$m || \
	  install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$m ${DESTDIR}${HELPDIR} ; \
	done

.include <bsd.prog.mk>
