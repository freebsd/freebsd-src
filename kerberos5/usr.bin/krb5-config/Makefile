# $FreeBSD$

srcdir=		${KRB5DIR}/tools

MAN1=		krb5-config.1

all:		krb5-config

CLEANFILES+=	krb5-config

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} krb5-config \
	    ${DESTDIR}${BINDIR}/krb5-config

krb5-config: krb5-config.in
	sed	-e "s,@PACKAGE\@,FreeBSD heimdal,g" \
		-e "s,@VERSION\@,0.3f,g" \
		-e "s,@prefix\@,/,g" \
		-e "s,@exec_prefix\@,/,g" \
		-e "s,@libdir\@,$(LIBDIR),g" \
		-e "s,@includedir\@,$(INCDIR),g" \
		-e "s,@LIB_crypt\@,,g" \
		-e "s,@LIB_dbopen\@,,g" \
		-e "s,@LIB_des_appl\@,-lcrypto,g" \
		-e "s,@LIBS\@,,g" \
		$(srcdir)/krb5-config.in > $@
	chmod +x $@

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/tools
