#
# $FreeBSD: src/gnu/usr.bin/perl/suidperl/Makefile,v 1.13.2.1 2000/06/04 23:19:48 ache Exp $
#

PROG=	suidperl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR} -DIAMSUID
SRCS=	perlmain.c sperl.c config.h
NOSHARED= no
DYNALOADER= lib/auto/DynaLoader/DynaLoader.a
DPADD=	${DYNALOADER} ${LIBPERL} ${LIBM} ${LIBCRYPT} ${LIBMD}
LDADD=	-Wl,-E ${DYNALOADER} -lperl -lm -lcrypt -lmd
BINOWN=	root
BINMODE=4511
LINKS=	${BINDIR}/${PROG} ${BINDIR}/sperl5.00503

CLEANFILES=	Config.pm perlmain.c sperl.c \
		autosplit pod2man.PL pod2man ext.libs \
		man3pages

STATIC_EXT=	DynaLoader/DynaLoader

.include <bsd.prog.mk>

MAKEMAKER_ARGS=	INSTALLDIRS=perl PERL_SRC=${.OBJDIR} \
		INSTALLMAN3DIR=${DESTDIR}/usr/share/perl/man3 \
		PERL=perl FULLPERL=perl DEFINE=-I${DESTDIR}/usr/include

${PROG}: Config.pm autosplit lib/auto/DynaLoader/DynaLoader.a

sperl.c: perl.c
	@ln -sf ${.OODATE} ${.TARGET}

Config.pm: links ${PERL5SRC}/configpm myconfig config.sh
	miniperl ${PERL5SRC}/configpm \
		Config.pm Porting/Glossary myconfig config.sh
	@cd lib ; ln -sf ../${.TARGET}

perlmain.c: config.sh writemain config.h
	sh writemain lib/auto/DynaLoader/DynaLoader.a > ${.TARGET}

autosplit: links Config.pm lib/*.pm lib/*/*.pm
	miniperl -I${.OBJDIR}/lib \
		-e 'use AutoSplit; autosplit_lib_modules(@ARGV)' \
		lib/*.pm lib/*/*.pm
	touch ${.TARGET}

.for I in ${STATIC_EXT}
ext/${I:H}/Makefile: links ext/${I:H}/Makefile.PL cflags Config.pm config.h
	@cd ext/${I:H}; \
	miniperl -I${.OBJDIR}/lib Makefile.PL ${MAKEMAKER_ARGS} \
		LINKTYPE=static LIBS="-lperl -lm"; \
	make -B config PERL_SRC=${.OBJDIR}

lib/auto/${I}.a: links ext/${I:H}/Makefile
	@cd ext/${I:H}; \
	make -B all PERL_SRC=${.OBJDIR}

all:	lib/auto/${I}.a

STATICS+=	lib/auto/${I}.a
.endfor

.PATH:	${PERL5SRC}
