#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

.include <src.opts.mk>

FILESGROUPS=	FILES1 FILES2
FILES1=		dot.login dot.login_conf dot.profile dot.shrc
FILES1DIR=	/usr/share/skel
FILES2DIR=	/usr/share/skel
FILES1MODE=	0644
FILES2MODE=	0600

.PATH: ${.OBJDIR} ${.CURDIR}

.if ${MK_MAIL} != "no"
FILES1+=	dot.mailrc
FILES2+=	dot.mail_aliases
.endif

.if ${MK_RCMDS} != "no"
FILES2+=	dot.rhosts
.endif

.if ${MK_TCSH} != "no"
FILES1+=	dot.cshrc
.endif

.if ${MK_GAMES} == "no"
dot.login_SED+=		-e "/\/usr\/games\/fortune /s/^/\#/"
dot.profile_SED+=	-e "/\/usr\/games\/fortune /s/^/\#/"
.endif

.if ${MK_VI} == "no"
dot.cshrc_SED+=		-e '/^setenv.*EDITOR.*vi$$/s/^/\#/'
dot.mailrc_SED+=	-e '/^set EDITOR=vi$$/s/^/\#/' -e '/^set VISUAL=vi$$/s/^/\#/'
dot.profile_SED+=	-e '/^EDITOR=vi; /s/^/\#/'
.endif

.for f in dot.cshrc dot.login dot.mailrc dot.profile
.if empty(${f}_SED)
$f: $f.in
	cp ${.ALLSRC} ${.TARGET}
.else
$f: $f.in
	sed ${${f}_SED} < ${.ALLSRC} > ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}
CLEANFILES+=	$f.tmp
.endif
CLEANFILES+=	$f
.endfor

.include <bsd.prog.mk>
