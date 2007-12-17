# $FreeBSD$

ARCH_SRCS = e_sqrt.S e_sqrtf.S s_llrint.S s_llrintf.S \
	    s_logbl.S s_lrint.S s_lrintf.S \
	    s_remquo.S s_remquof.S s_scalbn.S s_scalbnf.S s_scalbnl.S
LDBL_PREC = 64
SYM_MAPS += ${.CURDIR}/amd64/Symbol.map
