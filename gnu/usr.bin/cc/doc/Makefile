# $Id$

INFO = gcc cpp reno gxxint

gcc.info: gcc.texi invoke.texi install.texi extend.texi rtl.texi md.texi \
	  tm.texi

reno.info: reno.texi templates.texi gpcompare.texi

.include "../Makefile.inc"
MAKEINFOFLAGS+= -I ${GCCDIR} -I ${GCCDIR}/cp

.include <bsd.info.mk>
