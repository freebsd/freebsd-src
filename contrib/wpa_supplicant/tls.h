#ifndef TLS_H
#define TLS_H

struct tls_connection;

struct tls_keys {
	const u8 *master_key;
	size_t master_key_len;
	const u8 *client_random;
	size_t client_random_len;
	const u8 *server_random;
	size_t server_random_len;
};

/**
 * tls_init - initialize TLS library
 *
 * Returns: Context data to be used as @tls_ctx in calls to other functions,
 * or %NULL on failure.
 *
 * Called once during program startup.
 */
void * tls_init(void);

/**
 * tls_deinit - deinitialize TLS library
 * @tls_ctx: TLS context data from tls_init()
 *
 * Called once during program shutdown.
 */
void tls_deinit(void *tls_ctx);

/**
 * tls_get_errors - process pending errors
 * @tls_ctx: TLS context data from tls_init()
 *
 * Returns: Number of found error, 0 if no errors detected.
 *
 * Process all pending TLS errors.
 */
int tls_get_errors(void *tls_ctx);

/**
 * tls_connection_init - initialize a new TLS connection
 * @tls_ctx: TLS context data from tls_init()
 *
 * Returns: Connection context data, @conn for other function calls
 */
struct tls_connection * tls_connection_init(void *tls_ctx);

/**
 * tls_connection_deinit - free TLS connection data
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 *
 * Release all resources allocated for TLS connection.
 */
void tls_connection_deinit(void *tls_ctx, struct tls_connection *conn);

/**
 * tls_connection_established - has the TLS connection been completed?
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 *
 * Returns: 1 if TLS connection has been completed, 0 if not.
 */
int tls_connection_established(void *tls_ctx, struct tls_connection *conn);

/**
 * tls_connection_shutdown - shutdown TLS connection data.
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 *
 * Returns: 0 on success, -1 on failure
 *
 * Shutdown current TLS connection without releasing all resources. New
 * connection can be started by using the same @conn without having to call
 * tls_connection_init() or setting certificates etc. again. The new
 * connection should try to use session resumption.
 */
int tls_connection_shutdown(void *tls_ctx, struct tls_connection *conn);

/**
 * tls_connection_ca_cert - set trusted CA certificate for TLS connection
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @ca_cert: File name for CA certificate in PEM or DER format
 * @subject_match: String to match in the subject of the peer certificate or
 * %NULL to allow all subjects
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_connection_ca_cert(void *tls_ctx, struct tls_connection *conn,
			   const char *ca_cert, const char *subject_match);

/**
 * tls_global_ca_cert - set trusted CA certificate for all TLS connections
 * @tls_ctx: TLS context data from tls_init()
 * @ca_cert: File name for CA certificate in PEM or DER format
 * %NULL to allow all subjects
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_global_ca_cert(void *tls_ctx, const char *ca_cert);

/**
 * tls_connection_ca_cert - set trusted CA certificate for TLS connection
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @verify_peer: 1 = verify peer certificate
 * @subject_match: String to match in the subject of the peer certificate or
 * %NULL to allow all subjects
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_connection_set_verify(void *tls_ctx, struct tls_connection *conn,
			      int verify_peer, const char *subject_match);

/**
 * tls_connection_client_cert - set client certificate for TLS connection
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @client_cert: File name for client certificate in PEM or DER format
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_connection_client_cert(void *tls_ctx, struct tls_connection *conn,
			       const char *client_cert);

/**
 * tls_global_client_cert - set client certificate for all TLS connections
 * @tls_ctx: TLS context data from tls_init()
 * @client_cert: File name for client certificate in PEM or DER format
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_global_client_cert(void *tls_ctx, const char *client_cert);

/**
 * tls_connection_private_key - set private key for TLS connection
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @private_key: File name for client private key in PEM or DER format
 * @private_key_passwd: Passphrase for decrypted private key, %NULL if no
 * passphrase is used.
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_connection_private_key(void *tls_ctx, struct tls_connection *conn,
			       const char *private_key,
			       const char *private_key_passwd);

/**
 * tls_global_private_key - set private key for all TLS connections
 * @tls_ctx: TLS context data from tls_init()
 * @private_key: File name for client private key in PEM or DER format
 * @private_key_passwd: Passphrase for decrypted private key, %NULL if no
 * passphrase is used.
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_global_private_key(void *tls_ctx, const char *private_key,
			   const char *private_key_passwd);

/**
 * tls_connection_dh - set DH/DSA parameters for TLS connection
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @dh_file: File name for DH/DSA data in PEM format.
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_connection_dh(void *tls_ctx, struct tls_connection *conn,
		      const char *dh_file);

/**
 * tls_connection_get_keys - get master key and random data from TLS connection
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @keys: Structure of key/random data (filled on success)
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_connection_get_keys(void *tls_ctx, struct tls_connection *conn,
			    struct tls_keys *keys);

/**
 * tls_connection_handshake - process TLS handshake (client side)
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @in_data: Input data from TLS peer
 * @in_len: Input data length
 * @out_len: Length of the output buffer.
 *
 * Returns: pointer to output data, %NULL on failure
 *
 * Caller is responsible for freeing returned output data.
 */
