# $FreeBSD$

# lib must be first, or it will not work.  This is because we reference
# the lib's in the directory where they are built from the binaries we
# want to build.

SUBDIR= lib libexec usr.bin usr.sbin

# These are programs which depend on crypto, but not Kerberos.
SPROGS=	bin/ed usr.sbin/ppp usr.sbin/pppd usr.sbin/tcpdump/tcpdump \
	lib/libfetch usr.bin/fetch \
	lib/libtelnet libexec/telnetd usr.bin/telnet
.if !defined(NO_SENDMAIL)
SPROGS+=usr.sbin/sendmail
.endif

.include <bsd.subdir.mk>
