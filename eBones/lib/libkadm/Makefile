#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.12 1996/08/30 04:06:58 peter Exp $

LIB=	kadm

SRCS=	kadm_err.c kadm_stream.c kadm_supp.c kadm_cli_wrap.c
CFLAGS+=	-I. -I${.CURDIR} -I${KRBOBJDIR} -DPOSIX -DKERBEROS

beforeinstall: kadm_err.c
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/kadm.h \
		${DESTDIR}/usr/include/kerberosIV
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 kadm_err.h \
		${DESTDIR}/usr/include/kerberosIV

.include <bsd.lib.mk>

kadm_err.c: ${KADMOBJDIR}/kadm_err.h
kadm_cli_wrap.o: ${KRBOBJDIR}/krb_err.h
