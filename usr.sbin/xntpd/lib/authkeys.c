/* authkeys.c,v 3.1 1993/07/06 01:07:51 jbj Exp
 * authkeys.c - routines to manage the storage of authentication keys
 */
#include <stdio.h>

#include "ntp_types.h"
#include "ntp_string.h"
#include "ntp_malloc.h"
#include "ntp_stdlib.h"

/*
 * Structure to store keys in in the hash table.
 */
struct savekey {
	struct savekey *next;
	union {
#ifdef	DES
	    U_LONG DES_key[2];
#endif
#ifdef	MD5
	    char MD5_key[32];
#endif
	} k;
	U_LONG keyid;
	u_short flags;
#ifdef	MD5
	int keylen;
#endif
};

#define	KEY_TRUSTED	0x1	/* this key is trusted */
#define	KEY_KNOWN	0x2	/* this key is known */

#ifdef	DES
#define	KEY_DES		0x100	/* this is a DES type key */
#endif

#ifdef	MD5
#define	KEY_MD5		0x200	/* this is a MD5 type key */
#endif

/*
 * The hash table.  This is indexed by the low order bits of the
 * keyid.  We make this fairly big for potentially busy servers.
 */
#define	HASHSIZE	64
#define	HASHMASK	((HASHSIZE)-1)
#define	KEYHASH(keyid)	((keyid) & HASHMASK)

struct savekey *key_hash[HASHSIZE];

U_LONG authkeynotfound;
U_LONG authkeylookups;
U_LONG authnumkeys;
U_LONG authuncached;
U_LONG authkeyuncached;
U_LONG authnokey;		/* calls to encrypt with no key */
U_LONG authencryptions;
U_LONG authdecryptions;
U_LONG authdecryptok;

/*
 * Storage for free key structures.  We malloc() such things but
 * never free them.
 */
struct savekey *authfreekeys;
int authnumfreekeys;

#define	MEMINC	12		/* number of new free ones to get at once */


#ifdef	DES
/*
 * Size of the key schedule
 */
#define	KEY_SCHED_SIZE	128	/* number of octets to store key schedule */

/*
 * The zero key, which we always have.  Store the permutted key
 * zero in here.
 */
#define	ZEROKEY_L	0x01010101	/* odd parity zero key */
#define	ZEROKEY_R	0x01010101	/* right half of same */
u_char DESzeroekeys[KEY_SCHED_SIZE];
u_char DESzerodkeys[KEY_SCHED_SIZE];
u_char DEScache_ekeys[KEY_SCHED_SIZE];
u_char DEScache_dkeys[KEY_SCHED_SIZE];
#endif

/*
 * The key cache.  We cache the last key we looked at here.
 */
U_LONG cache_keyid;
u_short cache_flags;

#ifdef	MD5
int	cache_keylen;
char	*cache_key;
#endif

/*
 * init_auth - initialize internal data
 */
void
init_auth()
{
	U_LONG zerokey[2];

	/*
	 * Initialize hash table and free list
	 */
	bzero((char *)key_hash, sizeof key_hash);
	cache_flags = cache_keyid = 0;

	authnumfreekeys =  authkeynotfound = authkeylookups = 0;
	authnumkeys = authuncached = authkeyuncached = authnokey = 0;
	authencryptions = authdecryptions = authdecryptok = 0;

#ifdef	DES
	/*
	 * Initialize the zero key
	 */
	zerokey[0] = ZEROKEY_L;
	zerokey[1] = ZEROKEY_R;
	/* could just zero all */
	DESauth_subkeys(zerokey, DESzeroekeys, DESzerodkeys);
#endif
}


/*
 * auth_findkey - find a key in the hash table
 */
struct savekey *
auth_findkey(keyno)
	U_LONG keyno;
{
	register struct savekey *sk;

	sk = key_hash[KEYHASH(keyno)];
	while (sk != 0) {
		if (keyno == sk->keyid)
			return sk;
		sk = sk->next;
	}
	return 0;
}


/*
 * auth_havekey - return whether a key is known
 */
int
auth_havekey(keyno)
	U_LONG keyno;
{
	register struct savekey *sk;

	if (keyno == 0 || (keyno == cache_keyid))
		return 1;

	sk = key_hash[KEYHASH(keyno)];
	while (sk != 0) {
		if (keyno == sk->keyid) {
			if (sk->flags & KEY_KNOWN)
				return 1;
			else {
				authkeynotfound++;
				return 0;
			}
		}
		sk = sk->next;
	}
	authkeynotfound++;
	return 0;
}


/*
 * authhavekey - return whether a key is known.  Permute and cache
 *		 the key as a side effect.
 */
