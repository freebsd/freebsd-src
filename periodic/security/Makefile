# $FreeBSD$

.include <bsd.own.mk>

FILES=	100.chksetuid \
	200.chkmounts \
	300.chkuid0 \
	400.passwdless \
	410.logincheck \
	700.kernelmsg \
	800.loginfail \
	900.tcpwrap \
	security.functions

# NB: keep these sorted by MK_* knobs

.if ${MK_IPFILTER} != "no"
FILES+=	510.ipfdenied
.endif

.if ${MK_IPFW} != "no"
FILES+=	500.ipfwdenied \
	550.ipfwlimit
.endif

.if ${MK_PF} != "no"
FILES+=	520.pfdenied
.endif

.include <bsd.prog.mk>
