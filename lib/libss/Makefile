#	$Id: Makefile,v 1.12 1996/06/24 04:23:28 jkh Exp $

LIB=	ss
SRCS=	ss_err.c data.c error.c execute_cmd.c help.c invocation.c list_rqs.c \
	listen.c pager.c parse.c prompt.c request_tbl.c requests.c \
	std_rqs.c
CFLAGS+= -I. -I${.CURDIR} -DPOSIX -DIN_LIBSS
CLEANFILES+=	ss ss_err.c ss_err.h std_rqs.c
.if exists(${.OBJDIR}/../libcom_err)
LDADD+=	-L${.OBJDIR}/../libcom_err
.else
LDADD+=	-L${.CURDIR}/../libcom_err
.endif
LDADD+=	-lcom_err

ss_err.h ss_err.c:	${.CURDIR}/ss_err.et
	test -e ss_err.et || ln -s ${.CURDIR}/ss_err.et .
	compile_et ss_err.et
	-test -h ss_err.et && rm -f ss_err.et

std_rqs.c:		${.CURDIR}/std_rqs.ct
	test -e std_rqs.ct || ln -s ${.CURDIR}/std_rqs.ct .
	mk_cmds std_rqs.ct
	-test -h std_rqs.ct && rm -f std_rqs.ct

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/ss.h \
		${DESTDIR}/usr/include/ss
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/copyright.h \
		${DESTDIR}/usr/include/ss/mit-sipb-copyright.h
.if exists(ss_err.h)
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ss_err.h \
		${DESTDIR}/usr/include/ss
.endif

.include <bsd.lib.mk>

