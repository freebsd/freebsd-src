# $Id$

PROG=   pkg_manage
LDADD+= -ldialog -lncurses -lmytinfo
DPADD+= ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}
CFLAGS+= -Wall -Wstrict-prototypes -DHELP_PATH=\"${HELPDIR}/\"

HELPDIR = /usr/share/misc/pkg_manage

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

