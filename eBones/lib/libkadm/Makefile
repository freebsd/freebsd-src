#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.11 1996/07/30 21:16:12 markm Exp $

LIB=	kadm

SRCS=	kadm_err.c kadm_stream.c kadm_supp.c kadm_cli_wrap.c
CFLAGS+=	-I. -I${.CURDIR} -I${KRBOBJDIR} -DPOSIX -DKERBEROS

beforeinstall: kadm_err.c
.for i in kadm.h kadm_err.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/$i \
	    ${DESTDIR}/usr/include/kerberosIV/$i
.endfor

.include <bsd.lib.mk>

kadm_err.c: ${KADMOBJDIR}/kadm_err.h
kadm_cli_wrap.o: ${KRBOBJDIR}/krb_err.h
