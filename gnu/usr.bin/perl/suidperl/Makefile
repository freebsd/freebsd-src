#
# $FreeBSD$
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

CLEANFILES=	Config.pm perlmain.c sperl.c \
		autosplit pod2man.PL pod2man ext.libs \
		man3pages

STATIC_EXT=	DynaLoader/DynaLoader

.include <bsd.prog.mk>

${PROG}: Config.pm autosplit lib/auto/DynaLoader/DynaLoader.a

sperl.c: perl.c
	@ln -sf ${.OODATE} ${.TARGET}

Config.pm: links ${PERL5SRC}/configpm myconfig config.sh
	miniperl ${PERL5SRC}/configpm \
		Config.pm Porting/Glossary myconfig config.sh
	cd lib ; ln -sf ../${.TARGET}

perlmain.c: config.sh writemain config.h
	sh writemain lib/auto/DynaLoader/DynaLoader.a > ${.TARGET}

autosplit: links Config.pm lib/*.pm lib/*/*.pm
	miniperl -I${.OBJDIR}/lib \
		-e 'use AutoSplit; autosplit_lib_modules(@ARGV)' \
		lib/*.pm lib/*/*.pm
	touch ${.TARGET}

.for I in ${STATIC_EXT}
ext/${I:H}/Makefile: links ext/${I:H}/Makefile.PL cflags Config.pm
	cd ext/${I:H}; \
	miniperl -I${.OBJDIR}/lib Makefile.PL \
		LINKTYPE=static INSTALLDIRS=perl PERL_SRC=${.OBJDIR} \
		LIBS="-lperl" INSTALLMAN3DIR=${DESTDIR}/usr/share/perl/man3; \
	make -B config PERL_SRC=${.OBJDIR}

lib/auto/${I}.a: links ext/${I:H}/Makefile
	cd ext/${I:H}; \
	make -B all PERL_SRC=${.OBJDIR}

all:	lib/auto/${I}.a

STATICS+=	lib/auto/${I}.a
.endfor

.PATH:	${PERL5SRC}
