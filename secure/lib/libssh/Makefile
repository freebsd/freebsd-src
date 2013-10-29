# $FreeBSD$

.include <bsd.own.mk>

LIB=	ssh
PRIVATELIB=	true
SHLIB_MAJOR=	5
SRCS=	authfd.c authfile.c bufaux.c bufbn.c buffer.c \
	canohost.c channels.c cipher.c cipher-aes.c \
	cipher-bf1.c cipher-ctr.c cipher-3des1.c cleanup.c \
	compat.c compress.c crc32.c deattack.c fatal.c hostfile.c \
	krl.c log.c match.c md-sha256.c moduli.c nchan.c packet.c \
	readpass.c rsa.c ttymodes.c xmalloc.c addrmatch.c \
	atomicio.c key.c dispatch.c kex.c mac.c uidswap.c uuencode.c misc.c \
	monitor_fdpass.c rijndael.c ssh-dss.c ssh-ecdsa.c ssh-rsa.c dh.c \
	kexdh.c kexgex.c kexdhc.c kexgexc.c bufec.c kexecdh.c kexecdhc.c \
	msg.c progressmeter.c dns.c entropy.c umac.c umac128.c jpake.c \
	schnorr.c ssh-pkcs11.c

# gss-genr.c should be in $SRCS but causes linking problems, so it is
# compiled directly into sshd instead.

# Portability layer
SRCS+=	bsd-misc.c fmt_scaled.c glob.c \
	openssl-compat.c port-tun.c strtonum.c timingsafe_bcmp.c \
	vis.c xcrypt.c xmmap.c

.if ${MK_LDNS} == "no"
SRCS+=	getrrsetbyname.c
.else
LDNSDIR=	${.CURDIR}/../../../contrib/ldns
CFLAGS+=	-DHAVE_LDNS=1 -I${LDNSDIR}
SRCS+=	getrrsetbyname-ldns.c
DPADD+=	${LIBLDNS}
LDADD+=	-lldns
USEPRIVATELIB+= ldns
.endif

CFLAGS+= -I${SSHDIR} -include ssh_namespace.h

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -include krb5_config.h
DPADD+=	${LIBGSSAPI} ${LIBKRB5} ${LIBHX509} ${LIBASN1} ${LIBCOM_ERR} ${LIBMD} ${LIBROKEN}
LDADD+=	-lgssapi -lkrb5 -lhx509 -lasn1 -lcom_err -lmd -lroken
.endif

.if ${MK_OPENSSH_NONE_CIPHER} != "no"
CFLAGS+= -DNONE_CIPHER_ENABLED
.endif

NO_LINT=

DPADD+=	${LIBCRYPTO} ${LIBCRYPT} ${LIBZ}
LDADD+=	-lcrypto -lcrypt -lz

.include <bsd.lib.mk>

.PATH: ${SSHDIR} ${SSHDIR}/openbsd-compat

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
.if ${MK_KERBEROS_SUPPORT} != "no"
${OBJS} ${POBJS} ${SOBJS}: krb5_config.h
.endif
