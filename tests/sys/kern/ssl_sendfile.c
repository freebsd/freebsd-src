/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Gleb Smirnoff <glebius@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdlib.h>
#include <poll.h>
#include <pthread.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509v3.h>

#include <atf-c.h>

#define	FSIZE	(size_t)(2 * 1024 * 1024)

struct ctx {
	EVP_PKEY *pkey;		/* Self-signed key ... */
	X509 *cert;		/* ... and certificate */
	SSL_CTX *ctx;		/* client context */
	SSL *cln;		/* client connection */
	SSL *srv;		/* server connection */
	int cs;			/* client socket */
	int ss;			/* server socket */
	int fd;			/* test file descriptor */
	void *mfd;		/* mapped contents of the test file */
	uint16_t port;		/* server listening port */
	pthread_t thr;		/* server thread */
	off_t offset;		/* SSL_sendfile offset */
	size_t size;		/* SSL_sendfile size */
	bool nb;		/* SSL_sendfile mode */
	ossl_ssize_t sbytes;	/* SSL_sendfile returned sbytes */
	enum {
		INIT,
		READY,
		RUNNING,
		EXITING,
	} state;
	pthread_mutex_t mtx;
	pthread_cond_t cv;
};

static void *server_thread(void *arg);

static void
common_init(struct ctx *c)
{
	char hostname[sizeof("localhost:65536")];
	char tempname[] = "/tmp/ssl_sendfile_test.XXXXXXXXXX";
	X509_NAME *name;
	X509_EXTENSION *ext;
	SSL *ssl;
	bool enable;
	size_t len = sizeof(enable);

	if (sysctlbyname("kern.ipc.tls.enable", &enable, &len, NULL, 0) == -1) {
		if (errno == ENOENT)
			atf_tc_skip("kernel does not have options KERN_TLS");
		atf_libc_error(errno, "Failed to read kern.ipc.tls.enable");
        }
	if (!enable)
		atf_tc_skip("kern.ipc.tls.enable is off");

	c->state = INIT;

	/*
	 * Generate self signed key & certificate.
	 */
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
	ATF_REQUIRE(c->pkey = EVP_RSA_gen(2048));
	ATF_REQUIRE(c->cert = X509_new());
	ASN1_INTEGER_set(X509_get_serialNumber(c->cert), 1);
	X509_set_version(c->cert, 2);
	X509_gmtime_adj(X509_get_notBefore(c->cert), 0);
	X509_gmtime_adj(X509_get_notAfter(c->cert), 60L*60*24*365);
	X509_set_pubkey(c->cert, c->pkey);
	name = X509_get_subject_name(c->cert);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
	    (unsigned char *)"localhost", -1, -1, 0);
	X509_set_issuer_name(c->cert, name);
	ext = X509V3_EXT_conf_nid(NULL, NULL, NID_basic_constraints,
	    "critical,CA:FALSE");
	X509_add_ext(c->cert, ext, -1);
	X509_EXTENSION_free(ext);
	ATF_REQUIRE(X509_sign(c->cert, c->pkey, EVP_sha256()) > 0);

	/*
	 * Create random filled file with memory mapping.
	 */
	ATF_REQUIRE((c->fd = mkstemp(tempname)) > 0);
	ATF_REQUIRE(unlink(tempname) == 0);
	ATF_REQUIRE(ftruncate(c->fd, FSIZE) == 0);
	ATF_REQUIRE((c->mfd = mmap(NULL, FSIZE, PROT_READ | PROT_WRITE,
	    MAP_SHARED, c->fd, 0)) != MAP_FAILED);
	arc4random_buf(c->mfd, FSIZE);

	ATF_REQUIRE(pthread_mutex_init(&c->mtx, NULL) == 0);
	ATF_REQUIRE(pthread_cond_init(&c->cv, NULL) == 0);

	/*
	 * Start server and wait for it to finish bind(2) + listen(2).
	 */
	ATF_REQUIRE(pthread_mutex_lock(&c->mtx) == 0);
	ATF_REQUIRE(pthread_create(&c->thr, NULL, server_thread, c) == 0);
	if (c->state != READY)
		ATF_REQUIRE(pthread_cond_wait(&c->cv, &c->mtx) == 0);
	ATF_REQUIRE(c->state == READY);
	ATF_REQUIRE(pthread_mutex_unlock(&c->mtx) == 0);

	/*
	 * Connect client.
	 */
	ATF_REQUIRE(c->ctx = SSL_CTX_new(TLS_client_method()));
	ATF_REQUIRE(X509_STORE_add_cert(SSL_CTX_get_cert_store(c->ctx),
	    c->cert));
	ATF_REQUIRE(ssl = c->cln = SSL_new(c->ctx));
	ATF_REQUIRE((c->cs = socket(AF_INET, SOCK_STREAM, 0)) > 0);
	ATF_REQUIRE(connect(c->cs, (struct sockaddr *)&(struct sockaddr_in)
	    { .sin_family = AF_INET, .sin_len = sizeof(struct sockaddr_in),
	      .sin_addr.s_addr = htonl(INADDR_LOOPBACK), .sin_port = c->port },
	    sizeof(struct sockaddr_in)) == 0);
	ATF_REQUIRE(SSL_set_fd(ssl, c->cs) == 1);
	ATF_REQUIRE(snprintf(hostname, sizeof(hostname), "localhost:%u",
	    ntohs(c->port)) >= (int)sizeof("localhost:0"));
	ATF_REQUIRE(SSL_set_tlsext_host_name(ssl, hostname) == 1);
	SSL_set_verify(ssl, SSL_VERIFY_PEER, NULL);
	ATF_REQUIRE(SSL_connect(ssl) == 1);
	SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
	ATF_REQUIRE(fcntl(c->cs, F_SETFL, O_NONBLOCK) != -1);
}

