#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

.include <bsd.own.mk>

#MISSING: eqnchar.7 ms.7 term.7
MAN=	adding_user.7 \
	ascii.7 \
	bsd.snmpmod.mk.7 \
	build.7 \
	clocks.7 \
	c99.7 \
	development.7 \
	environ.7 \
	ffs.7 \
	firewall.7 \
	hier.7 \
	hostname.7 \
	intro.7 \
	maclabel.7 \
	mailaddr.7 \
	operator.7 \
	ports.7 \
	release.7 \
	sdoc.7 \
	security.7 \
	sprog.7 \
	stdint.7 \
	sticky.7 \
	tuning.7

MLINKS=	intro.7 miscellaneous.7
MLINKS+= security.7 securelevel.7
MLINKS+= c99.7 c.7
MLINKS+= c99.7 c78.7
MLINKS+= c99.7 c89.7
MLINKS+= c99.7 c90.7

.if ${MK_ATF} != "no"
ATF=            ${.CURDIR}/../../../contrib/atf
.PATH:          ${ATF}/doc
MAN+=		atf.7

CLEANFILES+=	atf.7 atf.7.tmp
atf.7: atf.7.in
	sed -e 's,__DOCDIR__,${DOCDIR}/atf,g' \
	    -e 's,__TESTSDIR__,${TESTSBASE},g' \
	    < ${.ALLSRC} > ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}
.endif

.include <bsd.prog.mk>
