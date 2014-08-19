# $FreeBSD$

LIB=	curl
SHLIB_MAJOR= 7

CFLAGS+=	-DHAVE_CONFIG_H \
		-I${.CURDIR} \
		-I${.CURDIR}/../../contrib/curl/include \
		-I${.CURDIR}/../../contrib/curl/lib

.PATH: ${.CURDIR}/../../contrib/curl/lib
SRCS=	\
	amigaos.c \
	asyn-ares.c \
	asyn-thread.c \
	base64.c \
	bundles.c \
	conncache.c \
	connect.c \
	content_encoding.c \
	cookie.c \
	curl_addrinfo.c \
	curl_fnmatch.c \
	curl_gethostname.c \
	curl_gssapi.c \
	curl_memrchr.c \
	curl_multibyte.c \
	curl_ntlm.c \
	curl_ntlm_core.c \
	curl_ntlm_msgs.c \
	curl_ntlm_wb.c \
	curl_rtmp.c \
	curl_sasl.c \
	curl_sasl_sspi.c \
	curl_sspi.c \
	curl_threads.c \
	dict.c \
	dotdot.c \
	easy.c \
	escape.c \
	file.c \
	fileinfo.c \
	formdata.c \
	ftp.c \
	ftplistparser.c \
	getenv.c \
	getinfo.c \
	gopher.c \
	hash.c \
	hmac.c \
	hostasyn.c \
	hostcheck.c \
	hostip.c \
	hostip4.c \
	hostip6.c \
	hostsyn.c \
	http.c \
	http2.c \
	http_chunks.c \
	http_digest.c \
	http_negotiate.c \
	http_negotiate_sspi.c \
	http_proxy.c \
	idn_win32.c \
	if2ip.c \
	imap.c \
	inet_ntop.c \
	inet_pton.c \
	krb5.c \
	ldap.c \
	llist.c \
	md4.c \
	md5.c \
	memdebug.c \
	mprintf.c \
	multi.c \
	netrc.c \
	non-ascii.c \
	nonblock.c \
	openldap.c \
	parsedate.c \
	pingpong.c \
	pipeline.c \
	pop3.c \
	progress.c \
	rawstr.c \
	rtsp.c \
	security.c \
	select.c \
	sendf.c \
	share.c \
	slist.c \
	smtp.c \
	socks.c \
	socks_gssapi.c \
	socks_sspi.c \
	speedcheck.c \
	splay.c \
	ssh.c \
	strdup.c \
	strequal.c \
	strerror.c \
	strtok.c \
	strtoofft.c \
	telnet.c \
	tftp.c \
	timeval.c \
	transfer.c \
	url.c \
	version.c \
	warnless.c \
	wildcard.c \
	x509asn1.c

.PATH: ${.CURDIR}/../../contrib/curl/lib/vtls
SRCS+=	\
	axtls.c \
	curl_darwinssl.c \
	curl_schannel.c \
	cyassl.c \
	gskit.c \
	gtls.c \
	nss.c \
	openssl.c \
	polarssl.c \
	polarssl_threadlock.c \
	qssl.c \
	vtls.c

.PATH: ${.CURDIR}/../../contrib/curl/include/curl
INCS=	\
	curl.h \
	curlbuild.h \
	curlrules.h \
	curlver.h \
	easy.h \
	mprintf.h \
	multi.h \
	stdcheaders.h \
	typecheck-gcc.h

INCSDIR=        ${INCLUDEDIR}/curl

WARNS?=	6

USE_CHERI=yes

.include <src.opts.mk>
.include <bsd.lib.mk>

CFLAGS+=	-Wno-cast-align
