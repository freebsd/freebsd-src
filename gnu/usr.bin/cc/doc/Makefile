# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

INFO=	gcc cpp gxxint

INFOSECTION=    "Gcc Documentation"
INFOENTRY_gcc=  "* gcc: (gcc).			The EGCS GNU C compiler."
INFOENTRY_cpp=  "* cpp: (cpp).			The EGCS GNU C pre-processor."
INFOENTRY_gxxint= "* gxxint: (gxxint).		The EGCS GNU compiler family internal documentation."

gcc.info: gcc.texi invoke.texi install.texi extend.texi gcov.texi rtl.texi \
	  md.texi tm.texi

MAKEINFOFLAGS+= -I ${GCCDIR} -I ${GCCDIR}/cp

.include <bsd.info.mk>
