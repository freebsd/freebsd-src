#
# $FreeBSD: src/gnu/usr.bin/perl/perl/Makefile,v 1.27.2.1 2000/06/04 23:19:44 ache Exp $
#

PROG=	perl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR}
SRCS=	perlmain.c config.h
NOSHARED= no
DYNALOADER= lib/auto/DynaLoader/DynaLoader.a
DPADD=	${DYNALOADER} ${LIBPERL} ${LIBM} ${LIBCRYPT} ${LIBMD}
LDADD=	-Wl,-E ${DYNALOADER} -lperl -lm -lcrypt -lmd
LINKS=	${BINDIR}/${PROG} ${BINDIR}/perl5 \
	${BINDIR}/${PROG} ${BINDIR}/perl5.00503

CLEANFILES=	Config.pm perlmain.c \
		autosplit pod2man.PL pod2man ext.libs \
		man3pages

.include <bsd.prog.mk>

STATIC_EXT=	DynaLoader/DynaLoader

DYNAMIC_EXT=	B/B DB_File/DB_File Data/Dumper/Dumper Fcntl/Fcntl IO/IO \
		IPC/SysV/SysV NDBM_File/NDBM_File Opcode/Opcode POSIX/POSIX \
		SDBM_File/SDBM_File Socket/Socket attrs/attrs re/re
.if ${PERL_THREADED} == "yes"
DYNAMIC_EXT+=	Thread/Thread
.endif

NONXS_EXT=	Errno/pm_to_blib

MAKEMAKER_ARGS=	INSTALLDIRS=perl PERL_SRC=${.OBJDIR} \
		INSTALLMAN3DIR=${DESTDIR}/usr/share/perl/man3 \
		PERL=perl FULLPERL=perl DEFINE=-I${DESTDIR}/usr/include

${PROG}: Config.pm autosplit lib/auto/DynaLoader/DynaLoader.a

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

.for I in ${DYNAMIC_EXT}
ext/${I:H}/Makefile: links ext/${I:H}/Makefile.PL cflags Config.pm pod2man config.h
	@cd ext/${I:H}; \
	miniperl -I${.OBJDIR}/lib Makefile.PL ${MAKEMAKER_ARGS} \
		LINKTYPE=dynamic LIBS="-lperl -lm" \
		INST_LIB=${.OBJDIR}/build/${I:H} \
		INST_ARCHLIB=${.OBJDIR}/build/${I:H}; \
	make -B config PERL_SRC=${.OBJDIR}

lib/auto/${I}.so: links ${PROG} ext/${I:H}/Makefile
	@cd ext/${I:H}; \
	make -B all PERL_SRC=${.OBJDIR}

all:	lib/auto/${I}.so

DYNAMICS+=	lib/auto/${I}.so
.endfor

.for I in ${NONXS_EXT}
ext/${I:H}/Makefile: links ext/${I:H}/Makefile.PL cflags lib/Config.pm
	mkdir -p ${.OBJDIR}/lib/auto/${I:H} ;\
	@cd ext/${I:H} ;\
	miniperl -I${.OBJDIR}/lib Makefile.PL ${MAKEMAKER_ARGS} \
		INST_LIB=${.OBJDIR}/lib/auto/${I:H} \
		INST_ARCHLIB=${.OBJDIR}/lib/auto/${I:H}; \
	make -B config PERL_SRC=${.OBJDIR}

lib/auto/${I}: links ${PROG} ext/${I:H}/Makefile
	@cd ext/${I:H}; \
	make -B all LINKTYPE=nonxs PERL_SRC=${.OBJDIR}

NONXSS+=	lib/auto/${I}
.endfor

pod2man: Config.pm autosplit ${PERL5SRC}/pod/pod2man.PL
	ln -sf ${PERL5SRC}/pod/pod2man.PL 
	miniperl -I${.OBJDIR}/lib pod2man.PL

man3pages: pod2man
	@cd ${.OBJDIR}/lib ;\
	for i in `find . -name \*.pm | grep -v Functions.pm` ; do \
		j=`echo $$i | sed -e 's|./||' -e 's|/|::|g' -e 's|.pm|.3|'` ;\
		echo Manifying $$j ;\
		miniperl -I${.OBJDIR}/lib ${.OBJDIR}/pod2man $$i > $$j ;\
		gzip -fn $$j ;\
	done
	@cd ${.OBJDIR}/ext ;\
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
	@cd ${.OBJDIR}/ext/${I:H} ;\
	make -B install \
		INSTALLPRIVLIB=${DESTDIR}/usr/libdata/perl/5.00503 \
		INSTALLARCHLIB=${DESTDIR}/usr/libdata/perl/5.00503/mach
.endfor
	@cd ${.OBJDIR}/lib ;\
	for i in `find . \! -type d \! -name \*.3.gz` ; do \
		j=`echo $$i|sed -e 's|auto/DynaLoader|mach/auto/DynaLoader|'` ;\
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
			$$i ${DESTDIR}/usr/libdata/perl/5.00503/$$j ;\
	done
	@cd ${.OBJDIR}/lib ;\
	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		*.3.gz ${DESTDIR}/usr/share/perl/man/man3
	@cd ${.OBJDIR} ;\
	${INSTALL} ${COPY} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		*.h ${DESTDIR}/usr/libdata/perl/5.00503/mach/CORE
	@cd ${.OBJDIR} ;\
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		Config.pm ${DESTDIR}/usr/libdata/perl/5.00503/mach

.PATH:	${PERL5SRC}
