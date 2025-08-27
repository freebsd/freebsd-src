#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include "../Makefile.inc"

SRC_ETDIR=	${KRB5_DIR}/util/et

SCRIPTNAME=	compile_et
SCRIPTS=	compile_et
BINDIR?=	/usr/bin
MAN=		compile_et.1
CLEANFILES=	compile_et
TAGS+=		dev

compile_et: compile_et.sh
	sh ${SRC_ETDIR}/config_script ${SRC_ETDIR}/compile_et.sh \
	"${KRB5_ETDIR}" /usr/bin/awk /usr/bin/sed > ${.TARGET}
	chmod 0755 ${.TARGET}

.include <bsd.prog.mk>

.SUFFIXES: .h .c .man .1

.man.1:
	@cp ${.ALLSRC} ${.TARGET}

.PATH: ${KRB5_DIR}/util/et
