# $FreeBSD$

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
