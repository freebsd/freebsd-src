#ifndef lint
static char sccsid[] = 	"@(#)setkey.c	2.2 88/08/10 4.0 RPCSRC; from Copyr 1988 Sun Micro";
#endif
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * Copyright (C) 1986, Sun Microsystems, Inc. 
 */

/*
 * Do the real work of the keyserver .
 * Store secret keys. Compute common keys,
 * and use them to decrypt and encrypt DES keys .
 * Cache the common keys, so the
 * expensive computation is avoided. 
 */
#include <stdio.h>
#include <sys/file.h>
#include <mp.h>
#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#include <des_crypt.h>
#include <sys/errno.h>

extern char *malloc();
extern char ROOTKEY[];

static MINT *MODULUS;
static char *fetchsecretkey();
static keystatus pk_crypt();


/*
 * Set the modulus for all our Diffie-Hellman operations 
 */
setmodulus(modx)
	char *modx;
{
	MODULUS = xtom(modx);
}


/*
 * Set the secretkey key for this uid 
 */
keystatus
pk_setkey(uid, skey)
	short uid;
	keybuf skey;
{
	if (!storesecretkey(uid, skey)) {
		return (KEY_SYSTEMERR);
	}
	return (KEY_SUCCESS);
}


/*
 * Encrypt the key using the public key associated with remote_name and the
 * secret key associated with uid. 
 */
keystatus
pk_encrypt(uid, remote_name, key)
	short uid;
	char *remote_name;
	des_block *key;
{
	return (pk_crypt(uid, remote_name, key, DES_ENCRYPT));
}


/*
 * Decrypt the key using the public key associated with remote_name and the
 * secret key associated with uid. 
 */
keystatus
pk_decrypt(uid, remote_name, key)
	short uid;
	char *remote_name;
	des_block *key;
{
	return (pk_crypt(uid, remote_name, key, DES_DECRYPT));
}


/*
 * Do the work of pk_encrypt && pk_decrypt 
 */
static keystatus
pk_crypt(uid, remote_name, key, mode)
	short uid;
	char *remote_name;
	des_block *key;
	int mode;
{
	char *xsecret;
	char xpublic[HEXKEYBYTES + 1];
	char xsecret_hold[HEXKEYBYTES + 1];
	des_block deskey;
	int err;
	MINT *public;
	MINT *secret;
	MINT *common;
	char zero[8];

	xsecret = fetchsecretkey(uid);
	if (xsecret == NULL) {
		bzero(zero, sizeof(zero));
		xsecret = xsecret_hold;
		if (!getsecretkey("nobody", xsecret, zero) ||
		    xsecret[0] == 0) {
			return (KEY_NOSECRET);
		}
	}
	if (!getpublickey(remote_name, xpublic) &&
	    !getpublickey("nobody", xpublic)) {
		return (KEY_UNKNOWN);
	}
	if (!readcache(xpublic, xsecret, &deskey)) {
		public = xtom(xpublic);
		secret = xtom(xsecret);
		common = itom(0);
		pow(public, secret, MODULUS, common);
		extractdeskey(common, &deskey);
		writecache(xpublic, xsecret, &deskey);
		mfree(secret);
		mfree(public);
		mfree(common);
	}
	err = ecb_crypt(&deskey, key, sizeof(des_block), DES_HW | mode);
	if (DES_FAILED(err)) {
		return (KEY_SYSTEMERR);
	}
	return (KEY_SUCCESS);
}


/*
 * Choose middle 64 bits of the common key to use as our des key, possibly
 * overwriting the lower order bits by setting parity. 
 */
static
extractdeskey(ck, deskey)
	MINT *ck;
	des_block *deskey;
{
	MINT *a;
	short r;
	int i;
	short base = (1 << 8);
	char *k;


	a = itom(0);
	move(ck, a);
	for (i = 0; i < ((KEYSIZE - 64) / 2) / 8; i++) {
		sdiv(a, base, a, &r);
	}
	k = deskey->c;
	for (i = 0; i < 8; i++) {
		sdiv(a, base, a, &r);
		*k++ = r;
	}
	mfree(a);
	des_setparity(deskey);
}


