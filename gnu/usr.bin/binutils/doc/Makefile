# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas/doc ${SRCDIR}/ld ${SRCDIR}/bfd/doc

INFO=	as ld gasp binutils
INFOSECTION=	"Programming & development tools."
INFOENTRY_as=	"* As: (as).            The GNU assembler."
INFOENTRY_gasp=	"* Gasp: (gasp).        The GNU Assembler Macro Preprocessor."
INFOENTRY_ld=	"* Ld: (ld).		The GNU linker."
INFOENTRY_binutils= "* Binutils: (binutils).		The GNU Binary Utilities."

MAKEINFOFLAGS+= --no-validate
MAKEINFOFLAGS+= -I ${SRCDIR}/gas/doc -I ${SRCDIR}/ld -I ${SRCDIR}/bfd/doc
MAKEINFOFLAGS+= -I ${SRCDIR}/binutils

CLEANFILES=	configdoc.texi config.texi gasver.texi ldver.texi

as.info:	as.texinfo asconfig.texi c-i386.texi gasver.texi
ld.info:	ld.texinfo bfdsumm.texi ldver.texi configdoc.texi

configdoc.texi: gen-doc.texi
	ln -sf ${.ALLSRC} ${.TARGET}

.PATH: ${SRCDIR}/binutils/doc
binutils.info: binutils.texi config.texi

config.texi gasver.texi ldver.texi:
	echo "@set VERSION ${VERSION}" > ${.TARGET}

.include <bsd.info.mk>
