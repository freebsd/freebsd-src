# $FreeBSD: src/gnu/usr.bin/perl/Makefile.inc,v 1.12 1999/12/04 12:40:38 marcel Exp $

PERL5SRC?= ${.CURDIR}/../../../../contrib/perl5
PERL5LIBSRC?= ${.CURDIR}/../libperl

BINDIR?=	/usr/bin

SHLIB_MAJOR?=	3
SHLIB_MINOR?=	0

HEADERS=EXTERN.h INTERN.h XSUB.h XSlock.h av.h bytecode.h byterun.h \
	cc_runtime.h cop.h cv.h dosish.h embed.h embedvar.h fakethr.h form.h \
	gv.h handy.h hv.h intrpvar.h iperlsys.h keywords.h mg.h nostdio.h \
	objXSUB.h objpp.h op.h opcode.h patchlevel.h perl.h perlio.h \
	perlsdio.h perlsfio.h perlvars.h perly.h pp.h pp_proto.h proto.h \
	regcomp.h regexp.h regnodes.h scope.h sv.h thrdvar.h thread.h \
	unixish.h util.h

DEPEND_H=

.for I in ${HEADERS}
${.OBJDIR}/${I}: ${I}
	@ln -sf ${.OODATE} ${.TARGET}

CLEANFILES+= ${.OBJDIR}/${I}
DEPEND_H+= ${.OBJDIR}/${I}
.endfor

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
	@mkdir lib/auto
	@mkdir build
	@ln -sf ${PERL5SRC}/config_h.SH config_h.sh
	@ln -sf ${PERL5SRC}/cflags.SH cflags.sh
	@ln -sf ${PERL5SRC}/ext/re/re.pm lib/re.pm
	@ln -sf ${PERL5SRC}/myconfig myconfig
	@ln -sf ${PERL5SRC}/miniperlmain.c miniperlmain.c
	@ln -sf ${PERL5SRC}/writemain.SH writemain.sh
	@ln -sf ${PERL5SRC}/regcomp.c regcomp.c
	@ln -sf ${PERL5SRC}/regexec.c regexec.c
	@touch ${.TARGET}

CLEANFILES+=	links
CLEANFILES+=	config.h config.sh config_h.sh
CLEANFILES+=	regcomp.c regexec.c miniperlmain.c
CLEANFILES+=	writemain writemain.sh cflags cflags.sh myconfig
CLEANDIRS+=	lib ext Porting hints build
FARMDIRS=	lib ext Porting hints

PERL_THREADED?= no
.if ${PERL_THREADED} == "yes"
CFLAGS+=        -pthread
THREAD=		threads-
.endif

PERL_DEBUGGING?=        no
.if ${PERL_DEBUGGING} == "yes"
CFLAGS+=        -DDEBUGGING
.endif

config.sh: ${PERL5LIBSRC}/config.SH-${THREAD}${OBJFORMAT}.${MACHINE_ARCH}
	ln -sf ${.OODATE} ${.TARGET}

config.h: links config_h.sh config.sh
	sh config_h.sh

cflags.sh config_h.sh writemain.sh: links

cflags: cflags.sh
	sh cflags.sh

writemain: writemain.sh
	sh writemain.sh
