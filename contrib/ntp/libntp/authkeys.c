/*
 * authkeys.c - routines to manage the storage of authentication keys
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <math.h>
#include <stdio.h>

#include "ntp.h"
#include "ntp_fp.h"
#include "ntpd.h"
#include "ntp_lists.h"
#include "ntp_string.h"
#include "ntp_malloc.h"
#include "ntp_stdlib.h"

/*
 * Structure to store keys in in the hash table.
 */
typedef struct savekey symkey;

struct savekey {
	symkey *	hlink;		/* next in hash bucket */
	DECL_DLIST_LINK(symkey, llink);	/* for overall & free lists */
	u_char *	secret;		/* shared secret */
	u_long		lifetime;	/* remaining lifetime */
	keyid_t		keyid;		/* key identifier */
	u_short		type;		/* OpenSSL digest NID */
	u_short		secretsize;	/* secret octets */
	u_short		flags;		/* KEY_ flags that wave */
};

/* define the payload region of symkey beyond the list pointers */
#define symkey_payload	secret

#define	KEY_TRUSTED	0x001	/* this key is trusted */

#ifdef DEBUG
typedef struct symkey_alloc_tag symkey_alloc;

struct symkey_alloc_tag {
	symkey_alloc *	link;
	void *		mem;		/* enable free() atexit */
};

symkey_alloc *	authallocs;
#endif	/* DEBUG */

static inline u_short	auth_log2(double x);
static void		auth_resize_hashtable(void);
static void		allocsymkey(symkey **, keyid_t,	u_short,
				    u_short, u_long, u_short, u_char *);
static void		freesymkey(symkey *, symkey **);
#ifdef DEBUG
static void		free_auth_mem(void);
#endif

symkey	key_listhead;		/* list of all in-use keys */;
/*
 * The hash table. This is indexed by the low order bits of the
 * keyid. We make this fairly big for potentially busy servers.
 */
#define	DEF_AUTHHASHSIZE	64
/*#define	HASHMASK	((HASHSIZE)-1)*/
#define	KEYHASH(keyid)	((keyid) & authhashmask)

int	authhashdisabled;
u_short	authhashbuckets = DEF_AUTHHASHSIZE;
u_short authhashmask = DEF_AUTHHASHSIZE - 1;
symkey **key_hash;

u_long authkeynotfound;		/* keys not found */
u_long authkeylookups;		/* calls to lookup keys */
u_long authnumkeys;		/* number of active keys */
u_long authkeyexpired;		/* key lifetime expirations */
u_long authkeyuncached;		/* cache misses */
u_long authnokey;		/* calls to encrypt with no key */
u_long authencryptions;		/* calls to encrypt */
u_long authdecryptions;		/* calls to decrypt */

/*
 * Storage for free symkey structures.  We malloc() such things but
 * never free them.
 */
symkey *authfreekeys;
int authnumfreekeys;

#define	MEMINC	16		/* number of new free ones to get */

/*
 * The key cache. We cache the last key we looked at here.
 */
keyid_t	cache_keyid;		/* key identifier */
u_char *cache_secret;		/* secret */
u_short	cache_secretsize;	/* secret length */
int	cache_type;		/* OpenSSL digest NID */
u_short cache_flags;		/* flags that wave */


/*
 * init_auth - initialize internal data
 */
void
init_auth(void)
{
	size_t newalloc;

	/*
	 * Initialize hash table and free list
	 */
	newalloc = authhashbuckets * sizeof(key_hash[0]);

	key_hash = erealloc(key_hash, newalloc);
	memset(key_hash, '\0', newalloc);

	INIT_DLIST(key_listhead, llink);

#ifdef DEBUG
	atexit(&free_auth_mem);
#endif
}


/*
 * free_auth_mem - assist in leak detection by freeing all dynamic
 *		   allocations from this module.
 */
#ifdef DEBUG
static void
free_auth_mem(void)
{
	symkey *	sk;
	symkey_alloc *	alloc;
	symkey_alloc *	next_alloc;

	while (NULL != (sk = HEAD_DLIST(key_listhead, llink))) {
		freesymkey(sk, &key_hash[KEYHASH(sk->keyid)]);
	}
	free(key_hash);
	key_hash = NULL;
	cache_keyid = 0;
	cache_flags = 0;
	for (alloc = authallocs; alloc != NULL; alloc = next_alloc) {
		next_alloc = alloc->link;
		free(alloc->mem);	
	}
	authfreekeys = NULL;
	authnumfreekeys = 0;
}
#endif	/* DEBUG */


/*
 * auth_moremem - get some more free key structures
 */
