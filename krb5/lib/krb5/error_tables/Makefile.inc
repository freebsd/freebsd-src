#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/krb5/error_tables

# CLEANFILES+=	${GEN_ET} ${GENI_ET}

# The order of these matters. See upstream src/include/Makefile.in

GEN_KRB5_ERR=	krb5_err.et
GEN_KRB5_ERR_C=	${GEN_KRB5_ERR:S/.et$/.c/}
GEN_KRB5_ERR_H=	${GEN_KRB5_ERR:S/.et$/.h/}
CLEANFILES+=	${GEN_KRB5_ERR_C} ${GEN_KRB5_ERR_H}
GEN_ET=		${GEN_KRB5_ERR_C}
GENI_ET=	${GEN_KRB5_ERR_H}

${GEN_KRB5_ERR:[2..-1]}: .NOMETA
${GEN_KRB5_ERR_H}: ${GEN_KRB5_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_KRB5_ERR_C}: ${GEN_KRB5_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

GEN_K5E1_ERR=	k5e1_err.et
GEN_K5E1_ERR_C=	${GEN_K5E1_ERR:S/.et$/.c/}
GEN_K5E1_ERR_H=	${GEN_K5E1_ERR:S/.et$/.h/}
CLEANFILES+=	${GEN_K5E1_ERR_C} ${GEN_K5E1_ERR_H}
GEN_ET+=	${GEN_K5E1_ERR_C}
GENI_ET+=	${GEN_K5E1_ERR_H}

${GEN_K5E1_ERR:[2..-1]}: .NOMETA
${GEN_K5E1_ERR_H}: ${GEN_K5E1_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_K5E1_ERR_C}: ${GEN_K5E1_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

GEN_KDB5_ERR=	kdb5_err.et
GEN_KDB5_ERR_C=	${GEN_KDB5_ERR:S/.et$/.c/}
GEN_KDB5_ERR_H=	${GEN_KDB5_ERR:S/.et$/.h/}
CLEANFILES+=	${GEN_KDB5_ERR_C} ${GEN_KDB5_ERR_H}
GEN_ET+=	${GEN_KDB5_ERR_C}
GENI_ET+=	${GEN_KDB5_ERR_H}

${GEN_KDB5_ERR:[2..-1]}: .NOMETA
${GEN_KDB5_ERR_H}: ${GEN_KDB5_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_KDB5_ERR_C}: ${GEN_KDB5_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

GEN_KRB5M_ERR=	kv5m_err.et
GEN_KRB5M_ERR_C=	${GEN_KRB5M_ERR:S/.et$/.c/}
GEN_KRB5M_ERR_H=	${GEN_KRB5M_ERR:S/.et$/.h/}
CLEANFILES+=	${GEN_KRB5M_ERR_C} ${GEN_KRB5M_ERR_H}
GEN_ET+=	${GEN_KRB5M_ERR_C}
GENI_ET+=	${GEN_KRB5M_ERR_H}

${GEN_KRB5M_ERR:[2..-1]}: .NOMETA
${GEN_KRB5M_ERR_H}: ${GEN_KRB5M_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_KRB5M_ERR_C}: ${GEN_KRB5M_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

GEN_KRB524_ERR=	krb524_err.et
GEN_KRB524_ERR_C=	${GEN_KRB524_ERR:S/.et$/.c/}
GEN_KRB524_ERR_H=	${GEN_KRB524_ERR:S/.et$/.h/}
CLEANFILES+=	${GEN_KRB524_ERR_C} ${GEN_KRB524_ERR_H}
GEN_ET+=	${GEN_KRB524_ERR_C}
GENI_ET+=	${GEN_KRB524_ERR_H}

${GEN_KRB524_ERR:[2..-1]}: .NOMETA
${GEN_KRB524_ERR_H}: ${GEN_KRB524_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_KRB524_ERR_C}: ${GEN_KRB524_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

GEN_ASN1_ERR=	asn1_err.et
GEN_ASN1_ERR_C=	${GEN_ASN1_ERR:S/.et$/.c/}
GEN_ASN1_ERR_H=	${GEN_ASN1_ERR:S/.et$/.h/}
CLEANFILES+=	${GEN_ASN1_ERR_C} ${GEN_ASN1_ERR_H}
GEN_ET+=	${GEN_ASN1_ERR_C}
GENI_ET+=	${GEN_ASN1_ERR_H}

${GEN_ASN1_ERR:[2..-1]}: .NOMETA
${GEN_ASN1_ERR_H}: ${GEN_ASN1_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_ASN1_ERR_C}: ${GEN_ASN1_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

SRCS+=	${GEN_ET}
