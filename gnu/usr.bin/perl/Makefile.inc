# $FreeBSD$

PERL5SRC?= ${.CURDIR}/../../../../contrib/perl5
PERL5LIBSRC?= ${.CURDIR}/../libperl
MINIPERL?=	miniperl

BINDIR?=	/usr/bin

SHLIB_MAJOR?=	4

VERSION=	5.6.0

DYNALOADER= lib/auto/DynaLoader/DynaLoader.a

HEADERS=EXTERN.h INTERN.h XSUB.h av.h cc_runtime.h cop.h cv.h dosish.h \
	embed.h embedvar.h fakethr.h form.h gv.h handy.h hv.h intrpvar.h \
	iperlsys.h keywords.h mg.h nostdio.h objXSUB.h op.h opcode.h \
	opnames.h patchlevel.h perl.h perlapi.h perlio.h perlsdio.h \
	perlsfio.h perlvars.h perly.h pp.h pp_proto.h proto.h regcomp.h \
	regexp.h regnodes.h scope.h sv.h thrdvar.h thread.h unixish.h \
	utf8.h util.h warnings.h

DEPEND_H=

.for I in ${HEADERS}
${.OBJDIR}/${I}: ${I}
	@ln -sf ${.OODATE} ${.TARGET}

CLEANFILES+= ${.OBJDIR}/${I}
DEPEND_H+= ${.OBJDIR}/${I}
.endfor

LDFLAGS+=	-L${.OBJDIR}/../libperl

links: ${DEPEND_H}
	@for d in ${FARMDIRS} ; do \
		rm -rf $${d} ;\
		for i in `cd ${PERL5SRC}; find $${d} -type d -a \! -name CVS` ;\
		do \
			mkdir -p $${i} ;\
		done ;\
		for i in `cd ${PERL5SRC}; find $${d} -type f | grep -v CVS` ;\
		do \
			ln -s ${PERL5SRC}/$${i} $${i} ;\
		done ;\
	done
	@ln -sf ${PERL5SRC}/ext/File/Glob/Glob.pm lib/File/Glob.pm
	@-mkdir lib/auto
	@-mkdir build
	@ln -sf ${PERL5SRC}/config_h.SH config_h.sh
	@ln -sf ${PERL5SRC}/cflags.SH cflags.sh
	@ln -sf ${PERL5SRC}/ext/re/re.pm lib/re.pm
	@ln -sf ${PERL5SRC}/myconfig.SH myconfig.SH
	@ln -sf ${PERL5SRC}/miniperlmain.c miniperlmain.c
	@ln -sf ${PERL5SRC}/writemain.SH writemain.sh
	@ln -sf ${PERL5SRC}/regcomp.c regcomp.c
	@ln -sf ${PERL5SRC}/regexec.c regexec.c
	@ln -sf ${PERL5LIBSRC}/config.SH-${THREAD}${OBJFORMAT}.${MACHINE_ARCH} \
		config.sh
	@touch ${.TARGET}

scripts: links
	@sh config_h.sh
	@sh cflags.sh
	@sh writemain.sh
	@sh myconfig.SH
	@${MINIPERL} ${PERL5SRC}/configpm \
		Config.pm Porting/Glossary myconfig
	@ln -sf ../Config.pm lib/Config.pm
	@touch ${.TARGET}

.if !defined(NO_PERL_SCRIPT_MAKE)
config.h:	scripts
.endif

CLEANFILES+=	links scripts
CLEANFILES+=	config.h config.sh config_h.sh Config.pm
CLEANFILES+=	myconfig myconfig.SH
CLEANFILES+=	regcomp.c regexec.c miniperlmain.c
CLEANFILES+=	writemain writemain.sh cflags cflags.sh
CLEANDIRS+=	lib ext Porting hints build

FARMDIRS=	lib ext Porting hints

MAKEMAKER_ARGS=	INSTALLDIRS=perl PERL_SRC=${.OBJDIR} \
		INSTALLMAN3DIR=${DESTDIR}/usr/share/perl/man3 \
		PERL=${MINIPERL} FULLPERL=perl DEFINE=-I${DESTDIR}/usr/include \
		DEFINE=-DPERL_CORE

CFLAGS+=	'-DAPPLLIB_EXP="/usr/libdata/perl/BSDPAN"'

.if defined(PERL_THREADED) && ${PERL_THREADED} == "true"
CFLAGS+=        -pthread
THREAD=		threads-
.endif

.if defined(PERL_DEBUGGING) && ${PERL_DEBUGGING} == "true"
CFLAGS+=        -DDEBUGGING
.endif

autosplit: scripts lib/*.pm lib/*/*.pm
	${MINIPERL} -I${.OBJDIR}/lib \
		-e 'use AutoSplit; autosplit_lib_modules(@ARGV)' \
		lib/*.pm lib/*/*.pm
	@touch ${.TARGET}

${DYNALOADER}:	scripts \
		ext/DynaLoader/DynaLoader.pm ext/DynaLoader/XSLoader.pm \
		ext/DynaLoader/Makefile
	@cd ext/DynaLoader; \
	make -B all PERL_SRC=${.OBJDIR}

ext/DynaLoader/DynaLoader.pm: scripts ext/DynaLoader/DynaLoader_pm.PL
	@cd ext/DynaLoader; \
	${MINIPERL} -I${.OBJDIR} -I${.OBJDIR}/lib DynaLoader_pm.PL DynaLoader.pm

ext/DynaLoader/XSLoader.pm: scripts ext/DynaLoader/XSLoader_pm.PL
	@cd ext/DynaLoader; \
	${MINIPERL} -I${.OBJDIR} -I${.OBJDIR}/lib XSLoader_pm.PL XSLoader.pm

ext/DynaLoader/Makefile: scripts ext/DynaLoader/Makefile.PL
	@cd ext/DynaLoader; \
	${MINIPERL} -I${.OBJDIR} -I${.OBJDIR}/lib Makefile.PL ${MAKEMAKER_ARGS} \
		LINKTYPE=static LIBS="-lperl -lm"; \
	make -B config PERL_SRC=${.OBJDIR}
