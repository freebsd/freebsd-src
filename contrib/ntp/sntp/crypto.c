#include <config.h>
#include "crypto.h"
#include <ctype.h>
#include "isc/string.h"
#include "libssl_compat.h"

struct key *key_ptr;
size_t key_cnt = 0;

int
make_mac(
	const void *pkt_data,
	int pkt_size,
	int mac_size,
	const struct key *cmp_key,
	void * digest
	)
{
	u_int		len = mac_size;
	int		key_type;
	EVP_MD_CTX *	ctx;
	
	if (cmp_key->key_len > 64)
		return 0;
	if (pkt_size % 4 != 0)
		return 0;

	INIT_SSL();
	key_type = keytype_from_text(cmp_key->type, NULL);
	
	ctx = EVP_MD_CTX_new();
	EVP_DigestInit(ctx, EVP_get_digestbynid(key_type));
	EVP_DigestUpdate(ctx, (const u_char *)cmp_key->key_seq, (u_int)cmp_key->key_len);
	EVP_DigestUpdate(ctx, pkt_data, (u_int)pkt_size);
	EVP_DigestFinal(ctx, digest, &len);
	EVP_MD_CTX_free(ctx);
	
	return (int)len;
}


/* Generates a md5 digest of the key specified in keyid concatenated with the 
 * ntp packet (exluding the MAC) and compares this digest to the digest in
 * the packet's MAC. If they're equal this function returns 1 (packet is 
 * authentic) or else 0 (not authentic).
 */
int
auth_md5(
	const void *pkt_data,
	int pkt_size,
	int mac_size,
	const struct key *cmp_key
	)
{
	int  hash_len;
	int  authentic;
	char digest[20];
	const u_char *pkt_ptr; 
	if (mac_size > (int)sizeof(digest))
		return 0;
	pkt_ptr = pkt_data;
	hash_len = make_mac(pkt_ptr, pkt_size, sizeof(digest), cmp_key,
			    digest);
	if (!hash_len) {
		authentic = FALSE;
	} else {
		/* isc_tsmemcmp will be better when its easy to link
		 * with.  sntp is a 1-shot program, so snooping for
		 * timing attacks is Harder.
		 */
		authentic = !memcmp(digest, (const char*)pkt_data + pkt_size + 4,
				    hash_len);
	}
	return authentic;
}

static int
hex_val(
	unsigned char x
	)
{
	int val;

	if ('0' <= x && x <= '9')
		val = x - '0';
	else if ('a' <= x && x <= 'f')
		val = x - 'a' + 0xa;
	else if ('A' <= x && x <= 'F')
		val = x - 'A' + 0xA;
	else
		val = -1;

	return val;
}

/* Load keys from the specified keyfile into the key structures.
 * Returns -1 if the reading failed, otherwise it returns the 
 * number of keys it read
 */
int
auth_init(
	const char *keyfile,
	struct key **keys
	)
{
	FILE *keyf = fopen(keyfile, "r"); 
	struct key *prev = NULL;
	int scan_cnt, line_cnt = 0;
	char kbuf[200];
	char keystring[129];

	if (keyf == NULL) {
		if (debug)
			printf("sntp auth_init: Couldn't open key file %s for reading!\n", keyfile);
		return -1;
	}
	if (feof(keyf)) {
		if (debug)
			printf("sntp auth_init: Key file %s is empty!\n", keyfile);
		fclose(keyf);
		return -1;
	}
	key_cnt = 0;
	while (!feof(keyf)) {
		char * octothorpe;
		struct key *act;
		int goodline = 0;

		if (NULL == fgets(kbuf, sizeof(kbuf), keyf))
			continue;

		kbuf[sizeof(kbuf) - 1] = '\0';
		octothorpe = strchr(kbuf, '#');
		if (octothorpe)
			*octothorpe = '\0';
		act = emalloc(sizeof(*act));
		scan_cnt = sscanf(kbuf, "%d %9s %128s", &act->key_id, act->type, keystring);
		if (scan_cnt == 3) {
			int len = strlen(keystring);
			if (len <= 20) {
				act->key_len = len;
				memcpy(act->key_seq, keystring, len + 1);
				goodline = 1;
			} else if ((len & 1) != 0) {
				goodline = 0; /* it's bad */
			} else {
				int j;
				goodline = 1;
				act->key_len = len >> 1;
				for (j = 0; j < len; j+=2) {
					int val;
					val = (hex_val(keystring[j]) << 4) |
					       hex_val(keystring[j+1]);
					if (val < 0) {
						goodline = 0; /* it's bad */
						break;
					}
					act->key_seq[j>>1] = (char)val;
				}
			}
		}
		if (goodline) {
			act->next = NULL;
			if (NULL == prev)
				*keys = act;
			else
				prev->next = act;
			prev = act;
			key_cnt++;
		} else {
			msyslog(LOG_DEBUG, "auth_init: scanf %d items, skipping line %d.",
				scan_cnt, line_cnt);
			free(act);
		}
		line_cnt++;
	}
	fclose(keyf);
	
	key_ptr = *keys;
	return key_cnt;
}

/* Looks for the key with keyid key_id and sets the d_key pointer to the 
 * address of the key. If no matching key is found the pointer is not touched.
 */
void
get_key(
	int key_id,
	struct key **d_key
	)
{
	struct key *itr_key;

	if (key_cnt == 0)
		return;
	for (itr_key = key_ptr; itr_key; itr_key = itr_key->next) {
		if (itr_key->key_id == key_id) {
			*d_key = itr_key;
			break;
		}
	}
	return;
}