static void
common_cleanup(struct ctx *c)
{

	ATF_REQUIRE(pthread_mutex_lock(&c->mtx) == 0);
	c->state = EXITING;
	ATF_REQUIRE(pthread_cond_signal(&c->cv) == 0);
	ATF_REQUIRE(pthread_mutex_unlock(&c->mtx) == 0);
	ATF_REQUIRE(pthread_join(c->thr, NULL) == 0);

	ATF_REQUIRE(pthread_mutex_destroy(&c->mtx) == 0);
	ATF_REQUIRE(pthread_cond_destroy(&c->cv) == 0);

	SSL_free(c->cln);
	SSL_CTX_free(c->ctx);
	X509_free(c->cert);
	EVP_PKEY_free(c->pkey);
}

static void *
server_thread(void *arg) {
	struct ctx *c = arg;
	SSL_CTX *srv;
	SSL *ssl;
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	int s;

	ATF_REQUIRE(srv = SSL_CTX_new(TLS_server_method()));
	ATF_REQUIRE(SSL_CTX_set_options(srv, SSL_OP_ENABLE_KTLS) &
	    SSL_OP_ENABLE_KTLS);
	SSL_CTX_use_PrivateKey(srv, c->pkey);
	SSL_CTX_use_certificate(srv, c->cert);
	ATF_REQUIRE((s = socket(AF_INET, SOCK_STREAM, 0)) > 0);
	ATF_REQUIRE(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &(socklen_t){1},
	     sizeof(int)) == 0);
	ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(getsockname(s, (struct sockaddr *)&sin,
	    &(socklen_t){ sizeof(sin) }) == 0);
	ATF_REQUIRE(listen(s, -1) == 0);

	ATF_REQUIRE(pthread_mutex_lock(&c->mtx) == 0);
	c->port = sin.sin_port;
	c->state = READY;
	ATF_REQUIRE(pthread_cond_signal(&c->cv) == 0);
	ATF_REQUIRE(pthread_mutex_unlock(&c->mtx) == 0);

	ATF_REQUIRE((c->ss = accept(s, NULL, NULL)) > 0);
	ssl = c->srv = SSL_new(srv);
	SSL_set_fd(ssl, c->ss);
	SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
	ATF_REQUIRE(SSL_accept(ssl) > 0);

	ATF_REQUIRE(pthread_mutex_lock(&c->mtx) == 0);
	while (c->state != EXITING) {
		if (c->state == RUNNING) {
			ATF_REQUIRE(fcntl(c->ss, F_SETFL,
			    c->nb ? O_NONBLOCK : 0) != -1);
			c->sbytes = SSL_sendfile(ssl, c->fd, c->offset,
			    c->size, 0);
			c->state = READY;
		}
		ATF_REQUIRE(c->state == READY);
		ATF_REQUIRE(pthread_cond_signal(&c->cv) == 0);
		ATF_REQUIRE(pthread_cond_wait(&c->cv, &c->mtx) == 0);
	}
	ATF_REQUIRE(pthread_mutex_unlock(&c->mtx) == 0);

	SSL_shutdown(ssl);
	SSL_free(ssl);
	close(c->ss);
	SSL_CTX_free(srv);
	close(s);

	return (NULL);
}

