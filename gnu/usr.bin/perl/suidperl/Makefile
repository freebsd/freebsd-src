#
# $Id: Makefile,v 1.7 1999/05/02 15:29:52 markm Exp $
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
LINKS=	${BINDIR}/${PROG} ${BINDIR}/sperl5.00503


CLEANFILES=	config.h config.sh config_h.sh writemain writemain.sh \
		Config.pm cflags cflags.sh myconfig perlmain.c miniperlmain.c \
		autosplit regcomp.c regexec.c ext.libs sperl.c

STATIC_EXT=	DynaLoader/DynaLoader

.include <bsd.prog.mk>

${PROG}: linkfarm Config.pm autosplit lib/auto/DynaLoader/DynaLoader.a

sperl.c: perl.c
	@ln -sf ${.OODATE} ${.TARGET}

config.h: linkfarm
	sh config_h.sh

cflags: linkfarm
	sh cflags.sh

Config.pm: linkfarm
	miniperl ${PERL5SRC}/configpm \
		Config.pm Porting/Glossary myconfig config.sh
	cd lib ; ln -sf ../${.TARGET}

perlmain.c: linkfarm writemain config.h
	sh writemain lib/auto/DynaLoader/DynaLoader.a > ${.TARGET}

writemain: linkfarm
	sh writemain.sh

autosplit: linkfarm Config.pm lib/*.pm lib/*/*.pm
	miniperl -I${.OBJDIR}/lib \
		-e 'use AutoSplit; autosplit_lib_modules(@ARGV)' \
		lib/*.pm lib/*/*.pm
	touch ${.TARGET}

.for I in ${STATIC_EXT}
ext/${I:H}/Makefile: linkfarm ext/${I:H}/Makefile.PL cflags Config.pm
	cd ext/${I:H}; \
	miniperl -I${.OBJDIR}/lib Makefile.PL \
		LINKTYPE=static INSTALLDIRS=perl PERL_SRC=${.OBJDIR} \
		LIBS="-lperl" INSTALLMAN3DIR=${DESTDIR}/usr/share/perl/man3; \
	make -B config PERL_SRC=${.OBJDIR}

lib/auto/${I}.a: linkfarm ext/${I:H}/Makefile
	cd ext/${I:H}; \
	make -B all PERL_SRC=${.OBJDIR}

all:	lib/auto/${I}.a

STATICS+=	lib/auto/${I}.a
.endfor

.PATH:	${PERL5SRC}