void
auth_moremem(
	int	keycount
	)
{
	symkey *	sk;
	int		i;
#ifdef DEBUG
	void *		base;
	symkey_alloc *	allocrec;
# define MOREMEM_EXTRA_ALLOC	(sizeof(*allocrec))
#else
# define MOREMEM_EXTRA_ALLOC	(0)
#endif

	i = (keycount > 0)
		? keycount
		: MEMINC;
	sk = emalloc_zero(i * sizeof(*sk) + MOREMEM_EXTRA_ALLOC);
#ifdef DEBUG
	base = sk;
#endif
	authnumfreekeys += i;

	for (; i > 0; i--, sk++) {
		LINK_SLIST(authfreekeys, sk, llink.f);
	}

#ifdef DEBUG
	allocrec = (void *)sk;
	allocrec->mem = base;
	LINK_SLIST(authallocs, allocrec, link);
#endif
}


/*
 * auth_prealloc_symkeys
 */
void
auth_prealloc_symkeys(
	int	keycount
	)
{
	int	allocated;
	int	additional;

	allocated = authnumkeys + authnumfreekeys;
	additional = keycount - allocated;
	if (additional > 0)
		auth_moremem(additional);
	auth_resize_hashtable();
}


static inline u_short
auth_log2(double x)
{
	return (u_short)(log10(x) / log10(2));
}


/*
 * auth_resize_hashtable
 *
 * Size hash table to average 4 or fewer entries per bucket initially,
 * within the bounds of at least 4 and no more than 15 bits for the hash
 * table index.  Populate the hash table.
 */
static void
auth_resize_hashtable(void)
{
	u_long		totalkeys;
	u_short		hashbits;
	u_short		hash;
	size_t		newalloc;
	symkey *	sk;

	totalkeys = authnumkeys + authnumfreekeys;
	hashbits = auth_log2(totalkeys / 4.0) + 1;
	hashbits = max(4, hashbits);
	hashbits = min(15, hashbits);

	authhashbuckets = 1 << hashbits;
	authhashmask = authhashbuckets - 1;
	newalloc = authhashbuckets * sizeof(key_hash[0]);

	key_hash = erealloc(key_hash, newalloc);
	memset(key_hash, '\0', newalloc);

	ITER_DLIST_BEGIN(key_listhead, sk, llink, symkey)
		hash = KEYHASH(sk->keyid);
		LINK_SLIST(key_hash[hash], sk, hlink);
	ITER_DLIST_END()
}


/*
 * allocsymkey - common code to allocate and link in symkey
 *
 * secret must be allocated with a free-compatible allocator.  It is
 * owned by the referring symkey structure, and will be free()d by
 * freesymkey().
 */
static void
allocsymkey(
	symkey **	bucket,
	keyid_t		id,
	u_short		flags,
	u_short		type,
	u_long		lifetime,
	u_short		secretsize,
	u_char *	secret
	)
{
	symkey *	sk;

	if (authnumfreekeys < 1)
		auth_moremem(-1);
	UNLINK_HEAD_SLIST(sk, authfreekeys, llink.f);
	DEBUG_ENSURE(sk != NULL);
	sk->keyid = id;
	sk->flags = flags;
	sk->type = type;
	sk->secretsize = secretsize;
	sk->secret = secret;
	sk->lifetime = lifetime;
	LINK_SLIST(*bucket, sk, hlink);
	LINK_TAIL_DLIST(key_listhead, sk, llink);
	authnumfreekeys--;
	authnumkeys++;
}


/*
 * freesymkey - common code to remove a symkey and recycle its entry.
 */
static void
freesymkey(
	symkey *	sk,
	symkey **	bucket
	)
{
	symkey *	unlinked;

	if (sk->secret != NULL) {
		memset(sk->secret, '\0', sk->secretsize);
		free(sk->secret);
	}
	UNLINK_SLIST(unlinked, *bucket, sk, hlink, symkey);
	DEBUG_ENSURE(sk == unlinked);
	UNLINK_DLIST(sk, llink);
	memset((char *)sk + offsetof(symkey, symkey_payload), '\0',
	       sizeof(*sk) - offsetof(symkey, symkey_payload));
	LINK_SLIST(authfreekeys, sk, llink.f);
	authnumkeys--;
	authnumfreekeys++;
}


/*
 * auth_findkey - find a key in the hash table
 */
struct savekey *
auth_findkey(
	keyid_t		id
	)
{
	symkey *	sk;

	for (sk = key_hash[KEYHASH(id)]; sk != NULL; sk = sk->hlink) {
		if (id == sk->keyid) {
			return sk;
		}
	}

	return NULL;
}


/*
 * auth_havekey - return TRUE if the key id is zero or known
 */
int
auth_havekey(
	keyid_t		id
	)
{
	symkey *	sk;

	if (0 == id || cache_keyid == id) {
		return TRUE;
	}

	for (sk = key_hash[KEYHASH(id)]; sk != NULL; sk = sk->hlink) {
		if (id == sk->keyid) {
			return TRUE;
		}
	}

	return FALSE;
}