/*
 * Key storage management
 */

struct secretkey_list {
	short uid;
	char secretkey[HEXKEYBYTES+1];
	struct secretkey_list *next;
};

static struct secretkey_list *g_secretkeys;

/*
 * Fetch the secret key for this uid 
 */
static char *
fetchsecretkey(uid)
	short uid;
{
	struct secretkey_list *l;

	for (l = g_secretkeys; l != NULL; l = l->next) {
		if (l->uid == uid) {
			return (l->secretkey);
		}
	}
	return (NULL);
}

/*
 * Store the secretkey for this uid 
 */
storesecretkey(uid, key)
	short uid;
	keybuf key;
{
	struct secretkey_list *new;
	struct secretkey_list **l;
	int nitems;


	nitems = 0;
	for (l = &g_secretkeys; *l != NULL && (*l)->uid != uid; 
	     l = &(*l)->next) {
		nitems++;
	}
	if (*l == NULL) {
		new = (struct secretkey_list *)malloc(sizeof(*new));
		if (new == NULL) {
			return (0);
		}
		new->uid = uid;
		new->next = NULL;
		*l = new;
	} else {
		new = *l;
	}
	bcopy(key, new->secretkey, HEXKEYBYTES);
	new->secretkey[HEXKEYBYTES] = 0;
	seekitem(nitems);
	writeitem(uid, new->secretkey);
	return (1);
}


hexdigit(val)
	int val;
{
	return ("0123456789abcdef"[val]);

}
bin2hex(bin, hex, size)
	unsigned char *bin;
	unsigned char *hex;
	int size;
{
	int i;

	for (i = 0; i < size; i++) {
		*hex++ = hexdigit(*bin >> 4);
		*hex++ = hexdigit(*bin++ & 0xf);
	}
}

hexval(dig)
	char dig;
{
	if ('0' <= dig && dig <= '9') {
		return (dig - '0');
	} else if ('a' <= dig && dig <= 'f') {
		return (dig - 'a' + 10);
	} else if ('A' <= dig && dig <= 'F') {
		return (dig - 'A' + 10);
	} else {
		return (-1);
	}
}

hex2bin(hex, bin, size)
	unsigned char *hex;
	unsigned char *bin;
	int size;
{
	int i;

	for (i = 0; i < size; i++) {
		*bin = hexval(*hex++) << 4;
		*bin++ |= hexval(*hex++);
	}
}

static char KEYSTORE[] = "/etc/keystore";
FILE *kf;

openstore()
{
	kf = fopen(KEYSTORE, "r+");
	if (kf == NULL) {
		kf = fopen(KEYSTORE, "w+");
		if (kf == NULL) {
			return (0);
		}
	}
	setbuf(kf, NULL);
	return (1);
}

static char rootkey[KEYBYTES];
static int haverootkey;
struct storedkey {
	short uid;
	char crypt[KEYBYTES];
};

readkeys()
{
	struct secretkey_list *node;
	struct secretkey_list **l;
	int uid;
	char secretkey[HEXKEYBYTES+1];

	if (kf == NULL) {
		return;
	}
	l = &g_secretkeys;
	seekitem(0);
	while (readitem(&uid, secretkey)) {
		node = (struct secretkey_list *)malloc(sizeof(*node));
		if (node == NULL) {
			return;
		}
		node->uid = uid;
		bcopy(secretkey, node->secretkey, HEXKEYBYTES + 1);
		node->next = NULL;
		*l = node;
		l = &node->next;
	}
}

writekeys()
{
	struct secretkey_list *k;

	seekitem(0);
	for (k = g_secretkeys; k != NULL; k = k->next) {
		writeitem(k->uid, k->secretkey);
	}
}

seekitem(item)
	int item;
{
	if (kf != NULL) {
		fseek(kf, item * sizeof(struct storedkey), 0);
	}
}