u8 * tls_connection_handshake(void *tls_ctx, struct tls_connection *conn,
			      const u8 *in_data, size_t in_len,
			      size_t *out_len);

/**
 * tls_connection_servr_handshake - process TLS handshake (server side)
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @in_data: Input data from TLS peer
 * @in_len: Input data length
 * @out_len: Length of the output buffer.
 *
 * Returns: pointer to output data, %NULL on failure
 *
 * Caller is responsible for freeing returned output data.
 */
u8 * tls_connection_server_handshake(void *tls_ctx,
				     struct tls_connection *conn,
				     const u8 *in_data, size_t in_len,
				     size_t *out_len);

/**
 * tls_connection_encrypt - encrypt data into TLS tunnel
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @in_data: Pointer to plaintext data to be encrypted
 * @in_len: Input buffer length
 * @out_data: Pointer to output buffer (encrypted TLS data)
 * @out_len: Maximum @out_data length 
 *
 * Returns: Number of bytes written to @out_data, -1 on failure
 */
int tls_connection_encrypt(void *tls_ctx, struct tls_connection *conn,
			   u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len);

/**
 * tls_connection_decrypt - decrypt data from TLS tunnel
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @in_data: Pointer to input buffer (encrypted TLS data)
 * @in_len: Input buffer length
 * @out_data: Pointer to output buffer (decrypted data from TLS tunnel)
 * @out_len: Maximum @out_data length
 *
 * Returns: Number of bytes written to @out_data, -1 on failure
 */
int tls_connection_decrypt(void *tls_ctx, struct tls_connection *conn,
			   u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len);

/**
 * tls_connection_resumed - was session resumption used
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 *
 * Returns: 1 if current session used session resumption, 0 if not
 */
int tls_connection_resumed(void *tls_ctx, struct tls_connection *conn);

/**
 * tls_connection_set_master_key - configure master secret for TLS connection
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 * @key: TLS pre-master-secret
 * @key_len: length of @key in bytes
 *
 * Returns: 0 on success, -1 on failure
 */
int tls_connection_set_master_key(void *ssl_ctx, struct tls_connection *conn,
				  const u8 *key, size_t key_len);

/**
 * tls_connection_set_anon_dh - configure TLS connection to use anonymous DH
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 *
 * Returns: 0 on success, -1 on failure
 *
 * TODO: consider changing this to more generic routine for configuring allowed
 * ciphers
 */
int tls_connection_set_anon_dh(void *ssl_ctx, struct tls_connection *conn);

/**
 * tls_get_cipher - get current cipher name
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 *
 * Returns: 0 on success, -1 on failure
 *
 * Get the name of the currently used cipher.
 */
int tls_get_cipher(void *ssl_ctx, struct tls_connection *conn,
		   char *buf, size_t buflen);

/**
 * tls_connection_enable_workaround - enable TLS workaround options
 * @tls_ctx: TLS context data from tls_init()
 * @conn: Connection context data from tls_connection_init()
 *
 * Returns: 0 on success, -1 on failure
 *
 * This function is used to enable connection-specific workaround options for
 * buffer SSL/TLS implementations.
 */
int tls_connection_enable_workaround(void *ssl_ctx,
				     struct tls_connection *conn);

int tls_connection_client_hello_ext(void *ssl_ctx, struct tls_connection *conn,
				    int ext_type, const u8 *data,
				    size_t data_len);

#endif /* TLS_H */