static void
sendme_locked(struct ctx *c, off_t offset, size_t size, bool nb)
{
	ATF_REQUIRE(c->state == READY);
	c->state = RUNNING;
	c->offset = offset;
	c->size = size;
	c->nb = nb;
	ATF_REQUIRE(pthread_cond_signal(&c->cv) == 0);
}

static void
sendme(struct ctx *c, off_t offset, size_t size, bool nb)
{
	ATF_REQUIRE(pthread_mutex_lock(&c->mtx) == 0);
	sendme_locked(c, offset, size, nb);
	ATF_REQUIRE(pthread_mutex_unlock(&c->mtx) == 0);
}

/*
 * Block until non-blocking socket has at least a byte.
 */
static int
SSL_read_b(SSL *ssl, void *buf, int size)
{
	int rv, fd;

	ATF_REQUIRE((fd = SSL_get_fd(ssl)) > 0);
	while ((rv = SSL_read(ssl, buf, size)) <= 0 &&
	    SSL_get_error(ssl, rv) == SSL_ERROR_WANT_READ)
		ATF_REQUIRE(poll(&(struct pollfd){ .fd = fd, .events = POLLIN },
		    1, INFTIM) == 1);

	return (rv);
}

ATF_TC_WITHOUT_HEAD(basic);
ATF_TC_BODY(basic, tc)
{
	struct ctx c;
	char buf[64];
	size_t nread;
	int n;

	common_init(&c);

	sendme(&c, 0, 0, false);
	nread = 0;
	while (nread < FSIZE && (n = SSL_read_b(c.cln, buf, sizeof(buf))) > 0) {
		ATF_REQUIRE(memcmp((char *)c.mfd + nread, buf, n) == 0);
		nread += n;
	}
	ATF_REQUIRE(nread == FSIZE);
	ATF_REQUIRE(c.sbytes == FSIZE);

	common_cleanup(&c);
}

ATF_TC_WITHOUT_HEAD(random);
ATF_TC_BODY(random, tc)
{
	struct ctx c;
#define	RSIZE	(256*1024)

	common_init(&c);

	for (u_int i = 0; i < 10; i++) {
		char buf[RSIZE];
		off_t offset;
		size_t size, n, nread, expect;

		offset = arc4random() % FSIZE;
		size = arc4random() % RSIZE;
		sendme(&c, offset, size, false);
		expect = offset + size < FSIZE ? size : FSIZE - offset;
		nread = 0;
		while (nread < expect &&
		    (n = SSL_read_b(c.cln, buf, sizeof(buf))) > 0) {
			ATF_REQUIRE(memcmp((char *)c.mfd + offset + nread, buf,
			    n) == 0);
			nread += n;
		}
		ATF_REQUIRE(nread == expect);
		ATF_REQUIRE(c.sbytes == (ssize_t)expect);
	}

        common_cleanup(&c);
}

