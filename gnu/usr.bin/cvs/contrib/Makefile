# $FreeBSD$

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/contrib
.PATH:	${CVSDIR}/man

#pvcs_to_rcs left out, it needs perl5

SCRIPTS= clmerge cln_hist commit_prep cvs2vendor cvs_acls cvscheck \
	 log log_accum mfpipe rcs-to-cvs rcs2log rcslock sccs2rcs \
	 easy-import

FILES=	 README cvscheck.man cvshelp.man descend.man intro.doc

EXAMPDIR=	/usr/share/examples/cvs
SCRIPTSDIR=	${EXAMPDIR}/contrib
PERLPATH=	/usr/bin/perl
CLEANFILES+=	$(SCRIPTS)

.SUFFIXES: .sh .csh .pl

.sh:
	cp $< $@

.csh:
	sed -e 's,xCSH_PATHx,/bin/csh,' $< > $@

.pl:
	sed -e 's,xPERL_PATHx,$(PERLPATH),' $< > $@


beforeinstall:
.for file in ${FILES}
	cd ${.CURDIR} ; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
		${CVSDIR}/contrib/${file} ${DESTDIR}${EXAMPDIR}/contrib
.endfor

.include <bsd.prog.mk>
