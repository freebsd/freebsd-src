#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include <src.opts.mk>

.include "../Makefile.inc"
# .include "${KRB5_SRCTOP}/Makefile.et"

LIB=		krb5ss
INTERNALLIB=

.PATH:	${KRB5_DIR}/util/ss

SRCS=	data.c \
	error.c \
	execute_cmd.c \
	help.c \
	invocation.c \
	list_rqs.c \
	listen.c \
	pager.c \
	parse.c \
	prompt.c \
	request_tbl.c \
	requests.c \
	copyright.h \
	mit-sipb-copyright.h \
	ss.h \
	ss_internal.h \
	${GEN}

SCRIPTS=${GEN_SCRIPTS}

CFLAGS+=-I${KRB5_DIR}/util/ss \
	-I${KRB5_DIR}/util/et \
	-I${KRB5_DIR}/util \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_DIR}/include \
	-I${.OBJDIR:H} \
	-I${.OBJDIR}

.if !defined(BOOTSTRAPPING)
CFLAGS+= -DHAVE_READLINE=1 \
	 -I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/edit
.endif

GEN=	std_rqs.c ${GEN_SS_ERR_C} ${GEN_SS_ERR_H}
GEN_SCRIPTS=	ct_c.awk ct_c.sed mk_cmds
GEN_SS_ERR_C=	${GEN_SS_ERR:S/.et$/.c/}
GEN_SS_ERR_H=	${GEN_SS_ERR:S/.et$/.h/}
CLEANFILES=	${GEN} ${GEN_SCRIPTS}

mk_cmds: config_script mk_cmds.sh ct_c.awk ct_c.sed
	sh ${KRB5_DIR}/util/ss/config_script ${KRB5_DIR}/util/ss/mk_cmds.sh \
		${.OBJDIR} awk sed > ${.TARGET}
	chmod 755 ${.TARGET}

ct_c.awk: ct_c_awk.in
	rm -f ${.TARGET}
	cp ${.ALLSRC} ${.TARGET}

ct_c.sed: ct_c_sed.in
	sed -e '/^#/d' ${.ALLSRC} > ${.TARGET}

GEN_SS_ERR=	ss_err.et
${GEN_SS_ERR:[2..-1]}: .NOMETA
${GEN_SS_ERR_H}: ${GEN_SS_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_SS_ERR_C}: ${GEN_SS_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

std_rqs.c: mk_cmds std_rqs.ct ss_err.h
	sh mk_cmds ${KRB5_DIR}/util/ss/std_rqs.ct

.include <bsd.lib.mk>

.SUFFIXES: .h .c .ct .et .awk .sed