/* Truncate the file while sendfile(2) is working on it. */
ATF_TC_WITHOUT_HEAD(truncate);
ATF_TC_BODY(truncate, tc)
{
	struct ctx c;
	char buf[128 * 1024];
	size_t nread;
	int n;
#define	TRUNC	(FSIZE - 1024)

	common_init(&c);

	ATF_REQUIRE(setsockopt(c.ss, SOL_SOCKET, SO_SNDBUF, &(int){FSIZE / 16},
	    sizeof(int)) == 0);
	ATF_REQUIRE(setsockopt(c.cs, SOL_SOCKET, SO_RCVBUF, &(int){FSIZE / 16},
	    sizeof(int)) == 0);

	sendme(&c, 0, 0, false);
	/* Make sure sender is waiting on the socket buffer. */
	while (poll(&(struct pollfd){ .fd = c.ss, .events = POLLOUT }, 1, 1)
	    != 0)
		;
	ATF_REQUIRE(ftruncate(c.fd, TRUNC) == 0);
	nread = 0;
	while (nread < TRUNC && (n = SSL_read_b(c.cln, buf, sizeof(buf))) > 0) {
		ATF_REQUIRE(memcmp((char *)c.mfd + nread, buf, n) == 0);
		nread += n;
	}
	ATF_REQUIRE(nread == TRUNC);
	ATF_REQUIRE(pthread_mutex_lock(&c.mtx) == 0);
	ATF_REQUIRE(c.sbytes == TRUNC);
	ATF_REQUIRE(pthread_mutex_unlock(&c.mtx) == 0);

	common_cleanup(&c);
}

/* Grow the file while sendfile(2) is working on it. */
ATF_TC_WITHOUT_HEAD(grow);
ATF_TC_BODY(grow, tc)
{
	struct ctx c;
	char buf[128 * 1024];
	size_t nread;
	void *map;
	int n;
#define	GROW	(FSIZE/2)

	common_init(&c);

	ATF_REQUIRE(setsockopt(c.ss, SOL_SOCKET, SO_SNDBUF, &(int){FSIZE / 16},
	    sizeof(int)) == 0);
	ATF_REQUIRE(setsockopt(c.cs, SOL_SOCKET, SO_RCVBUF, &(int){FSIZE / 16},
	    sizeof(int)) == 0);

	sendme(&c, 0, 0, false);
	/* Make sure sender is waiting on the socket buffer. */
	while (poll(&(struct pollfd){ .fd = c.ss, .events = POLLOUT }, 1, 1)
	    != 0)
		;
	/* Grow the file and create second map. */
	ATF_REQUIRE(ftruncate(c.fd, FSIZE + GROW) == 0);
	ATF_REQUIRE((map = mmap(NULL, GROW, PROT_READ | PROT_WRITE,
	    MAP_SHARED, c.fd, FSIZE)) != MAP_FAILED);
	arc4random_buf(map, GROW);

	/* Read out original part. */
	nread = 0;
	while (nread < FSIZE && (n = SSL_read_b(c.cln, buf,
	    FSIZE - nread > sizeof(buf) ? sizeof(buf) : FSIZE - nread)) > 0) {
		ATF_REQUIRE(memcmp((char *)c.mfd + nread, buf, n) == 0);
		nread += n;
	}
	ATF_REQUIRE(nread == FSIZE);
	/* Read out grown part. */
	nread = 0;
	while (nread < GROW && (n = SSL_read_b(c.cln, buf, sizeof(buf))) > 0) {
		ATF_REQUIRE(memcmp((char *)map + nread, buf, n) == 0);
		nread += n;
	}
	ATF_REQUIRE(nread == GROW);
	ATF_REQUIRE(pthread_mutex_lock(&c.mtx) == 0);
	ATF_REQUIRE(c.sbytes == FSIZE + GROW);
	ATF_REQUIRE(pthread_mutex_unlock(&c.mtx) == 0);

	common_cleanup(&c);
}

ATF_TC_WITHOUT_HEAD(offset_beyond_eof);
ATF_TC_BODY(offset_beyond_eof, tc)
{
	struct ctx c;

	common_init(&c);

	c.sbytes = -1;
	sendme(&c, FSIZE + 1, 0, false);
	ATF_REQUIRE(pthread_mutex_lock(&c.mtx) == 0);
	while (c.state != READY)
		ATF_REQUIRE(pthread_cond_wait(&c.cv, &c.mtx) == 0);
	ATF_REQUIRE(c.sbytes == 0);
	ATF_REQUIRE(pthread_mutex_unlock(&c.mtx) == 0);

	common_cleanup(&c);
}

