#
# $Id: Makefile,v 1.16 1998/10/15 18:50:04 markm Exp $
#

PROG=	perl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR}
SRCS=	perlmain.c config.h
NOSHARED= no
DPADD=	lib/auto/DynaLoader/DynaLoader.a ${LIBPERL} ${LIBM} ${LIBCRYPT}
LDADD=	lib/auto/DynaLoader/DynaLoader.a -lperl -lm -lcrypt
LINKS=	${BINDIR}/${PROG} ${BINDIR}/perl5 \
	${BINDIR}/${PROG} ${BINDIR}/perl5.00503

CLEANFILES=	config.h config.sh config_h.sh writemain writemain.sh \
		Config.pm cflags cflags.sh myconfig perlmain.c miniperlmain.c \
		autosplit regcomp.c regexec.c pod2man.PL pod2man ext.libs \
		man3pages

STATIC_EXT=	DynaLoader/DynaLoader

DYNAMIC_EXT=	B/B DB_File/DB_File Data/Dumper/Dumper Fcntl/Fcntl IO/IO \
		IPC/SysV/SysV NDBM_File/NDBM_File Opcode/Opcode POSIX/POSIX \
		SDBM_File/SDBM_File Socket/Socket attrs/attrs re/re

NONXS_EXT=	Errno/pm_to_blib

.include <bsd.prog.mk>

${PROG}: linkfarm Config.pm autosplit lib/auto/DynaLoader/DynaLoader.a

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

.for I in ${DYNAMIC_EXT}
ext/${I:H}/Makefile: linkfarm ext/${I:H}/Makefile.PL cflags Config.pm pod2man
	cd ext/${I:H}; \
	miniperl -I${.OBJDIR}/lib Makefile.PL \
		LINKTYPE=dynamic INSTALLDIRS=perl PERL_SRC=${.OBJDIR} \
		LIBS="-lperl" INSTALLMAN3DIR=${DESTDIR}/usr/share/perl/man3 \
		INST_LIB=${.OBJDIR}/build/${I:H} \
		INST_ARCHLIB=${.OBJDIR}/build/${I:H} ;\
	make -B config PERL_SRC=${.OBJDIR}
	

lib/auto/${I}.so: linkfarm ${PROG} ext/${I:H}/Makefile
	cd ext/${I:H}; \
	make -B all PERL_SRC=${.OBJDIR}

all:	lib/auto/${I}.so

DYNAMICS+=	lib/auto/${I}.so
.endfor

.for I in ${NONXS_EXT}
ext/${I:H}/Makefile: linkfarm ext/${I:H}/Makefile.PL cflags lib/Config.pm
	mkdir -p ${.OBJDIR}/lib/auto/${I:H} ;\
	cd ext/${I:H} ;\
	miniperl -I${.OBJDIR}/lib Makefile.PL \
		INSTALLDIRS=perl PERL_SRC=${.OBJDIR} \
		INSTALLMAN3DIR=/usr/share/perl/man3 \
		INST_LIB=${.OBJDIR}/lib/auto/${I:H} \
		INST_ARCHLIB=${.OBJDIR}/lib/auto/${I:H} ;\
	make -B config PERL_SRC=${.OBJDIR}

lib/auto/${I}: linkfarm ${PROG} ext/${I:H}/Makefile
	cd ext/${I:H}; \
	make -B all LINKTYPE=nonxs PERL_SRC=${.OBJDIR}

NONXSS+=	lib/auto/${I}
.endfor

pod2man: Config.pm autosplit ${PERL5SRC}/pod/pod2man.PL
	ln -sf ${PERL5SRC}/pod/pod2man.PL 
	miniperl -I${.OBJDIR}/lib pod2man.PL

man3pages: pod2man
	cd ${.OBJDIR}/lib ;\
	for i in `find . -name \*.pm | grep -v Functions.pm` ; do \
		j=`echo $$i | sed -e 's|./||' -e 's|/|::|g' -e 's|.pm|.3|'` ;\
		echo Manifying $$j ;\
		miniperl -I${.OBJDIR}/lib ${.OBJDIR}/pod2man $$i > $$j ;\
		gzip -fn $$j ;\
	done
	cd ${.OBJDIR}/ext ;\
	for i in `find . -name \*.pm -o -name \*.pod | grep -v POSIX.pm` ; do \
		j=`echo $$i | sed -e 's|./||' -e 's|/SysV/|/IPC/|' \
			-e 's|/Dumper/|/Data/|' -e 's|/lib/|/|' \
			-e 's|^[^/]*/||' \
			-e 's|/|::|g' -e 's|.pm|.3|' -e 's|.pod|.3|'`;\
		i=`echo $$i | sed -e 's|./||'` ;\
		echo Manifying $$j ;\
		miniperl -I${.OBJDIR}/lib ${.OBJDIR}/pod2man $$i > ../lib/$$j ;\
		gzip -fn ../lib/$$j ;\
	done
	touch ${.TARGET}

all: man3pages

install:
.for I in ${DYNAMIC_EXT}
	cd ${.OBJDIR}/ext/${I:H} ;\
	make -B install \
		INSTALLPRIVLIB=${DESTDIR}/usr/libdata/perl/5.00503 \
		INSTALLARCHLIB=${DESTDIR}/usr/libdata/perl/5.00503/mach
.endfor
	cd ${.OBJDIR}/lib ;\
	for i in `find . \! -type d \! -name \*.3.gz` ; do \
		j=`echo $$i|sed -e 's|auto/DynaLoader|mach/auto/DynaLoader|'` ;\
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
			$$i ${DESTDIR}/usr/libdata/perl/5.00503/$$j ;\
	done
	cd ${.OBJDIR}/lib ;\
	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		*.3.gz ${DESTDIR}/usr/share/perl/man/man3
	cd ${.OBJDIR} ;\
	${INSTALL} ${COPY} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		*.h ${DESTDIR}/usr/libdata/perl/5.00503/mach/CORE
	cd ${.OBJDIR} ;\
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		Config.pm ${DESTDIR}/usr/libdata/perl/5.00503/mach

.PATH:	${PERL5SRC}
