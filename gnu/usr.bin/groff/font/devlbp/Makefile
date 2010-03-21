# $FreeBSD: src/gnu/usr.bin/groff/font/devlbp/Makefile,v 1.2.38.1 2010/02/10 00:26:20 kensmith Exp $

DEV=		lbp
LBPPRINT=	${PSPRINT}
DEVFILES=	DESC HB HBI HI HR HNB HNBI HNI HNR \
		TB TBI TI TR CR CB CI ER EB EI

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
