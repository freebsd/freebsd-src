# $FreeBSD$

LIB=		ssl
SHLIB_MAJOR=	3

NOLINT=		true

.include "../libcrypto/Makefile.inc"

SRCS=	bio_ssl.c s23_clnt.c s23_lib.c s23_meth.c s23_pkt.c s23_srvr.c \
	s2_clnt.c s2_enc.c s2_lib.c s2_meth.c s2_pkt.c s2_srvr.c \
	s3_both.c s3_clnt.c s3_enc.c s3_lib.c s3_meth.c s3_pkt.c \
	s3_srvr.c ssl_algs.c ssl_asn1.c ssl_cert.c ssl_ciph.c \
	ssl_err.c ssl_err2.c ssl_lib.c ssl_rsa.c ssl_sess.c ssl_stat.c \
	ssl_txt.c t1_clnt.c t1_enc.c t1_lib.c t1_meth.c t1_srvr.c

.if defined(WANT_OPENSSL_MANPAGES)
MAN3=	SSL_CIPHER_get_name.3 SSL_COMP_add_compression_method.3 \
	SSL_CTX_add_extra_chain_cert.3 SSL_CTX_add_session.3 SSL_CTX_ctrl.3 \
	SSL_CTX_flush_sessions.3 SSL_CTX_free.3 SSL_CTX_get_ex_new_index.3 \
	SSL_CTX_get_verify_mode.3 SSL_CTX_load_verify_locations.3 \
	SSL_CTX_new.3 SSL_CTX_sess_number.3 SSL_CTX_sess_set_cache_size.3 \
	SSL_CTX_sess_set_get_cb.3 SSL_CTX_sessions.3 SSL_CTX_set_cert_store.3 \
	SSL_CTX_set_cert_verify_callback.3 SSL_CTX_set_cipher_list.3 \
	SSL_CTX_set_client_CA_list.3 SSL_CTX_set_client_cert_cb.3 \
	SSL_CTX_set_default_passwd_cb.3 SSL_CTX_set_generate_session_id.3 \
	SSL_CTX_set_info_callback.3 SSL_CTX_set_max_cert_list.3 \
	SSL_CTX_set_mode.3 SSL_CTX_set_msg_callback.3 SSL_CTX_set_options.3 \
	SSL_CTX_set_quiet_shutdown.3 SSL_CTX_set_session_cache_mode.3 \
	SSL_CTX_set_session_id_context.3 SSL_CTX_set_ssl_version.3 \
	SSL_CTX_set_timeout.3 SSL_CTX_set_tmp_dh_callback.3 \
	SSL_CTX_set_tmp_rsa_callback.3 SSL_CTX_set_verify.3 \
	SSL_CTX_use_certificate.3 SSL_SESSION_free.3 \
	SSL_SESSION_get_ex_new_index.3 SSL_SESSION_get_time.3 SSL_accept.3 \
	SSL_alert_type_string.3 SSL_clear.3 SSL_connect.3 SSL_do_handshake.3 \
	SSL_free.3 SSL_get_SSL_CTX.3 SSL_get_ciphers.3 \
	SSL_get_client_CA_list.3 SSL_get_current_cipher.3 \
	SSL_get_default_timeout.3 SSL_get_error.3 \
	SSL_get_ex_data_X509_STORE_CTX_idx.3 SSL_get_ex_new_index.3 \
	SSL_get_fd.3 SSL_get_peer_cert_chain.3 SSL_get_peer_certificate.3 \
	SSL_get_rbio.3 SSL_get_session.3 SSL_get_verify_result.3 \
	SSL_get_version.3 SSL_library_init.3 SSL_load_client_CA_file.3 \
	SSL_new.3 SSL_pending.3 SSL_read.3 SSL_rstate_string.3 \
	SSL_session_reused.3 SSL_set_bio.3 SSL_set_connect_state.3 \
	SSL_set_fd.3 SSL_set_session.3 SSL_set_shutdown.3 \
	SSL_set_verify_result.3 SSL_shutdown.3 SSL_state_string.3 \
	SSL_want.3 SSL_write.3 d2i_SSL_SESSION.3 ssl.3
.endif

INCS=	kssl.h ssl.h ssl2.h ssl23.h ssl3.h tls1.h
INCSDIR=${INCLUDEDIR}/openssl

.include <bsd.lib.mk>

.PATH:	${LCRYPTO_SRC}/ssl \
	${.CURDIR}/man
