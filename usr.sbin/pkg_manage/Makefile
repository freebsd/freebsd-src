# $Id: Makefile,v 1.1.1.1 1995/02/15 20:47:56 ache Exp $

PROG=   pkg_manage
SRCS=   pkg_main.c pkg_manage.c pkg_ui.c version.c
LDADD+= ../libdialog/libdialog.a ../libncurses/libncurses.a -lmytinfo
DPADD+= ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO} 
CFLAGS+= -Wall -Wstrict-prototypes -DHELP_PATH=\"${HELPDIR}/\" \
	 -I../libdialog -I../libncurses -g

#HELPDIR = /usr/share/misc/pkg_manage
HELPDIR = /home/marc/src/dialog/pkg_manage

HELPS=  pkg_view-inst.hlp pkg_del-inst.hlp pkg_preview.hlp \
	pkg_install.hlp pkg_main.hlp

afterinstall:
	cd ${.CURDIR}; \
	for m in ${HELPS} ; do \
	  cmp -s $$m ${DESTDIR}${HELPDIR}/$$m || \
	  install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$m ${DESTDIR}${HELPDIR} ; \
	done

.include <bsd.prog.mk>

pkg_manage: ../libdialog/libdialog.a
