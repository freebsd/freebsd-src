#
# $Id$
#

PROG=	suidperl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR} -DIAMSUID
SRCS=	perlmain.c sperl.c config.h
LDADD=	lib/auto/DynaLoader/DynaLoader.a -lperl -lm -lcrypt
BINOWN=	root
BINMODE=4511

CLEANFILES=	config.h config.sh config_h.sh writemain writemain.sh \
		Config.pm cflags cflags.sh myconfig perlmain.c miniperlmain.c
CLEANDIRS=	lib ext Porting hints

.include <bsd.prog.mk>

HEADERS=EXTERN.h INTERN.h XSUB.h XSlock.h av.h bytecode.h byterun.h \
	cc_runtime.h cop.h cv.h dosish.h embed.h embedvar.h fakethr.h form.h \
	gv.h handy.h hv.h intrpvar.h iperlsys.h keywords.h mg.h nostdio.h \
	objXSUB.h objpp.h op.h opcode.h patchlevel.h perl.h perlio.h \
	perlsdio.h perlsfio.h perlvars.h perly.h pp.h pp_proto.h proto.h \
	regcomp.h regexp.h regnodes.h scope.h sv.h thrdvar.h thread.h \
	unixish.h util.h

${PROG}: lib/auto/DynaLoader/DynaLoader.a

config.h: config_h.sh config.sh
	sh config_h.sh

config.sh: ${.CURDIR}/../libperl/config.SH
	@ln -sf ${.OODATE} ${.TARGET}

config_h.sh: config_h.SH
	@ln -sf ${.OODATE} ${.TARGET}

cflags.sh: cflags.SH
	@ln -sf ${.OODATE} ${.TARGET}

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
	@cd lib; ln -s ../${.OODATE}

myconfig: ${PERL5SRC}/myconfig
	@ln -sf ${.OODATE} ${.TARGET}

Config.pm: myconfig lib ext hints Porting lib/re.pm
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
	cd ext/DynaLoader; \
	miniperl -I../../lib Makefile.PL \
		INSTALLDIRS=perl PERL_SRC=${.OBJDIR}; \
	make config PERL_SRC=${.OBJDIR}; \
	make all LINKTYPE=static PERL_SRC=${.OBJDIR}

${.OBJDIR}/miniperlmain.c: miniperlmain.c
	@ln -sf ${.OODATE} ${.TARGET}

sperl.c: perl.c
	@ln -sf ${.OODATE} ${.TARGET}

perlmain.c: ${.OBJDIR}/miniperlmain.c config.sh writemain
	sh writemain lib/auto/DynaLoader/DynaLoader.a > ${.TARGET}

writemain.sh: writemain.SH
	@ln -sf ${.OODATE} ${.TARGET}

writemain: writemain.sh
	sh ${.OODATE}

.PATH:	${PERL5SRC}
