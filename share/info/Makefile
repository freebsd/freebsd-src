#       @(#)Makefile    8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.5 1998/01/03 14:06:26 wosch Exp $

NOOBJ=  noobj

all clean cleandir depend lint tags:

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
	${INSTALL} -c -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} dir-tmpl \
		${DESTDIR}${INFODIR}/dir-tmpl
.if !exists(${DESTDIR}${INFODIR}/dir)
	${INSTALL} -c -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} \
		 dir-tmpl ${DESTDIR}${INFODIR}/dir
.endif

.include <bsd.prog.mk>
