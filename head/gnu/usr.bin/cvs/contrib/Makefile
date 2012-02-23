# $FreeBSD$

.include "../Makefile.inc"

.PATH:	${CVSDIR}/contrib
.PATH:	${CVSDIR}/man

SCRIPTS= clmerge cln_hist commit_prep cvs2vendor cvs_acls cvscheck \
	 log log_accum mfpipe rcs-to-cvs rcs2log rcslock sccs2rcs \
	 easy-import

FILES=	 README cvscheck.man cvshelp.man descend.man intro.doc

EXAMPDIR=	${SHAREDIR}/examples/cvs
FILESDIR=	${EXAMPDIR}/contrib
SCRIPTSDIR=	${FILESDIR}
PERLPATH=	/usr/bin/perl
CLEANFILES+=	$(SCRIPTS)

.SUFFIXES: .sh .pl .in

# Prevent Makefile.in from overwriting Makefile through the suffix rules.
Makefile:
	@:

.sh:
	cp -f ${.IMPSRC} ${.TARGET}

.pl:
	sed -e 's,xPERL_PATHx,$(PERLPATH),' ${.IMPSRC} > ${.TARGET}

.in:
	sed -e 's,@CSH@,/bin/csh,' -e 's,@PERL@,$(PERLPATH),' ${.IMPSRC} > ${.TARGET}

.include <bsd.prog.mk>
