#	$Id: Makefile.inc,v 1.4 1998/09/12 19:16:32 markm Exp $

PERL5SRC?= ${.CURDIR}/../../../../contrib/perl5

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

lib ext Porting hints:
	@rm -rf ${.TARGET}
	for i in `cd ${PERL5SRC}; find ${.TARGET} -type d` ; do \
		mkdir -p $${i} ;\
	done
	for i in `cd ${PERL5SRC}; find ${.TARGET} -type f` ; do \
		ln -s ${PERL5SRC}/$${i} $${i} ;\
	done
