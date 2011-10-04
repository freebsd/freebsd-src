# $FreeBSD$

PORTSDIR?=	/usr/ports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk

# Needed to keep bsd.own.mk from reading in /etc/src.conf
# and setting MK_* variables when building ports.
_WITHOUT_SRCCONF=

# Enable CTF conversion on request.
.if defined(WITH_CTF)
.undef NO_CTF
.endif

.include <bsd.own.mk>
.include "${BSDPORTMK}"

.if !defined(BEFOREPORTMK) && !defined(INOPTIONSMK)
# Work around an issue where FreeBSD 10.0 is detected as FreeBSD 1.x.
run-autotools-fixup:
	find ${WRKSRC} -type f \( -name config.libpath -o \
		-name config.rpath -o -name configure -o -name libtool.m4 \) \
		-exec sed -i '' -e 's/freebsd1\*)/SHOULDNOTMATCHANYTHING1)/' \
		-e 's/freebsd\[123\]\*)/SHOULDNOTMATCHANYTHING2)/' {} +

.ORDER: run-autotools run-autotools-fixup do-configure
do-configure: run-autotools-fixup
.endif
