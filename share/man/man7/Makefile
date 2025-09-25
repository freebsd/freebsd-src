.include <src.opts.mk>

MANGROUPS=	MAN

#MISSING: eqnchar.7 ms.7 term.7
MAN=	arch.7 \
	ascii.7 \
	bsd.snmpmod.mk.7 \
	build.7 \
	c.7 \
	d.7 \
	clocks.7 \
	crypto.7 \
	development.7 \
	environ.7 \
	firewall.7 \
	growfs.7 \
	hier.7 \
	hostname.7 \
	intro.7 \
	maclabel.7 \
	mitigations.7 \
	named_attribute.7 \
	operator.7 \
	orders.7 \
	freebsd-base.7 \
	ports.7 \
	release.7 \
	sdoc.7 \
	security.7 \
	simd.7 \
	sizeof.7 \
	sprog.7 \
	stats.7 \
	stdint.7 \
	sticky.7 \
	tracing.7 \
	tuning.7

MLINKS=	intro.7 miscellaneous.7
MLINKS+= growfs.7 growfs_fstab.7
MLINKS+= freebsd-base.7 pkgbase.7
MLINKS+= security.7 securelevel.7
MLINKS+= c.7 c78.7
MLINKS+= c.7 c89.7
MLINKS+= c.7 c90.7
MLINKS+= c.7 c95.7
MLINKS+= c.7 c99.7
MLINKS+= c.7 c11.7
MLINKS+= c.7 c17.7
MLINKS+= c.7 c23.7
MLINKS+= c.7 c2y.7

.if ${MK_INET} != "no"
MAN+=	networking.7
MLINKS+= networking.7 wifi.7
.endif

.if ${MK_TESTS} != "no"
MANGROUPS+=	ATF TESTS

TESTS=		tests.7
TESTSPACKAGE=	tests

ATF=		atf.7
ATFPACKAGE=	atf

ATFPATH=	${SRCTOP}/contrib/atf
.PATH:	${ATFPATH}/doc

CLEANFILES+= atf.7
atf.7: atf.7.in
	sed -e 's,__DOCDIR__,/usr/share/doc/atf,g' \
	    <"${ATFPATH}/doc/atf.7.in" >atf.7
.endif

.include <bsd.prog.mk>
