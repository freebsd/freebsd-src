.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

LIB=	ssh
PRIVATELIB=	true
SHLIB_MAJOR=	5

LIBOPENSSH_SRCS=\
	ssh_api.c \
	ssherr.c \
	sshbuf.c \
	sshkey.c \
	sshbuf-getput-basic.c \
	sshbuf-misc.c \
	sshbuf-getput-crypto.c \
	krl.c \
	bitmap.c

# gss-genr.c should be in $SRCS but causes linking problems, so it is
# compiled directly into sshd instead.
SRCS=	${LIBOPENSSH_SRCS} \
	authfd.c authfile.c \
	canohost.c channels.c cipher.c cipher-aes.c cipher-aesctr.c \
	cleanup.c \
	compat.c fatal.c hostfile.c \
	log.c match.c moduli.c nchan.c packet.c \
	readpass.c ttymodes.c xmalloc.c addr.c addrmatch.c \
	atomicio.c dispatch.c mac.c misc.c utf8.c \
	monitor_fdpass.c rijndael.c ssh-dss.c ssh-ecdsa.c ssh-ecdsa-sk.c \
	ssh-ed25519-sk.c ssh-rsa.c dh.c \
	msg.c dns.c entropy.c umac.c umac128.c \
	ssh-pkcs11.c smult_curve25519_ref.c \
	poly1305.c chacha.c cipher-chachapoly.c cipher-chachapoly-libcrypto.c \
	ssh-ed25519.c digest-openssl.c digest-libc.c \
	hmac.c ed25519.c hash.c \
	kex.c kex-names.c kexdh.c kexgex.c kexecdh.c kexc25519.c \
	kexgexc.c kexgexs.c \
	kexsntrup761x25519.c kexmlkem768x25519.c sntrup761.c kexgen.c \
	sftp-realpath.c platform-pledge.c platform-tracing.c platform-misc.c \
	sshbuf-io.c

# Add ${SKSRCS} to avoid linking problems.
SRCS+=	${SKSRCS}

PACKAGE=	ssh

# Portability layer
SRCS+=	bcrypt_pbkdf.c blowfish.c bsd-misc.c bsd-signal.c explicit_bzero.c \
	fmt_scaled.c freezero.c glob.c \
	libressl-api-compat.c \
	mktemp.c \
	openssl-compat.c port-net.c \
	recallocarray.c strtonum.c timingsafe_bcmp.c vis.c xcrypt.c

.if ${MK_LDNS} == "no"
SRCS+=	getrrsetbyname.c
.else
LDNSDIR=	${SRCTOP}/contrib/ldns
CFLAGS+=	-I${LDNSDIR}
SRCS+=	getrrsetbyname-ldns.c
LIBADD+=	ldns
.endif

.if defined(LOCALBASE)
CFLAGS+= -D_PATH_SSH_ASKPASS_DEFAULT='"${LOCALBASE}/bin/ssh-askpass"'
.endif

NO_LINT=

LIBADD+=	crypto crypt z

.include <bsd.lib.mk>

.PATH: ${SSHDIR} ${SSHDIR}/openbsd-compat
