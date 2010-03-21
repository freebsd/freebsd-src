#       @(#)Makefile    8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/info/Makefile,v 1.14.24.1 2010/02/10 00:26:20 kensmith Exp $

NO_OBJ=

.include <bsd.prog.mk>

beforeinstall:
.if !exists(${DESTDIR}${INFODIR})
	@echo "Warning: the directory ${DESTDIR}${INFODIR} does not exist!"
	@echo "Perhaps the variable INFODIR is set incorrectly"
	@echo "or your mtree database files are broken."
	@echo ""
	@echo "As a workaround you can create the directory by hand, e.g.:"
	@echo -n "install -d -o ${INFOOWN} -g ${INFOGRP} "
	@echo "-m 0755 ${DESTDIR}${INFODIR}"
	@exit 3;
.endif
.if !exists(${DESTDIR}${INFODIR}/dir)
	${INSTALL} -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} \
		 dir-tmpl ${DESTDIR}${INFODIR}/dir
.endif
