# $FreeBSD: src/gnu/usr.bin/cc/doc/Makefile,v 1.7.2.1 1999/08/29 14:32:43 peter Exp $

INFO = gcc cpp reno gxxint

INFOSECTION=    "Gcc Documentation"
INFOENTRY_gcc=  "* gcc: (gcc).			The GNU C compiler."
INFOENTRY_cpp=  "* cpp: (cpp).			The GNU C pre-processor."
INFOENTRY_gxxint= "* gxxint: (gxxint).		The GNU compiler family internal documentation."

gcc.info: gcc.texi invoke.texi install.texi extend.texi rtl.texi md.texi \
	  tm.texi

reno.info: reno.texi templates.texi gpcompare.texi

MAKEINFOFLAGS+= -I ${GCCDIR} -I ${GCCDIR}/cp

.include <bsd.info.mk>
