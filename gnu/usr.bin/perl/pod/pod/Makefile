#
# $FreeBSD$
#

PODS= \
	perl.pod \
	perldelta.pod \
	perl5004delta.pod \
	perl5005delta.pod \
	perldata.pod \
	perlsyn.pod \
	perlop.pod \
	perlre.pod \
	perlrun.pod \
	perlfunc.pod \
	perlopentut.pod \
	perlvar.pod \
	perlsub.pod \
	perlmod.pod \
	perlmodlib.pod \
	perlmodinstall.pod \
	perlfork.pod \
	perlform.pod \
	perllocale.pod \
	perlref.pod \
	perlreftut.pod \
	perldsc.pod \
	perllol.pod \
	perlboot.pod \
	perltoot.pod \
	perltootc.pod \
	perlobj.pod \
	perltie.pod \
	perlbot.pod \
	perlipc.pod \
	perlthrtut.pod \
	perldbmfilter.pod \
	perldebguts.pod \
	perldebug.pod \
	perlnumber.pod \
	perldiag.pod \
	perlsec.pod \
	perltrap.pod \
	perlport.pod \
	perlstyle.pod \
	perlpod.pod \
	perlbook.pod \
	perlembed.pod \
	perlapio.pod \
	perlxs.pod \
	perlxstut.pod \
	perlguts.pod \
	perlcall.pod \
	perlcompile.pod \
	perltodo.pod \
	perlapi.pod \
	perlintern.pod \
	perlhack.pod \
	perlhist.pod \
	perlfaq.pod \
	perlfaq1.pod \
	perlfaq2.pod \
	perlfaq3.pod \
	perlfaq4.pod \
	perlfaq5.pod \
	perlfaq6.pod \
	perlfaq7.pod \
	perlfaq8.pod \
	perlfaq9.pod \
	perltoc.pod

.for I in ${PODS:R}
${.OBJDIR}/${I}.pod: ${I}.pod
	@ln -sf ${.OODATE} ${.TARGET}

${I}.1: ${I}.pod
	${MINIPERL} ${MINIPERLOPT} ${.OBJDIR}/../pod2man/pod2man ${.OODATE} \
		> ${.TARGET}

MAN+=	${I}.1

CLEANFILES+= ${.OBJDIR}/${I}.pod ${I}.1
.endfor

.include <bsd.prog.mk>

install:
	cd ${PERL5SRC}/pod ;\
	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		${PODS} ${DESTDIR}/usr/libdata/perl/${VERSION}/pod

.PATH: ${PERL5SRC}/pod
