#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

LIB=	kadm

SRCS=	kadm_err.c kadm_stream.c kadm_supp.c kadm_cli_wrap.c krb_err.h
CFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/../../include -DPOSIX -Wall
CLEANFILES+=	kadm_err.c kadm_err.h krb_err.c krb_err.h

kadm_err.c kadm_err.h:	kadm_err.et
	test -e kadm_err.et || ln -s ${.CURDIR}/kadm_err.et .
	compile_et kadm_err.et

krb_err.h:
	test -e krb_err.et || ln -s ${.CURDIR}/../libkrb/krb_err.et .
	compile_et krb_err.et

beforeinstall:
	-cd ${.CURDIR}; cmp -s kadm.h \
	    ${DESTDIR}/usr/include/kerberosIV/kadm.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 kadm.h \
	    ${DESTDIR}/usr/include/kerberosIV
	-cd ${.OBJDIR}; cmp -s kadm_err.h \
	    ${DESTDIR}/usr/include/kerberosIV/kadm_err.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 kadm_err.h \
	    ${DESTDIR}/usr/include/kerberosIV

.include <bsd.lib.mk>
