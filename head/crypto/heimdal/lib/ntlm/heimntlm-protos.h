/* This is a generated file */
#ifndef __heimntlm_protos_h__
#define __heimntlm_protos_h__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

int
heim_ntlm_build_ntlm1_master (
	void */*key*/,
	size_t /*len*/,
	struct ntlm_buf */*session*/,
	struct ntlm_buf */*master*/);

int
heim_ntlm_calculate_ntlm1 (
	void */*key*/,
	size_t /*len*/,
	unsigned char challange[8],
	struct ntlm_buf */*answer*/);

int
heim_ntlm_calculate_ntlm2 (
	const void */*key*/,
	size_t /*len*/,
	const char */*username*/,
	const char */*target*/,
	const unsigned char serverchallange[8],
	const struct ntlm_buf */*infotarget*/,
	unsigned char ntlmv2[16],
	struct ntlm_buf */*answer*/);

int
heim_ntlm_calculate_ntlm2_sess (
	const unsigned char clnt_nonce[8],
	const unsigned char svr_chal[8],
	const unsigned char ntlm_hash[16],
	struct ntlm_buf */*lm*/,
	struct ntlm_buf */*ntlm*/);

int
heim_ntlm_decode_targetinfo (
	const struct ntlm_buf */*data*/,
	int /*ucs2*/,
	struct ntlm_targetinfo */*ti*/);

int
heim_ntlm_decode_type1 (
	const struct ntlm_buf */*buf*/,
	struct ntlm_type1 */*data*/);

int
heim_ntlm_decode_type2 (
	const struct ntlm_buf */*buf*/,
	struct ntlm_type2 */*type2*/);

int
heim_ntlm_decode_type3 (
	const struct ntlm_buf */*buf*/,
	int /*ucs2*/,
	struct ntlm_type3 */*type3*/);

int
heim_ntlm_encode_targetinfo (
	const struct ntlm_targetinfo */*ti*/,
	int /*ucs2*/,
	struct ntlm_buf */*data*/);

int
heim_ntlm_encode_type1 (
	const struct ntlm_type1 */*type1*/,
	struct ntlm_buf */*data*/);

int
heim_ntlm_encode_type2 (
	const struct ntlm_type2 */*type2*/,
	struct ntlm_buf */*data*/);

int
heim_ntlm_encode_type3 (
	const struct ntlm_type3 */*type3*/,
	struct ntlm_buf */*data*/);

void
heim_ntlm_free_buf (struct ntlm_buf */*p*/);

void
heim_ntlm_free_targetinfo (struct ntlm_targetinfo */*ti*/);

void
heim_ntlm_free_type1 (struct ntlm_type1 */*data*/);

void
heim_ntlm_free_type2 (struct ntlm_type2 */*data*/);

void
heim_ntlm_free_type3 (struct ntlm_type3 */*data*/);

int
heim_ntlm_nt_key (
	const char */*password*/,
	struct ntlm_buf */*key*/);

void
heim_ntlm_ntlmv2_key (
	const void */*key*/,
	size_t /*len*/,
	const char */*username*/,
	const char */*target*/,
	unsigned char ntlmv2[16]);

int
heim_ntlm_verify_ntlm2 (
	const void */*key*/,
	size_t /*len*/,
	const char */*username*/,
	const char */*target*/,
	time_t /*now*/,
	const unsigned char serverchallange[8],
	const struct ntlm_buf */*answer*/,
	struct ntlm_buf */*infotarget*/,
	unsigned char ntlmv2[16]);

#ifdef __cplusplus
}
#endif

#endif /* __heimntlm_protos_h__ */
