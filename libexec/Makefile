#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# Present but disabled: kpasswdd
SUBDIR=	atrun \
	bootpd \
	comsat \
	fingerd \
	ftpd \
	getNAME \
	getty \
	makekey \
	mknetid \
	pppoed \
	pt_chown \
	rbootd \
	revnetgroup \
	rexecd \
	rlogind \
	rpc.rquotad \
	rpc.rstatd \
	rpc.rusersd \
	rpc.rwalld \
	rpc.sprayd \
	rshd \
	save-entropy \
	talkd \
	tcpd \
	tftpd \
	ypxfr

.if ${MACHINE_ARCH} != amd64 && !defined(NOPIC)
SUBDIR+=rtld-elf
.endif

.if !defined(NO_BIND)
SUBDIR+=named-xfer
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=mail.local smrsh
.endif

.if defined(RELEASEDIR) || \
    (!exists(${.CURDIR}/../secure) && !exists(${.CURDIR}/../kerberos5)) || \
    defined(NOCRYPT) || defined(NO_OPENSSL) || \
    (defined(NOSECURE) && defined(NO_KERBEROS))
# make release needs both
SUBDIR+=telnetd
.endif

.if defined(WANT_LUKEMFTPD)
SUBDIR+=lukemftpd
.endif

.include <bsd.subdir.mk>
