#	$Id: Makefile,v 1.3 1995/12/11 01:23:42 peter Exp $

SUBDIR=	 pcl-cvs

SCRIPTS= ccvs-rsh rcs2log clmerge cln_hist commit_prep cvs_acls cvscheck \
	 log log_accum mfpipe rcs-to-cvs rcs2log rcslock sccs2rcs \
	 easy-import

FILES=	 README cvscheck.man cvshelp.man descend.man intro.doc

EXAMPDIR=	/usr/share/examples/cvs
PERLPATH=	/usr/bin/perl
CLEANFILES+=	$(SCRIPTS)

.SUFFIXES: .sh .csh .pl

all:	${SCRIPTS}

.sh:
	cp $< $@

.csh:
	sed -e 's,xCSH_PATHx,/bin/csh,' $< > $@

.pl:
	sed -e 's,xPERL_PATHx,$(PERLPATH),' $< > $@


beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPTS} ${DESTDIR}${EXAMPDIR}/contrib
	cd ${.CURDIR} ; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
		${FILES} ${DESTDIR}${EXAMPDIR}/contrib

.include "../../Makefile.inc"
.include <bsd.prog.mk>
