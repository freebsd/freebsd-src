# $FreeBSD: src/gnu/usr.bin/groff/devlbp/Makefile,v 1.2.2.1 2000/12/07 09:57:01 ru Exp $

DEV=		lbp
LBPPRINT=	${PSPRINT}
DEVFILES=	DESC HB HBI HI HR TB TBI TI TR

CLEANFILES+=	DESC

DESC:	DESC.in
	-rm -f DESC
	cat ${DIST_DIR}/DESC.in >DESC
	if test "$(PAGE)" = A4; then \
	  echo "papersize a4" >>DESC; \
        else \
	  echo "papersize letter" >>DESC; \
        fi
	test -z '$(LBPPRINT)' || echo print '$(LBPPRINT)' >>DESC

.include "../Makefile.dev"
