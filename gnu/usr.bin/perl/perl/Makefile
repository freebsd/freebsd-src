#
# $Id: Makefile,v 1.11 1998/09/14 18:34:19 markm Exp $
#

PROG=	perl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR}
SRCS=	perlmain.c config.h
NOSHARED= no
DPADD=	lib/auto/DynaLoader/DynaLoader.a ${LIBPERL} ${LIBM} ${LIBCRYPT}
LDADD=	lib/auto/DynaLoader/DynaLoader.a -lperl -lm -lcrypt
LINKS=	${BINDIR}/${PROG} ${BINDIR}/perl5 \
	${BINDIR}/${PROG} ${BINDIR}/perl5.00502

CLEANFILES=	config.h config.sh config_h.sh writemain writemain.sh \
		Config.pm cflags cflags.sh myconfig perlmain.c miniperlmain.c \
		${.OBJDIR}/regcomp.c ${.OBJDIR}/regexec.c
CLEANDIRS=	lib ext Porting hints

.include <bsd.prog.mk>

HEADERS=EXTERN.h INTERN.h XSUB.h XSlock.h av.h bytecode.h byterun.h \
	cc_runtime.h cop.h cv.h dosish.h embed.h embedvar.h fakethr.h form.h \
	gv.h handy.h hv.h intrpvar.h iperlsys.h keywords.h mg.h nostdio.h \
	objXSUB.h objpp.h op.h opcode.h patchlevel.h perl.h perlio.h \
	perlsdio.h perlsfio.h perlvars.h perly.h pp.h pp_proto.h proto.h \
	regcomp.h regexp.h regnodes.h scope.h sv.h thrdvar.h thread.h \
	unixish.h util.h

DYNAMIC_EXT=	B/B DB_File/DB_File Data/Dumper/Dumper Fcntl/Fcntl IO/IO \
		IPC/SysV/SysV NDBM_File/NDBM_File Opcode/Opcode POSIX/POSIX \
		SDBM_File/SDBM_File Socket/Socket attrs/attrs re/re

${PROG}: lib/auto/DynaLoader/DynaLoader.a

.ORDER: lib/auto/DynaLoader/DynaLoader.a ${DYNAMIC_EXT}

config.h: config_h.sh config.sh
	sh config_h.sh

config.sh: ${.CURDIR}/../libperl/config.SH-${OBJFORMAT}.${MACHINE_ARCH}
	cp ${.OODATE} ${.TARGET}

config_h.sh: config_h.SH
	cp ${.OODATE} ${.TARGET}

cflags.sh: cflags.SH
	cp ${.OODATE} ${.TARGET}

cflags: cflags.sh
	sh ${.OODATE}

lib:
	@rm -rf lib
	cp -rp ${PERL5SRC}/lib .

ext:
	@rm -rf ext
	cp -rp ${PERL5SRC}/ext .

Porting:
	@rm -rf Porting
	cp -rp ${PERL5SRC}/Porting .

hints:
	@rm -rf hints
	cp -rp ${PERL5SRC}/hints .

lib/re.pm: lib ext hints ext/re/re.pm
	cat ${PERL5SRC}/ext/re/re.pm > ${.OBJDIR}/lib/re.pm

lib/Config.pm: Config.pm
	cp -p ${.OODATE} ${.TARGET}

myconfig: ${PERL5SRC}/myconfig
	ln -sf ${.OODATE} ${.TARGET}

Config.pm: myconfig config.sh lib ext hints Porting lib/re.pm
	miniperl ${PERL5SRC}/configpm \
		Config.pm Porting/Glossary myconfig config.sh

DEPEND_H=

.for I in ${HEADERS}
${.OBJDIR}/${I}: ${I}
	@ln -sf ${.OODATE} ${.TARGET}

CLEANFILES+= ${.OBJDIR}/${I}
DEPEND_H+= ${.OBJDIR}/${I}
.endfor

lib/auto/DynaLoader/DynaLoader.a: cflags lib/Config.pm ${DEPEND_H}
	miniperl -I${.OBJDIR}/lib \
		-e 'use AutoSplit; autosplit_lib_modules(@ARGV)' \
		lib/*.pm lib/*/*.pm
	cd ext/DynaLoader; \
	miniperl -I${.OBJDIR}/lib Makefile.PL \
		INSTALLDIRS=perl PERL_SRC=${.OBJDIR}; \
	make -B config PERL_SRC=${.OBJDIR}; \
	make -B all LINKTYPE=static PERL_SRC=${.OBJDIR}

${.OBJDIR}/miniperlmain.c: miniperlmain.c
	@ln -sf ${.OODATE} ${.TARGET}

perlmain.c: ${.OBJDIR}/miniperlmain.c config.sh writemain config.h
	sh writemain lib/auto/DynaLoader/DynaLoader.a > ${.TARGET}

writemain.sh: writemain.SH
	@ln -sf ${.OODATE} ${.TARGET}

writemain: writemain.sh
	sh ${.OODATE}

${.OBJDIR}/regcomp.c: ${PERL5SRC}/regcomp.c
	@ln -sf ${.OODATE} ${.TARGET}

${.OBJDIR}/regexec.c: ${PERL5SRC}/regexec.c
	@ln -sf ${.OODATE} ${.TARGET}

.for I in ${DYNAMIC_EXT}
lib/auto/${I}.so:	${PROG} ${.OBJDIR}/regcomp.c ${.OBJDIR}/regexec.c
	cd ext/${I:H}; \
	miniperl -I${.OBJDIR}/lib Makefile.PL \
		INSTALLDIRS=perl PERL_SRC=${.OBJDIR} LIBS="-lperl"; \
	make -B config PERL_SRC=${.OBJDIR}; \
	make -B all LINKTYPE=dynamic PERL_SRC=${.OBJDIR}

DYNAMICS+=	lib/auto/${I}.so
.endfor

all:	${DYNAMICS}

install:
	miniperl ${.CURDIR}/install_perl_libs -d ${DESTDIR}

.PATH:	${PERL5SRC}