writeitem(uid, key)
	int uid;
	char *key;
{
	struct storedkey item;
	char rootkey_tmp[KEYBYTES];
	int reencrypt;
	
	if (kf == NULL) {
		return (1);
	}
	if (uid == 0) {
		writerootkey(key);
		hex2bin(key, rootkey_tmp, KEYBYTES);
		reencrypt = (haverootkey &&
			     bcmp(rootkey, rootkey_tmp, KEYBYTES) != 0);
		bcopy(rootkey_tmp, rootkey, KEYBYTES);
		haverootkey = 1;
		if (reencrypt) {
			writekeys();
			return (1);
		}
	}
	if (!haverootkey) {
		return (1);
	}
	item.uid = uid;
	hex2bin(key, item.crypt, KEYBYTES);
	ecb_crypt(rootkey, item.crypt, KEYBYTES, DES_ENCRYPT|DES_HW);
	return (fwrite(&item, sizeof(item), 1, kf) >= 0);
}


readitem(uidp, key)
	int *uidp;
	char *key;
{
	struct storedkey item;

	if (!haverootkey || kf == NULL) {
		return (0);
	}
	if (fread(&item, sizeof(item), 1, kf) != 1) {
		return (0);
	}
	*uidp = item.uid;
	ecb_crypt(rootkey, item.crypt, KEYBYTES, DES_DECRYPT|DES_HW);
	bin2hex(item.crypt, key, KEYBYTES);
	key[HEXKEYBYTES] = 0;
	return (1);
}
	
/*
 * Root users store their key in /etc/$ROOTKEY so
 * that they can auto reboot without having to be
 * around to type a password. Storing this in a file
 * is rather dubious: it should really be in the EEPROM
 * so it does not go over the net for diskless machines.
 */
writerootkey(secret)
	char *secret;
{
	char newline = '\n';
	int fd;

	fd = open(ROOTKEY, O_WRONLY|O_TRUNC|O_CREAT, 0);
	if (fd < 0) {
		perror(ROOTKEY);
	} else {
		if (write(fd, secret, strlen(secret)) < 0 ||
		    write(fd, &newline, sizeof(newline)) < 0) {
			(void)fprintf(stderr, "%s: ", ROOTKEY);
			perror("write");
		}
		close(fd);
	}
}


/*
 * Exponential caching management
 */
struct cachekey_list {
	keybuf secret;
	keybuf public;
	des_block deskey;
	struct cachekey_list *next;
};
static struct cachekey_list *g_cachedkeys;


/*
 * cache result of expensive multiple precision exponential operation 
 */
static
writecache(pub, sec, deskey)
	char *pub;
	char *sec;
	des_block *deskey;
{
	struct cachekey_list *new;

	new = (struct cachekey_list *) malloc(sizeof(struct cachekey_list));
	if (new == NULL) {
		return;
	}
	bcopy(pub, new->public, sizeof(keybuf));
	bcopy(sec, new->secret, sizeof(keybuf));
	new->deskey = *deskey;
	new->next = g_cachedkeys;
	g_cachedkeys = new;
}

/*
 * Try to find the common key in the cache 
 */
static
readcache(pub, sec, deskey)
	char *pub;
	char *sec;
	des_block *deskey;
{
	struct cachekey_list *found;
	register struct cachekey_list **l;

#define cachehit(pub, sec, list)	\
		(bcmp(pub, (list)->public, sizeof(keybuf)) == 0 && \
		bcmp(sec, (list)->secret, sizeof(keybuf)) == 0)

	for (l = &g_cachedkeys;
	     (*l) != NULL && !cachehit(pub, sec, *l);
	     l = &(*l)->next);
	if ((*l) == NULL) {
		return (0);
	}
	found = *l;
	(*l) = (*l)->next;
	found->next = g_cachedkeys;
	g_cachedkeys = found;
	*deskey = found->deskey;
	return (1);
}
