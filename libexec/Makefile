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
	talkd \
	tftpd \
	xtend \
	ypxfr

.if !defined(NO_BIND)
SUBDIR+=named-xfer
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=mail.local smrsh
.endif

.if !defined(NOUUCP)
SUBDIR+=uucpd
.endif

.if ${OBJFORMAT} == aout
SUBDIR+=rtld-aout
.endif

.if ${OBJFORMAT} == elf
SUBDIR+=rtld-elf
.endif

.if defined(RELEASEDIR) || \
    (!exists(${.CURDIR}/../secure) && !exists(${.CURDIR}/../kerberosIV)) || \
    defined(NOCRYPT) || (defined(NOSECURE) && !defined(MAKE_KERBEROS4))
# make release needs both
SUBDIR+=telnetd
.endif

.include <bsd.subdir.mk>
