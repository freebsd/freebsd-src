# $Id: Makefile,v 1.6 1995/05/30 03:50:08 rgrimes Exp $

PROG=   pkg_manage
SRCS=   pkg_main.c pkg_manage.c pkg_ui.c version.c
LDADD+= -ldialog -lncurses -lmytinfo
DPADD+= ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}
CFLAGS+= -Wall -Wstrict-prototypes -DHELP_PATH=\"${HELPDIR}/\" \
	 -I${.CURDIR}/../../gnu/lib/libdialog \
	 -I${.CURDIR}/../../lib/libncurses

HELPDIR = /usr/share/misc/pkg_manage

HELPS=	pkg_install.hlp         pkg_preview_fs.hlp \
	pkg_del_inst.hlp        pkg_main.hlp       \
	pkg_ds_install.hlp      pkg_preview.hlp    \
	pkg_view_inst.hlp

afterinstall:
	@if [ ! -d ${HELPDIR} ]; then rm -f ${HELPDIR}; mkdir -p ${HELPDIR}; fi
	@cd ${.CURDIR}; \
	for m in ${HELPS} ; do \
	  cmp -s $$m ${DESTDIR}${HELPDIR}/$$m || \
	  ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$m ${DESTDIR}${HELPDIR} ; \
	done

.include <bsd.prog.mk>
