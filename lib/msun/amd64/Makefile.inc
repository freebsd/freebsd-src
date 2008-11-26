# $FreeBSD: src/lib/msun/amd64/Makefile.inc,v 1.4.12.1 2008/10/02 02:57:24 kensmith Exp $

ARCH_SRCS = e_sqrt.S e_sqrtf.S s_llrint.S s_llrintf.S s_lrint.S s_lrintf.S \
	    s_remquo.S s_remquof.S s_scalbn.S s_scalbnf.S s_scalbnl.S
LDBL_PREC = 64
