# $FreeBSD: src/gnu/usr.bin/groff/font/devlbp/Makefile,v 1.1.2.1 2001/04/26 17:08:06 ru Exp $

DEV=		lbp
LBPPRINT=	${PSPRINT}
DEVFILES=	DESC HB HBI HI HR TB TBI TI TR

CLEANFILES+=	DESC

DESC:	DESC.in
	cat ${.ALLSRC} >${.TARGET}
	if test "${PAGE}" = A4; then \
	  echo "papersize a4" >>${.TARGET}; \
        else \
	  echo "papersize letter" >>${.TARGET}; \
        fi
	test -z '${LBPPRINT}' || echo print '${LBPPRINT}' >>${.TARGET}

.include "../Makefile.dev"