/*
 * Prove that we can differentiate a short write due to EAGAIN from one due to
 * end of file.
 */
ATF_TC_WITHOUT_HEAD(eagain_vs_eof);
ATF_TC_BODY(eagain_vs_eof, tc)
{
	struct ctx c;
	char buf[16 * 1024];
	ssize_t nread;
	int n;

	common_init(&c);

	/*
	 * Exercise short write due to no buffer space on non-blocking
	 * socket.  Internall sendfile(2) returns -1 and errno == EAGAIN.
	 */
	ATF_REQUIRE(pthread_mutex_lock(&c.mtx) == 0);
	sendme_locked(&c, 0, FSIZE, true);
	while (c.state != READY)
		ATF_REQUIRE(pthread_cond_wait(&c.cv, &c.mtx) == 0);
	ATF_REQUIRE(c.sbytes > 0);
	ATF_REQUIRE(SSL_get_error(c.srv, c.sbytes) == 0);
#if 0	/* see https://github.com/openssl/openssl/issues/29742 */
	ATF_REQUIRE(BIO_should_retry(SSL_get_wbio(c.srv)));
#endif

	/*
	 * Exercise second attempt on already full buffer.
	 */
	sendme_locked(&c, 0, FSIZE, true);
	while (c.state != READY)
		ATF_REQUIRE(pthread_cond_wait(&c.cv, &c.mtx) == 0);
	ATF_REQUIRE(c.sbytes == -1);
	ATF_REQUIRE(SSL_get_error(c.srv, c.sbytes) == SSL_ERROR_WANT_WRITE);
	ATF_REQUIRE(BIO_should_retry(SSL_get_wbio(c.srv)));

	/* Clear the buffer. */
	nread = 0;
	while (nread < c.sbytes &&
	    (n = SSL_read_b(c.cln, buf, sizeof(buf))) > 0) {
		ATF_REQUIRE(memcmp((char *)c.mfd + nread, buf, n) == 0);
		nread += n;
	}

	/*
	 * Exercise zero length write: offset == file size.
	 *
	 * SSL_ERROR_SYSCALL seems a strange error code, as the syscall did not
	 * fail, and errno is clear, because a request to send 0 bytes is
	 * legitimate one.  This test just documents the existing behavior
	 * rather than asserts that this is a correct behavior.
	 */
	sendme_locked(&c, FSIZE, 0, true);
	while (c.state != READY)
		ATF_REQUIRE(pthread_cond_wait(&c.cv, &c.mtx) == 0);
	ATF_REQUIRE(c.sbytes == 0);
	ATF_REQUIRE(SSL_get_error(c.srv, c.sbytes) == SSL_ERROR_SYSCALL);
#if 0	/* see https://github.com/openssl/openssl/issues/29742 */
	ATF_REQUIRE(!BIO_should_retry(SSL_get_wbio(c.srv)));
#endif

	/*
	 * Exercise short write due to end of file.
	 */
	sendme_locked(&c, FSIZE - 100, 0, true);
	while (c.state != READY)
		ATF_REQUIRE(pthread_cond_wait(&c.cv, &c.mtx) == 0);
	ATF_REQUIRE(c.sbytes == 100);
	ATF_REQUIRE(SSL_get_error(c.srv, c.sbytes) == 0);
#if 0	/* see https://github.com/openssl/openssl/issues/29742 */
	ATF_REQUIRE(!BIO_should_retry(SSL_get_wbio(c.srv)));
#endif

	ATF_REQUIRE(pthread_mutex_unlock(&c.mtx) == 0);

	common_cleanup(&c);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, basic);
	ATF_TP_ADD_TC(tp, random);
	ATF_TP_ADD_TC(tp, truncate);
	ATF_TP_ADD_TC(tp, grow);
	ATF_TP_ADD_TC(tp, offset_beyond_eof);
	ATF_TP_ADD_TC(tp, eagain_vs_eof);

	return atf_no_error();
}
