#ifndef CRYPTO_H
#define CRYPTO_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ntp_fp.h>
#include <ntp.h>
#include <ntp_stdlib.h>
#include "utilities.h"
#include "sntp-opts.h"

#define LEN_PKT_MAC	LEN_PKT_NOMAC + sizeof(u_int32)

/* #include "sntp-opts.h" */

struct key {
	struct key *	next;
	keyid_t		key_id;
	size_t		key_len;
	int		typei;
	char		typen[20];
	char		key_seq[64];
};

extern	int	auth_init(const char *keyfile, struct key **keys);
extern	void	get_key(keyid_t key_id, struct key **d_key);
extern	size_t	make_mac(const void *pkt_data, size_t pkt_len,
			 const struct key *cmp_key, void *digest,
			 size_t dig_sz);
extern	int	auth_md5(const void *pkt_data, size_t pkt_len,
			 size_t dig_len, const struct key *cmp_key);

#endif
