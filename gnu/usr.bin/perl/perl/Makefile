#
# $Id: Makefile,v 1.12 1998/09/16 17:25:52 markm Exp $
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

STATIC_EXT=	DynaLoader/DynaLoader

DYNAMIC_EXT=	B/B DB_File/DB_File Data/Dumper/Dumper Fcntl/Fcntl IO/IO \
		IPC/SysV/SysV NDBM_File/NDBM_File Opcode/Opcode POSIX/POSIX \
		SDBM_File/SDBM_File Socket/Socket attrs/attrs re/re

#NONXS_EXT=	Errno/pm_to_blib

${PROG}: Config.pm lib/auto/DynaLoader/DynaLoader.a

lib/auto/DynaLoader/DynaLoader.a: Config.pm autosplit

.ORDER: ${STATIC_EXT} ${DYNAMIC_EXT} ${NONXS_EXT}

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

autosplit: lib ext Porting hints
	miniperl -I${.OBJDIR}/lib \
		-e 'use AutoSplit; autosplit_lib_modules(@ARGV)' \
		lib/*.pm lib/*/*.pm

.for I in ${STATIC_EXT}
lib/auto/${I}.a: ext/${I:H}/Makefile.PL cflags lib/Config.pm ${DEPEND_H}
	cd ext/${I:H}; \
	miniperl -I${.OBJDIR}/lib Makefile.PL \
		INSTALLDIRS=perl PERL_SRC=${.OBJDIR}; \
	make -B config PERL_SRC=${.OBJDIR}; \
	make -B all LINKTYPE=static PERL_SRC=${.OBJDIR}

all:	lib/auto/${I}.a

STATICS+=	lib/auto/${I}.a
.endfor

.for I in ${DYNAMIC_EXT}
lib/auto/${I}.so: ${PROG} ext/${I:H}/Makefile.PL cflags lib/Config.pm \
		${DEPEND_H} ${.OBJDIR}/regcomp.c ${.OBJDIR}/regexec.c
	cd ext/${I:H}; \
	miniperl -I${.OBJDIR}/lib Makefile.PL \
		INSTALLDIRS=perl PERL_SRC=${.OBJDIR} LIBS="-lperl"; \
	make -B config PERL_SRC=${.OBJDIR}; \
	make -B all LINKTYPE=dynamic PERL_SRC=${.OBJDIR}

all:	lib/auto/${I}.so

DYNAMICS+=	lib/auto/${I}.so
.endfor

#.for I in ${NONXS_EXT}
#lib/auto/${I}: ${PROG} ext/${I:H}/Makefile.PL cflags lib/Config.pm
#	cd ext/${I:H}; \
#	miniperl -I${.OBJDIR}/lib Makefile.PL \
#		INSTALLDIRS=perl PERL_SRC=${.OBJDIR}; \
#	make -B config PERL_SRC=${.OBJDIR}; \
#	make -B all LINKTYPE=nonxs PERL_SRC=${.OBJDIR}
#
#NONXSS+=	lib/auto/${I}
#.endfor

#all:	${DYNAMICS} ${NONXSS}

install:
	miniperl ${.CURDIR}/install_perl_libs -d ${DESTDIR}

.PATH:	${PERL5SRC}
