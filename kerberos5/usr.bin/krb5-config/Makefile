# $FreeBSD$

SCRIPTS=	krb5-config
MAN=		krb5-config.1
CLEANFILES=	krb5-config

krb5-config: krb5-config.in
	sed	-e "s,@PACKAGE\@,FreeBSD heimdal,g" \
		-e "s,@VERSION\@,0.3f,g" \
		-e "s,@prefix\@,/,g" \
		-e "s,@exec_prefix\@,/,g" \
		-e "s,@libdir\@,${LIBDIR},g" \
		-e "s,@includedir\@,${INCDIR},g" \
		-e "s,@LIB_crypt\@,,g" \
		-e "s,@LIB_dbopen\@,,g" \
		-e "s,@LIB_des_appl\@,-lcrypto,g" \
		-e "s,@LIBS\@,,g" \
		${.ALLSRC} > ${.TARGET}
	chmod +x ${.TARGET}

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/tools