int
authhavekey(keyno)
	U_LONG keyno;
{
	register struct savekey *sk;

	authkeylookups++;
	if (keyno == 0 || keyno == cache_keyid)
		return 1;

	sk = key_hash[KEYHASH(keyno)];
	while (sk != 0) {
		if (keyno == sk->keyid)
			break;
		sk = sk->next;
	}

	if (sk == 0 || !(sk->flags & KEY_KNOWN)) {
		authkeynotfound++;
		return 0;
	}
	
	cache_keyid = sk->keyid;
	cache_flags = sk->flags;
#ifdef	MD5
	if (sk->flags & KEY_MD5) {
	    cache_keylen = sk->keylen;
	    cache_key = (char *) sk->k.MD5_key;	/* XXX */
	    return 1;
	}
#endif

#ifdef	DES
	if (sk->flags & KEY_DES) {
	    DESauth_subkeys(sk->k.DES_key, DEScache_ekeys, DEScache_dkeys);
	    return 1;
	}
#endif
	return 0;
}


/*
 * auth_moremem - get some more free key structures
 */
int
auth_moremem()
{
	register struct savekey *sk;
	register int i;

	sk = (struct savekey *)malloc(MEMINC * sizeof(struct savekey));
	if (sk == 0)
		return 0;
	
	for (i = MEMINC; i > 0; i--) {
		sk->next = authfreekeys;
		authfreekeys = sk++;
	}
	authnumfreekeys += MEMINC;
	return authnumfreekeys;
}


/*
 * authtrust - declare a key to be trusted/untrusted
 */
void
authtrust(keyno, trust)
	U_LONG keyno;
	int trust;
{
	register struct savekey *sk;

	sk = key_hash[KEYHASH(keyno)];
	while (sk != 0) {
		if (keyno == sk->keyid)
			break;
		sk = sk->next;
	}

	if (sk == 0 && !trust)
		return;
	
	if (sk != 0) {
		if (cache_keyid == keyno)
			cache_flags = cache_keyid = 0;

		if (trust) {
			sk->flags |= KEY_TRUSTED;
			return;
		}

		sk->flags &= ~KEY_TRUSTED;
		if (!(sk->flags & KEY_KNOWN)) {
			register struct savekey *skp;

			skp = key_hash[KEYHASH(keyno)];
			if (skp == sk) {
				key_hash[KEYHASH(keyno)] = sk->next;
			} else {
				while (skp->next != sk)
					skp = skp->next;
				skp->next = sk->next;
			}
			authnumkeys--;

			sk->next = authfreekeys;
			authfreekeys = sk;
			authnumfreekeys++;
		}
		return;
	}

	if (authnumfreekeys == 0)
		if (auth_moremem() == 0)
			return;

	sk = authfreekeys;
	authfreekeys = sk->next;
	authnumfreekeys--;

	sk->keyid = keyno;
	sk->flags = KEY_TRUSTED;
	sk->next = key_hash[KEYHASH(keyno)];
	key_hash[KEYHASH(keyno)] = sk;
	authnumkeys++;
	return;
}


/*
 * authistrusted - determine whether a key is trusted
 */
int
authistrusted(keyno)
	U_LONG keyno;
{
	register struct savekey *sk;

	if (keyno == cache_keyid)
		return ((cache_flags & KEY_TRUSTED) != 0);

	authkeyuncached++;

	sk = key_hash[KEYHASH(keyno)];
	while (sk != 0) {
		if (keyno == sk->keyid)
			break;
		sk = sk->next;
	}

	if (sk == 0 || !(sk->flags & KEY_TRUSTED))
		return 0;
	return 1;
}



#ifdef	DES
/*
 * DESauth_setkey - set a key into the key array
 */
void
DESauth_setkey(keyno, key)
	U_LONG keyno;
	const U_LONG *key;
{
	register struct savekey *sk;

	/*
	 * See if we already have the key.  If so just stick in the
	 * new value.
	 */
	sk = key_hash[KEYHASH(keyno)];
	while (sk != 0) {
		if (keyno == sk->keyid) {
			sk->k.DES_key[0] = key[0];
			sk->k.DES_key[1] = key[1];
			sk->flags |= KEY_KNOWN | KEY_DES;
			if (cache_keyid == keyno)
				cache_flags = cache_keyid = 0;
			return;
		}
		sk = sk->next;
	}

	/*
	 * Need to allocate new structure.  Do it.
	 */
	if (authnumfreekeys == 0) {
		if (auth_moremem() == 0)
			return;
	}

	sk = authfreekeys;
	authfreekeys = sk->next;
	authnumfreekeys--;

	sk->k.DES_key[0] = key[0];
	sk->k.DES_key[1] = key[1];
	sk->keyid = keyno;
	sk->flags = KEY_KNOWN | KEY_DES;
	sk->next = key_hash[KEYHASH(keyno)];
	key_hash[KEYHASH(keyno)] = sk;
	authnumkeys++;
	return;
}
#endif

