#
# $Id: Makefile,v 1.3 1998/09/16 17:25:52 markm Exp $
#

PROG=	suidperl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR} -DIAMSUID
SRCS=	perlmain.c sperl.c config.h
NOSHARED= no
DPADD=	lib/auto/DynaLoader/DynaLoader.a ${LIBPERL} ${LIBM} ${LIBCRYPT}
LDADD=	lib/auto/DynaLoader/DynaLoader.a -lperl -lm -lcrypt
BINOWN=	root
BINMODE=4511

CLEANFILES=	config.h config.sh config_h.sh writemain writemain.sh \
		Config.pm cflags cflags.sh myconfig perlmain.c miniperlmain.c
CLEANDIRS=	lib ext Porting hints

.include <bsd.prog.mk>

STATIC_EXT=	DynaLoader/DynaLoader

${PROG}: Config.pm lib/auto/DynaLoader/DynaLoader.a

lib/auto/DynaLoader/DynaLoader.a: Config.pm autosplit

config.h: config_h.sh config.sh
	sh config_h.sh

config.sh: ${.CURDIR}/../libperl/config.SH-${OBJFORMAT}.${MACHINE_ARCH}
	ln -sf ${.OODATE} ${.TARGET}

config_h.sh: config_h.SH
	ln -sf ${.OODATE} ${.TARGET}

cflags.sh: cflags.SH
	ln -sf ${.OODATE} ${.TARGET}

cflags: cflags.sh
	sh ${.OODATE}

lib/re.pm: lib ext hints ext/re/re.pm
	cat ${PERL5SRC}/ext/re/re.pm > ${.OBJDIR}/lib/re.pm

lib/Config.pm: Config.pm
	cp -p ${.OODATE} ${.TARGET}

myconfig: ${PERL5SRC}/myconfig
	ln -sf ${.OODATE} ${.TARGET}

Config.pm: myconfig config.sh lib ext hints Porting lib/re.pm
	miniperl ${PERL5SRC}/configpm \
		Config.pm Porting/Glossary myconfig config.sh

${.OBJDIR}/miniperlmain.c: miniperlmain.c
	@ln -sf ${.OODATE} ${.TARGET}

sperl.c: perl.c
	@ln -sf ${.OODATE} ${.TARGET}

perlmain.c: ${.OBJDIR}/miniperlmain.c config.sh writemain config.h
	sh writemain lib/auto/DynaLoader/DynaLoader.a > ${.TARGET}

writemain.sh: writemain.SH
	@ln -sf ${.OODATE} ${.TARGET}

writemain: writemain.sh
	sh ${.OODATE}

autosplit: lib ext Porting hints
	miniperl -I${.OBJDIR}/lib \
		-e 'use AutoSplit; autosplit_lib_modules(@ARGV)' \
		lib/*.pm lib/*/*.pm

.for I in ${STATIC_EXT}
lib/auto/${I}.a: cflags lib/Config.pm ${DEPEND_H}
	cd ext/${I:H}; \
	miniperl -I${.OBJDIR}/lib Makefile.PL \
		INSTALLDIRS=perl PERL_SRC=${.OBJDIR}; \
	make -B config PERL_SRC=${.OBJDIR}; \
	make -B all LINKTYPE=static PERL_SRC=${.OBJDIR}

STATICS+=	lib/auto/${I}.a
.endfor

.PATH:	${PERL5SRC}
