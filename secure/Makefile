# $FreeBSD$

SUBDIR= lib libexec usr.bin usr.sbin

# These are programs which depend on crypto, but not Kerberos.
SPROGS=	bin/ed usr.sbin/ppp usr.sbin/pppd usr.sbin/tcpdump/tcpdump \
	lib/libfetch usr.bin/fetch usr.sbin/pkg_install \
	lib/libtelnet libexec/telnetd usr.bin/telnet
.if !defined(NO_SENDMAIL)
SPROGS+=usr.sbin/sendmail
.endif

.include <bsd.subdir.mk>
