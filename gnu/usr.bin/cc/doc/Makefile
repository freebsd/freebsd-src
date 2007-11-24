# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/doc

INFO=	gcc cpp gccint cppinternals

INFOSECTION=    "Gcc Documentation"
INFOENTRY_gcc=  "* gcc: (gcc).			The GNU C compiler."
INFOENTRY_cpp=  "* cpp: (cpp).			The GNU C pre-processor."
INFOENTRY_gccint= "* gccint: (gccint).		The GNU compiler family internal documentation."
INFOENTRY_cppinternals= "* cppinternals: (cppinternals).		The GNU compiler preprocessor internal documentation."

gcc.info: gcc.texi include/gcc-common.texi frontends.texi standards.texi \
    invoke.texi extend.texi md.texi objc.texi gcov.texi trouble.texi \
    bugreport.texi service.texi contribute.texi compat.texi \
    include/funding.texi gnu.texi include/gpl.texi include/fdl.texi \
    contrib.texi cppenv.texi cppopts.texi

gccint.info: gccint.texi include/gcc-common.texi contribute.texi \
    makefile.texi configterms.texi portability.texi interface.texi \
    passes.texi c-tree.texi rtl.texi md.texi tm.texi hostconfig.texi \
    fragments.texi configfiles.texi collect2.texi headerdirs.texi \
    include/funding.texi gnu.texi include/gpl.texi include/fdl.texi \
    contrib.texi languages.texi sourcebuild.texi gty.texi libgcc.texi

cpp.info: cpp.texi include/fdl.texi cppenv.texi cppopts.texi

MAKEINFOFLAGS+= -I ${GCCDIR}/doc -I ${GCCDIR}/doc/include

.include <bsd.info.mk>
