#ifndef EAP_SIM_DB_H
#define EAP_SIM_DB_H

#ifdef EAP_SIM

/* Initialize EAP-SIM database/authentication gateway interface.
 * Returns pointer to a private data structure. */
void * eap_sim_db_init(const char *config);

/* Deinitialize EAP-SIM database/authentication gateway interface.
 * priv is the pointer from eap_sim_db_init(). */
void eap_sim_db_deinit(void *priv);

/* Get GSM triplets for user name identity (identity_len bytes). In most cases,
 * the user name is '1' | IMSI, i.e., 1 followed by the IMSI in ASCII format.
 * priv is the pointer from eap_sim_db_init().
 * Returns the number of triplets received (has to be less than or equal to
 * max_chal) or -1 on error (e.g., user not found). rand, kc, and sres are
 * pointers to data areas for the triplets. */
int eap_sim_db_get_gsm_triplets(void *priv, const u8 *identity,
				size_t identity_len, int max_chal,
				u8 *rand, u8 *kc, u8 *sres);

/* Verify whether the given user identity (identity_len bytes) is known. In
 * most cases, the user name is '1' | IMSI, i.e., 1 followed by the IMSI in
 * ASCII format.
 * priv is the pointer from eap_sim_db_init().
 * Returns 0 if the user is found and GSM triplets would be available for it or
 * -1 on error (e.g., user not found or no triplets available). */
int eap_sim_db_identity_known(void *priv, const u8 *identity,
			      size_t identity_len);

#else /* EAP_SIM */
static inline void * eap_sim_db_init(const char *config)
{
	return NULL;
}

static inline void eap_sim_db_deinit(void *priv)
{
}
#endif /* EAP_SIM */

#endif /* EAP_SIM_DB_H */
