#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

LIB=	kadm

SRCS=	kadm_err.c kadm_stream.c kadm_supp.c kadm_cli_wrap.c
CFLAGS+=	-I. -I${.CURDIR} -I${KRBOBJDIR} -DPOSIX -DKERBEROS

beforeinstall: kadm_err.c
	-cd ${.CURDIR}; cmp -s kadm.h \
	    ${DESTDIR}/usr/include/kerberosIV/kadm.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 kadm.h \
	    ${DESTDIR}/usr/include/kerberosIV
	-cd ${.OBJDIR}; cmp -s kadm_err.h \
	    ${DESTDIR}/usr/include/kerberosIV/kadm_err.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 kadm_err.h \
	    ${DESTDIR}/usr/include/kerberosIV

.include <bsd.lib.mk>

kadm_err.c: ${KADMOBJDIR}/kadm_err.h
kadm_cli_wrap.o: ${KRBOBJDIR}/krb_err.h
