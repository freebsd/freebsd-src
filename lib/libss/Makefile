#	$Id: Makefile,v 1.10 1995/08/06 12:41:07 bde Exp $

LIB=	ss
SRCS=	ss_err.c data.c error.c execute_cmd.c help.c invocation.c list_rqs.c \
	listen.c pager.c parse.c prompt.c request_tbl.c requests.c \
	std_rqs.c
CFLAGS+= -I. -I${.CURDIR} -DPOSIX -DIN_LIBSS
CLEANFILES+=	ss ss_err.c ss_err.h std_rqs.c
.if exists(${.CURDIR}/../libcom_err/obj/.)
LDADD+=	-L${.CURDIR}/../libcom_err/obj
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
	-cd ${.CURDIR}; cmp -s ss.h ${DESTDIR}/usr/include/ss/ss.h || \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ss.h \
	    ${DESTDIR}/usr/include/ss
	-cd ${.CURDIR}; cmp -s copyright.h \
	    ${DESTDIR}/usr/include/ss/mit-sipb-copyright.h || \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 copyright.h \
	    ${DESTDIR}/usr/include/ss/mit-sipb-copyright.h
	-cd ${.OBJDIR}; \
	    if [ -f ss_err.h ]; then \
		cmp -s ss_err.h ${DESTDIR}/usr/include/ss/ss_err.h || \
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ss_err.h \
		${DESTDIR}/usr/include/ss; \
	    else \
		true; \
	    fi

.include <bsd.lib.mk>

