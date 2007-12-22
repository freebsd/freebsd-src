# $FreeBSD$

.include "../Makefile.inc"
.include "../Makefile.ver"

.PATH: ${GCCDIR}/doc ${GCCDIR}/doc/include

INFO=	gcc cpp gccint cppinternals

INFOSECTION=    "Gcc Documentation"
INFOENTRY_gcc=  "* gcc: (gcc).			The GNU C compiler."
INFOENTRY_cpp=  "* cpp: (cpp).			The GNU C pre-processor."
INFOENTRY_gccint= "* gccint: (gccint).		The GNU compiler family internal documentation."
INFOENTRY_cppinternals= "* cppinternals: (cppinternals).		The GNU compiler preprocessor internal documentation."

gcc.info: gcc.texi gcc-common.texi gcc-vers.texi frontends.texi		\
	 standards.texi invoke.texi extend.texi md.texi objc.texi	\
	 gcov.texi trouble.texi bugreport.texi service.texi		\
	 contribute.texi compat.texi funding.texi gnu.texi gpl.texi	\
	 fdl.texi contrib.texi cppenv.texi cppopts.texi			\
	 implement-c.texi

gccint.info: gccint.texi gcc-common.texi gcc-vers.texi			\
	 contribute.texi makefile.texi configterms.texi options.texi	\
	 portability.texi interface.texi passes.texi c-tree.texi	\
	 rtl.texi md.texi tm.texi hostconfig.texi fragments.texi	\
	 configfiles.texi collect2.texi headerdirs.texi funding.texi	\
	 gnu.texi gpl.texi fdl.texi contrib.texi languages.texi		\
	 sourcebuild.texi gty.texi libgcc.texi cfg.texi tree-ssa.texi	\
	 loop.texi

cpp.info: cpp.texi fdl.texi cppenv.texi cppopts.texi gcc-common.texi \
	gcc-vers.texi

cppinternals.info:  cppinternals.texi gcc-common.texi gcc-vers.texi

MAKEINFOFLAGS+= -I ${GCCDIR}/doc -I ${GCCDIR}/doc/include

gcc-vers.texi:
	(echo "@set version-GCC $(BASEVER)"; \
	if [ "$(DEVPHASE)" = "experimental" ]; then \
		echo "@set DEVELOPMENT"; \
	else \
		echo "@clear DEVELOPMENT"; \
	fi; \
	echo "@set srcdir $(GCCLIB)/libiberty" ) > ${.TARGET}

CLEANFILES+=    gcc-vers.texi

.include <bsd.info.mk>