/*
 * authhavekey - return TRUE and cache the key, if zero or both known
 *		 and trusted.
 */
int
authhavekey(
	keyid_t		id
	)
{
	symkey *	sk;

	authkeylookups++;
	if (0 == id || cache_keyid == id) {
		return TRUE;
	}

	/*
	 * Seach the bin for the key. If found and the key type
	 * is zero, somebody marked it trusted without specifying
	 * a key or key type. In this case consider the key missing.
	 */
	authkeyuncached++;
	for (sk = key_hash[KEYHASH(id)]; sk != NULL; sk = sk->hlink) {
		if (id == sk->keyid) {
			if (0 == sk->type) {
				authkeynotfound++;
				return FALSE;
			}
			break;
		}
	}

	/*
	 * If the key is not found, or if it is found but not trusted,
	 * the key is not considered found.
	 */
	if (NULL == sk) {
		authkeynotfound++;
		return FALSE;
	}
	if (!(KEY_TRUSTED & sk->flags)) {
		authnokey++;
		return FALSE;
	}

	/*
	 * The key is found and trusted. Initialize the key cache.
	 */
	cache_keyid = sk->keyid;
	cache_type = sk->type;
	cache_flags = sk->flags;
	cache_secret = sk->secret;
	cache_secretsize = sk->secretsize;

	return TRUE;
}


/*
 * authtrust - declare a key to be trusted/untrusted
 */
void
authtrust(
	keyid_t		id,
	u_long		trust
	)
{
	symkey **	bucket;
	symkey *	sk;
	u_long		lifetime;

	/*
	 * Search bin for key; if it does not exist and is untrusted,
	 * forget it.
	 */
	bucket = &key_hash[KEYHASH(id)];
	for (sk = *bucket; sk != NULL; sk = sk->hlink) {
		if (id == sk->keyid)
			break;
	}
	if (!trust && NULL == sk)
		return;

	/*
	 * There are two conditions remaining. Either it does not
	 * exist and is to be trusted or it does exist and is or is
	 * not to be trusted.
	 */	
	if (sk != NULL) {
		if (cache_keyid == id) {
			cache_flags = 0;
			cache_keyid = 0;
		}

		/*
		 * Key exists. If it is to be trusted, say so and
		 * update its lifetime. 
		 */
		if (trust > 0) {
			sk->flags |= KEY_TRUSTED;
			if (trust > 1)
				sk->lifetime = current_time + trust;
			else
				sk->lifetime = 0;
			return;
		}

		/* No longer trusted, return it to the free list. */
		freesymkey(sk, bucket);
		return;
	}

	/*
	 * keyid is not present, but the is to be trusted.  We allocate
	 * a new key, but do not specify a key type or secret.
	 */
	if (trust > 1) {
		lifetime = current_time + trust;
	} else {
		lifetime = 0;
	}
	allocsymkey(bucket, id, KEY_TRUSTED, 0, lifetime, 0, NULL);
}


/*
 * authistrusted - determine whether a key is trusted
 */
int
authistrusted(
	keyid_t		keyno
	)
{
	symkey *	sk;
	symkey **	bucket;

	if (keyno == cache_keyid)
		return !!(KEY_TRUSTED & cache_flags);

	authkeyuncached++;
	bucket = &key_hash[KEYHASH(keyno)];
	for (sk = *bucket; sk != NULL; sk = sk->hlink) {
		if (keyno == sk->keyid)
			break;
	}
	if (NULL == sk || !(KEY_TRUSTED & sk->flags)) {
		authkeynotfound++;
		return FALSE;
	}
	return TRUE;
}

/* Note: There are two locations below where 'strncpy()' is used. While
 * this function is a hazard by itself, it's essential that it is used
 * here. Bug 1243 involved that the secret was filled with NUL bytes
 * after the first NUL encountered, and 'strlcpy()' simply does NOT have
 * this behaviour. So disabling the fix and reverting to the buggy
 * behaviour due to compatibility issues MUST also fill with NUL and
 * this needs 'strncpy'. Also, the secret is managed as a byte blob of a
 * given size, and eventually truncating it and replacing the last byte
 * with a NUL would be a bug.
 * perlinger@ntp.org 2015-10-10
 */
