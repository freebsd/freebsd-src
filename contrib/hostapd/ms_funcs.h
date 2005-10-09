#ifndef MS_FUNCS_H
#define MS_FUNCS_H

void generate_nt_response(u8 *auth_challenge, u8 *peer_challenge,
			  u8 *username, size_t username_len,
			  u8 *password, size_t password_len,
			  u8 *response);
void generate_authenticator_response(u8 *password, size_t password_len,
				     u8 *peer_challenge,
				     u8 *auth_challenge,
				     u8 *username, size_t username_len,
				     u8 *nt_response, u8 *response);
void nt_challenge_response(u8 *challenge, u8 *password, size_t password_len,
			   u8 *response);

void challenge_response(u8 *challenge, u8 *password_hash, u8 *response);
void nt_password_hash(u8 *password, size_t password_len, u8 *password_hash);
void hash_nt_password_hash(u8 *password_hash, u8 *password_hash_hash);
void get_master_key(const u8 *password_hash_hash, const u8 *nt_response,
		    u8 *master_key);
void get_asymetric_start_key(const u8 *master_key, u8 *session_key,
			     size_t session_key_len, int is_send,
			     int is_server);

#endif /* MS_FUNCS_H */
