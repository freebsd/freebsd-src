#	$Id: Makefile,v 1.2 1995/07/25 00:31:51 bde Exp $

SCRIPTS= ccvs-rsh rcs2log clmerge cln_hist commit_prep cvs_acls cvscheck \
	 log log_accum mfpipe rcs-to-cvs rcs2log rcslock sccs2rcs

FILES=	 README cvscheck.man cvshelp.man descend.man intro.doc

PCLCVS=	ChangeLog INSTALL NEWS README \
	compile-all.el pcl-cvs-lucid.el pcl-cvs-lucid.elc pcl-cvs-startup.el \
	pcl-cvs.el pcl-cvs.elc pcl-cvs.texinfo

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


afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPTS} ${DESTDIR}${EXAMPDIR}/contrib
	cd ${.CURDIR} ; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
		${FILES} ${DESTDIR}${EXAMPDIR}/contrib
	cd ${.CURDIR}/pcl-cvs ; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
		${PCLCVS} ${DESTDIR}${EXAMPDIR}/pcl-cvs

.include "../../Makefile.inc"
.include <bsd.prog.mk>