void
MD5auth_setkey(
	keyid_t keyno,
	int	keytype,
	const u_char *key,
	size_t len
	)
{
	symkey *	sk;
	symkey **	bucket;
	u_char *	secret;
	size_t		secretsize;
	
	DEBUG_ENSURE(keytype <= USHRT_MAX);
	DEBUG_ENSURE(len < 4 * 1024);
	/*
	 * See if we already have the key.  If so just stick in the
	 * new value.
	 */
	bucket = &key_hash[KEYHASH(keyno)];
	for (sk = *bucket; sk != NULL; sk = sk->hlink) {
		if (keyno == sk->keyid) {
			/* TALOS-CAN-0054: make sure we have a new buffer! */
			if (NULL != sk->secret) {
				memset(sk->secret, 0, sk->secretsize);
				free(sk->secret);
			}
			sk->secret = emalloc(len);
			sk->type = (u_short)keytype;
			secretsize = len;
			sk->secretsize = (u_short)secretsize;
#ifndef DISABLE_BUG1243_FIX
			memcpy(sk->secret, key, secretsize);
#else
			/* >MUST< use 'strncpy()' here! See above! */
			strncpy((char *)sk->secret, (const char *)key,
				secretsize);
#endif
			if (cache_keyid == keyno) {
				cache_flags = 0;
				cache_keyid = 0;
			}
			return;
		}
	}

	/*
	 * Need to allocate new structure.  Do it.
	 */
	secretsize = len;
	secret = emalloc(secretsize);
#ifndef DISABLE_BUG1243_FIX
	memcpy(secret, key, secretsize);
#else
	/* >MUST< use 'strncpy()' here! See above! */
	strncpy((char *)secret, (const char *)key, secretsize);
#endif
	allocsymkey(bucket, keyno, 0, (u_short)keytype, 0,
		    (u_short)secretsize, secret);
#ifdef DEBUG
	if (debug >= 4) {
		size_t	j;

		printf("auth_setkey: key %d type %d len %d ", (int)keyno,
		    keytype, (int)secretsize);
		for (j = 0; j < secretsize; j++)
			printf("%02x", secret[j]);
		printf("\n");
	}	
#endif
}


/*
 * auth_delkeys - delete non-autokey untrusted keys, and clear all info
 *                except the trusted bit of non-autokey trusted keys, in
 *		  preparation for rereading the keys file.
 */
void
auth_delkeys(void)
{
	symkey *	sk;

	ITER_DLIST_BEGIN(key_listhead, sk, llink, symkey)
		if (sk->keyid > NTP_MAXKEY) {	/* autokey */
			continue;
		}

		/*
		 * Don't lose info as to which keys are trusted. Make
		 * sure there are no dangling pointers!
		 */
		if (KEY_TRUSTED & sk->flags) {
			if (sk->secret != NULL) {
				memset(sk->secret, 0, sk->secretsize);
				free(sk->secret);
				sk->secret = NULL; /* TALOS-CAN-0054 */
			}
			sk->secretsize = 0;
			sk->lifetime = 0;
		} else {
			freesymkey(sk, &key_hash[KEYHASH(sk->keyid)]);
		}
	ITER_DLIST_END()
}


/*
 * auth_agekeys - delete keys whose lifetimes have expired
 */
void
auth_agekeys(void)
{
	symkey *	sk;

	ITER_DLIST_BEGIN(key_listhead, sk, llink, symkey)
		if (sk->lifetime > 0 && current_time > sk->lifetime) {
			freesymkey(sk, &key_hash[KEYHASH(sk->keyid)]);
			authkeyexpired++;
		}
	ITER_DLIST_END()
	DPRINTF(1, ("auth_agekeys: at %lu keys %lu expired %lu\n",
		    current_time, authnumkeys, authkeyexpired));
}


/*
 * authencrypt - generate message authenticator
 *
 * Returns length of authenticator field, zero if key not found.
 */
size_t
authencrypt(
	keyid_t		keyno,
	u_int32 *	pkt,
	size_t		length
	)
{
	/*
	 * A zero key identifier means the sender has not verified
	 * the last message was correctly authenticated. The MAC
	 * consists of a single word with value zero.
	 */
	authencryptions++;
	pkt[length / 4] = htonl(keyno);
	if (0 == keyno) {
		return 4;
	}
	if (!authhavekey(keyno)) {
		return 0;
	}

	return MD5authencrypt(cache_type, cache_secret, pkt, length);
}


/*
 * authdecrypt - verify message authenticator
 *
 * Returns TRUE if authenticator valid, FALSE if invalid or not found.
 */
int
authdecrypt(
	keyid_t		keyno,
	u_int32 *	pkt,
	size_t		length,
	size_t		size
	)
{
	/*
	 * A zero key identifier means the sender has not verified
	 * the last message was correctly authenticated.  For our
	 * purpose this is an invalid authenticator.
	 */
	authdecryptions++;
	if (0 == keyno || !authhavekey(keyno) || size < 4) {
		return FALSE;
	}

	return MD5authdecrypt(cache_type, cache_secret, pkt, length,
			      size);
}