#ifdef	MD5
void
MD5auth_setkey(keyno, key)
    U_LONG keyno;
    const U_LONG *key;
{
	register struct savekey *sk;

	/*
	 * See if we already have the key.  If so just stick in the
	 * new value.
	 */
	sk = key_hash[KEYHASH(keyno)];
	while (sk != 0) {
		if (keyno == sk->keyid) {
			strncpy(sk->k.MD5_key, (char *)key, sizeof(sk->k.MD5_key));
			if ((sk->keylen = strlen((char *)key)) > 
			                          sizeof(sk->k.MD5_key))
			    sk->keylen = sizeof(sk->k.MD5_key);

			sk->flags |= KEY_KNOWN | KEY_MD5;
			if (cache_keyid == keyno)
				cache_flags = cache_keyid = 0;
			return;
		}
		sk = sk->next;
	}

	/*
	 * Need to allocate new structure.  Do it.
	 */
	if (authnumfreekeys == 0) {
		if (auth_moremem() == 0)
			return;
	}

	sk = authfreekeys;
	authfreekeys = sk->next;
	authnumfreekeys--;

	strncpy(sk->k.MD5_key, (char *)key, sizeof(sk->k.MD5_key));
	if ((sk->keylen = strlen((char *)key)) > sizeof(sk->k.MD5_key))
	    sk->keylen = sizeof(sk->k.MD5_key);

	sk->keyid = keyno;
	sk->flags = KEY_KNOWN | KEY_MD5;
	sk->next = key_hash[KEYHASH(keyno)];
	key_hash[KEYHASH(keyno)] = sk;
	authnumkeys++;
	return;
}
#endif
    
/*
 * auth_delkeys - delete all known keys, in preparation for rereading
 *		  the keys file (presumably)
 */
void
auth_delkeys()
{
	register struct savekey *sk;
	register struct savekey **skp;
	register int i;

	for (i = 0; i < HASHSIZE; i++) {
		skp = &(key_hash[i]);
		sk = key_hash[i];
		while (sk != 0) {
			sk->flags &= ~(KEY_KNOWN
#ifdef	MD5
				       | KEY_MD5
#endif
#ifdef	DES
				       | KEY_DES
#endif
				       );
			if (sk->flags == 0) {
				*skp = sk->next;
				authnumkeys--;
				sk->next = authfreekeys;
				authfreekeys = sk;
				authnumfreekeys++;
				sk = *skp;
			} else {
				skp = &(sk->next);
				sk = sk->next;
			}
		}
	}
}


/*
 *  auth1crypt - support for two stage encryption, part 1.
 */
void
auth1crypt(keyno, pkt, length)
	U_LONG keyno;
	U_LONG *pkt;
	int length;	/* length of all encrypted data */
{
    if (keyno && keyno != cache_keyid) {
	authkeyuncached++;
	if (!authhavekey(keyno)) {
	    authnokey++;
	    return;
	}
    }

#ifdef	DES
    if (!keyno || (cache_flags & KEY_DES)) {
	DESauth1crypt(keyno, pkt, length);
	return;
    }
#endif

#ifdef	MD5
    if (cache_flags & KEY_MD5) {
	MD5auth1crypt(keyno, pkt, length);
	return;
    }
#endif
}


/*
 *  auth1crypt - support for two stage encryption, part 1.
 */
int
auth2crypt(keyno, pkt, length)
	U_LONG keyno;
	U_LONG *pkt;
	int length;	/* total length of encrypted area */
{
    if (keyno && keyno != cache_keyid) {
	authkeyuncached++;
	if (!authhavekey(keyno)) {
	    authnokey++;
	    return 0;
	}
    }

#ifdef	DES
    if (!keyno || (cache_flags & KEY_DES))
	return DESauth2crypt(keyno, pkt, length);
#endif

#ifdef	MD5
    if (cache_flags & KEY_MD5)
	return MD5auth2crypt(keyno, pkt, length);
#endif

    return 0;
}

int
authencrypt(keyno, pkt, length)
	U_LONG keyno;
	U_LONG *pkt;
	int length;	/* length of encrypted portion of packet */
{
    int sendlength = 0;

    if (keyno && keyno != cache_keyid) {
	authkeyuncached++;
	if (!authhavekey(keyno)) {
	    authnokey++;
	    return 0;
	}
    }

#ifdef	DES
    if (!keyno || (cache_flags & KEY_DES))
	return sendlength = DESauthencrypt(keyno, pkt, length);
#endif

#ifdef	MD5
    if (cache_flags & KEY_MD5)
	return MD5authencrypt(keyno, pkt, length);
#endif
    return 0;
}


int
authdecrypt(keyno, pkt, length)
    U_LONG keyno;
    U_LONG *pkt;
    int length;	/* length of variable data in octets */
{
    if (keyno && (keyno != cache_keyid)) {
	authkeyuncached++;
	if (!authhavekey(keyno)) {
	    authnokey++;
	    return 0;
	}
    }

#ifdef	DES
    if (!keyno || (cache_flags & KEY_DES))
	return DESauthdecrypt(keyno, pkt, length);
#endif

#ifdef	MD5
    if (cache_flags & KEY_MD5)
	return MD5authdecrypt(keyno, pkt, length);
#endif

    return 0;
}
