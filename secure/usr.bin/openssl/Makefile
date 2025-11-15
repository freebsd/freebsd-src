CONFS=	openssl.cnf
CONFSDIR=	/etc/ssl
PROG=	openssl
PACKAGE=	openssl

LIBADD=	ssl crypto

.if exists(Makefile.man)
.include "Makefile.man"
.endif
.include "../../lib/libcrypto/Makefile.inc"

CFLAGS+=	-I${LCRYPTO_SRC}/apps/include
CFLAGS+=	-I${OBJTOP}/secure/lib/libcrypto

SRCS=	asn1parse.c ca.c ciphers.c cmp.c cms.c crl.c crl2pkcs7.c dgst.c
SRCS+=	dhparam.c dsa.c dsaparam.c ec.c ecparam.c enc.c engine.c errstr.c
SRCS+=	fipsinstall.c gendsa.c genpkey.c genrsa.c info.c kdf.c
SRCS+=	lib/cmp_mock_srv.c list.c lib/log.c mac.c nseq.c ocsp.c openssl.c
SRCS+=	passwd.c pkcs12.c pkcs7.c pkcs8.c pkey.c pkeyparam.c pkeyutl.c prime.c
SRCS+=	progs.c rand.c rehash.c req.c rsa.c rsautl.c s_client.c s_server.c
SRCS+=	s_time.c sess_id.c skeyutl.c smime.c speed.c spkac.c srp.c storeutl.c
SRCS+=	ts.c verify.c version.c x509.c

# libapps
SRCS+=	lib/apps.c lib/apps_ui.c lib/opt.c lib/fmt.c lib/s_cb.c lib/s_socket.c
SRCS+=	lib/app_rand.c
SRCS+=	lib/columns.c lib/app_params.c lib/names.c lib/app_provider.c
SRCS+=	lib/app_x509.c lib/http_server.c
SRCS+=	lib/engine.c lib/engine_loader.c lib/app_libctx.c lib/apps_opt_printf.c
SRCS+=	lib/tlssrp_depr.c

.include <bsd.prog.mk>

.PATH:	${LCRYPTO_SRC}/apps \
	${.CURDIR}/man
