#       @(#)Makefile    8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/info/Makefile,v 1.9.2.3 2002/08/07 16:31:50 ru Exp $

NOOBJ=  noobj

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
